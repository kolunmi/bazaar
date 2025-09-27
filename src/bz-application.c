/* bz-application.c
 *
 * Copyright 2025 Adam Masciola
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "BAZAAR::CORE"

#include "config.h"

// #include <bazaar-ui.h>
#include <glib/gi18n.h>

#include "bz-application-map-factory.h"
#include "bz-application.h"
#include "bz-backend-notification.h"
#include "bz-content-provider.h"
#include "bz-download-worker.h"
#include "bz-entry-cache-manager.h"
#include "bz-entry-group.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-flathub-state.h"
#include "bz-flatpak-entry.h"
#include "bz-flatpak-instance.h"
#include "bz-gnome-shell-search-provider.h"
#include "bz-inspector.h"
#include "bz-preferences-dialog.h"
#include "bz-result.h"
#include "bz-state-info.h"
#include "bz-transaction-manager.h"
#include "bz-util.h"
#include "bz-window.h"
#include "bz-yaml-parser.h"

struct _BzApplication
{
  AdwApplication parent_instance;

  GSettings       *settings;
  GHashTable      *config;
  GListModel      *blocklists;
  GListModel      *content_configs;
  GtkCssProvider  *css;
  GtkMapListModel *content_configs_to_files;

  gboolean   running;
  DexFuture *refresh_task;
  GTimer    *init_timer;
  DexFuture *notif_watch;

  BzEntryCacheManager        *cache;
  BzTransactionManager       *transactions;
  BzSearchEngine             *search_engine;
  BzGnomeShellSearchProvider *gs_search;

  BzFlatpakInstance *flatpak;
  DexChannel        *channel;
  char              *waiting_to_open_appstream;
  GFile             *waiting_to_open_file;
  BzFlathubState    *flathub;
  BzContentProvider *content_provider;

  GHashTable *last_installed_set;
  GListStore *groups;
  GHashTable *ids_to_groups;
  GListStore *installed_apps;

  BzApplicationMapFactory *entry_factory;
  GtkCustomFilter         *application_filter;
  BzApplicationMapFactory *application_factory;

  BzStateInfo *state;
};

G_DEFINE_FINAL_TYPE (BzApplication, bz_application, ADW_TYPE_APPLICATION)

BZ_DEFINE_DATA (
    open_flatpakref,
    OpenFlatpakref,
    {
      BzApplication *self;
      GFile         *file;
    },
    BZ_RELEASE_DATA (self, g_object_unref);
    BZ_RELEASE_DATA (file, g_object_unref))

static void
init_service_struct (BzApplication *self);

static DexFuture *
open_flatpakref_fiber (OpenFlatpakrefData *data);

static void
open_generic_id (BzApplication *self,
                 const char    *generic_id);

static void
transaction_success (BzApplication        *self,
                     BzTransaction        *transaction,
                     GHashTable           *errored,
                     BzTransactionManager *manager);

static DexFuture *
refresh_fiber (BzApplication *self);

static DexFuture *
watch_backend_notifs_fiber (BzApplication *self);

static void
refresh (BzApplication *self);

static GtkWindow *
new_window (BzApplication *self);

static void
open_appstream_take (BzApplication *self,
                     char          *appstream);

static void
open_flatpakref_take (BzApplication *self,
                      GFile         *file);

static void
command_line_open_location (BzApplication           *self,
                            GApplicationCommandLine *cmdline,
                            const char              *path);

static gint
cmp_group (BzEntryGroup *a,
           BzEntryGroup *b,
           gpointer      user_data);

static void
bz_application_dispose (GObject *object)
{
  BzApplication *self = BZ_APPLICATION (object);

  dex_clear (&self->refresh_task);
  dex_clear (&self->notif_watch);
  g_clear_object (&self->settings);
  g_clear_object (&self->blocklists);
  g_clear_object (&self->content_configs);
  g_clear_object (&self->transactions);
  g_clear_object (&self->content_provider);
  g_clear_object (&self->content_configs_to_files);
  g_clear_object (&self->css);
  g_clear_object (&self->search_engine);
  g_clear_object (&self->gs_search);
  g_clear_object (&self->flatpak);
  g_clear_object (&self->waiting_to_open_file);
  g_clear_object (&self->entry_factory);
  g_clear_object (&self->application_filter);
  g_clear_object (&self->application_factory);
  g_clear_object (&self->flathub);
  g_clear_object (&self->cache);
  g_clear_object (&self->groups);
  g_clear_object (&self->installed_apps);
  g_clear_object (&self->state);
  g_clear_pointer (&self->waiting_to_open_appstream, g_free);
  g_clear_pointer (&self->init_timer, g_timer_destroy);
  g_clear_pointer (&self->last_installed_set, g_hash_table_unref);
  g_clear_pointer (&self->ids_to_groups, g_hash_table_unref);

  G_OBJECT_CLASS (bz_application_parent_class)->dispose (object);
}

static void
bz_application_activate (GApplication *app)
{
}

static int
bz_application_command_line (GApplication            *app,
                             GApplicationCommandLine *cmdline)
{
  BzApplication *self               = BZ_APPLICATION (app);
  g_autoptr (GError) local_error    = NULL;
  gint argc                         = 0;
  g_auto (GStrv) argv               = NULL;
  g_autofree GStrv argv_shallow     = NULL;
  const char      *command          = NULL;
  gboolean         window_autostart = FALSE;
  g_autofree char *location         = NULL;

  argv = g_application_command_line_get_arguments (cmdline, &argc);
  g_debug ("Handling gapplication command line; argc=%d", argc);

  // This is hack to avoid reorganizing the whole arg parsing logic atm
  // but ideally it should be done at some point, and made so:
  // 1. service always autostarts if its not running already
  // 2. When executing the binary without any arguments, it defaults
  // to initializing the service (if its not running) and activating
  // the window.
  if (argv == NULL || argc < 2)
    {
      command          = "window";
      window_autostart = TRUE;
      argv             = NULL;
      argc             = 0;
    }
  else
    {
      command = argv[1];
      argc--;
      argv_shallow = g_memdup2 (argv + 1, sizeof (*argv) * argc);
    }

  if (g_strcmp0 (command, "--help") == 0)
    {
      if (self->running)
        {
          g_application_command_line_printerr (
              cmdline,
              "The Bazaar service is running. The available commands are:\n\n"
              "  window|open|status|query|transact|quit\n\n"
              "Add \"--help\" to a command to get information specific to that command.\n");
        }
      else
        {
          g_application_command_line_printerr (
              cmdline,
              "The Bazaar service is not running.\n"
              "The following commands will start the daemon:\n"
              "  bazaar service\n"
              "  bazaar window --auto-service\n"
              "Exiting...\n");
        }

      return EXIT_SUCCESS;
    }

  if (g_strcmp0 (command, "window") == 0)
    {
      g_autoptr (GOptionContext) pre_context = NULL;

      GOptionEntry entries[] = {
        { "auto-service", 0, 0, G_OPTION_ARG_NONE, &window_autostart, NULL },
        { NULL }
      };

      pre_context = g_option_context_new (NULL);
      g_option_context_set_help_enabled (pre_context, FALSE);
      g_option_context_set_ignore_unknown_options (pre_context, TRUE);
      g_option_context_add_main_entries (pre_context, entries, NULL);
      g_option_context_parse (pre_context, &argc, &argv_shallow, NULL);
    }

  if (window_autostart || g_strcmp0 (command, "service") == 0)
    {
      g_autoptr (GOptionContext) context        = NULL;
      gboolean help                             = FALSE;
      gboolean is_running                       = FALSE;
      g_auto (GStrv) blocklists_strv            = NULL;
      g_autoptr (GtkStringList) blocklists      = NULL;
      g_auto (GStrv) content_configs_strv       = NULL;
      g_autoptr (GtkStringList) content_configs = NULL;
      g_auto (GStrv) locations                  = NULL;

      GOptionEntry main_entries[] = {
        { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
        { "is-running", 0, 0, G_OPTION_ARG_NONE, &is_running, "Exit successfully if the Bazaar service is running" },
        { "extra-blocklist", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &blocklists_strv, "Add an extra blocklist to read from" },
        { "extra-content-config", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &content_configs_strv, "Add an extra yaml file with which to configure the app browser" },
        { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &locations, "flatpakref file to open" },
        { NULL }
      };

      context = g_option_context_new ("- an app center for GNOME");
      g_option_context_set_help_enabled (context, FALSE);
      g_option_context_add_main_entries (context, main_entries, NULL);
      g_option_context_set_ignore_unknown_options (context, window_autostart);
      if (!g_option_context_parse (context, &argc, &argv_shallow, &local_error))
        {
          g_application_command_line_printerr (cmdline, "%s\n", local_error->message);
          return EXIT_FAILURE;
        }

      if (!window_autostart)
        {
          if (help)
            {
              g_autofree char *help_text = NULL;

              g_option_context_set_summary (context, "Options for command \"service\"");

              help_text = g_option_context_get_help (context, TRUE, NULL);
              g_application_command_line_printerr (cmdline, "%s\n", help_text);
              return EXIT_SUCCESS;
            }

          if (is_running)
            return self->running ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      if (!self->running || !window_autostart)
        {
          if (self->running)
            {
              g_application_command_line_printerr (
                  cmdline,
                  "The Bazaar service is already running.\n"
                  "Invoke \"bazaar --help\" to for available commands.\n");
              return EXIT_FAILURE;
            }

          g_debug ("Starting daemon!");
          g_application_hold (G_APPLICATION (self));
          self->running = TRUE;

          init_service_struct (self);

          blocklists = gtk_string_list_new (NULL);
#ifdef HARDCODED_BLOCKLIST
          g_debug ("Bazaar was configured with a hardcoded blocklist at %s, adding that now...",
                   HARDCODED_BLOCKLIST);
          gtk_string_list_append (blocklists, HARDCODED_BLOCKLIST);
#endif
          if (blocklists_strv != NULL)
            gtk_string_list_splice (
                blocklists,
                g_list_model_get_n_items (G_LIST_MODEL (blocklists)),
                0,
                (const char *const *) blocklists_strv);

          content_configs = gtk_string_list_new (NULL);
#ifdef HARDCODED_CONTENT_CONFIG
          g_debug ("Bazaar was configured with a hardcoded curated content config at %s, adding that now...",
                   HARDCODED_CONTENT_CONFIG);
          gtk_string_list_append (content_configs, HARDCODED_CONTENT_CONFIG);
#endif
          if (content_configs_strv != NULL)
            gtk_string_list_splice (
                content_configs,
                g_list_model_get_n_items (G_LIST_MODEL (content_configs)),
                0,
                (const char *const *) content_configs_strv);

          g_clear_object (&self->blocklists);
          g_clear_object (&self->content_configs);
          self->blocklists      = G_LIST_MODEL (g_steal_pointer (&blocklists));
          self->content_configs = G_LIST_MODEL (g_steal_pointer (&content_configs));

          refresh (self);

          gtk_map_list_model_set_model (
              self->content_configs_to_files, self->content_configs);
          bz_state_info_set_blocklists (self->state, self->blocklists);
          bz_state_info_set_curated_configs (self->state, self->content_configs);
        }

      if (locations != NULL && *locations != NULL)
        {
          g_clear_pointer (&location, g_free);
          location = g_strdup (*locations);
        }
    }
  else if (!self->running)
    {
      g_application_command_line_printerr (
          cmdline,
          "The Bazaar service is not running.\n"
          "Invoke \"bazaar service\" to initialize the daemon.\n");
      return EXIT_FAILURE;
    }

  if (g_strcmp0 (command, "service") != 0)
    {
      g_autoptr (GOptionContext) context = NULL;

      context = g_option_context_new ("- an app center for GNOME");
      g_option_context_set_help_enabled (context, FALSE);

      if (g_strcmp0 (command, "window") == 0)
        {
          gboolean         help         = FALSE;
          gboolean         search       = FALSE;
          g_autofree char *search_text  = NULL;
          gboolean         auto_service = TRUE;
          GtkWindow       *window       = NULL;

          GOptionEntry main_entries[] = {
            { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
            { "search", 0, 0, G_OPTION_ARG_NONE, &search, "Immediately open the search dialog upon startup" },
            { "search-text", 0, 0, G_OPTION_ARG_STRING, &search_text, "Specify the initial text used with --search" },
            { "auto-service", 0, 0, G_OPTION_ARG_NONE, &auto_service, "Initialize the Bazaar service if not already running" },
            { NULL }
          };

          g_option_context_add_main_entries (context, main_entries, NULL);
          if (!g_option_context_parse (context, &argc, &argv_shallow, &local_error))
            {
              g_application_command_line_printerr (cmdline, "%s\n", local_error->message);
              return EXIT_FAILURE;
            }

          if (help)
            {
              g_autofree char *help_text = NULL;

              g_option_context_set_summary (context, "Options for command \"window\"");

              help_text = g_option_context_get_help (context, TRUE, NULL);
              g_application_command_line_printerr (cmdline, "%s\n", help_text);
              return EXIT_SUCCESS;
            }

          window = new_window (self);
          if (search)
            bz_window_search (BZ_WINDOW (window), search_text);
        }
      else if (g_strcmp0 (command, "open") == 0)
        {
          gboolean help            = FALSE;
          g_auto (GStrv) locations = NULL;

          GOptionEntry main_entries[] = {
            { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
            { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &locations, "flatpakref file to open" },
            { NULL }
          };

          g_option_context_add_main_entries (context, main_entries, NULL);
          if (!g_option_context_parse (context, &argc, &argv_shallow, &local_error))
            {
              g_application_command_line_printerr (cmdline, "%s\n", local_error->message);
              return EXIT_FAILURE;
            }

          if (help)
            {
              g_autofree char *help_text = NULL;

              g_option_context_set_summary (context, "Options for command \"open\"");

              help_text = g_option_context_get_help (context, TRUE, NULL);
              g_application_command_line_printerr (cmdline, "%s\n", help_text);
              return EXIT_SUCCESS;
            }

          if (locations == NULL || *locations == NULL)
            {
              g_application_command_line_printerr (cmdline, "Command \"open\" requires a file path argument\n");
              return EXIT_FAILURE;
            }

          /* Ensure there is instant visual feedback for the user */
          if (gtk_application_get_active_window (GTK_APPLICATION (self)) == NULL)
            new_window (self);

          g_clear_pointer (&location, g_free);
          /* Just take the first one for now */
          location = g_strdup (*locations);
        }
      else if (g_strcmp0 (command, "status") == 0)
        {
          gboolean help                            = FALSE;
          gboolean current_only                    = FALSE;
          g_autoptr (GListModel) transaction_model = NULL;
          guint    n_transactions                  = 0;
          gboolean current_found_candidate         = FALSE;

          GOptionEntry main_entries[] = {
            { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
            { "current-only", 0, 0, G_OPTION_ARG_NONE, &current_only, "Only output the currently active transaction" },
            { NULL }
          };

          g_option_context_add_main_entries (context, main_entries, NULL);
          if (!g_option_context_parse (context, &argc, &argv_shallow, &local_error))
            {
              g_application_command_line_printerr (cmdline, "%s\n", local_error->message);
              return EXIT_FAILURE;
            }

          if (help)
            {
              g_autofree char *help_text = NULL;

              g_option_context_set_summary (context, "Options for command \"status\"");

              help_text = g_option_context_get_help (context, TRUE, NULL);
              g_application_command_line_printerr (cmdline, "%s\n", help_text);
              return EXIT_SUCCESS;
            }

          g_object_get (self->transactions, "transactions", &transaction_model, NULL);
          n_transactions = g_list_model_get_n_items (G_LIST_MODEL (transaction_model));

          for (guint i = 0; i < n_transactions; i++)
            {
              g_autoptr (BzTransaction) transaction = NULL;
              g_autofree char *name                 = NULL;
              g_autoptr (GListModel) installs       = NULL;
              g_autoptr (GListModel) updates        = NULL;
              g_autoptr (GListModel) removals       = NULL;
              gboolean         pending              = FALSE;
              g_autofree char *status               = NULL;
              double           progress             = 0.0;
              gboolean         finished             = FALSE;
              gboolean         success              = FALSE;
              g_autofree char *error                = NULL;

              transaction = g_list_model_get_item (transaction_model, i);
              g_object_get (
                  transaction,
                  "name", &name,
                  "installs", &installs,
                  "updates", &updates,
                  "removals", &removals,
                  "pending", &pending,
                  "status", &status,
                  "progress", &progress,
                  "finished", &finished,
                  "success", &success,
                  "error", &error,
                  NULL);

              if (current_only)
                {
                  if (pending || finished)
                    continue;
                  current_found_candidate = TRUE;
                }

              g_application_command_line_print (
                  cmdline,
                  "%s:\n"
                  "  number of installs: %d\n"
                  "  number of updates: %d\n"
                  "  number of removals: %d\n"
                  "  status: %s\n"
                  "  progress: %.02f%%\n"
                  "  finished: %s\n"
                  "  success: %s\n"
                  "  error: %s\n\n",
                  name,
                  g_list_model_get_n_items (installs),
                  g_list_model_get_n_items (updates),
                  g_list_model_get_n_items (removals),
                  status != NULL ? status : "N/A",
                  progress * 100.0,
                  finished ? "true" : "false",
                  success ? "true" : "false",
                  error != NULL ? error : "N/A");

              if (current_only)
                break;
            }

          if (n_transactions == 0 || (current_only && !current_found_candidate))
            g_application_command_line_printerr (cmdline, "No active transactions\n");
        }
      else if (g_strcmp0 (command, "query") == 0)
        {
          g_application_command_line_printerr (cmdline, "This feature is currently disabled\n");
        }
      else if (g_strcmp0 (command, "transact") == 0)
        {
          g_application_command_line_printerr (cmdline, "This feature is currently disabled\n");
        }
      else if (g_strcmp0 (command, "quit") == 0)
        {
          g_application_quit (G_APPLICATION (self));
          self->running = FALSE;
        }
      else
        {
          g_application_command_line_printerr (
              cmdline,
              "Unrecognized command \"%s\"\n"
              "Invoke \"bazaar --help\" to for available commands.\n",
              command);
          return EXIT_FAILURE;
        }
    }

  if (location != NULL)
    command_line_open_location (self, cmdline, location);

  return EXIT_SUCCESS;
}

static gboolean
bz_application_local_command_line (GApplication *application,
                                   gchar      ***arguments,
                                   int          *exit_status)
{
  return FALSE;
}

static gboolean
bz_application_dbus_register (GApplication    *application,
                              GDBusConnection *connection,
                              const gchar     *object_path,
                              GError         **error)
{
  BzApplication *self = BZ_APPLICATION (application);
  return bz_gnome_shell_search_provider_set_connection (self->gs_search, connection, error);
}

static void
bz_application_dbus_unregister (GApplication    *application,
                                GDBusConnection *connection,
                                const gchar     *object_path)
{
  BzApplication *self = BZ_APPLICATION (application);
  bz_gnome_shell_search_provider_set_connection (self->gs_search, NULL, NULL);
}

static void
bz_application_class_init (BzApplicationClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class    = G_APPLICATION_CLASS (klass);

  object_class->dispose = bz_application_dispose;

  app_class->activate           = bz_application_activate;
  app_class->command_line       = bz_application_command_line;
  app_class->local_command_line = bz_application_local_command_line;
  app_class->dbus_register      = bz_application_dbus_register;
  app_class->dbus_unregister    = bz_application_dbus_unregister;

  g_type_ensure (BZ_TYPE_RESULT);
}

static void
bz_application_bazaar_inspector_action (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
  BzApplication *self      = user_data;
  BzInspector   *inspector = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  inspector = bz_inspector_new ();
  bz_inspector_set_state (inspector, self->state);

  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (inspector));
  gtk_window_present (GTK_WINDOW (inspector));
}

static void
bz_application_flatseal_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  BzApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (window != NULL)
    bz_show_error_for_widget (
        GTK_WIDGET (window),
        _ ("This functionality is currently disabled. It is recommended "
           "you download and install Flatseal to manage app permissions."));
}

static void
bz_application_donate_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  g_app_info_launch_default_for_uri (
      DONATE_LINK, NULL, NULL);
}

static void
bz_application_toggle_transactions_action (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data)
{
  BzApplication *self   = user_data;
  GtkWindow     *window = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  bz_window_toggle_transactions (BZ_WINDOW (window));
}

static void
bz_application_search_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  BzApplication *self         = user_data;
  GtkWindow     *window       = NULL;
  const char    *initial_text = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (window == NULL)
    window = new_window (self);

  if (parameter != NULL)
    initial_text = g_variant_get_string (parameter, NULL);

  bz_window_search (BZ_WINDOW (window), initial_text);
}

static void
bz_application_about_action (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  BzApplication   *self               = user_data;
  GtkWindow       *window             = NULL;
  AdwDialog       *dialog             = NULL;
  g_autofree char *translators_string = NULL;

  const char *developers[] = {
    C_ ("About Dialog Developer Credit", "Adam Masciola <kolunmi@posteo.net>"),
    /* This array MUST be NULL terminated */
    NULL
  };
  const char *translators[] = {
    C_ ("About Dialog Translator Credit", "Ahmed Najmawi"),
    C_ ("About Dialog Translator Credit", "AtomHare"),
    C_ ("About Dialog Translator Credit", "Azenyr"),
    C_ ("About Dialog Translator Credit", "Goudarz Jafari"),
    C_ ("About Dialog Translator Credit", "Jill Fiore (Lumaeris)"),
    C_ ("About Dialog Translator Credit", "João Victor (Leal)"),
    C_ ("About Dialog Translator Credit", "KiKaraage"),
    C_ ("About Dialog Translator Credit", "Lucosec"),
    C_ ("About Dialog Translator Credit", "Léane GRASSER"),
    C_ ("About Dialog Translator Credit", "Marcel Mrówka (Microwave)"),
    C_ ("About Dialog Translator Credit", "Peter Dave Hello"),
    C_ ("About Dialog Translator Credit", "Pietro F."),
    C_ ("About Dialog Translator Credit", "Shihfu Juan"),
    C_ ("About Dialog Translator Credit", "Shinsei"),
    C_ ("About Dialog Translator Credit", "Vlastimil Dědek"),
    C_ ("About Dialog Translator Credit", "asen23"),
    C_ ("About Dialog Translator Credit", "camegone"),
    C_ ("About Dialog Translator Credit", "renner"),
    C_ ("About Dialog Translator Credit", "robotta"),
    /* This array MUST be NULL terminated */
    NULL
  };

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  dialog = adw_about_dialog_new ();

  translators_string = g_strjoinv ("\n", (gchar **) translators);

  g_object_set (
      dialog,
      "application-name", "Bazaar",
      "application-icon", "io.github.kolunmi.Bazaar",
      "developer-name", _ ("Adam Masciola"),
      "translator-credits", translators_string,
      "version", PACKAGE_VERSION,
      "developers", developers,
      "copyright", "© 2025 Adam Masciola",
      "license-type", GTK_LICENSE_GPL_3_0,
      "website", "https://github.com/kolunmi/bazaar",
      "issue-url", "https://github.com/kolunmi/bazaar/issues",
      NULL);

  adw_dialog_present (dialog, GTK_WIDGET (window));
}

static void
bz_application_preferences_action (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  BzApplication *self        = user_data;
  GtkWindow     *window      = NULL;
  AdwDialog     *preferences = NULL;

  g_assert (BZ_IS_APPLICATION (self));

  window      = gtk_application_get_active_window (GTK_APPLICATION (self));
  preferences = bz_preferences_dialog_new (self->settings);

  adw_dialog_present (preferences, GTK_WIDGET (window));
}

static void
bz_application_refresh_action (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  refresh (self);
}

static void
bz_application_quit_action (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  BzApplication *self = user_data;

  g_assert (BZ_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  {                "quit",                bz_application_quit_action, NULL },
  {             "refresh",             bz_application_refresh_action, NULL },
  {         "preferences",         bz_application_preferences_action, NULL },
  {               "about",               bz_application_about_action, NULL },
  {              "search",              bz_application_search_action,  "s" },
  { "toggle-transactions", bz_application_toggle_transactions_action, NULL },
  {              "donate",              bz_application_donate_action, NULL },
  {            "flatseal",            bz_application_flatseal_action, NULL },
  {    "bazaar-inspector",    bz_application_bazaar_inspector_action, NULL },
};

static gpointer
map_strings_to_files (GtkStringObject *string,
                      gpointer         data)
{
  const char *path   = NULL;
  GFile      *result = NULL;

  path   = gtk_string_object_get_string (string);
  result = g_file_new_for_path (path);

  g_object_unref (string);
  return result;
}

static gpointer
map_generic_ids_to_groups (GtkStringObject *string,
                           BzApplication   *self)
{
  BzEntryGroup *group = NULL;

  if (bz_state_info_get_busy (self->state))
    return NULL;

  group = g_hash_table_lookup (
      self->ids_to_groups,
      gtk_string_object_get_string (string));

  g_object_unref (string);
  return group != NULL ? g_object_ref (group) : NULL;
}

static gpointer
map_ids_to_entries (GtkStringObject *string,
                    BzApplication   *self)
{
  g_autoptr (GError) local_error = NULL;
  const char *id                 = NULL;
  g_autoptr (DexFuture) future   = NULL;
  g_autoptr (BzResult) result    = NULL;

  if (bz_state_info_get_busy (self->state))
    return NULL;

  id     = gtk_string_object_get_string (string);
  future = bz_entry_cache_manager_get (self->cache, id);
  result = bz_result_new (future);

  g_object_unref (string);
  return g_steal_pointer (&result);
}

static gboolean
filter_application_ids (GtkStringObject *string,
                        BzApplication   *self)
{
  gboolean result = FALSE;

  if (bz_state_info_get_busy (self->state))
    return FALSE;

  result = g_hash_table_contains (
      self->ids_to_groups,
      gtk_string_object_get_string (string));
  return result;
}

static void
bz_application_init (BzApplication *self)
{
  self->running   = FALSE;
  self->gs_search = bz_gnome_shell_search_provider_new ();

  g_action_map_add_action_entries (
      G_ACTION_MAP (self),
      app_actions,
      G_N_ELEMENTS (app_actions),
      self);
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.quit",
      (const char *[]) { "<primary>q", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.refresh",
      (const char *[]) { "<primary>r", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.search('')",
      (const char *[]) { "<primary>f", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.toggle-transactions",
      (const char *[]) { "<primary>d", NULL });
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.bazaar-inspector",
      (const char *[]) { "<primary><alt><shift>i", NULL });
}

static void
init_service_struct (BzApplication *self)
{
  const char *app_id = NULL;
#ifdef HARDCODED_MAIN_CONFIG
  g_autoptr (GError) local_error  = NULL;
  g_autoptr (GFile) config_file   = NULL;
  g_autoptr (GBytes) config_bytes = NULL;
#endif
  GtkCustomFilter *filter = NULL;

  // bazaar_ui_init ();

#ifdef HARDCODED_MAIN_CONFIG
  config_file  = g_file_new_for_path (HARDCODED_MAIN_CONFIG);
  config_bytes = g_file_load_bytes (config_file, NULL, NULL, &local_error);
  if (config_bytes != NULL)
    {
      g_autoptr (BzYamlParser) parser      = NULL;
      g_autoptr (GHashTable) parse_results = NULL;

      parser = bz_yaml_parser_new_for_resource_schema (
          "/io/github/kolunmi/Bazaar/main-config-schema.xml");

      parse_results = bz_yaml_parser_process_bytes (
          parser, config_bytes, &local_error);
      if (parse_results != NULL)
        self->config = g_steal_pointer (&parse_results);
      else
        g_critical ("Could not load main config at %s: %s",
                    HARDCODED_MAIN_CONFIG, local_error->message);
    }
  else
    g_critical ("Could not load main config at %s: %s",
                HARDCODED_MAIN_CONFIG, local_error->message);

  g_clear_pointer (&local_error, g_error_free);
#endif

  self->init_timer = g_timer_new ();

  (void) bz_download_worker_get_default ();

  app_id = g_application_get_application_id (G_APPLICATION (self));
  g_assert (app_id != NULL);
  g_debug ("Constructing gsettings for %s ...", app_id);
  self->settings = g_settings_new (app_id);

  self->groups         = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->installed_apps = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->ids_to_groups  = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);

  self->entry_factory = bz_application_map_factory_new (
      (GtkMapListModelMapFunc) map_ids_to_entries,
      self, NULL, NULL, NULL);

  filter = gtk_custom_filter_new (
      (GtkCustomFilterFunc) filter_application_ids, self, NULL);
  self->application_filter  = g_object_ref_sink (filter);
  self->application_factory = bz_application_map_factory_new (
      (GtkMapListModelMapFunc) map_generic_ids_to_groups,
      self, NULL, NULL, GTK_FILTER (filter));

  self->search_engine = bz_search_engine_new ();
  bz_search_engine_set_model (self->search_engine, G_LIST_MODEL (self->groups));
  bz_gnome_shell_search_provider_set_engine (self->gs_search, self->search_engine);

  self->content_provider         = bz_content_provider_new ();
  self->content_configs_to_files = gtk_map_list_model_new (
      NULL, (GtkMapListModelMapFunc) map_strings_to_files, NULL, NULL);
  bz_content_provider_set_input_files (
      self->content_provider, G_LIST_MODEL (self->content_configs_to_files));
  bz_content_provider_set_factory (self->content_provider, self->application_factory);

  self->flathub = bz_flathub_state_new ();
  bz_flathub_state_set_map_factory (self->flathub, self->application_factory);

  self->transactions = bz_transaction_manager_new ();
  if (self->config != NULL)
    bz_transaction_manager_set_config (self->transactions, self->config);
  g_signal_connect_swapped (self->transactions, "success",
                            G_CALLBACK (transaction_success), self);

  self->state = bz_state_info_new ();
  bz_state_info_set_application_factory (self->state, self->application_factory);
  bz_state_info_set_blocklists (self->state, self->blocklists);
  bz_state_info_set_curated_provider (self->state, self->content_provider);
  bz_state_info_set_entry_factory (self->state, self->entry_factory);
  bz_state_info_set_flathub (self->state, self->flathub);
  bz_state_info_set_main_config (self->state, self->config);
  bz_state_info_set_search_engine (self->state, self->search_engine);
  bz_state_info_set_settings (self->state, self->settings);
  bz_state_info_set_transaction_manager (self->state, self->transactions);
}

static DexFuture *
open_flatpakref_fiber (OpenFlatpakrefData *data)
{
  BzApplication *self            = data->self;
  GFile         *file            = data->file;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (DexFuture) future   = NULL;
  GtkWindow    *window           = NULL;
  const GValue *value            = NULL;

  future = bz_backend_load_local_package (BZ_BACKEND (self->flatpak), file, NULL);
  dex_await (dex_ref (future), NULL);

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (window == NULL)
    window = new_window (self);

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      if (G_VALUE_HOLDS_OBJECT (value))
        {
          BzEntry    *entry         = NULL;
          const char *unique_id     = NULL;
          g_autoptr (BzEntry) equiv = NULL;

          entry     = g_value_get_object (value);
          unique_id = bz_entry_get_unique_id (entry);

          equiv = dex_await_object (
              bz_entry_cache_manager_get (self->cache, unique_id),
              NULL);

          if (equiv != NULL)
            {
              if (bz_entry_is_of_kinds (equiv, BZ_ENTRY_KIND_APPLICATION))
                {
                  const char   *generic_id = NULL;
                  BzEntryGroup *group      = NULL;

                  generic_id = bz_entry_get_id (entry);
                  group      = g_hash_table_lookup (self->ids_to_groups, generic_id);

                  if (group != NULL)
                    bz_window_show_group (BZ_WINDOW (window), group);
                  else
                    bz_window_show_entry (BZ_WINDOW (window), equiv);
                }
              else
                bz_window_show_entry (BZ_WINDOW (window), equiv);
            }
          else
            bz_window_show_entry (BZ_WINDOW (window), entry);
        }
      else
        open_generic_id (self, g_value_get_string (value));
    }
  else
    bz_show_error_for_widget (GTK_WIDGET (window), local_error->message);

  return NULL;
}

static void
open_generic_id (BzApplication *self,
                 const char    *generic_id)
{
  BzEntryGroup *group  = NULL;
  GtkWindow    *window = NULL;

  group = g_hash_table_lookup (self->ids_to_groups, generic_id);

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (window == NULL)
    window = new_window (self);

  if (group != NULL)
    bz_window_show_group (BZ_WINDOW (window), group);
  else
    {
      g_autofree char *message = NULL;

      message = g_strdup_printf ("ID '%s' was not found", generic_id);
      bz_show_error_for_widget (GTK_WIDGET (window), message);
    }
}

static void
transaction_success (BzApplication        *self,
                     BzTransaction        *transaction,
                     GHashTable           *errored,
                     BzTransactionManager *manager)
{
  GListModel *installs   = NULL;
  GListModel *removals   = NULL;
  guint       n_installs = 0;
  guint       n_removals = 0;

  installs = bz_transaction_get_installs (transaction);
  removals = bz_transaction_get_removals (transaction);

  if (installs != NULL)
    n_installs = g_list_model_get_n_items (installs);
  if (removals != NULL)
    n_removals = g_list_model_get_n_items (removals);

  for (guint i = 0; i < n_installs; i++)
    {
      g_autoptr (BzEntry) entry = NULL;
      const char *unique_id     = NULL;

      entry = g_list_model_get_item (installs, i);
      if (g_hash_table_contains (errored, entry))
        continue;

      bz_entry_set_installed (entry, TRUE);
      unique_id = bz_entry_get_unique_id (entry);
      g_hash_table_add (self->last_installed_set, g_strdup (unique_id));

      if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
        {
          BzEntryGroup *group = NULL;

          group = g_hash_table_lookup (self->ids_to_groups, bz_entry_get_id (entry));
          if (group != NULL)
            {
              gboolean found    = FALSE;
              guint    position = 0;

              found = g_list_store_find (self->installed_apps, group, &position);
              if (!found)
                g_list_store_insert_sorted (self->installed_apps, group, (GCompareDataFunc) cmp_group, NULL);
            }
        }
      dex_future_disown (bz_entry_cache_manager_add (self->cache, entry));
    }

  for (guint i = 0; i < n_removals; i++)
    {
      g_autoptr (BzEntry) entry = NULL;
      const char *unique_id     = NULL;

      entry = g_list_model_get_item (removals, i);
      if (g_hash_table_contains (errored, entry))
        continue;

      bz_entry_set_installed (entry, FALSE);
      unique_id = bz_entry_get_unique_id (entry);
      /* TODO this doesn't account for related refs */
      g_hash_table_remove (self->last_installed_set, unique_id);

      if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
        {
          BzEntryGroup *group = NULL;

          group = g_hash_table_lookup (self->ids_to_groups, bz_entry_get_id (entry));
          if (group != NULL && !bz_entry_group_get_removable (group))
            {
              gboolean found    = FALSE;
              guint    position = 0;

              found = g_list_store_find (self->installed_apps, group, &position);
              if (found)
                g_list_store_remove (self->installed_apps, position);
            }
        }
      dex_future_disown (bz_entry_cache_manager_add (self->cache, entry));
    }
}

static void
fiber_check_for_updates (BzApplication *self)
{
  g_autoptr (GError) local_error   = NULL;
  g_autoptr (GPtrArray) update_ids = NULL;
  GtkWindow *window                = NULL;

  g_debug ("Checking for updates...");
  bz_state_info_set_checking_for_updates (self->state, TRUE);

  update_ids = dex_await_boxed (
      bz_backend_retrieve_update_ids (BZ_BACKEND (self->flatpak), NULL),
      &local_error);
  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  if (update_ids != NULL)
    {
      if (update_ids->len > 0)
        {
          g_autoptr (GPtrArray) futures = NULL;
          g_autoptr (GListStore) store  = NULL;

          futures = g_ptr_array_new_with_free_func (dex_unref);
          for (guint i = 0; i < update_ids->len; i++)
            {
              const char *unique_id = NULL;

              unique_id = g_ptr_array_index (update_ids, i);
              g_ptr_array_add (futures, bz_entry_cache_manager_get (self->cache, unique_id));
            }

          dex_await (
              dex_future_allv ((DexFuture *const *) futures->pdata, futures->len),
              &local_error);

          store = g_list_store_new (BZ_TYPE_ENTRY);
          for (guint i = 0; i < futures->len; i++)
            {
              DexFuture    *future = NULL;
              const GValue *value  = NULL;

              future = g_ptr_array_index (futures, i);
              value  = dex_future_get_value (future, &local_error);

              if (value != NULL)
                g_list_store_append (store, g_value_get_object (value));
              else
                {
                  const char *unique_id = NULL;

                  unique_id = g_ptr_array_index (update_ids, i);
                  g_critical ("%s could not be resolved for the update list and thus will not be included: %s",
                              unique_id, local_error->message);
                  g_clear_pointer (&local_error, g_error_free);
                }
            }

          bz_state_info_set_available_updates (self->state, G_LIST_MODEL (store));
          // if (window != NULL)
          //   bz_window_push_update_dialog (BZ_WINDOW (window));
        }
    }
  else if (window != NULL)
    bz_show_error_for_widget (GTK_WIDGET (window), local_error->message);
  g_clear_pointer (&local_error, g_error_free);

  bz_state_info_set_checking_for_updates (self->state, FALSE);
}

static DexFuture *
refresh_fiber (BzApplication *self)
{
  g_autoptr (GError) local_error            = NULL;
  gboolean         has_flathub              = FALSE;
  g_autofree char *busy_step_label          = NULL;
  g_autofree char *busy_progress_label      = NULL;
  g_autoptr (GHashTable) installed_set      = NULL;
  guint total                               = 0;
  guint out_of                              = 0;
  g_autoptr (DexChannel) channel            = NULL;
  g_autoptr (DexFuture) sync_future         = NULL;
  g_autoptr (GHashTable) sys_name_to_addons = NULL;
  g_autoptr (GHashTable) usr_name_to_addons = NULL;
  g_autoptr (GPtrArray) cache_futures       = NULL;
  GtkWindow    *window                      = NULL;
  gboolean      result                      = FALSE;
  const GValue *sync_value                  = NULL;

  if (self->flatpak == NULL)
    {
      bz_state_info_set_busy_step_label (self->state, _ ("Constructing Flatpak instance..."));
      g_debug ("Constructing flatpak instance for the first time...");
      self->flatpak = dex_await_object (bz_flatpak_instance_new (), &local_error);
      if (self->flatpak == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));
      bz_transaction_manager_set_backend (self->transactions, BZ_BACKEND (self->flatpak));
      bz_state_info_set_backend (self->state, BZ_BACKEND (self->flatpak));

      dex_clear (&self->notif_watch);
      self->notif_watch = dex_scheduler_spawn (
          dex_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) watch_backend_notifs_fiber,
          self, NULL);
    }
  else
    {
      bz_state_info_set_busy_step_label (self->state, _ ("Reusing last Flatpak instance..."));
      g_debug ("Reusing previous flatpak instance...");
    }

  has_flathub = dex_await_boolean (
      bz_flatpak_instance_has_flathub (self->flatpak, NULL),
      &local_error);
  if (local_error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  if (has_flathub)
    bz_state_info_set_flathub (self->state, self->flathub);
  else
    {
      g_autofree char *response = NULL;

      window = gtk_application_get_active_window (GTK_APPLICATION (self));
      if (window != NULL)
        {
          AdwDialog *alert = NULL;

          alert = adw_alert_dialog_new (NULL, NULL);
          adw_alert_dialog_set_prefer_wide_layout (ADW_ALERT_DIALOG (alert), TRUE);
          adw_alert_dialog_format_heading (
              ADW_ALERT_DIALOG (alert),
              _ ("Flathub is not registered on this system"));
          adw_alert_dialog_format_body (
              ADW_ALERT_DIALOG (alert),
              _ ("Would you like to add Flathub as a remote? "
                 "If you decline, the Flathub page will not be available. "
                 "You can change this later."));
          adw_alert_dialog_add_responses (
              ADW_ALERT_DIALOG (alert),
              "later", _ ("Later"),
              "add", _ ("Add Flathub"),
              NULL);
          adw_alert_dialog_set_response_appearance (
              ADW_ALERT_DIALOG (alert), "add", ADW_RESPONSE_SUGGESTED);
          adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "add");
          adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "later");

          adw_dialog_present (alert, GTK_WIDGET (window));
          response = dex_await_string (
              bz_make_alert_dialog_future (ADW_ALERT_DIALOG (alert)),
              NULL);
        }

      if (response != NULL &&
          g_strcmp0 (response, "add") == 0)
        {
          result = dex_await (
              bz_flatpak_instance_ensure_has_flathub (self->flatpak, NULL),
              &local_error);
          if (!result)
            return dex_future_new_for_error (g_steal_pointer (&local_error));

          bz_state_info_set_flathub (self->state, self->flathub);
        }
    }

  if (bz_state_info_get_flathub (self->state) != NULL)
    {
      g_debug ("Updating Flathub state...");
      bz_flathub_state_update_to_today (self->flathub);
    }

  busy_step_label = g_strdup_printf (_ ("Identifying installed entries..."));
  bz_state_info_set_busy_step_label (self->state, busy_step_label);
  g_clear_pointer (&busy_step_label, g_free);

  installed_set = dex_await_boxed (
      bz_backend_retrieve_install_ids (
          BZ_BACKEND (self->flatpak), NULL),
      &local_error);
  if (installed_set == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  busy_step_label = g_strdup_printf (
      _ ("Beginning remote entry retrieval while referencing %d blocklist(s)..."),
      g_list_model_get_n_items (self->blocklists));
  bz_state_info_set_busy_step_label (self->state, busy_step_label);
  g_clear_pointer (&busy_step_label, g_free);

  channel            = dex_channel_new (100);
  sys_name_to_addons = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
  usr_name_to_addons = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
  cache_futures = g_ptr_array_new_with_free_func (dex_unref);

  sync_future = bz_backend_retrieve_remote_entries_with_blocklists (
      BZ_BACKEND (self->flatpak),
      channel,
      self->blocklists,
      NULL, self, NULL);

  for (;;)
    {
      g_autoptr (DexFuture) channel_future = NULL;
      const GValue *value                  = NULL;

      channel_future = dex_channel_receive (channel);
      dex_await (dex_ref (channel_future), NULL);

      value = dex_future_get_value (channel_future, NULL);
      if (value == NULL)
        break;

      if (G_VALUE_HOLDS_OBJECT (value))
        {
          g_autoptr (BzEntry) entry        = NULL;
          const char *id                   = NULL;
          const char *unique_id            = NULL;
          gboolean    user                 = FALSE;
          gboolean    installed            = FALSE;
          const char *flatpak_id           = NULL;
          g_autoptr (DexFuture) cache_task = NULL;

          entry     = g_value_dup_object (value);
          id        = bz_entry_get_id (entry);
          unique_id = bz_entry_get_unique_id (entry);
          user      = bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry));

          installed = g_hash_table_contains (installed_set, unique_id);
          bz_entry_set_installed (entry, installed);

          flatpak_id = bz_flatpak_entry_get_flatpak_id (BZ_FLATPAK_ENTRY (entry));
          if (flatpak_id != NULL)
            {
              GPtrArray *addons = NULL;

              addons = g_hash_table_lookup (
                  user
                      ? usr_name_to_addons
                      : sys_name_to_addons,
                  flatpak_id);
              if (addons != NULL)
                {
                  g_debug ("Appending %d addons to %s", addons->len, unique_id);
                  for (guint i = 0; i < addons->len; i++)
                    {
                      const char *addon_id = NULL;

                      addon_id = g_ptr_array_index (addons, i);
                      bz_entry_append_addon (entry, addon_id);
                    }
                  g_hash_table_remove (
                      user
                          ? usr_name_to_addons
                          : sys_name_to_addons,
                      flatpak_id);
                  addons = NULL;
                }
            }

          if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
            {
              BzEntryGroup *group = NULL;

              group = g_hash_table_lookup (self->ids_to_groups, id);
              if (group != NULL)
                {
                  bz_entry_group_add (group, entry);
                  if (installed && !g_list_store_find (self->installed_apps, group, NULL))
                    g_list_store_append (self->installed_apps, group);
                }
              else
                {
                  g_autoptr (BzEntryGroup) new_group = NULL;

                  g_debug ("Creating new application group for id %s", id);
                  new_group = bz_entry_group_new (self->entry_factory);

                  g_list_store_append (self->groups, new_group);
                  g_hash_table_replace (self->ids_to_groups, g_strdup (id), g_object_ref (new_group));
                  bz_entry_group_add (new_group, entry);

                  if (installed)
                    g_list_store_append (self->installed_apps, new_group);
                }
            }

          if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_ADDON))
            {
              const char *extension_of_what = NULL;

              extension_of_what = bz_flatpak_entry_get_addon_extension_of_ref (
                  BZ_FLATPAK_ENTRY (entry));
              if (extension_of_what != NULL)
                {
                  GPtrArray *addons = NULL;

                  /* BzFlatpakInstance ensures addons come before applications */
                  addons = g_hash_table_lookup (
                      user
                          ? usr_name_to_addons
                          : sys_name_to_addons,
                      extension_of_what);
                  if (addons == NULL)
                    {
                      addons = g_ptr_array_new_with_free_func (g_free);
                      g_hash_table_replace (
                          user
                              ? usr_name_to_addons
                              : sys_name_to_addons,
                          g_strdup (extension_of_what), addons);
                    }
                  g_ptr_array_add (addons, g_strdup (unique_id));
                }
              else
                g_warning ("Entry with unique id %s is an addon but "
                           "does not seem to extend anything",
                           unique_id);
            }

          cache_task = bz_entry_cache_manager_add (self->cache, entry);
          g_ptr_array_add (cache_futures, g_steal_pointer (&cache_task));

          total++;
        }
      else if (G_VALUE_HOLDS_INT (value))
        out_of += g_value_get_int (value);
      else
        g_assert_not_reached ();

      bz_state_info_set_busy_progress (self->state, (double) total / (double) out_of);
      busy_step_label     = g_strdup_printf (_ ("Receiving Entries"));
      busy_progress_label = g_strdup_printf (_ ("%'d of %'d"), total, out_of);
      bz_state_info_set_busy_step_label (self->state, busy_step_label);
      bz_state_info_set_busy_progress_label (self->state, busy_progress_label);
      g_clear_pointer (&busy_step_label, g_free);
    }
  g_clear_pointer (&self->last_installed_set, g_hash_table_unref);
  self->last_installed_set = g_steal_pointer (&installed_set);
  g_list_store_sort (self->groups, (GCompareDataFunc) cmp_group, NULL);
  g_list_store_sort (self->installed_apps, (GCompareDataFunc) cmp_group, NULL);

  busy_step_label = g_strdup_printf (_ ("Waiting for background indexing tasks to catch up...")),
  bz_state_info_set_busy_step_label (self->state, busy_step_label);
  g_clear_pointer (&busy_step_label, g_free);

  dex_await (dex_future_allv (
                 (DexFuture *const *) cache_futures->pdata,
                 cache_futures->len),
             NULL);

  result = dex_await (dex_ref (sync_future), &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  sync_value = dex_future_get_value (sync_future, NULL);
  if (G_VALUE_HOLDS_STRING (sync_value))
    {
      const char *warning = NULL;

      warning = g_value_get_string (sync_value);
      g_warning ("%s\n", warning);

      window = gtk_application_get_active_window (GTK_APPLICATION (self));
      if (window != NULL)
        bz_show_error_for_widget (GTK_WIDGET (window), warning);
    }

  g_debug ("Finished synchronizing with remotes, notifying UI...");
  bz_state_info_set_online (self->state, TRUE);
  bz_state_info_set_all_entry_groups (self->state, G_LIST_MODEL (self->groups));
  bz_search_engine_set_model (self->search_engine, G_LIST_MODEL (self->groups));
  bz_state_info_set_busy (self->state, FALSE);

  gtk_filter_changed (GTK_FILTER (self->application_filter), GTK_FILTER_CHANGE_DIFFERENT);
  bz_state_info_set_all_installed_entry_groups (self->state, G_LIST_MODEL (self->installed_apps));

  busy_step_label = g_strdup_printf (
      _ ("Completed initialization in %0.2f seconds"),
      g_timer_elapsed (self->init_timer, NULL));
  bz_state_info_set_busy_step_label (self->state, busy_step_label);
  g_clear_pointer (&busy_step_label, g_free);

  bz_state_info_set_background_task_label (self->state, _ ("Checking for updates..."));
  fiber_check_for_updates (self);
  bz_state_info_set_background_task_label (self->state, NULL);

  return dex_future_new_true ();
}

static DexFuture *
watch_backend_notifs_fiber (BzApplication *self)
{
  for (;;)
    {
      g_autoptr (DexChannel) channel = NULL;

      channel = bz_backend_create_notification_channel (
          BZ_BACKEND (self->flatpak));
      if (channel == NULL)
        break;

      for (;;)
        {
          g_autoptr (GError) local_error          = NULL;
          g_autoptr (BzBackendNotification) notif = NULL;
          g_autoptr (GHashTable) installed_set    = NULL;
          g_autoptr (GPtrArray) diff_reads        = NULL;
          GHashTableIter old_iter                 = { 0 };
          GHashTableIter new_iter                 = { 0 };
          g_autoptr (GPtrArray) diff_writes       = NULL;

          notif = dex_await_object (dex_channel_receive (channel), NULL);
          if (notif == NULL)
            break;

          if (self->refresh_task != NULL)
            {
              g_debug ("Ignoring backend notification since we are currently refreshing");
              continue;
            }

          bz_state_info_set_background_task_label (self->state, _ ("Synchronizing..."));

          installed_set = dex_await_boxed (
              bz_backend_retrieve_install_ids (
                  BZ_BACKEND (self->flatpak), NULL),
              &local_error);
          if (installed_set == NULL)
            {
              g_critical ("Failed to enumerate installed entries: %s", local_error->message);
              bz_state_info_set_background_task_label (self->state, NULL);
              continue;
            }

          diff_reads = g_ptr_array_new_with_free_func (dex_unref);

          g_hash_table_iter_init (&old_iter, self->last_installed_set);
          for (;;)
            {
              char *unique_id = NULL;

              if (!g_hash_table_iter_next (
                      &old_iter, (gpointer *) &unique_id, NULL))
                break;

              if (!g_hash_table_contains (installed_set, unique_id))
                g_ptr_array_add (
                    diff_reads,
                    bz_entry_cache_manager_get (self->cache, unique_id));
            }

          g_hash_table_iter_init (&new_iter, installed_set);
          for (;;)
            {
              char *unique_id = NULL;

              if (!g_hash_table_iter_next (
                      &new_iter, (gpointer *) &unique_id, NULL))
                break;

              if (!g_hash_table_contains (self->last_installed_set, unique_id))
                g_ptr_array_add (
                    diff_reads,
                    bz_entry_cache_manager_get (self->cache, unique_id));
            }

          if (diff_reads->len > 0)
            {
              dex_await (dex_future_allv (
                             (DexFuture *const *) diff_reads->pdata,
                             diff_reads->len),
                         NULL);

              diff_writes = g_ptr_array_new_with_free_func (dex_unref);
              for (guint i = 0; i < diff_reads->len; i++)
                {
                  DexFuture *future = NULL;

                  future = g_ptr_array_index (diff_reads, i);
                  if (dex_future_is_resolved (future))
                    {
                      BzEntry      *entry     = NULL;
                      const char   *id        = NULL;
                      const char   *unique_id = NULL;
                      BzEntryGroup *group     = NULL;
                      gboolean      installed = FALSE;

                      entry = g_value_get_object (dex_future_get_value (future, NULL));
                      id    = bz_entry_get_id (entry);
                      group = g_hash_table_lookup (self->ids_to_groups, id);
                      if (group != NULL)
                        bz_entry_group_connect_living (group, entry);

                      unique_id = bz_entry_get_unique_id (entry);
                      installed = g_hash_table_contains (installed_set, unique_id);
                      bz_entry_set_installed (entry, installed);

                      if (group != NULL)
                        {
                          gboolean found    = FALSE;
                          guint    position = 0;

                          found = g_list_store_find (self->installed_apps, group, &position);
                          if (installed && !found)
                            g_list_store_insert_sorted (
                                self->installed_apps, group,
                                (GCompareDataFunc) cmp_group, NULL);
                          else if (!installed && found &&
                                   bz_entry_group_get_removable (group) == 0)
                            g_list_store_remove (self->installed_apps, position);
                        }

                      g_ptr_array_add (
                          diff_writes,
                          bz_entry_cache_manager_add (self->cache, entry));
                    }
                }

              dex_await (dex_future_allv (
                             (DexFuture *const *) diff_writes->pdata,
                             diff_writes->len),
                         NULL);
            }
          g_clear_pointer (&self->last_installed_set, g_hash_table_unref);
          self->last_installed_set = g_steal_pointer (&installed_set);

          fiber_check_for_updates (self);
          bz_state_info_set_background_task_label (self->state, NULL);
        }
    }

  return NULL;
}

static DexFuture *
refresh_finally (DexFuture     *future,
                 BzApplication *self)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  dex_clear (&self->refresh_task);
  if (dex_future_is_rejected (future))
    {
      bz_state_info_set_background_task_label (self->state, NULL);
      bz_state_info_set_checking_for_updates (self->state, FALSE);
      bz_state_info_set_all_entry_groups (self->state, G_LIST_MODEL (self->groups));
      bz_state_info_set_all_installed_entry_groups (self->state, G_LIST_MODEL (self->installed_apps));
      bz_search_engine_set_model (self->search_engine, G_LIST_MODEL (self->groups));
      bz_state_info_set_busy (self->state, FALSE);
    }

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      bz_state_info_set_online (self->state, TRUE);
      g_debug ("We are online!");
    }
  else
    {
      GtkWindow *window = NULL;

      g_debug ("Failed to achieve online status, reason: %s", local_error->message);
      bz_state_info_set_online (self->state, FALSE);

      window = gtk_application_get_active_window (GTK_APPLICATION (self));
      if (window != NULL)
        {
          g_autofree char *error_string = NULL;

          error_string = g_strdup_printf (
              "Could not retrieve remote content: %s",
              local_error->message);
          bz_show_error_for_widget (GTK_WIDGET (window), error_string);
        }
    }

  g_debug ("Completely done with the refresh process!");

  if (self->waiting_to_open_appstream != NULL)
    {
      g_debug ("An appstream link was requested to be opened during refresh. Doing that now...");
      open_appstream_take (self, g_steal_pointer (&self->waiting_to_open_appstream));
    }

  if (self->waiting_to_open_file != NULL)
    {
      g_debug ("A flatpakref was requested to be opened during refresh. Doing that now...");
      open_flatpakref_take (self, g_steal_pointer (&self->waiting_to_open_file));
    }

  return NULL;
}

static void
refresh (BzApplication *self)
{
  g_autoptr (DexFuture) future = NULL;

  if (self->refresh_task != NULL)
    {
      g_warning ("Bazaar is currently refreshing, so it cannot refresh right now");
      return;
    }

  g_debug ("Refreshing complete application state...");

  bz_state_info_set_all_entry_groups (self->state, NULL);
  bz_state_info_set_all_installed_entry_groups (self->state, NULL);
  bz_state_info_set_flathub (self->state, NULL);
  bz_search_engine_set_model (self->search_engine, NULL);

  g_list_store_remove_all (self->groups);
  g_hash_table_remove_all (self->ids_to_groups);
  g_list_store_remove_all (self->installed_apps);

  bz_state_info_set_busy (self->state, TRUE);
  bz_state_info_set_busy_progress (self->state, 0.0);
  bz_state_info_set_available_updates (self->state, NULL);
  bz_state_info_set_online (self->state, FALSE);

  g_clear_object (&self->cache);
  self->cache = bz_entry_cache_manager_new ();

  g_timer_start (self->init_timer);
  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) refresh_fiber,
      self, NULL);
  future = dex_future_finally (
      future, (DexFutureCallback) refresh_finally,
      self, NULL);
  self->refresh_task = g_steal_pointer (&future);
}

static GtkWindow *
new_window (BzApplication *self)
{
  BzWindow *window = NULL;

  window = bz_window_new (self->state);
  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));

  gtk_window_present (GTK_WINDOW (window));
  return GTK_WINDOW (window);
}

static void
open_appstream_take (BzApplication *self,
                     char          *appstream)
{
  g_assert (appstream != NULL);

  if (bz_state_info_get_busy (self->state))
    {
      g_debug ("Bazaar is currently refreshing, so we will load "
               "the appstream link %s when that is done",
               appstream);

      g_clear_pointer (&self->waiting_to_open_appstream, g_free);
      self->waiting_to_open_appstream = g_steal_pointer (&appstream);
    }
  else if (g_str_has_prefix (appstream, "appstream://"))
    open_generic_id (self, appstream + strlen ("appstream://"));
  else
    open_generic_id (self, appstream + strlen ("appstream:"));

  if (appstream != NULL)
    g_free (appstream);
}

static void
open_flatpakref_take (BzApplication *self,
                      GFile         *file)
{
  g_autofree char *path = NULL;

  g_assert (file != NULL);
  path = g_file_get_path (file);

  if (bz_state_info_get_busy (self->state))
    {
      g_debug ("Bazaar is currently refreshing, so we will load "
               "the local flatpakref at %s when that is done",
               path);

      g_clear_object (&self->waiting_to_open_file);
      self->waiting_to_open_file = g_steal_pointer (&file);
    }
  else
    {
      g_autoptr (OpenFlatpakrefData) data = NULL;
      g_autoptr (DexFuture) future        = NULL;

      g_debug ("Loading local flatpakref at %s now...", path);

      data       = open_flatpakref_data_new ();
      data->self = g_object_ref (self);
      data->file = g_steal_pointer (&file);

      future = dex_scheduler_spawn (
          dex_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) open_flatpakref_fiber,
          open_flatpakref_data_ref (data),
          open_flatpakref_data_unref);
      dex_future_disown (g_steal_pointer (&future));
    }
}

static void
command_line_open_location (BzApplication           *self,
                            GApplicationCommandLine *cmdline,
                            const char              *location)
{
  if (g_uri_is_valid (location, G_URI_FLAGS_NONE, NULL))
    {
      if (g_str_has_prefix (location, "appstream:"))
        open_appstream_take (self, g_strdup (location));
      else
        open_flatpakref_take (self, g_file_new_for_uri (location));
    }
  else if (g_path_is_absolute (location))
    open_flatpakref_take (self, g_file_new_for_path (location));
  else
    {
      const char *cwd = NULL;

      cwd = g_application_command_line_get_cwd (cmdline);
      if (cwd != NULL)
        open_flatpakref_take (self, g_file_new_build_filename (cwd, location, NULL));
      else
        open_flatpakref_take (self, g_file_new_for_path (location));
    }
}

static gint
cmp_group (BzEntryGroup *a,
           BzEntryGroup *b,
           gpointer      user_data)
{
  const char *title_a = NULL;
  const char *title_b = NULL;

  title_a = bz_entry_group_get_title (a);
  title_b = bz_entry_group_get_title (b);

  if (title_a == NULL)
    return 1;
  if (title_b == NULL)
    return -1;

  return g_strcmp0 (title_a, title_b);
}

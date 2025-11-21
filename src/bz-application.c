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

#define MAX_IDS_PER_BLOCKLIST 2048

#include "config.h"

#include <glib/gi18n.h>
#include <malloc.h>

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
#include "bz-root-blocklist.h"
#include "bz-state-info.h"
#include "bz-transaction-manager.h"
#include "bz-util.h"
#include "bz-window.h"
#include "bz-yaml-parser.h"

struct _BzApplication
{
  AdwApplication parent_instance;

  BzYamlParser *blocklist_parser;

  GSettings       *settings;
  GHashTable      *config;
  GListModel      *blocklists;
  GPtrArray       *blocklist_regexes;
  GListModel      *content_configs;
  GtkCssProvider  *css;
  GtkMapListModel *content_configs_to_files;

  gboolean    running;
  GWeakRef    main_window;
  GTimer     *init_timer;
  DexChannel *flatpak_notifs;
  DexFuture  *notif_watch;
  int         n_incoming;
  GHashTable *eol_runtimes;
  GHashTable *sys_name_to_addons;
  GHashTable *usr_name_to_addons;

  DexFuture *periodic_sync;
  guint      periodic_timeout;

  BzEntryCacheManager        *cache;
  BzTransactionManager       *transactions;
  BzSearchEngine             *search_engine;
  BzGnomeShellSearchProvider *gs_search;

  BzFlatpakInstance *flatpak;
  BzFlathubState    *flathub;
  BzContentProvider *content_provider;

  GHashTable *installed_set;
  GListStore *groups;
  GHashTable *ids_to_groups;
  GListStore *installed_apps;

  BzApplicationMapFactory *entry_factory;
  GtkCustomFilter         *appid_filter;
  BzApplicationMapFactory *application_factory;

  GtkCustomFilter    *group_filter;
  GtkFilterListModel *group_filter_model;

  BzStateInfo *state;
};

G_DEFINE_FINAL_TYPE (BzApplication, bz_application, ADW_TYPE_APPLICATION)

BZ_DEFINE_DATA (
    blocklist_regex,
    BlocklistRegex,
    {
      int     priority;
      GRegex *block;
      GRegex *allow;
    },
    BZ_RELEASE_DATA (block, g_regex_unref);
    BZ_RELEASE_DATA (allow, g_regex_unref))

BZ_DEFINE_DATA (
    respond_to_flatpak,
    RespondToFlatpak,
    {
      GWeakRef              *self;
      BzBackendNotification *notif;
    },
    BZ_RELEASE_DATA (self, bz_weak_release);
    BZ_RELEASE_DATA (notif, g_object_unref))

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

static DexFuture *
init_fiber (GWeakRef *wr);

static DexFuture *
init_finally (DexFuture *future,
              GWeakRef  *wr);

static gboolean
window_close_request (BzApplication *self,
                      GtkWidget     *window);

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

static DexFuture *
watch_backend_notifs_then_loop_cb (DexFuture *future,
                                   GWeakRef  *wr);

static void
blocklists_changed (BzApplication *self,
                    guint          position,
                    guint          removed,
                    guint          added,
                    GListModel    *model);

static gint
cmp_group (BzEntryGroup *a,
           BzEntryGroup *b,
           gpointer      user_data);

static void
bz_application_dispose (GObject *object)
{
  BzApplication *self = BZ_APPLICATION (object);

  dex_clear (&self->periodic_sync);
  dex_clear (&self->notif_watch);
  dex_clear (&self->flatpak_notifs);
  g_clear_handle_id (&self->periodic_timeout, g_source_remove);
  g_clear_object (&self->blocklist_parser);
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
  g_clear_object (&self->entry_factory);
  g_clear_object (&self->appid_filter);
  g_clear_object (&self->group_filter_model);
  g_clear_object (&self->group_filter);
  g_clear_object (&self->application_factory);
  g_clear_object (&self->flathub);
  g_clear_object (&self->cache);
  g_clear_object (&self->groups);
  g_clear_object (&self->installed_apps);
  g_clear_object (&self->state);
  g_clear_pointer (&self->init_timer, g_timer_destroy);
  g_clear_pointer (&self->installed_set, g_hash_table_unref);
  g_clear_pointer (&self->ids_to_groups, g_hash_table_unref);
  g_clear_pointer (&self->eol_runtimes, g_hash_table_unref);
  g_clear_pointer (&self->sys_name_to_addons, g_hash_table_unref);
  g_clear_pointer (&self->usr_name_to_addons, g_hash_table_unref);
  g_clear_pointer (&self->blocklist_regexes, g_ptr_array_unref);
  g_weak_ref_clear (&self->main_window);

  G_OBJECT_CLASS (bz_application_parent_class)->dispose (object);
}

static void
bz_application_activate (GApplication *app)
{
  BzApplication *self = BZ_APPLICATION (app);

  new_window (self);
}

static int
bz_application_command_line (GApplication            *app,
                             GApplicationCommandLine *cmdline)
{
  BzApplication *self                       = BZ_APPLICATION (app);
  g_autoptr (GError) local_error            = NULL;
  gint argc                                 = 0;
  g_auto (GStrv) argv                       = NULL;
  gboolean help                             = FALSE;
  gboolean no_window                        = FALSE;
  g_auto (GStrv) blocklists_strv            = NULL;
  g_autoptr (GtkStringList) blocklists      = NULL;
  g_auto (GStrv) content_configs_strv       = NULL;
  g_autoptr (GtkStringList) content_configs = NULL;
  g_auto (GStrv) locations                  = NULL;

  GOptionEntry main_entries[] = {
    { "help", 0, 0, G_OPTION_ARG_NONE, &help, "Print help" },
    { "no-window", 0, 0, G_OPTION_ARG_NONE, &no_window, "Ensure the service is running without creating a new window" },
    { "extra-blocklist", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &blocklists_strv, "Add an extra blocklist to read from" },
    { "extra-curated-config", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &content_configs_strv, "Add an extra yaml file with which to configure the app browser" },
    /* Here for backwards compat */
    { "extra-content-config", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &content_configs_strv, "Add an extra yaml file with which to configure the app browser (backwards compat)" },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &locations, "flatpakref file to open" },
    { NULL }
  };

  argv = g_application_command_line_get_arguments (cmdline, &argc);
  g_debug ("Handling gapplication command line; argc=%d, argv= \\", argc);
  for (guint i = 0; i < argc; i++)
    {
      g_debug ("  [%d] %s", i, argv[i]);
    }

  if (argv != NULL && argc > 0)
    {
      g_autofree GStrv argv_shallow      = NULL;
      g_autoptr (GOptionContext) context = NULL;

      argv_shallow = g_memdup2 (argv, sizeof (*argv) * argc);

      context = g_option_context_new ("- an app center for GNOME");
      g_option_context_set_help_enabled (context, FALSE);
      g_option_context_add_main_entries (context, main_entries, NULL);
      if (!g_option_context_parse (context, &argc, &argv_shallow, &local_error))
        {
          g_application_command_line_printerr (cmdline, "%s\n", local_error->message);
          return EXIT_FAILURE;
        }

      if (help)
        {
          g_autofree char *help_text = NULL;

          if (self->running)
            g_application_command_line_printerr (cmdline, "The Bazaar service is running.\n\n");
          else
            g_application_command_line_printerr (cmdline, "The Bazaar service is not running.\n\n");

          help_text = g_option_context_get_help (context, TRUE, NULL);
          g_application_command_line_printerr (cmdline, "%s\n", help_text);
          return EXIT_SUCCESS;
        }
    }

  if (!self->running)
    {
      g_autoptr (DexFuture) init = NULL;

      g_debug ("Starting daemon!");
      g_application_hold (G_APPLICATION (self));
      self->running = TRUE;

      init_service_struct (self);

      blocklists = gtk_string_list_new (NULL);
      g_signal_connect_swapped (blocklists, "items-changed", G_CALLBACK (blocklists_changed), self);
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

      self->blocklists      = G_LIST_MODEL (g_steal_pointer (&blocklists));
      self->content_configs = G_LIST_MODEL (g_steal_pointer (&content_configs));

      gtk_map_list_model_set_model (
          self->content_configs_to_files, self->content_configs);
      bz_state_info_set_blocklists (self->state, self->blocklists);
      bz_state_info_set_curated_configs (self->state, self->content_configs);

      g_timer_start (self->init_timer);
      init = dex_scheduler_spawn (
          dex_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) init_fiber,
          bz_track_weak (self),
          bz_weak_release);
      init = dex_future_finally (
          init,
          (DexFutureCallback) init_finally,
          bz_track_weak (self),
          bz_weak_release);
      dex_future_disown (g_steal_pointer (&init));
    }

  if (!no_window)
    new_window (self);

  if (locations != NULL && *locations != NULL)
    command_line_open_location (self, cmdline, locations[0]);

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
bz_application_toggle_debug_mode_action (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
  BzApplication *self       = user_data;
  gboolean       debug_mode = FALSE;

  debug_mode = bz_state_info_get_debug_mode (self->state);
  bz_state_info_set_debug_mode (self->state, !debug_mode);
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
  BzApplication *self                    = user_data;
  GtkWindow     *window                  = NULL;
  AdwDialog     *dialog                  = NULL;
  g_autoptr (GBytes) release_notes_bytes = NULL;
  const char *release_notes_text         = NULL;

  const char *developers[] = {
    C_ ("About Dialog Developer Credit", "Adam Masciola <kolunmi@posteo.net>"),
    C_ ("About Dialog Developer Credit", "Alexander Vanhee"),
    /* This array MUST be NULL terminated */
    NULL
  };

  g_assert (BZ_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));
  dialog = adw_about_dialog_new ();

  release_notes_bytes = g_resources_lookup_data (
      "/io/github/kolunmi/Bazaar/release-notes.xml",
      G_RESOURCE_LOOKUP_FLAGS_NONE,
      NULL);

  if (release_notes_bytes != NULL)
    release_notes_text = g_bytes_get_data (release_notes_bytes, NULL);

  g_object_set (
      dialog,
      "application-name", "Bazaar",
      "application-icon", "io.github.kolunmi.Bazaar",
      "developer-name", _ ("Adam Masciola"),
      "developers", developers,
      // Translators: Put one translator per line, in the form NAME <EMAIL>, YEAR1, YEAR2
      "translator-credits", _ ("translator-credits"),
      "version", PACKAGE_VERSION,
      "copyright", "© 2025 Adam Masciola",
      "license-type", GTK_LICENSE_GPL_3_0,
      "website", "https://github.com/kolunmi/bazaar",
      "issue-url", "https://github.com/kolunmi/bazaar/issues",
      "release-notes", release_notes_text,
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
  {         "preferences",         bz_application_preferences_action, NULL },
  {               "about",               bz_application_about_action, NULL },
  {              "search",              bz_application_search_action,  "s" },
  { "toggle-transactions", bz_application_toggle_transactions_action, NULL },
  {              "donate",              bz_application_donate_action, NULL },
  {            "flatseal",            bz_application_flatseal_action, NULL },
  {    "bazaar-inspector",    bz_application_bazaar_inspector_action, NULL },
  {   "toggle-debug-mode",   bz_application_toggle_debug_mode_action, NULL },
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

  id     = gtk_string_object_get_string (string);
  future = bz_entry_cache_manager_get (self->cache, id);
  result = bz_result_new (future);

  g_object_unref (string);
  return g_steal_pointer (&result);
}

static inline gboolean
validate_group_for_ui (BzApplication *self,
                       BzEntryGroup  *group)
{
  const char *id               = NULL;
  int         allowed_priority = G_MAXINT;
  int         blocked_priority = G_MAXINT;

  if (bz_state_info_get_hide_eol (self->state) &&
      bz_entry_group_get_eol (group) != NULL)
    return FALSE;

  id = bz_entry_group_get_id (group);
  for (guint i = 0; i < self->blocklist_regexes->len; i++)
    {
      GPtrArray *regex_datas = NULL;

      regex_datas = g_ptr_array_index (self->blocklist_regexes, i);
      for (guint j = 0; j < regex_datas->len; j++)
        {
          BlocklistRegexData *data = NULL;

          data = g_ptr_array_index (regex_datas, j);

          if (data->allow != NULL &&
              data->priority < allowed_priority &&
              g_regex_match (data->allow, id, G_REGEX_MATCH_DEFAULT, NULL))
            allowed_priority = data->priority;
          if (data->block != NULL &&
              data->priority < blocked_priority &&
              g_regex_match (data->block, id, G_REGEX_MATCH_DEFAULT, NULL))
            blocked_priority = data->priority;
        }
    }

  return allowed_priority <= blocked_priority;
}

static gboolean
filter_application_ids (GtkStringObject *string,
                        BzApplication   *self)
{
  BzEntryGroup *group = NULL;

  group = g_hash_table_lookup (
      self->ids_to_groups,
      gtk_string_object_get_string (string));
  if (group != NULL)
    return validate_group_for_ui (self, group);
  else
    return FALSE;
}

static gboolean
filter_entry_groups (BzEntryGroup  *group,
                     BzApplication *self)
{
  return validate_group_for_ui (self, group);
}

static void
bz_application_init (BzApplication *self)
{
  self->running = FALSE;
  g_weak_ref_init (&self->main_window, NULL);

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
      "app.preferences",
      (const char *[]) { "<primary>comma", NULL });
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
  gtk_application_set_accels_for_action (
      GTK_APPLICATION (self),
      "app.toggle-debug-mode",
      (const char *[]) { "<primary><alt>d", NULL });
}

BzStateInfo *
bz_state_info_get_default (void)
{
  GApplication  *app  = NULL;
  BzApplication *self = NULL;

  app = g_application_get_default ();
  if G_UNLIKELY (app == NULL)
    return NULL;

  self = (BzApplication *) app;
  g_assert (BZ_IS_APPLICATION (self));

  return self->state;
}

static void
hide_eol_changed (BzApplication *self,
                  const char    *key,
                  GSettings     *settings)
{
  g_object_freeze_notify (G_OBJECT (self->state));
  bz_state_info_set_hide_eol (self->state, g_settings_get_boolean (self->settings, "hide-eol"));
  gtk_filter_changed (GTK_FILTER (self->group_filter), GTK_FILTER_CHANGE_DIFFERENT);
  gtk_filter_changed (GTK_FILTER (self->appid_filter), GTK_FILTER_CHANGE_DIFFERENT);
  g_object_thaw_notify (G_OBJECT (self->state));
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
        g_warning ("Could not load main config at %s: %s",
                   HARDCODED_MAIN_CONFIG, local_error->message);
    }
  else
    g_warning ("Could not load main config at %s: %s",
               HARDCODED_MAIN_CONFIG, local_error->message);

  g_clear_pointer (&local_error, g_error_free);
#endif

  self->init_timer = g_timer_new ();

  (void) bz_download_worker_get_default ();

  g_type_ensure (BZ_TYPE_ROOT_BLOCKLIST);
  g_type_ensure (BZ_TYPE_BLOCKLIST);
  g_type_ensure (BZ_TYPE_BLOCKLIST_CONDITION);
  self->blocklist_parser = bz_yaml_parser_new_for_resource_schema (
      "/io/github/kolunmi/Bazaar/blocklist-schema.xml");

  self->cache = bz_entry_cache_manager_new ();
  self->state = bz_state_info_new ();

  app_id = g_application_get_application_id (G_APPLICATION (self));
  g_assert (app_id != NULL);
  g_debug ("Constructing gsettings for %s ...", app_id);
  self->settings = g_settings_new (app_id);
  bz_state_info_set_hide_eol (self->state, g_settings_get_boolean (self->settings, "hide-eol"));
  g_signal_connect_swapped (
      self->settings,
      "changed::hide-eol",
      G_CALLBACK (hide_eol_changed),
      self);

  self->blocklist_regexes = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_ptr_array_unref);
  self->groups         = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->installed_apps = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  self->ids_to_groups  = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->eol_runtimes = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, g_object_unref);
  self->sys_name_to_addons = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
  self->usr_name_to_addons = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);

  self->entry_factory = bz_application_map_factory_new (
      (GtkMapListModelMapFunc) map_ids_to_entries,
      self, NULL, NULL, NULL);

  filter = gtk_custom_filter_new (
      (GtkCustomFilterFunc) filter_application_ids, self, NULL);
  self->appid_filter        = g_object_ref_sink (g_steal_pointer (&filter));
  self->application_factory = bz_application_map_factory_new (
      (GtkMapListModelMapFunc) map_generic_ids_to_groups,
      self, NULL, NULL, GTK_FILTER (self->appid_filter));

  filter = gtk_custom_filter_new (
      (GtkCustomFilterFunc) filter_entry_groups, self, NULL);
  self->group_filter       = g_object_ref_sink (g_steal_pointer (&filter));
  self->group_filter_model = gtk_filter_list_model_new (
      g_object_ref (G_LIST_MODEL (self->groups)),
      g_object_ref (GTK_FILTER (self->group_filter)));

  self->search_engine = bz_search_engine_new ();
  bz_search_engine_set_model (self->search_engine, G_LIST_MODEL (self->group_filter_model));
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

  bz_state_info_set_all_installed_entry_groups (self->state, G_LIST_MODEL (self->installed_apps));
  bz_state_info_set_application_factory (self->state, self->application_factory);
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
  if (update_ids != NULL &&
      update_ids->len > 0)
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
          NULL);

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
              g_warning ("%s could not be resolved for the update list and thus will not be included: %s",
                         unique_id, local_error->message);
              g_clear_pointer (&local_error, g_error_free);
            }
        }

      if (g_list_model_get_n_items (G_LIST_MODEL (store)) > 0)
        bz_state_info_set_available_updates (self->state, G_LIST_MODEL (store));
    }
  else if (local_error != NULL)
    {
      g_warning ("Failed to check for updates: %s", local_error->message);

      if (window != NULL)
        bz_show_error_for_widget (GTK_WIDGET (window), local_error->message);
    }

  bz_state_info_set_checking_for_updates (self->state, FALSE);
}

static DexFuture *
init_fiber (GWeakRef *wr)
{
  g_autoptr (BzApplication) self = NULL;
  g_autoptr (GError) local_error = NULL;
  gboolean has_flathub           = FALSE;
  gboolean result                = FALSE;

  bz_weak_get_or_return_reject (self, wr);
  bz_state_info_set_background_task_label (self->state, _ ("Performing setup..."));

  g_clear_object (&self->flatpak);
  self->flatpak = dex_await_object (bz_flatpak_instance_new (), &local_error);
  if (self->flatpak == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));
  bz_transaction_manager_set_backend (self->transactions, BZ_BACKEND (self->flatpak));
  bz_state_info_set_backend (self->state, BZ_BACKEND (self->flatpak));

  has_flathub = dex_await_boolean (
      bz_flatpak_instance_has_flathub (self->flatpak, NULL),
      &local_error);
  if (local_error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  if (has_flathub)
    bz_state_info_set_flathub (self->state, self->flathub);
  else
    {
      GtkWindow       *window   = NULL;
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

  return dex_future_new_true ();
}

static DexFuture *
receive_from_flatpak_cb (DexFuture *future,
                         GWeakRef  *wr)
{
  g_autoptr (BzApplication) self = NULL;

  bz_weak_get_or_return_reject (self, wr);
  return dex_channel_receive (self->flatpak_notifs);
}

static DexFuture *
respond_to_flatpak_fiber (RespondToFlatpakData *data)
{
  g_autoptr (BzApplication) self = NULL;
  BzBackendNotification *notif   = data->notif;
  g_autoptr (GError) local_error = NULL;
  BzBackendNotificationKind kind = 0;

  bz_weak_get_or_return_reject (self, data->self);

  if (self->installed_set == NULL)
    {
      self->installed_set = dex_await_boxed (
          bz_backend_retrieve_install_ids (
              BZ_BACKEND (self->flatpak), NULL),
          &local_error);
      if (self->installed_set == NULL)
        {
          g_warning ("Unable to enumerate installed entries from flatpak backend; "
                     "no entries will appear to be installed: %s",
                     local_error->message);
          g_clear_error (&local_error);

          self->installed_set = g_hash_table_new_full (
              g_str_hash, g_str_equal, g_free, NULL);
        }
    }

  kind = bz_backend_notification_get_kind (notif);
  switch (kind)
    {
    case BZ_BACKEND_NOTIFICATION_KIND_ERROR:
      {
        const char *error  = NULL;
        GtkWindow  *window = NULL;

        error = bz_backend_notification_get_error (notif);
        if (error == NULL)
          goto done;

        g_warning ("Received an error from the flatpak backend: %s", error);

        window = gtk_application_get_active_window (GTK_APPLICATION (self));
        if (window != NULL)
          bz_show_error_for_widget (GTK_WIDGET (window), error);
      }
      break;
    case BZ_BACKEND_NOTIFICATION_KIND_TELL_INCOMING:
      {
        int n_incoming = 0;

        n_incoming = bz_backend_notification_get_n_incoming (notif);
        self->n_incoming += n_incoming;

        if (n_incoming > 0)
          {
            g_autofree char *label = NULL;

            label = g_strdup_printf (_ ("Receiving %d entries..."), self->n_incoming);
            bz_state_info_set_background_task_label (self->state, label);
          }
        else
          {
            bz_state_info_set_background_task_label (self->state, _ ("Checking for updates..."));
            fiber_check_for_updates (self);
            bz_state_info_set_background_task_label (self->state, NULL);
          }
      }
      break;
    case BZ_BACKEND_NOTIFICATION_KIND_REPLACE_ENTRY:
      {
        BzEntry    *entry      = NULL;
        const char *id         = NULL;
        const char *unique_id  = NULL;
        gboolean    user       = FALSE;
        gboolean    installed  = FALSE;
        const char *flatpak_id = NULL;

        entry     = bz_backend_notification_get_entry (notif);
        id        = bz_entry_get_id (entry);
        unique_id = bz_entry_get_unique_id (entry);
        user      = bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry));

        installed = g_hash_table_contains (self->installed_set, unique_id);
        bz_entry_set_installed (entry, installed);

        flatpak_id = bz_flatpak_entry_get_flatpak_id (BZ_FLATPAK_ENTRY (entry));
        if (flatpak_id != NULL)
          {
            GPtrArray *addons = NULL;

            addons = g_hash_table_lookup (
                user
                    ? self->usr_name_to_addons
                    : self->sys_name_to_addons,
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
                        ? self->usr_name_to_addons
                        : self->sys_name_to_addons,
                    flatpak_id);
                addons = NULL;
              }
          }

        if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
          {
            BzEntryGroup *group        = NULL;
            const char   *runtime_name = NULL;
            BzEntry      *eol_runtime  = NULL;

            group = g_hash_table_lookup (self->ids_to_groups, id);

            runtime_name = bz_flatpak_entry_get_application_runtime (BZ_FLATPAK_ENTRY (entry));
            if (runtime_name != NULL)
              eol_runtime = g_hash_table_lookup (self->eol_runtimes, runtime_name);

            if (group != NULL)
              {
                bz_entry_group_add (group, entry, eol_runtime);
                if (installed && !g_list_store_find (self->installed_apps, group, NULL))
                  g_list_store_append (self->installed_apps, group);
              }
            else
              {
                g_autoptr (BzEntryGroup) new_group = NULL;

                g_debug ("Creating new application group for id %s", id);
                new_group = bz_entry_group_new (self->entry_factory);
                bz_entry_group_add (new_group, entry, eol_runtime);

                g_list_store_append (self->groups, new_group);
                g_hash_table_replace (self->ids_to_groups, g_strdup (id), g_object_ref (new_group));

                if (installed)
                  g_list_store_append (self->installed_apps, new_group);
              }

            if (eol_runtime != NULL)
              g_hash_table_remove (self->eol_runtimes, runtime_name);
          }

        if (flatpak_id != NULL &&
            bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_RUNTIME) &&
            g_str_has_prefix (flatpak_id, "runtime/"))
          {
            const char *eol = NULL;

            eol = bz_entry_get_eol (entry);
            if (eol != NULL)
              {
                g_autofree char *stripped = NULL;

                stripped = g_strdup (flatpak_id + strlen ("runtime/"));
                g_hash_table_replace (
                    self->eol_runtimes,
                    g_steal_pointer (&stripped),
                    g_object_ref (entry));
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
                        ? self->usr_name_to_addons
                        : self->sys_name_to_addons,
                    extension_of_what);
                if (addons == NULL)
                  {
                    addons = g_ptr_array_new_with_free_func (g_free);
                    g_hash_table_replace (
                        user
                            ? self->usr_name_to_addons
                            : self->sys_name_to_addons,
                        g_strdup (extension_of_what), addons);
                  }
                g_ptr_array_add (addons, g_strdup (unique_id));
              }
            else
              g_warning ("Entry with unique id %s is an addon but "
                         "does not seem to extend anything",
                         unique_id);
          }

        if (dex_await (bz_entry_cache_manager_add (self->cache, entry), NULL) &&
            bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
          gtk_filter_changed (GTK_FILTER (self->appid_filter), GTK_FILTER_CHANGE_LESS_STRICT);

        self->n_incoming--;
        if (self->n_incoming > 0)
          {
            g_autofree char *label = NULL;

            label = g_strdup_printf (_ ("Receiving %d entries..."), self->n_incoming);
            bz_state_info_set_background_task_label (self->state, label);
          }
        else
          {
            bz_state_info_set_background_task_label (self->state, _ ("Checking for updates..."));
            fiber_check_for_updates (self);
            bz_state_info_set_background_task_label (self->state, NULL);
          }
      }
      break;
    case BZ_BACKEND_NOTIFICATION_KIND_INSTALL_DONE:
    case BZ_BACKEND_NOTIFICATION_KIND_UPDATE_DONE:
    case BZ_BACKEND_NOTIFICATION_KIND_REMOVE_DONE:
      {
        const char *unique_id     = NULL;
        g_autoptr (BzEntry) entry = NULL;

        unique_id = bz_backend_notification_get_unique_id (notif);
        entry     = dex_await_object (
            bz_entry_cache_manager_get (self->cache, unique_id),
            &local_error);
        if (entry == NULL)
          {
            g_warning ("Backend notification references an entry "
                       "which couldn't be decached: %s",
                       local_error->message);
            goto done;
          }

        switch (kind)
          {
          case BZ_BACKEND_NOTIFICATION_KIND_INSTALL_DONE:
            {
              bz_entry_set_installed (entry, TRUE);
              g_hash_table_replace (self->installed_set, g_strdup (unique_id), NULL);

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
            }
            break;
          case BZ_BACKEND_NOTIFICATION_KIND_UPDATE_DONE:
            {
            }
            break;
          case BZ_BACKEND_NOTIFICATION_KIND_REMOVE_DONE:
            {
              bz_entry_set_installed (entry, FALSE);
              g_hash_table_remove (self->installed_set, unique_id);

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
            }
            break;
          case BZ_BACKEND_NOTIFICATION_KIND_ERROR:
          case BZ_BACKEND_NOTIFICATION_KIND_TELL_INCOMING:
          case BZ_BACKEND_NOTIFICATION_KIND_REPLACE_ENTRY:
          case BZ_BACKEND_NOTIFICATION_KIND_EXTERNAL_CHANGE:
          default:
            g_assert_not_reached ();
          };

        dex_await (bz_entry_cache_manager_add (self->cache, entry), NULL);
      }
      break;
    case BZ_BACKEND_NOTIFICATION_KIND_EXTERNAL_CHANGE:
      {
        g_autoptr (GHashTable) installed_set = NULL;
        g_autoptr (GPtrArray) diff_reads     = NULL;
        GHashTableIter old_iter              = { 0 };
        GHashTableIter new_iter              = { 0 };
        g_autoptr (GPtrArray) diff_writes    = NULL;

        bz_state_info_set_background_task_label (self->state, _ ("Synchronizing..."));

        installed_set = dex_await_boxed (
            bz_backend_retrieve_install_ids (
                BZ_BACKEND (self->flatpak), NULL),
            &local_error);
        if (installed_set == NULL)
          {
            g_warning ("Failed to enumerate installed entries: %s", local_error->message);
            bz_state_info_set_background_task_label (self->state, NULL);
            goto done;
          }

        diff_reads = g_ptr_array_new_with_free_func (dex_unref);

        g_hash_table_iter_init (&old_iter, self->installed_set);
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

            if (!g_hash_table_contains (self->installed_set, unique_id))
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
        g_clear_pointer (&self->installed_set, g_hash_table_unref);
        self->installed_set = g_steal_pointer (&installed_set);

        fiber_check_for_updates (self);
        bz_state_info_set_background_task_label (self->state, NULL);
      }
      break;
    default:
      g_assert_not_reached ();
    }

done:
  return dex_future_new_true ();
}

static DexFuture *
watch_backend_notifs_then_loop_cb (DexFuture *future,
                                   GWeakRef  *wr)
{
  g_autoptr (BzApplication) self        = NULL;
  g_autoptr (GError) local_error        = NULL;
  const GValue          *value          = NULL;
  BzBackendNotification *notif          = NULL;
  g_autoptr (RespondToFlatpakData) data = NULL;
  g_autoptr (DexFuture) ret_future      = NULL;

  bz_weak_get_or_return_reject (self, wr);

  value = dex_future_get_value (future, NULL);
  g_assert (value != NULL);
  notif = g_value_get_object (value);

  data        = respond_to_flatpak_data_new ();
  data->self  = bz_track_weak (self);
  data->notif = g_object_ref (notif);

  ret_future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) respond_to_flatpak_fiber,
      respond_to_flatpak_data_ref (data),
      respond_to_flatpak_data_unref);
  ret_future = dex_future_finally (
      ret_future,
      (DexFutureCallback) receive_from_flatpak_cb,
      bz_track_weak (self),
      bz_weak_release);
  return g_steal_pointer (&ret_future);
}

static gboolean
periodic_timeout_cb (BzApplication *self)
{
  /* If for some reason the last update check is still happening, let it
     finish */
  if (self->periodic_sync == NULL ||
      !dex_future_is_pending (self->periodic_sync))
    {
      dex_clear (&self->periodic_sync);
      if (self->n_incoming == 0)
        self->periodic_sync = bz_backend_retrieve_remote_entries (
            BZ_BACKEND (self->flatpak), NULL);
    }

  return G_SOURCE_CONTINUE;
}

static DexFuture *
init_finally (DexFuture *future,
              GWeakRef  *wr)
{
  g_autoptr (BzApplication) self = NULL;
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  bz_weak_get_or_return_reject (self, wr);

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      self->flatpak_notifs = bz_backend_create_notification_channel (
          BZ_BACKEND (self->flatpak));
      self->notif_watch = dex_future_then_loop (
          dex_channel_receive (self->flatpak_notifs),
          (DexFutureCallback) watch_backend_notifs_then_loop_cb,
          bz_track_weak (self),
          bz_weak_release);
      self->periodic_sync = bz_backend_retrieve_remote_entries (
          BZ_BACKEND (self->flatpak), NULL);

      self->periodic_timeout = g_timeout_add_seconds (
          /* Check every ten minutes*/
          60 * 10, (GSourceFunc) periodic_timeout_cb, self);

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
              "Could not initialize: %s",
              local_error->message);
          bz_show_error_for_widget (GTK_WIDGET (window), error_string);
        }
    }

  return dex_future_new_true ();
}

static GtkWindow *
new_window (BzApplication *self)
{
  BzWindow *window                  = NULL;
  g_autoptr (GtkWidget) main_window = NULL;
  int width                         = 0;
  int height                        = 0;

  window = bz_window_new (self->state);
  gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));

  main_window = g_weak_ref_get (&self->main_window);
  if (main_window != NULL)
    {
      width  = gtk_widget_get_width (main_window);
      height = gtk_widget_get_height (main_window);

      g_settings_set (self->settings, "window-dimensions", "(ii)", width, height);
    }
  else
    {
      g_settings_get (self->settings, "window-dimensions", "(ii)", &width, &height);

      g_signal_connect_object (
          window, "close-request",
          G_CALLBACK (window_close_request),
          self, G_CONNECT_SWAPPED);
      g_weak_ref_init (&self->main_window, window);
    }

  gtk_window_set_default_size (GTK_WINDOW (window), width, height);
  gtk_window_present (GTK_WINDOW (window));

  return GTK_WINDOW (window);
}

static gboolean
window_close_request (BzApplication *self,
                      GtkWidget     *window)
{
  int width  = 0;
  int height = 0;

  width  = gtk_widget_get_width (window);
  height = gtk_widget_get_height (window);

  g_settings_set (self->settings, "window-dimensions",
                  "(ii)", width, height);

  /* Do not stop other handlers from being invoked for the signal */
  return FALSE;
}

static void
open_appstream_take (BzApplication *self,
                     char          *appstream)
{
  g_assert (appstream != NULL);

  if (g_str_has_prefix (appstream, "appstream://"))
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
  g_autofree char *path               = NULL;
  g_autoptr (OpenFlatpakrefData) data = NULL;
  g_autoptr (DexFuture) future        = NULL;

  path = g_file_get_path (file);
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

static void
blocklists_changed (BzApplication *self,
                    guint          position,
                    guint          removed,
                    guint          added,
                    GListModel    *model)
{
  g_autoptr (GError) local_error = NULL;
  gboolean result                = FALSE;

  if (removed > 0)
    g_ptr_array_remove_range (self->blocklist_regexes, position, removed);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (GtkStringObject) string = NULL;
      const char      *filename          = NULL;
      gsize            length            = 0;
      g_autofree char *contents          = NULL;
      g_autoptr (BzRootBlocklist) root   = NULL;
      g_autoptr (GPtrArray) regex_datas  = NULL;

      string   = g_list_model_get_item (model, position + i);
      filename = gtk_string_object_get_string (string);

      /* IO on main thread but this will basically never happen except at the
         beginning of the program and when using the debug menu */
      result = g_file_get_contents (filename, &contents, &length, &local_error);
      if (result)
        {
          g_autoptr (GBytes) bytes             = NULL;
          g_autoptr (GHashTable) parse_results = NULL;

          bytes         = g_bytes_new_take (g_steal_pointer (&contents), length);
          parse_results = bz_yaml_parser_process_bytes (self->blocklist_parser, bytes, &local_error);
          if (parse_results != NULL)
            root = g_value_dup_object (g_hash_table_lookup (parse_results, "/"));
          else
            {
              g_warning ("Unable to parse blocklist at path %s: %s", filename, local_error->message);
              g_clear_error (&local_error);
            }
        }
      else
        {
          g_warning ("Unable to read blocklist at path %s: %s", filename, local_error->message);
          g_clear_error (&local_error);
        }

      regex_datas = g_ptr_array_new_with_free_func (blocklist_regex_data_unref);
      if (root != NULL)
        {
          GListModel *blocklists = NULL;

          blocklists = bz_root_blocklist_get_blocklists (root);
          if (blocklists != NULL)
            {
              guint n_blocklists = 0;

              n_blocklists = g_list_model_get_n_items (blocklists);
              for (guint blocklist_idx = 0; blocklist_idx < n_blocklists; blocklist_idx++)
                {
                  g_autoptr (BzBlocklist) blocklist   = NULL;
                  GListModel *allow                   = NULL;
                  GListModel *allow_regex             = NULL;
                  GListModel *block                   = NULL;
                  GListModel *block_regex             = NULL;
                  g_autoptr (BlocklistRegexData) data = NULL;

                  blocklist   = g_list_model_get_item (blocklists, blocklist_idx);
                  allow       = bz_blocklist_get_allow (blocklist);
                  allow_regex = bz_blocklist_get_allow_regex (blocklist);
                  block       = bz_blocklist_get_block (blocklist);
                  block_regex = bz_blocklist_get_block_regex (blocklist);

                  if (allow == NULL &&
                      allow_regex == NULL &&
                      block == NULL &&
                      block_regex == NULL)
                    {
                      g_warning ("Blocklist file at path %s has an empty blocklist which will be ignored", filename);
                      continue;
                    }

                  data           = blocklist_regex_data_new ();
                  data->priority = bz_blocklist_get_priority (blocklist);

#define BUILD_REGEX(_name, _builder)                                                             \
  if (_name != NULL)                                                                             \
    {                                                                                            \
      guint _n_strings = 0;                                                                      \
                                                                                                 \
      _n_strings = g_list_model_get_n_items (_name);                                             \
      for (guint _i = 0; _i < _n_strings; _i++)                                                  \
        {                                                                                        \
          g_autoptr (GtkStringObject) _object = NULL;                                            \
          const char *_string                 = NULL;                                            \
          g_autoptr (GRegex) _regex           = NULL;                                            \
                                                                                                 \
          _object = g_list_model_get_item (_name, _i);                                           \
          _string = gtk_string_object_get_string (_object);                                      \
          _regex  = g_regex_new (_string, G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT, &local_error); \
                                                                                                 \
          if (_regex != NULL)                                                                    \
            g_strv_builder_add (_builder, _string);                                              \
          else                                                                                   \
            {                                                                                    \
              g_warning ("Blocklist file at path %s has an invalid "                             \
                         "regular expression '%s': %s",                                          \
                         filename, _string, local_error->message);                               \
              g_clear_error (&local_error);                                                      \
            }                                                                                    \
        }                                                                                        \
    }

#define BUILD_REGEX_ESCAPED(_name, _builder)                                   \
  if (_name != NULL)                                                           \
    {                                                                          \
      guint _n_strings = 0;                                                    \
                                                                               \
      _n_strings = g_list_model_get_n_items (_name);                           \
      for (guint _i = 0; _i < _n_strings; _i++)                                \
        {                                                                      \
          g_autoptr (GtkStringObject) _object = NULL;                          \
          const char *_string                 = NULL;                          \
                                                                               \
          _object = g_list_model_get_item (_name, _i);                         \
          _string = gtk_string_object_get_string (_object);                    \
                                                                               \
          g_strv_builder_take (_builder, g_regex_escape_string (_string, -1)); \
        }                                                                      \
    }

#define GATHER(name)                                                    \
  if (name != NULL ||                                                   \
      name##_regex != NULL)                                             \
    {                                                                   \
      g_autoptr (GStrvBuilder) _builder = NULL;                         \
      g_auto (GStrv) _patterns          = NULL;                         \
                                                                        \
      _builder = g_strv_builder_new ();                                 \
                                                                        \
      BUILD_REGEX_ESCAPED (name, _builder)                              \
      BUILD_REGEX (name##_regex, _builder)                              \
                                                                        \
      _patterns = g_strv_builder_end (_builder);                        \
      if (_patterns != NULL)                                            \
        {                                                               \
          g_autofree char *_joined       = NULL;                        \
          g_autofree char *_regex_string = NULL;                        \
                                                                        \
          _joined       = g_strjoinv ("|", _patterns);                  \
          _regex_string = g_strdup_printf ("^(%s)$", _joined);          \
          data->name    = g_regex_new (_regex_string, G_REGEX_OPTIMIZE, \
                                       G_REGEX_MATCH_DEFAULT, NULL);    \
        }                                                               \
    }

                  GATHER (allow);
                  GATHER (block);

#undef GATHER
#undef BUILD_REGEX_ESCAPED
#undef BUILD_REGEX

                  if (data->allow != NULL || data->block != NULL)
                    g_ptr_array_add (regex_datas, g_steal_pointer (&data));
                }
            }
        }

      g_ptr_array_insert (self->blocklist_regexes,
                          position + i,
                          g_steal_pointer (&regex_datas));
    }

  gtk_filter_changed (GTK_FILTER (self->group_filter), GTK_FILTER_CHANGE_DIFFERENT);
  gtk_filter_changed (GTK_FILTER (self->appid_filter), GTK_FILTER_CHANGE_DIFFERENT);
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

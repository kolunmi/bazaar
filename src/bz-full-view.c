/* bz-full-view.c
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

#define G_LOG_DOMAIN "BAZAAR::FULL-VIEW-WIDGET"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "bz-decorated-screenshot.h"
#include "bz-dynamic-list-view.h"
#include "bz-error.h"
#include "bz-flatpak-entry.h"
#include "bz-full-view.h"
#include "bz-global-state.h"
#include "bz-lazy-async-texture-model.h"
#include "bz-screenshot.h"
#include "bz-section-view.h"
#include "bz-share-dialog.h"
#include "bz-state-info.h"
#include "bz-stats-dialog.h"

struct _BzFullView
{
  AdwBin parent_instance;

  BzStateInfo          *state;
  BzTransactionManager *transactions;
  BzEntryGroup         *group;
  BzResult             *ui_entry;
  BzResult             *debounced_ui_entry;
  BzResult             *group_model;

  guint      debounce_timeout;
  DexFuture *loading_forge_stars;

  /* Template widgets */
  AdwViewStack *stack;
  GtkLabel     *forge_stars_label;
};

G_DEFINE_FINAL_TYPE (BzFullView, bz_full_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_STATE,
  PROP_TRANSACTION_MANAGER,
  PROP_ENTRY_GROUP,
  PROP_UI_ENTRY,
  PROP_DEBOUNCED_UI_ENTRY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,
  SIGNAL_REMOVE,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
debounce_timeout (BzFullView *self);

static DexFuture *
retrieve_star_string_fiber (BzFullView *self);

static void
bz_full_view_dispose (GObject *object)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  g_clear_object (&self->state);
  g_clear_object (&self->transactions);
  g_clear_object (&self->group);
  g_clear_object (&self->ui_entry);
  g_clear_object (&self->debounced_ui_entry);
  g_clear_object (&self->group_model);

  dex_clear (&self->loading_forge_stars);
  g_clear_handle_id (&self->debounce_timeout, g_source_remove);

  G_OBJECT_CLASS (bz_full_view_parent_class)->dispose (object);
}

static void
bz_full_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    case PROP_TRANSACTION_MANAGER:
      g_value_set_object (value, bz_full_view_get_transaction_manager (self));
      break;
    case PROP_ENTRY_GROUP:
      g_value_set_object (value, bz_full_view_get_entry_group (self));
      break;
    case PROP_UI_ENTRY:
      g_value_set_object (value, self->ui_entry);
      break;
    case PROP_DEBOUNCED_UI_ENTRY:
      g_value_set_object (value, self->debounced_ui_entry);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_full_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_clear_object (&self->state);
      self->state = g_value_dup_object (value);
      break;
    case PROP_TRANSACTION_MANAGER:
      bz_full_view_set_transaction_manager (self, g_value_get_object (value));
      break;
    case PROP_ENTRY_GROUP:
      bz_full_view_set_entry_group (self, g_value_get_object (value));
      break;
    case PROP_UI_ENTRY:
    case PROP_DEBOUNCED_UI_ENTRY:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static char *
format_recent_downloads (gpointer object,
                         int      value)
{
  if (value > 0)
    return g_strdup_printf ("%'d", value);
  else
    return g_strdup ("---");
}

static char *
format_size (gpointer object, guint64 value)
{
  return g_format_size (value);
}

static char *
format_timestamp (gpointer object,
                  guint64  value)
{
  g_autoptr (GDateTime) date = NULL;

  date = g_date_time_new_from_unix_utc (value);
  return g_date_time_format (date, _ ("Released %x"));
}

static char *
format_as_link (gpointer    object,
                const char *value)
{
  if (value != NULL)
    return g_strdup_printf ("<a href=\"%s\" title=\"%s\">%s</a>",
                            value, value, value);
  else
    return g_strdup (_ ("No URL"));
}

static char *
pick_license_warning (gpointer object,
                      gboolean value)
{
  return value
             ? g_strdup (_ ("This application has a FLOSS license, meaning the source code can be audited for safety."))
             : g_strdup (_ ("This application has a proprietary license, meaning the source code is developed privately and cannot be audited by an independent third party."));
}

static void
open_url_cb (BzFullView   *self,
             AdwActionRow *row)
{
  BzEntry    *entry = NULL;
  const char *url   = NULL;

  entry = BZ_ENTRY (bz_result_get_object (self->ui_entry));
  url   = bz_entry_get_url (entry);

  if (url != NULL && *url != '\0')
    g_app_info_launch_default_for_uri (url, NULL, NULL);
  else
    g_warning ("Invalid or empty URL provided");
}

static void
share_cb (BzFullView *self,
          GtkButton  *button)
{
  AdwDialog *share_dialog = NULL;

  if (self->group == NULL)
    return;

  share_dialog = bz_share_dialog_new (bz_result_get_object (self->ui_entry));
  gtk_widget_set_size_request (GTK_WIDGET (share_dialog), 400, -1);

  adw_dialog_present (share_dialog, GTK_WIDGET (self));
}

static void
dl_stats_cb (BzFullView *self,
             GtkButton  *button)
{
  AdwDialog *dialog   = NULL;
  BzEntry   *ui_entry = NULL;

  if (self->group == NULL)
    return;

  dialog = bz_stats_dialog_new (NULL);
  adw_dialog_set_content_width (dialog, 2000);
  adw_dialog_set_content_height (dialog, 1500);

  ui_entry = bz_result_get_object (self->ui_entry);
  g_object_bind_property (ui_entry, "download-stats", dialog, "model", G_BINDING_SYNC_CREATE);

  adw_dialog_present (dialog, GTK_WIDGET (self));
  bz_stats_dialog_animate_open (BZ_STATS_DIALOG (dialog));
}

static void
run_cb (BzFullView *self,
        GtkButton  *button)
{
  GListModel *model   = NULL;
  guint       n_items = 0;

  if (self->group == NULL || !bz_result_get_resolved (self->group_model))
    return;

  model   = bz_result_get_object (self->group_model);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (model, i);

      if (BZ_IS_FLATPAK_ENTRY (entry) && bz_entry_is_installed (entry))
        {
          g_autoptr (GError) local_error = NULL;
          gboolean result                = FALSE;

          result = bz_flatpak_entry_launch (
              BZ_FLATPAK_ENTRY (entry),
              BZ_FLATPAK_INSTANCE (bz_state_info_get_backend (self->state)),
              &local_error);
          if (!result)
            {
              GtkWidget *window = NULL;

              window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
              if (window != NULL)
                bz_show_error_for_widget (window, local_error->message);
            }
          break;
        }
    }
}

static void
install_cb (BzFullView *self,
            GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_INSTALL], 0);
}

static void
remove_cb (BzFullView *self,
           GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_REMOVE], 0);
}

static void
support_cb (BzFullView *self,
            GtkButton  *button)
{
  BzEntry *entry = NULL;

  entry = bz_result_get_object (self->ui_entry);
  if (entry != NULL)
    {
      const char *url = NULL;

      url = bz_entry_get_donation_url (entry);
      g_app_info_launch_default_for_uri (url, NULL, NULL);
    }
}

static void
forge_cb (BzFullView *self,
          GtkButton  *button)
{
  BzEntry *entry = NULL;

  entry = bz_result_get_object (self->ui_entry);
  if (entry != NULL)
    {
      const char *url = NULL;

      url = bz_entry_get_forge_url (entry);
      g_app_info_launch_default_for_uri (url, NULL, NULL);
    }
}

static void
screenshots_bind_widget_cb (BzFullView            *self,
                            BzDecoratedScreenshot *screenshot,
                            GdkPaintable          *paintable,
                            BzDynamicListView     *view)
{
  gtk_widget_set_focusable (GTK_WIDGET (screenshot), TRUE);
  gtk_widget_set_margin_top (GTK_WIDGET (screenshot), 5);
  gtk_widget_set_margin_bottom (GTK_WIDGET (screenshot), 5);
}

static void
screenshots_unbind_widget_cb (BzFullView            *self,
                              BzDecoratedScreenshot *screenshot,
                              GdkPaintable          *paintable,
                              BzDynamicListView     *view)
{
}

static void
bz_full_view_class_init (BzFullViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_full_view_dispose;
  object_class->get_property = bz_full_view_get_property;
  object_class->set_property = bz_full_view_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSACTION_MANAGER] =
      g_param_spec_object (
          "transaction-manager",
          NULL, NULL,
          BZ_TYPE_TRANSACTION_MANAGER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ENTRY_GROUP] =
      g_param_spec_object (
          "entry-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_UI_ENTRY] =
      g_param_spec_object (
          "ui-entry",
          NULL, NULL,
          BZ_TYPE_RESULT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEBOUNCED_UI_ENTRY] =
      g_param_spec_object (
          "debounced-ui-entry",
          NULL, NULL,
          BZ_TYPE_RESULT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__VOIDv);

  signals[SIGNAL_REMOVE] =
      g_signal_new (
          "remove",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_REMOVE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__VOIDv);

  g_type_ensure (BZ_TYPE_DECORATED_SCREENSHOT);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_ENTRY);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);
  g_type_ensure (BZ_TYPE_LAZY_ASYNC_TEXTURE_MODEL);
  g_type_ensure (BZ_TYPE_SCREENSHOT);
  g_type_ensure (BZ_TYPE_SECTION_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-full-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzFullView, stack);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, forge_stars_label);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, format_recent_downloads);
  gtk_widget_class_bind_template_callback (widget_class, format_size);
  gtk_widget_class_bind_template_callback (widget_class, format_timestamp);
  gtk_widget_class_bind_template_callback (widget_class, format_as_link);
  gtk_widget_class_bind_template_callback (widget_class, open_url_cb);
  gtk_widget_class_bind_template_callback (widget_class, share_cb);
  gtk_widget_class_bind_template_callback (widget_class, dl_stats_cb);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, support_cb);
  gtk_widget_class_bind_template_callback (widget_class, forge_cb);
  gtk_widget_class_bind_template_callback (widget_class, screenshots_bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, screenshots_unbind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, pick_license_warning);
}

static void
bz_full_view_init (BzFullView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_full_view_new (void)
{
  return g_object_new (BZ_TYPE_FULL_VIEW, NULL);
}

void
bz_full_view_set_transaction_manager (BzFullView           *self,
                                      BzTransactionManager *transactions)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (transactions == NULL ||
                    BZ_IS_TRANSACTION_MANAGER (transactions));

  g_clear_object (&self->transactions);
  if (transactions != NULL)
    self->transactions = g_object_ref (transactions);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSACTION_MANAGER]);
}

BzTransactionManager *
bz_full_view_get_transaction_manager (BzFullView *self)
{
  g_return_val_if_fail (BZ_IS_FULL_VIEW (self), NULL);
  return self->transactions;
}

void
bz_full_view_set_entry_group (BzFullView   *self,
                              BzEntryGroup *group)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (group == NULL ||
                    BZ_IS_ENTRY_GROUP (group));

  g_clear_handle_id (&self->debounce_timeout, g_source_remove);
  g_clear_object (&self->group);
  g_clear_object (&self->ui_entry);
  g_clear_object (&self->debounced_ui_entry);
  g_clear_object (&self->group_model);

  gtk_label_set_label (self->forge_stars_label, "...");

  if (group != NULL)
    {
      g_autoptr (DexFuture) future = NULL;

      self->group            = g_object_ref (group);
      self->ui_entry         = bz_entry_group_dup_ui_entry (group);
      self->debounce_timeout = g_timeout_add_once (
          300, (GSourceOnceFunc) debounce_timeout, self);

      future            = bz_entry_group_dup_all_into_model (group);
      self->group_model = bz_result_new (future);

      adw_view_stack_set_visible_child_name (self->stack, "content");
    }
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY_GROUP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UI_ENTRY]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEBOUNCED_UI_ENTRY]);
}

BzEntryGroup *
bz_full_view_get_entry_group (BzFullView *self)
{
  g_return_val_if_fail (BZ_IS_FULL_VIEW (self), NULL);
  return self->group;
}

static void
debounce_timeout (BzFullView *self)
{
  self->debounce_timeout = 0;
  if (self->group == NULL)
    return;

  g_clear_object (&self->debounced_ui_entry);
  self->debounced_ui_entry = g_object_ref (self->ui_entry);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEBOUNCED_UI_ENTRY]);

  /* Disabled for now since we don't want to users to be rate limited
     by github */
  // dex_clear (&self->loading_forge_stars);
  // self->loading_forge_stars = dex_scheduler_spawn (
  //     dex_scheduler_get_default (),
  //     bz_get_dex_stack_size (),
  //     (DexFiberFunc) retrieve_star_string_fiber,
  //     self, NULL);
}

static DexFuture *
retrieve_star_string_fiber (BzFullView *self)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  const char      *forge_link    = NULL;
  g_autofree char *star_url      = NULL;
  g_autoptr (JsonNode) node      = NULL;
  JsonObject      *object        = NULL;
  gint64           star_count    = 0;
  g_autofree char *fmt           = NULL;

  entry = dex_await_object (bz_result_dup_future (self->ui_entry), NULL);
  if (entry == NULL)
    goto done;

  forge_link = bz_entry_get_forge_url (entry);
  if (forge_link == NULL)
    goto done;

  if (g_regex_match_simple (
          "https://github.com/.*/.*",
          forge_link,
          G_REGEX_DEFAULT,
          G_REGEX_MATCH_DEFAULT))
    star_url = g_strdup_printf (
        "https://api.github.com/repos/%s",
        forge_link + strlen ("https://github.com/"));
  else
    goto done;

  node = dex_await_boxed (bz_https_query_json (star_url), &local_error);
  if (node == NULL)
    {
      g_warning ("Could not retrieve vcs star count at %s: %s",
                 forge_link, local_error->message);
      goto done;
    }

  object     = json_node_get_object (node);
  star_count = json_object_get_int_member_with_default (object, "stargazers_count", 0);
  fmt        = g_strdup_printf ("%'zu", star_count);

done:
  gtk_label_set_label (self->forge_stars_label, fmt != NULL ? fmt : "?");
  return NULL;
}

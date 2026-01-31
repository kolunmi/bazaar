/* bz-transaction-view.c
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

#include <glib/gi18n.h>

#include "bz-application-map-factory.h"
#include "bz-entry-group.h"
#include "bz-entry.h"
#include "bz-flatpak-entry.h"
#include "bz-state-info.h"
#include "bz-transaction-view.h"
#include "bz-window.h"

struct _BzTransactionView
{
  AdwBin parent_instance;

  BzTransaction *transaction;
};

G_DEFINE_FINAL_TYPE (BzTransactionView, bz_transaction_view, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_TRANSACTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static BzEntryGroup *
resolve_group_from_entry (BzEntry  *entry,
                          BzWindow *window);

static void
bz_transaction_view_dispose (GObject *object)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  g_clear_pointer (&self->transaction, g_object_unref);

  G_OBJECT_CLASS (bz_transaction_view_parent_class)->dispose (object);
}

static void
bz_transaction_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      g_value_set_object (value, bz_transaction_view_get_transaction (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      bz_transaction_view_set_transaction (self, g_value_get_object (value));
      break;
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
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static char *
format_download_size (gpointer object,
                      guint64  value)
{
  return g_format_size (value);
}

static char *
format_installed_size (gpointer object,
                       guint64  value)
{
  return g_format_size (value);
}

static char *
format_bytes_transferred (gpointer object,
                          guint64  value)
{
  g_autofree char *size = NULL;

  size = g_format_size (value);
  return g_strdup_printf (_ ("Transferred %s so far"), size);
}

static char *
format_download_progress (gpointer object,
                          double   progress,
                          guint64  total_size)
{
  guint64          downloaded     = (guint64) (progress * total_size);
  g_autofree char *downloaded_str = g_format_size (downloaded);
  g_autofree char *total_str      = g_format_size (total_size);

  return g_strdup_printf ("%s / %s", downloaded_str, total_str);
}

static gboolean
filter_finished_ops_by_app_id (gpointer item,
                               gpointer user_data)
{
  BzTransactionTask             *task     = BZ_TRANSACTION_TASK (item);
  BzTransactionEntryTracker     *tracker  = BZ_TRANSACTION_ENTRY_TRACKER (user_data);
  BzEntry                       *entry    = NULL;
  const char                    *entry_id = NULL;
  BzBackendTransactionOpPayload *op       = NULL;
  const char                    *op_name  = NULL;
  const char                    *error    = NULL;

  if (task == NULL || tracker == NULL)
    return TRUE;

  error = bz_transaction_task_get_error (task);
  if (error != NULL)
    return TRUE;

  entry = bz_transaction_entry_tracker_get_entry (tracker);
  if (entry == NULL)
    return TRUE;

  entry_id = bz_entry_get_id (entry);
  if (entry_id == NULL)
    return TRUE;

  op = bz_transaction_task_get_op (task);
  if (op == NULL)
    return TRUE;

  op_name = bz_backend_transaction_op_payload_get_name (op);
  if (op_name == NULL)
    return TRUE;

  return strstr (op_name, entry_id) == NULL;
}

static GtkFilter *
create_app_id_filter (gpointer                   object,
                      BzTransactionEntryTracker *tracker)
{
  GtkCustomFilter *filter = NULL;

  if (tracker == NULL)
    return NULL;

  filter = gtk_custom_filter_new (filter_finished_ops_by_app_id,
                                  g_object_ref (tracker),
                                  g_object_unref);

  return GTK_FILTER (filter);
}

static gboolean
is_transaction_type (gpointer                   object,
                     BzTransactionEntryTracker *tracker,
                     int                        type)
{
  if (tracker == NULL)
    return FALSE;

  return bz_transaction_entry_tracker_get_kind (tracker) == type;
}

static gboolean
is_transaction_tracker_install (gpointer                   object,
                                BzTransactionEntryTracker *tracker)
{
  return is_transaction_type (object, tracker, BZ_TRANSACTION_ENTRY_KIND_INSTALL);
}

static gboolean
is_transaction_tracker_update (gpointer                   object,
                               BzTransactionEntryTracker *tracker)
{
  return is_transaction_type (object, tracker, BZ_TRANSACTION_ENTRY_KIND_UPDATE);
}

static gboolean
is_transaction_tracker_removal (gpointer                   object,
                                BzTransactionEntryTracker *tracker)
{
  return is_transaction_type (object, tracker, BZ_TRANSACTION_ENTRY_KIND_REMOVAL);
}

static gboolean
list_has_items (gpointer    object,
                GListModel *model)
{
  if (model == NULL)
    return FALSE;

  return g_list_model_get_n_items (model) > 0;
}

static gboolean
is_queued (gpointer                 object,
           BzTransactionEntryStatus status)
{
  return status == BZ_TRANSACTION_ENTRY_STATUS_QUEUED;
}

static gboolean
is_ongoing (gpointer                 object,
            BzTransactionEntryStatus status)
{
  return status == BZ_TRANSACTION_ENTRY_STATUS_ONGOING;
}

static gboolean
is_completed (gpointer                 object,
              BzTransactionEntryStatus status)
{
  return status == BZ_TRANSACTION_ENTRY_STATUS_DONE;
}

static gboolean
is_both (gpointer object,
         gboolean first,
         gboolean second)
{
  return first && second;
}

static GdkPaintable *
get_main_icon (GtkListItem               *list_item,
               BzTransactionEntryTracker *tracker)
{
  BzEntry      *entry          = NULL;
  GdkPaintable *icon_paintable = NULL;

  if (tracker == NULL)
    goto return_generic;

  entry = bz_transaction_entry_tracker_get_entry (tracker);
  if (entry == NULL)
    goto return_generic;

  icon_paintable = bz_entry_get_icon_paintable (entry);
  if (icon_paintable != NULL)
    return g_object_ref (icon_paintable);
  else if (BZ_IS_FLATPAK_ENTRY (entry))
    {
      BzWindow *window               = NULL;
      g_autoptr (BzEntryGroup) group = NULL;

      window = (BzWindow *) gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_WINDOW);
      if (window == NULL)
        goto return_generic;

      group = resolve_group_from_entry (entry, window);
      if (group == NULL)
        goto return_generic;

      icon_paintable = bz_entry_group_get_icon_paintable (group);
      if (icon_paintable != NULL)
        return g_object_ref (icon_paintable);
    }

return_generic:
  return (GdkPaintable *) gtk_icon_theme_lookup_icon (
      gtk_icon_theme_get_for_display (gdk_display_get_default ()),
      "application-x-executable",
      NULL,
      64,
      1,
      gtk_widget_get_default_direction (),
      GTK_ICON_LOOKUP_NONE);
}

static gboolean
is_entry_kind (gpointer                   object,
               BzTransactionEntryTracker *tracker,
               int                        kind)
{
  BzEntry *entry = NULL;

  if (tracker == NULL)
    return FALSE;

  entry = bz_transaction_entry_tracker_get_entry (tracker);
  if (entry == NULL)
    return FALSE;

  return bz_entry_is_of_kinds (entry, kind);
}

/* Sucks but we can't rely on magic numbers in the blueprint */
static gboolean
is_entry_application (gpointer                   object,
                      BzTransactionEntryTracker *tracker)
{
  return is_entry_kind (object, tracker, BZ_ENTRY_KIND_APPLICATION);
}

static gboolean
is_entry_runtime (gpointer                   object,
                  BzTransactionEntryTracker *tracker)
{
  return is_entry_kind (object, tracker, BZ_ENTRY_KIND_RUNTIME);
}

static gboolean
is_entry_addon (gpointer                   object,
                BzTransactionEntryTracker *tracker)
{
  return is_entry_kind (object, tracker, BZ_ENTRY_KIND_ADDON);
}

static void
activate (BzTransactionView *self,
          guint              position,
          GtkListView       *list_view)
{
  g_autoptr (GListModel) model                  = NULL;
  g_autoptr (BzTransactionEntryTracker) tracker = NULL;
  BzEntry  *entry                               = NULL;
  BzWindow *window                              = NULL;
  g_autoptr (BzEntryGroup) group                = NULL;

  g_object_get (self->transaction, "trackers", &model, NULL);
  tracker = g_list_model_get_item (model, position);
  entry   = bz_transaction_entry_tracker_get_entry (tracker);

  window = (BzWindow *) gtk_widget_get_ancestor (GTK_WIDGET (self), BZ_TYPE_WINDOW);
  if (window == NULL)
    return;

  group = resolve_group_from_entry (entry, window);
  if (group == NULL)
    return;

  bz_window_show_group (window, group);
}

static void
bz_transaction_view_class_init (BzTransactionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_transaction_view_set_property;
  object_class->get_property = bz_transaction_view_get_property;
  object_class->dispose      = bz_transaction_view_dispose;

  props[PROP_TRANSACTION] =
      g_param_spec_object (
          "transaction",
          NULL, NULL,
          BZ_TYPE_TRANSACTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-transaction-view.ui");
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, format_download_size);
  gtk_widget_class_bind_template_callback (widget_class, format_installed_size);
  gtk_widget_class_bind_template_callback (widget_class, format_bytes_transferred);
  gtk_widget_class_bind_template_callback (widget_class, format_download_progress);
  gtk_widget_class_bind_template_callback (widget_class, get_main_icon);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_application);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_runtime);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_addon);
  gtk_widget_class_bind_template_callback (widget_class, activate);
  gtk_widget_class_bind_template_callback (widget_class, create_app_id_filter);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_install);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_update);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_removal);
  gtk_widget_class_bind_template_callback (widget_class, list_has_items);
  gtk_widget_class_bind_template_callback (widget_class, is_queued);
  gtk_widget_class_bind_template_callback (widget_class, is_ongoing);
  gtk_widget_class_bind_template_callback (widget_class, is_completed);
  gtk_widget_class_bind_template_callback (widget_class, is_both);
}

static void
bz_transaction_view_init (BzTransactionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzTransactionView *
bz_transaction_view_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_VIEW, NULL);
}

BzTransaction *
bz_transaction_view_get_transaction (BzTransactionView *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_VIEW (self), NULL);
  return self->transaction;
}

void
bz_transaction_view_set_transaction (BzTransactionView *self,
                                     BzTransaction     *transaction)
{
  g_return_if_fail (BZ_IS_TRANSACTION_VIEW (self));

  g_clear_pointer (&self->transaction, g_object_unref);
  if (transaction != NULL)
    self->transaction = g_object_ref (transaction);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSACTION]);
}

static BzEntryGroup *
resolve_group_from_entry (BzEntry  *entry,
                          BzWindow *window)
{
  BzStateInfo     *info                 = NULL;
  const char      *extension_of_ref     = NULL;
  g_autofree char *extension_of_ref_dup = NULL;
  char            *generic_id           = NULL;
  char            *generic_id_term      = NULL;
  g_autoptr (BzEntryGroup) group        = NULL;

  info = bz_window_get_state_info (window);
  if (info == NULL)
    return NULL;

  if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION))
    {
      const char *id = NULL;

      id    = bz_entry_get_id (entry);
      group = bz_application_map_factory_convert_one (
          bz_state_info_get_application_factory (info),
          gtk_string_object_new (id));
      if (group != NULL)
        return g_steal_pointer (&group);
    }

  extension_of_ref = bz_flatpak_entry_get_addon_extension_of_ref (BZ_FLATPAK_ENTRY (entry));
  if (extension_of_ref == NULL)
    return NULL;

  extension_of_ref_dup = g_strdup (extension_of_ref);
  generic_id           = strchr (extension_of_ref_dup, '/');
  if (generic_id == NULL)
    return NULL;

  generic_id++;
  generic_id_term = strchr (generic_id, '/');
  if (generic_id_term != NULL)
    *generic_id_term = '\0';

  group = bz_application_map_factory_convert_one (
      bz_state_info_get_application_factory (info),
      gtk_string_object_new (generic_id));
  if (group == NULL)
    return NULL;

  return g_steal_pointer (&group);
}

/* End of bz-transaction-view.c */

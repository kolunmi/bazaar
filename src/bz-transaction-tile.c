/* bz-transaction-tile.c
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
#include "bz-application.h"
#include "bz-entry-group.h"
#include "bz-entry.h"
#include "bz-error.h"
#include "bz-flatpak-entry.h"
#include "bz-list-tile.h"
#include "bz-state-info.h"
#include "bz-template-callbacks.h"
#include "bz-transaction-tile.h"
#include "bz-window.h"

struct _BzTransactionTile
{
  BzListTile parent_instance;

  BzTransactionEntryTracker *tracker;
};

G_DEFINE_FINAL_TYPE (BzTransactionTile, bz_transaction_tile, BZ_TYPE_LIST_TILE);

enum
{
  PROP_0,

  PROP_TRACKER,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static BzEntryGroup *
resolve_group_from_entry (BzEntry  *entry,
                          BzWindow *window);

static void
bz_transaction_tile_dispose (GObject *object)
{
  BzTransactionTile *self = BZ_TRANSACTION_TILE (object);

  g_clear_pointer (&self->tracker, g_object_unref);

  G_OBJECT_CLASS (bz_transaction_tile_parent_class)->dispose (object);
}

static void
bz_transaction_tile_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzTransactionTile *self = BZ_TRANSACTION_TILE (object);

  switch (prop_id)
    {
    case PROP_TRACKER:
      g_value_set_object (value, bz_transaction_tile_get_tracker (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_tile_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzTransactionTile *self = BZ_TRANSACTION_TILE (object);

  switch (prop_id)
    {
    case PROP_TRACKER:
      bz_transaction_tile_set_tracker (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
format_removal_size (gpointer object,
                     guint64  value)
{
  g_autofree char *size = NULL;

  size = g_format_size (value);
  return g_strdup_printf (_ ("%s Freed"), size);
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
is_transaction_tracker_errored (gpointer    object,
                                GListModel *finished_ops)
{
  guint n_items = 0;

  if (finished_ops == NULL)
    return FALSE;

  n_items = g_list_model_get_n_items (finished_ops);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzTransactionTask) task = NULL;

      task = g_list_model_get_item (finished_ops, i);
      if (bz_transaction_task_get_error (task) != NULL)
        return TRUE;
    }

  return FALSE;
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
get_main_icon (gpointer                   object,
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

      window = (BzWindow *) gtk_widget_get_ancestor (object, BZ_TYPE_WINDOW);
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
run_cb (BzTransactionTile *self,
        GtkButton         *button)
{
  BzTransactionEntryTracker *tracker = NULL;
  BzEntry                   *entry   = NULL;

  tracker = bz_transaction_tile_get_tracker (self);
  if (tracker == NULL)
    return;

  entry = bz_transaction_entry_tracker_get_entry (tracker);
  if (entry == NULL || !BZ_IS_FLATPAK_ENTRY (entry))
    return;

  if (bz_entry_is_installed (entry))
    {
      g_autoptr (GError) local_error = NULL;
      gboolean     result            = FALSE;
      BzWindow    *window            = NULL;
      BzStateInfo *state             = NULL;

      window = (BzWindow *) gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_WINDOW);
      if (window == NULL)
        return;

      state = bz_window_get_state_info (window);
      if (state == NULL)
        return;

      result = bz_flatpak_entry_launch (
          BZ_FLATPAK_ENTRY (entry),
          BZ_FLATPAK_INSTANCE (bz_state_info_get_backend (state)),
          &local_error);

      if (!result)
        bz_show_error_for_widget (GTK_WIDGET (window), _ ("Failed to launch application"), local_error->message);
    }
}

static void
cancel_cb (BzTransactionTile *self,
           GtkButton         *button)
{
  gboolean                   result  = FALSE;
  BzTransactionEntryTracker *tracker = NULL;
  BzEntry                   *entry   = NULL;
  BzStateInfo               *state   = NULL;
  BzBackend                 *backend = NULL;

  tracker = bz_transaction_tile_get_tracker (self);
  if (tracker == NULL)
    return;

  entry = bz_transaction_entry_tracker_get_entry (tracker);
  if (entry == NULL || !BZ_IS_FLATPAK_ENTRY (entry))
    return;

  state   = bz_state_info_get_default ();
  backend = bz_state_info_get_backend (state);
  if (backend == NULL)
    return;

  result = bz_backend_cancel_task_for_entry (backend, entry);
  if (result)
    gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
}

static void
bz_transaction_tile_class_init (BzTransactionTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_transaction_tile_set_property;
  object_class->get_property = bz_transaction_tile_get_property;
  object_class->dispose      = bz_transaction_tile_dispose;

  props[PROP_TRACKER] =
      g_param_spec_object (
          "tracker",
          NULL, NULL,
          BZ_TYPE_TRANSACTION_ENTRY_TRACKER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_LIST_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-transaction-tile.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_callback (widget_class, format_removal_size);
  gtk_widget_class_bind_template_callback (widget_class, format_download_progress);
  gtk_widget_class_bind_template_callback (widget_class, get_main_icon);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_application);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_runtime);
  gtk_widget_class_bind_template_callback (widget_class, is_entry_addon);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_install);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_update);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_removal);
  gtk_widget_class_bind_template_callback (widget_class, is_transaction_tracker_errored);
  gtk_widget_class_bind_template_callback (widget_class, list_has_items);
  gtk_widget_class_bind_template_callback (widget_class, is_queued);
  gtk_widget_class_bind_template_callback (widget_class, is_ongoing);
  gtk_widget_class_bind_template_callback (widget_class, is_completed);
  gtk_widget_class_bind_template_callback (widget_class, is_both);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_cb);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
bz_transaction_tile_init (BzTransactionTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzTransactionTile *
bz_transaction_tile_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_TILE, NULL);
}

BzTransactionEntryTracker *
bz_transaction_tile_get_tracker (BzTransactionTile *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_TILE (self), NULL);
  return self->tracker;
}

void
bz_transaction_tile_set_tracker (BzTransactionTile         *self,
                                 BzTransactionEntryTracker *tracker)
{
  g_return_if_fail (BZ_IS_TRANSACTION_TILE (self));

  g_clear_pointer (&self->tracker, g_object_unref);
  if (tracker != NULL)
    self->tracker = g_object_ref (tracker);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRACKER]);
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

/* End of bz-transaction-tile.c */

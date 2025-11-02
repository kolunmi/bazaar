/* bz-rich-app-tile.c
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
#include "bz-rich-app-tile.h"
#include "bz-entry.h"
#include "bz-group-tile-css-watcher.h"
#include "adwaita.h"
#include "bz-decorated-screenshot.h"
#include "bz-screenshot.h"
#include "bz-rounded-picture.h"
struct _BzRichAppTile
{
  AdwBin parent_instance;
  BzEntryGroup *group;
  GdkPaintable *first_screenshot;
  gboolean has_screenshot;
  BzGroupTileCssWatcher *css;

  GtkWidget *picture_box;
};

G_DEFINE_FINAL_TYPE (BzRichAppTile, bz_rich_app_tile, ADW_TYPE_BIN);

enum
{
  PROP_0,
  PROP_GROUP,
  PROP_FIRST_SCREENSHOT,
  PROP_HAS_SCREENSHOT,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void update_screenshot (BzRichAppTile *self);

static void
ui_entry_resolved_cb (BzResult      *result,
                      GParamSpec    *pspec,
                      BzRichAppTile *self)
{
  update_screenshot (self);
}

static inline void
notify_properties (BzRichAppTile *self, gboolean has_screenshot)
{
  if (self->has_screenshot != has_screenshot)
    {
      self->has_screenshot = has_screenshot;
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_SCREENSHOT]);
    }
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FIRST_SCREENSHOT]);
}

static void
update_screenshot (BzRichAppTile *self)
{
  g_autoptr (BzResult) ui_entry_result = NULL;
  g_autoptr (GListModel) screenshots   = NULL;
  BzEntry *ui_entry;
  gboolean has_screenshot = FALSE;

  g_clear_object (&self->first_screenshot);

  if (self->group == NULL)
    {
      notify_properties (self, has_screenshot);
      return;
    }

  ui_entry_result = bz_entry_group_dup_ui_entry (self->group);
  if (ui_entry_result == NULL)
    {
      notify_properties (self, has_screenshot);
      return;
    }

  if (!bz_result_get_resolved (ui_entry_result))
    {
      g_signal_connect (ui_entry_result, "notify::resolved",
                        G_CALLBACK (ui_entry_resolved_cb), self);
      return;
    }

  ui_entry = bz_result_get_object (ui_entry_result);
  if (ui_entry == NULL)
    {
      notify_properties (self, has_screenshot);
      return;
    }

  g_object_get (ui_entry, "screenshot-paintables", &screenshots, NULL);
  if (screenshots == NULL)
    {
      notify_properties (self, has_screenshot);
      return;
    }

  if (g_list_model_get_n_items (screenshots) == 0)
    {
      notify_properties (self, has_screenshot);
      return;
    }

  self->first_screenshot = g_list_model_get_item (screenshots, 0);
  has_screenshot         = TRUE;

  notify_properties (self, has_screenshot);
}

static void
bz_rich_app_tile_dispose (GObject *object)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);
  g_clear_object (&self->group);
  g_clear_object (&self->first_screenshot);
  g_clear_object (&self->css);
  G_OBJECT_CLASS (bz_rich_app_tile_parent_class)->dispose (object);
}

static void
bz_rich_app_tile_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);
  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, bz_rich_app_tile_get_group (self));
      break;
    case PROP_FIRST_SCREENSHOT:
      g_value_set_object (value, self->first_screenshot);
      break;
    case PROP_HAS_SCREENSHOT:
      g_value_set_boolean (value, self->has_screenshot);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_rich_app_tile_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  BzRichAppTile *self = BZ_RICH_APP_TILE (object);
  switch (prop_id)
    {
    case PROP_GROUP:
      bz_rich_app_tile_set_group (self, g_value_get_object (value));
      break;
    case PROP_FIRST_SCREENSHOT:
    case PROP_HAS_SCREENSHOT:
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

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static void
bz_rich_app_tile_class_init (BzRichAppTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  object_class->set_property = bz_rich_app_tile_set_property;
  object_class->get_property = bz_rich_app_tile_get_property;
  object_class->dispose      = bz_rich_app_tile_dispose;
  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FIRST_SCREENSHOT] =
      g_param_spec_object ("first-screenshot", NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_HAS_SCREENSHOT] =
      g_param_spec_boolean ("has-screenshot", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_type_ensure (BZ_TYPE_ROUNDED_PICTURE);
  g_object_class_install_properties (object_class, LAST_PROP, props);
  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-rich-app-tile.ui");
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_child (widget_class, BzRichAppTile, picture_box);
}

static void
bz_rich_app_tile_init (BzRichAppTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->css = bz_group_tile_css_watcher_new ();
  bz_group_tile_css_watcher_set_widget (self->css, self->picture_box);
}

GtkWidget *
bz_rich_app_tile_new (void)
{
  return g_object_new (BZ_TYPE_RICH_APP_TILE, NULL);
}

BzEntryGroup *
bz_rich_app_tile_get_group (BzRichAppTile *self)
{
  g_return_val_if_fail (BZ_IS_RICH_APP_TILE (self), NULL);
  return self->group;
}

void
bz_rich_app_tile_set_group (BzRichAppTile    *self,
                       BzEntryGroup *group)
{
  g_return_if_fail (BZ_IS_RICH_APP_TILE (self));
  g_clear_object (&self->group);
  if (group != NULL)
    {
      self->group = g_object_ref (group);
      update_screenshot (self);
    }

  bz_group_tile_css_watcher_set_group (self->css, group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}
/* End of bz-rich-app-tile.c */

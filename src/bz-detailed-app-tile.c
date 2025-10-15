/* bz-detailed-app-tile.c
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

#include <adwaita.h>

#include "bz-detailed-app-tile.h"
#include "bz-group-tile-css-watcher.h"

struct _BzDetailedAppTile
{
  GtkButton parent_instance;

  BzEntryGroup *group;

  BzGroupTileCssWatcher *css;
};

G_DEFINE_FINAL_TYPE (BzDetailedAppTile, bz_detailed_app_tile, GTK_TYPE_BUTTON);

enum
{
  PROP_0,

  PROP_GROUP,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_detailed_app_tile_dispose (GObject *object)
{
  BzDetailedAppTile *self = BZ_DETAILED_APP_TILE (object);

  g_clear_pointer (&self->group, g_object_unref);
  g_clear_pointer (&self->css, g_object_unref);

  G_OBJECT_CLASS (bz_detailed_app_tile_parent_class)->dispose (object);
}

static void
bz_detailed_app_tile_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BzDetailedAppTile *self = BZ_DETAILED_APP_TILE (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, bz_detailed_app_tile_get_group (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_detailed_app_tile_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzDetailedAppTile *self = BZ_DETAILED_APP_TILE (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      bz_detailed_app_tile_set_group (self, g_value_get_object (value));
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
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static void
bz_detailed_app_tile_class_init (BzDetailedAppTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_detailed_app_tile_set_property;
  object_class->get_property = bz_detailed_app_tile_get_property;
  object_class->dispose      = bz_detailed_app_tile_dispose;

  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-detailed-app-tile.ui");

  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
}

static void
bz_detailed_app_tile_init (BzDetailedAppTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->css = bz_group_tile_css_watcher_new ();
  bz_group_tile_css_watcher_set_widget (self->css, GTK_WIDGET (self));
}

BzDetailedAppTile *
bz_detailed_app_tile_new (void)
{
  return g_object_new (BZ_TYPE_DETAILED_APP_TILE, NULL);
}

BzEntryGroup *
bz_detailed_app_tile_get_group (BzDetailedAppTile *self)
{
  g_return_val_if_fail (BZ_IS_DETAILED_APP_TILE (self), NULL);
  return self->group;
}

void
bz_detailed_app_tile_set_group (BzDetailedAppTile *self,
                                BzEntryGroup      *group)
{
  g_return_if_fail (BZ_IS_DETAILED_APP_TILE (self));

  g_clear_pointer (&self->group, g_object_unref);
  if (group != NULL)
    self->group = g_object_ref (group);

  bz_group_tile_css_watcher_set_group (self->css, group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

/* End of bz-detailed-app-tile.c */

/* bz-category-tile.c
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "bz-category-tile.h"

struct _BzCategoryTile
{
  GtkButton parent_instance;
  BzFlathubCategory *category;
};

G_DEFINE_FINAL_TYPE (BzCategoryTile, bz_category_tile, GTK_TYPE_BUTTON);

enum
{
  PROP_0,
  PROP_CATEGORY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_category_tile_dispose (GObject *object)
{
  BzCategoryTile *self = BZ_CATEGORY_TILE (object);

  g_clear_object (&self->category);

  G_OBJECT_CLASS (bz_category_tile_parent_class)->dispose (object);
}

static void
bz_category_tile_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  BzCategoryTile *self = BZ_CATEGORY_TILE (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_object (value, bz_category_tile_get_category (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_category_tile_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  BzCategoryTile *self = BZ_CATEGORY_TILE (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      bz_category_tile_set_category (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static void
bz_category_tile_class_init (BzCategoryTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_category_tile_set_property;
  object_class->get_property = bz_category_tile_get_property;
  object_class->dispose = bz_category_tile_dispose;

  props[PROP_CATEGORY] =
    g_param_spec_object (
      "category",
      NULL, NULL,
      BZ_TYPE_FLATHUB_CATEGORY,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-category-tile.ui");

  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
}

static void
bz_category_tile_init (BzCategoryTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_add_css_class (GTK_WIDGET (self), "category-tile");
}

GtkWidget *
bz_category_tile_new (void)
{
  return g_object_new (BZ_TYPE_CATEGORY_TILE, NULL);
}

BzFlathubCategory *
bz_category_tile_get_category (BzCategoryTile *self)
{
  g_return_val_if_fail (BZ_IS_CATEGORY_TILE (self), NULL);

  return self->category;
}

void
bz_category_tile_set_category (BzCategoryTile *self,
                               BzFlathubCategory *category)
{
  const char *category_name;
  g_autofree char *css_class = NULL;

  g_return_if_fail (BZ_IS_CATEGORY_TILE (self));

  g_clear_object (&self->category);

  if (category != NULL)
    {
      self->category = g_object_ref (category);
      
      category_name = bz_flathub_category_get_name (category);
      if (category_name != NULL)
        {
          g_autofree char *lowercase_name = g_ascii_strdown (category_name, -1);
          css_class = g_strdup_printf ("category-%s", lowercase_name);
          g_strdelimit (css_class, " &/", '-');
          gtk_widget_add_css_class (GTK_WIDGET (self), css_class);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CATEGORY]);
}
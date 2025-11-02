/* bz-flathub-category.c
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

#include "bz-flathub-category.h"
#include <glib/gi18n.h>

struct _BzFlathubCategory
{
  GObject parent_instance;

  BzApplicationMapFactory *map_factory;
  char                    *name;
  GListModel              *applications;
  GListModel              *quality_applications;
  int                      total_entries;
};

G_DEFINE_FINAL_TYPE (BzFlathubCategory, bz_flathub_category, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MAP_FACTORY,
  PROP_NAME,
  PROP_DISPLAY_NAME,
  PROP_SHORT_NAME,
  PROP_ICON_NAME,
  PROP_APPLICATIONS,
  PROP_QUALITY_APPLICATIONS,
  PROP_TOTAL_ENTRIES,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_flathub_category_dispose (GObject *object)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  g_clear_pointer (&self->map_factory, g_object_unref);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->applications, g_object_unref);
  g_clear_pointer (&self->quality_applications, g_object_unref);

  G_OBJECT_CLASS (bz_flathub_category_parent_class)->dispose (object);
}

static void
bz_flathub_category_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  switch (prop_id)
    {
    case PROP_MAP_FACTORY:
      g_value_set_object (value, bz_flathub_category_get_map_factory (self));
      break;
    case PROP_NAME:
      g_value_set_string (value, bz_flathub_category_get_name (self));
      break;
    case PROP_APPLICATIONS:
      g_value_take_object (value, bz_flathub_category_dup_applications (self));
      break;
    case PROP_QUALITY_APPLICATIONS:
      g_value_take_object (value, bz_flathub_category_dup_quality_applications (self));
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, bz_flathub_category_get_display_name (self));
      break;
    case PROP_SHORT_NAME:
      g_value_set_string (value, bz_flathub_category_get_short_name (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, bz_flathub_category_get_icon_name (self));
      break;
    case PROP_TOTAL_ENTRIES:
      g_value_set_int (value, bz_flathub_category_get_total_entries (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzFlathubCategory *self = BZ_FLATHUB_CATEGORY (object);

  switch (prop_id)
    {
    case PROP_MAP_FACTORY:
      bz_flathub_category_set_map_factory (self, g_value_get_object (value));
      break;
    case PROP_NAME:
      bz_flathub_category_set_name (self, g_value_get_string (value));
      break;
    case PROP_APPLICATIONS:
      bz_flathub_category_set_applications (self, g_value_get_object (value));
      break;
    case PROP_QUALITY_APPLICATIONS:
      bz_flathub_category_set_quality_applications (self, g_value_get_object (value));
      break;
    case PROP_TOTAL_ENTRIES:
      bz_flathub_category_set_total_entries (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_class_init (BzFlathubCategoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_flathub_category_set_property;
  object_class->get_property = bz_flathub_category_get_property;
  object_class->dispose      = bz_flathub_category_dispose;

  props[PROP_MAP_FACTORY] =
      g_param_spec_object (
          "map-factory",
          NULL, NULL,
          BZ_TYPE_APPLICATION_MAP_FACTORY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_DISPLAY_NAME] =
      g_param_spec_string (
          "display-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_SHORT_NAME] =
      g_param_spec_string (
          "short-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_ICON_NAME] =
      g_param_spec_string (
          "icon-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_APPLICATIONS] =
      g_param_spec_object (
          "applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_QUALITY_APPLICATIONS] =
      g_param_spec_object (
          "quality-applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_TOTAL_ENTRIES] =
      g_param_spec_int (
          "total-entries",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_flathub_category_init (BzFlathubCategory *self)
{
}

BzFlathubCategory *
bz_flathub_category_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_CATEGORY, NULL);
}

BzApplicationMapFactory *
bz_flathub_category_get_map_factory (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return self->map_factory;
}

const char *
bz_flathub_category_get_name (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return self->name;
}

GListModel *
bz_flathub_category_dup_applications (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);

  if (self->applications != NULL)
    {
      if (self->map_factory != NULL)
        return bz_application_map_factory_generate (
            self->map_factory, G_LIST_MODEL (self->applications));
      else
        return G_LIST_MODEL (g_object_ref (self->applications));
    }
  else
    return NULL;
}

GListModel *
bz_flathub_category_dup_quality_applications (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);

  if (self->quality_applications != NULL)
    {
      if (self->map_factory != NULL)
        return bz_application_map_factory_generate (
            self->map_factory, G_LIST_MODEL (self->quality_applications));
      else
        return G_LIST_MODEL (g_object_ref (self->quality_applications));
    }
  else
    return NULL;
}

int
bz_flathub_category_get_total_entries (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), 0);
  return self->total_entries;
}

void
bz_flathub_category_set_map_factory (BzFlathubCategory       *self,
                                     BzApplicationMapFactory *map_factory)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));
  g_return_if_fail (map_factory == NULL || BZ_IS_APPLICATION_MAP_FACTORY (map_factory));

  g_clear_object (&self->map_factory);
  if (map_factory != NULL)
    self->map_factory = g_object_ref (map_factory);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAP_FACTORY]);
}

void
bz_flathub_category_set_name (BzFlathubCategory *self,
                              const char        *name)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

void
bz_flathub_category_set_applications (BzFlathubCategory *self,
                                      GListModel        *applications)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  g_clear_pointer (&self->applications, g_object_unref);
  if (applications != NULL)
    self->applications = g_object_ref (applications);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPLICATIONS]);
}

void
bz_flathub_category_set_quality_applications (BzFlathubCategory *self,
                                              GListModel        *applications)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  g_clear_pointer (&self->quality_applications, g_object_unref);
  if (applications != NULL)
    self->quality_applications = g_object_ref (applications);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_QUALITY_APPLICATIONS]);
}

void
bz_flathub_category_set_total_entries (BzFlathubCategory *self,
                                       int                total_entries)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY (self));

  if (self->total_entries == total_entries)
    return;

  self->total_entries = total_entries;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TOTAL_ENTRIES]);
}

static const char *
get_category_display_name (const char *category_id)
{
  if (g_strcmp0 (category_id, "audiovideo") == 0)
    return _ ("Audio & Video");
  if (g_strcmp0 (category_id, "development") == 0)
    return _ ("Developer Tools");
  if (g_strcmp0 (category_id, "education") == 0)
    return _ ("Education");
  if (g_strcmp0 (category_id, "game") == 0)
    return _ ("Gaming");
  if (g_strcmp0 (category_id, "graphics") == 0)
    return _ ("Graphics & Photography");
  if (g_strcmp0 (category_id, "network") == 0)
    return _ ("Networking");
  if (g_strcmp0 (category_id, "office") == 0)
    return _ ("Productivity");
  if (g_strcmp0 (category_id, "science") == 0)
    return _ ("Science");
  if (g_strcmp0 (category_id, "system") == 0)
    return _ ("System");
  if (g_strcmp0 (category_id, "utility") == 0)
    return _ ("Utilities");

  return category_id;
}

static const char *
get_category_short_name (const char *category_id)
{
  if (g_strcmp0 (category_id, "audiovideo") == 0)
    return _ ("Media");
  if (g_strcmp0 (category_id, "development") == 0)
    return _ ("Develop");
  if (g_strcmp0 (category_id, "education") == 0)
    return _ ("Learn");
  if (g_strcmp0 (category_id, "game") == 0)
    return _ ("Play");
  if (g_strcmp0 (category_id, "graphics") == 0)
    return _ ("Create");
  if (g_strcmp0 (category_id, "network") == 0)
    return _ ("Internet");
  if (g_strcmp0 (category_id, "office") == 0)
    return _ ("Work");
  if (g_strcmp0 (category_id, "science") == 0)
    return _ ("Science");
  if (g_strcmp0 (category_id, "system") == 0)
    return _ ("System");
  if (g_strcmp0 (category_id, "utility") == 0)
    return _ ("Tools");

  return category_id;
}

static const char *
get_category_icon_name (const char *category_id)
{
  if (g_strcmp0 (category_id, "audiovideo") == 0)
    return "io.github.kolumni.Bazaar.Audiovideo";
  if (g_strcmp0 (category_id, "development") == 0)
    return "io.github.kolumni.Bazaar.Develop";
  if (g_strcmp0 (category_id, "education") == 0)
    return "io.github.kolumni.Bazaar.Learn";
  if (g_strcmp0 (category_id, "game") == 0)
    return "io.github.kolumni.Bazaar.Play";
  if (g_strcmp0 (category_id, "graphics") == 0)
    return "io.github.kolumni.Bazaar.Create";
  if (g_strcmp0 (category_id, "network") == 0)
    return "io.github.kolumni.Bazaar.Network";
  if (g_strcmp0 (category_id, "office") == 0)
    return "io.github.kolumni.Bazaar.Work";
  if (g_strcmp0 (category_id, "science") == 0)
    return "io.github.kolumni.Bazaar.Science";
  if (g_strcmp0 (category_id, "system") == 0)
    return "io.github.kolumni.Bazaar.System";
  if (g_strcmp0 (category_id, "utility") == 0)
    return "io.github.kolumni.Bazaar.Utilities";

  return NULL;
}

const char *
bz_flathub_category_get_display_name (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return get_category_display_name (self->name);
}

const char *
bz_flathub_category_get_short_name (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return get_category_short_name (self->name);
}

const char *
bz_flathub_category_get_icon_name (BzFlathubCategory *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY (self), NULL);
  return get_category_icon_name (self->name);
}

/* End of bz-flathub-category.c */

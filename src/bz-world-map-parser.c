/* bz-world-map-parser.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include <gio/gio.h>

#include "bz-country.h"
#include "bz-world-map-parser.h"

struct _BzWorldMapParser
{
  GObject parent_instance;

  GListStore *countries;
};

G_DEFINE_FINAL_TYPE (BzWorldMapParser, bz_world_map_parser, G_TYPE_OBJECT)

static const char *get_translated_name (GVariant   *translations,
                                        const char *fallback_name);

static void
bz_world_map_parser_dispose (GObject *object)
{
  BzWorldMapParser *self = BZ_WORLD_MAP_PARSER (object);

  g_clear_object (&self->countries);

  G_OBJECT_CLASS (bz_world_map_parser_parent_class)->dispose (object);
}

static void
bz_world_map_parser_class_init (BzWorldMapParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_world_map_parser_dispose;
}

static void
bz_world_map_parser_init (BzWorldMapParser *self)
{
  self->countries = g_list_store_new (BZ_TYPE_COUNTRY);
}

BzWorldMapParser *
bz_world_map_parser_new (void)
{
  return g_object_new (BZ_TYPE_WORLD_MAP_PARSER, NULL);
}

gboolean
bz_world_map_parser_load_from_resource (BzWorldMapParser *self,
                                        const char       *resource_path,
                                        GError          **error)
{
  g_autoptr (GBytes) bytes  = NULL;
  g_autoptr (GVariant) data = NULL;
  GVariantIter iter         = { 0 };
  const char  *name         = NULL;
  const char  *iso_code     = NULL;
  GVariant    *translations = NULL;
  GVariant    *coordinates  = NULL;

  g_return_val_if_fail (BZ_IS_WORLD_MAP_PARSER (self), FALSE);
  g_return_val_if_fail (resource_path != NULL, FALSE);

  bytes = g_resources_lookup_data (resource_path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   error);
  if (bytes == NULL)
    return FALSE;

  data = g_variant_new_from_bytes (G_VARIANT_TYPE ("a(ssa{ss}aaa(dd))"),
                                   bytes, TRUE);

  g_list_store_remove_all (self->countries);

  g_variant_iter_init (&iter, data);
  while (g_variant_iter_next (&iter, "(&s&s@a{ss}@aaa(dd))",
                              &name, &iso_code, &translations, &coordinates))
    {
      g_autoptr (BzCountry) country = NULL;
      const char *display_name      = NULL;

      display_name = get_translated_name (translations, name);

      country = bz_country_new ();
      bz_country_set_name (country, display_name);
      bz_country_set_iso_code (country, iso_code);
      bz_country_set_coordinates (country, coordinates);

      g_list_store_append (self->countries, country);

      g_variant_unref (translations);
      g_variant_unref (coordinates);
    }

  return TRUE;
}

GListModel *
bz_world_map_parser_get_countries (BzWorldMapParser *self)
{
  g_return_val_if_fail (BZ_IS_WORLD_MAP_PARSER (self), NULL);
  return G_LIST_MODEL (self->countries);
}

static const char *
get_translated_name (GVariant   *translations,
                     const char *fallback_name)
{
  const char *const *language_names = NULL;

  language_names = g_get_language_names ();

  for (guint i = 0; language_names[i] != NULL; i++)
    {
      const char *translated = NULL;

      if (g_variant_lookup (translations, language_names[i], "&s", &translated))
        return translated;
    }

  return fallback_name;
}

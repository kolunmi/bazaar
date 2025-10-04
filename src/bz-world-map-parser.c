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

#include <json-glib/json-glib.h>

#include "bz-world-map-parser.h"
#include "bz-country.h"

struct _BzWorldMapParser
{
  GObject parent_instance;

  JsonParser *parser;
  GListStore *countries;
};

G_DEFINE_FINAL_TYPE (BzWorldMapParser, bz_world_map_parser, G_TYPE_OBJECT)

static void
bz_world_map_parser_dispose (GObject *object)
{
  BzWorldMapParser *self = BZ_WORLD_MAP_PARSER (object);

  g_clear_object (&self->parser);
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
  self->parser    = json_parser_new ();
  self->countries = g_list_store_new (BZ_TYPE_COUNTRY);
}

BzWorldMapParser *
bz_world_map_parser_new (void)
{
  return g_object_new (BZ_TYPE_WORLD_MAP_PARSER, NULL);
}

gboolean
bz_world_map_parser_load_from_resource (BzWorldMapParser  *self,
                                        const char        *resource_path,
                                        GError           **error)
{
  g_autoptr (GBytes) bytes       = NULL;
  g_autoptr (JsonNode) root      = NULL;
  JsonObject                    *root_object = NULL;
  JsonArray                     *features    = NULL;
  const char                    *json_data   = NULL;
  gsize                          size        = 0;
  guint                          length      = 0;

  g_return_val_if_fail (BZ_IS_WORLD_MAP_PARSER (self), FALSE);
  g_return_val_if_fail (resource_path != NULL, FALSE);

  bytes = g_resources_lookup_data (resource_path,
                                    G_RESOURCE_LOOKUP_FLAGS_NONE,
                                    error);
  if (bytes == NULL)
    return FALSE;

  json_data = g_bytes_get_data (bytes, &size);

  if (!json_parser_load_from_data (self->parser, json_data, size, error))
    return FALSE;

  root = json_parser_get_root (self->parser);
  if (!JSON_NODE_HOLDS_OBJECT (root))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Root node is not an object");
      return FALSE;
    }

  root_object = json_node_get_object (root);
  if (!json_object_has_member (root_object, "features"))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Missing features member");
      return FALSE;
    }

  features = json_object_get_array_member (root_object, "features");
  length   = json_array_get_length (features);

  g_list_store_remove_all (self->countries);

  for (guint i = 0; i < length; i++)
    {
      JsonObject            *feature_obj  = NULL;
      const char            *name         = NULL;
      const char            *iso_code     = NULL;
      JsonArray             *coordinates  = NULL;
      g_autoptr (BzCountry)  country      = NULL;

      feature_obj = json_array_get_object_element (features, i);

      if (json_object_has_member (feature_obj, "N"))
        name = json_object_get_string_member (feature_obj, "N");

      if (json_object_has_member (feature_obj, "I"))
        iso_code = json_object_get_string_member (feature_obj, "I");

      if (json_object_has_member (feature_obj, "C"))
        coordinates = json_object_get_array_member (feature_obj, "C");

      country = bz_country_new ();
      bz_country_set_name (country, name);
      bz_country_set_iso_code (country, iso_code);
      bz_country_set_coordinates (country, coordinates);

      g_list_store_append (self->countries, country);
    }

  return TRUE;
}

GListModel *
bz_world_map_parser_get_countries (BzWorldMapParser *self)
{
  g_return_val_if_fail (BZ_IS_WORLD_MAP_PARSER (self), NULL);
  return G_LIST_MODEL (self->countries);
}

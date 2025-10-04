/* bz-world-map-parser.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define BZ_TYPE_WORLD_MAP_PARSER (bz_world_map_parser_get_type ())

G_DECLARE_FINAL_TYPE (BzWorldMapParser, bz_world_map_parser, BZ, WORLD_MAP_PARSER, GObject)

BzWorldMapParser *bz_world_map_parser_new                (void);
gboolean          bz_world_map_parser_load_from_resource (BzWorldMapParser  *self,
                                                          const char        *resource_path,
                                                          GError           **error);
GListModel       *bz_world_map_parser_get_countries       (BzWorldMapParser  *self);

G_END_DECLS

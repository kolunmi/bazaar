/* bz-featured-tile.h
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

#include <gtk/gtk.h>
#include "bz-entry-group.h"

G_BEGIN_DECLS

#define BZ_TYPE_FEATURED_TILE (bz_featured_tile_get_type ())

G_DECLARE_FINAL_TYPE (BzFeaturedTile, bz_featured_tile, BZ, FEATURED_TILE, GtkButton)

BzFeaturedTile *bz_featured_tile_new (BzEntryGroup *group);

BzEntryGroup *bz_featured_tile_get_group (BzFeaturedTile *self);

void bz_featured_tile_set_group (BzFeaturedTile *self,
                                  BzEntryGroup   *group);

G_END_DECLS

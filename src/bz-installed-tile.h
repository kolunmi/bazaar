/* bz-installed-tile.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "bz-entry-group.h"
#include "bz-list-tile.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_INSTALLED_TILE (bz_installed_tile_get_type ())

G_DECLARE_FINAL_TYPE (BzInstalledTile, bz_installed_tile, BZ, INSTALLED_TILE, BzListTile)

GtkWidget *
bz_installed_tile_new (void);

void
bz_installed_tile_set_group (BzInstalledTile *self,
                             BzEntryGroup    *group);

BzEntryGroup *
bz_installed_tile_get_group (BzInstalledTile *self);

G_END_DECLS

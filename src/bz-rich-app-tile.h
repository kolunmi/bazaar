/* bz-rich-app-tile.h
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

#include <gtk/gtk.h>
#include "adwaita.h"

#include "bz-entry-group.h"

G_BEGIN_DECLS

#define BZ_TYPE_RICH_APP_TILE (bz_rich_app_tile_get_type ())

G_DECLARE_FINAL_TYPE (BzRichAppTile, bz_rich_app_tile, BZ, RICH_APP_TILE, AdwBin)

GtkWidget *
bz_rich_app_tile_new (void);

BzEntryGroup *
bz_rich_app_tile_get_group (BzRichAppTile *self);

void
bz_rich_app_tile_set_group (BzRichAppTile    *self,
                            BzEntryGroup *group);

G_END_DECLS
/* End of bz-rich-app-tile.h */

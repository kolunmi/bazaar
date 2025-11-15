/* bz-list-tile.h
 *
 * Copyright 2025 Hari Rana <theevilskeleton@riseup.net>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_LIST_TILE (bz_list_tile_get_type())

G_DECLARE_DERIVABLE_TYPE (BzListTile, bz_list_tile, BZ, LIST_TILE, GtkWidget)

struct _BzListTileClass
{
  GtkWidgetClass parent_class;
};

BzListTile *bz_list_tile_new       (void);

GtkWidget  *bz_list_tile_get_child (BzListTile *self);

void        bz_list_tile_set_child (BzListTile *self,
                                    GtkWidget  *child);

G_END_DECLS
/* bz-context-tile.h
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

G_BEGIN_DECLS

#define BZ_TYPE_CONTEXT_TILE (bz_context_tile_get_type ())

G_DECLARE_FINAL_TYPE (BzContextTile, bz_context_tile, BZ, CONTEXT_TILE, GtkButton)

BzContextTile *bz_context_tile_new (void);

GtkWidget *bz_context_tile_get_lozenge_child (BzContextTile *self);
void       bz_context_tile_set_lozenge_child (BzContextTile *self,
                                              GtkWidget     *child);

const char *bz_context_tile_get_label (BzContextTile *self);
void        bz_context_tile_set_label (BzContextTile *self,
                                       const char    *label);

const char *bz_context_tile_get_lozenge_style (BzContextTile *self);
void        bz_context_tile_set_lozenge_style (BzContextTile *self,
                                               const char    *style);

G_END_DECLS

/* bz-zoom.h
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

#define BZ_TYPE_ZOOM (bz_zoom_get_type ())

G_DECLARE_FINAL_TYPE (BzZoom, bz_zoom, BZ, ZOOM, GtkWidget)

GtkWidget *bz_zoom_new (void);

GtkWidget *bz_zoom_get_child (BzZoom *self);
void       bz_zoom_set_child (BzZoom    *self,
                              GtkWidget *child);

double bz_zoom_get_zoom_level (BzZoom *self);
void   bz_zoom_set_zoom_level (BzZoom *self,
                               double  zoom_level);

double bz_zoom_get_min_zoom (BzZoom *self);
void   bz_zoom_set_min_zoom (BzZoom *self,
                             double  min_zoom);

double bz_zoom_get_max_zoom (BzZoom *self);
void   bz_zoom_set_max_zoom (BzZoom *self,
                             double  max_zoom);

void bz_zoom_zoom_in (BzZoom *self);
void bz_zoom_zoom_out (BzZoom *self);
void bz_zoom_reset (BzZoom *self);
void bz_zoom_fit_to_window (BzZoom *self);

G_END_DECLS

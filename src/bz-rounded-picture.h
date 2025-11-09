/* bz-rounded-picture.h
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

#define BZ_TYPE_ROUNDED_PICTURE (bz_rounded_picture_get_type ())

G_DECLARE_FINAL_TYPE (BzRoundedPicture, bz_rounded_picture, BZ, ROUNDED_PICTURE, GtkWidget)

GtkWidget *
bz_rounded_picture_new (void);

void
bz_rounded_picture_set_paintable (BzRoundedPicture *self,
                                  GdkPaintable     *paintable);

GdkPaintable *
bz_rounded_picture_get_paintable (BzRoundedPicture *self);

void
bz_rounded_picture_set_radius (BzRoundedPicture *self,
                               double            radius);

double
bz_rounded_picture_get_radius (BzRoundedPicture *self);

G_END_DECLS

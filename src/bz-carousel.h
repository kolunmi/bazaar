/* bz-carousel.h
 *
 * Copyright 2025 Alexander Vanhee
 * Copyright (C) 2019 Alice Mikhaylenko <alicem@gnome.org>
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

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_CAROUSEL (bz_carousel_get_type ())

G_DECLARE_FINAL_TYPE (BzCarousel, bz_carousel, BZ, CAROUSEL, GtkWidget)

GtkWidget *bz_carousel_new (void) G_GNUC_WARN_UNUSED_RESULT;

void bz_carousel_append (BzCarousel *self,
                         GtkWidget  *child);

void bz_carousel_remove (BzCarousel *self,
                         GtkWidget  *child);

void bz_carousel_scroll_to (BzCarousel *self,
                            GtkWidget  *widget,
                            gboolean    animate);

GtkWidget *bz_carousel_get_nth_page (BzCarousel *self,
                                     guint       n);

guint bz_carousel_get_n_pages (BzCarousel *self);

double bz_carousel_get_position (BzCarousel *self);

G_END_DECLS

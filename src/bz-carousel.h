/* bz-carousel.h
 *
 * Copyright 2026 Eva M
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

#define BZ_TYPE_CAROUSEL (bz_carousel_get_type ())
G_DECLARE_FINAL_TYPE (BzCarousel, bz_carousel, BZ, CAROUSEL, GtkWidget)

GtkWidget *
bz_carousel_new (void);

gboolean
bz_carousel_get_auto_scroll (BzCarousel *self);

gboolean
bz_carousel_get_allow_long_swipes (BzCarousel *self);

gboolean
bz_carousel_get_allow_mouse_drag (BzCarousel *self);

gboolean
bz_carousel_get_allow_scroll_wheel (BzCarousel *self);

gboolean
bz_carousel_get_allow_raise (BzCarousel *self);

gboolean
bz_carousel_get_raised (BzCarousel *self);

GtkSingleSelection *
bz_carousel_get_model (BzCarousel *self);

void
bz_carousel_set_auto_scroll (BzCarousel *self,
                             gboolean    auto_scroll);

void
bz_carousel_set_allow_long_swipes (BzCarousel *self,
                                   gboolean    allow_long_swipes);

void
bz_carousel_set_allow_mouse_drag (BzCarousel *self,
                                  gboolean    allow_mouse_drag);

void
bz_carousel_set_allow_scroll_wheel (BzCarousel *self,
                                    gboolean    allow_scroll_wheel);

void
bz_carousel_set_allow_raise (BzCarousel *self,
                             gboolean    allow_raise);

void
bz_carousel_set_raised (BzCarousel *self,
                        gboolean    raised);

void
bz_carousel_set_model (BzCarousel         *self,
                       GtkSingleSelection *model);

G_END_DECLS

/* End of bz-carousel.h */

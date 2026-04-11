/* bge-carousel.h
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

#ifndef BGE_INSIDE
#error "Only <bge.h> can be included directly."
#endif

G_BEGIN_DECLS

#define BGE_TYPE_CAROUSEL (bge_carousel_get_type ())
G_DECLARE_FINAL_TYPE (BgeCarousel, bge_carousel, BGE, CAROUSEL, GtkWidget)

BGE_AVAILABLE_IN_ALL
GtkWidget *
bge_carousel_new (void);

BGE_AVAILABLE_IN_ALL
gboolean
bge_carousel_get_allow_mouse_drag (BgeCarousel *self);

BGE_AVAILABLE_IN_ALL
gboolean
bge_carousel_get_allow_overshoot (BgeCarousel *self);

BGE_AVAILABLE_IN_ALL
gboolean
bge_carousel_get_allow_scroll_wheel (BgeCarousel *self);

BGE_AVAILABLE_IN_ALL
gboolean
bge_carousel_get_allow_raise (BgeCarousel *self);

BGE_AVAILABLE_IN_ALL
GtkSingleSelection *
bge_carousel_get_model (BgeCarousel *self);

BGE_AVAILABLE_IN_ALL
GtkWidget *
bge_carousel_get_nth_page (BgeCarousel *self,
                           guint        index);

BGE_AVAILABLE_IN_ALL
void
bge_carousel_set_allow_mouse_drag (BgeCarousel *self,
                                   gboolean     allow_mouse_drag);

BGE_AVAILABLE_IN_ALL
void
bge_carousel_set_allow_overshoot (BgeCarousel *self,
                                  gboolean     allow_overshoot);

BGE_AVAILABLE_IN_ALL
void
bge_carousel_set_allow_scroll_wheel (BgeCarousel *self,
                                     gboolean     allow_scroll_wheel);

BGE_AVAILABLE_IN_ALL
void
bge_carousel_set_allow_raise (BgeCarousel *self,
                              gboolean     allow_raise);

BGE_AVAILABLE_IN_ALL
void
bge_carousel_set_model (BgeCarousel        *self,
                        GtkSingleSelection *model);

G_END_DECLS

/* End of bge-carousel.h */

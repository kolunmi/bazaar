/* bz-carousel-indicator-dots.h
 *
 * Copyright (C) 2020 Alice Mikhaylenko <alicem@gnome.org>
 * Copyright 2026 Alexander Vanhee
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

#include "bz-carousel.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_CAROUSEL_INDICATOR_DOTS (bz_carousel_indicator_dots_get_type ())

G_DECLARE_FINAL_TYPE (BzCarouselIndicatorDots, bz_carousel_indicator_dots, BZ, CAROUSEL_INDICATOR_DOTS, GtkWidget)

GtkWidget *bz_carousel_indicator_dots_new (void) G_GNUC_WARN_UNUSED_RESULT;

BzCarousel *bz_carousel_indicator_dots_get_carousel (BzCarouselIndicatorDots *self);
void        bz_carousel_indicator_dots_set_carousel (BzCarouselIndicatorDots *self,
                                                     BzCarousel              *carousel);

G_END_DECLS

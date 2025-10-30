/* bz-featured-carousel.c
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

#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_FEATURED_CAROUSEL (bz_featured_carousel_get_type ())

G_DECLARE_FINAL_TYPE (BzFeaturedCarousel, bz_featured_carousel, BZ, FEATURED_CAROUSEL, GtkBox)

BzFeaturedCarousel *bz_featured_carousel_new (void);

GListModel *bz_featured_carousel_get_model (BzFeaturedCarousel *self);

void bz_featured_carousel_set_model (BzFeaturedCarousel *self,
                                     GListModel         *model);

gboolean bz_featured_carousel_get_is_aotd (BzFeaturedCarousel *self);

void bz_featured_carousel_set_is_aotd (BzFeaturedCarousel *self,
                                       gboolean            is_aotd);

G_END_DECLS

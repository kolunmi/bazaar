/* bz-fading-clamp.h
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

#define BZ_TYPE_FADING_CLAMP (bz_fading_clamp_get_type ())

G_DECLARE_FINAL_TYPE (BzFadingClamp, bz_fading_clamp, BZ, FADING_CLAMP, GtkWidget)

GtkWidget *bz_fading_clamp_new (void);

GtkWidget *bz_fading_clamp_get_child (BzFadingClamp *self);

void bz_fading_clamp_set_child (BzFadingClamp *self,
                                GtkWidget     *child);

int bz_fading_clamp_get_max_height (BzFadingClamp *self);

void bz_fading_clamp_set_max_height (BzFadingClamp *self,
                                     int            max_height);

G_END_DECLS

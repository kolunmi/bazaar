/* bz-favorite-button.h
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

#include "bz-entry.h"
#include "bz-state-info.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_FAVORITE_BUTTON (bz_favorite_button_get_type ())

G_DECLARE_FINAL_TYPE (BzFavoriteButton, bz_favorite_button, BZ, FAVORITE_BUTTON, GtkButton)

GtkWidget *
bz_favorite_button_new (void);

void
bz_favorite_button_set_entry (BzFavoriteButton *self,
                              BzEntry          *entry);

BzEntry *
bz_favorite_button_get_entry (BzFavoriteButton *self);

void
bz_favorite_button_set_state (BzFavoriteButton *self,
                              BzStateInfo      *state);

BzStateInfo *
bz_favorite_button_get_state (BzFavoriteButton *self);

G_END_DECLS

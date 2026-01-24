/* bz-subcategory-list.h
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

#include "bz-flathub-category.h"
#include "bz-flathub-state.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_SUBCATEGORY_LIST (bz_subcategory_list_get_type ())

G_DECLARE_FINAL_TYPE (BzSubcategoryList, bz_subcategory_list, BZ, SUBCATEGORY_LIST, GtkBox)
GtkWidget *bz_subcategory_list_new (void);

BzFlathubCategory *bz_subcategory_list_get_category (BzSubcategoryList *self);

void bz_subcategory_list_set_category (BzSubcategoryList *self,
                                       BzFlathubCategory *category);

BzFlathubState *bz_subcategory_list_get_flathub_state (BzSubcategoryList *self);

void bz_subcategory_list_set_flathub_state (BzSubcategoryList *self,
                                            BzFlathubState    *flathub_state);

G_END_DECLS

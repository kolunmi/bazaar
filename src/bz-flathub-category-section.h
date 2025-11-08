/* bz-flathub-category-section.h
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
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_FLATHUB_CATEGORY_SECTION (bz_flathub_category_section_get_type ())

G_DECLARE_FINAL_TYPE (BzFlathubCategorySection, bz_flathub_category_section, BZ, FLATHUB_CATEGORY_SECTION, GtkBox)

GtkWidget         *bz_flathub_category_section_new (void);
void               bz_flathub_category_section_set_category (BzFlathubCategorySection *self,
                                                             BzFlathubCategory        *category);
BzFlathubCategory *bz_flathub_category_section_get_category (BzFlathubCategorySection *self);
void               bz_flathub_category_section_set_max_items (BzFlathubCategorySection *self,
                                                              guint                     max_items);
guint              bz_flathub_category_section_get_max_items (BzFlathubCategorySection *self);

G_END_DECLS

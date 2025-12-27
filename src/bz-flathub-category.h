/* bz-flathub-category.h
 *
 * Copyright 2025 Adam Masciola
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

#include "bz-application-map-factory.h"

G_BEGIN_DECLS

#define BZ_TYPE_FLATHUB_CATEGORY (bz_flathub_category_get_type ())
G_DECLARE_FINAL_TYPE (BzFlathubCategory, bz_flathub_category, BZ, FLATHUB_CATEGORY, GObject)

BzFlathubCategory *
bz_flathub_category_new (void);

BzApplicationMapFactory *
bz_flathub_category_get_map_factory (BzFlathubCategory *self);

const char *
bz_flathub_category_get_name (BzFlathubCategory *self);

GListModel *
bz_flathub_category_dup_applications (BzFlathubCategory *self);

GListModel *
bz_flathub_category_dup_quality_applications (BzFlathubCategory *self);

void
bz_flathub_category_set_map_factory (BzFlathubCategory       *self,
                                     BzApplicationMapFactory *map_factory);

void
bz_flathub_category_set_name (BzFlathubCategory *self,
                              const char        *name);

void
bz_flathub_category_set_applications (BzFlathubCategory *self,
                                      GListModel        *applications);

void
bz_flathub_category_set_quality_applications (BzFlathubCategory *self,
                                              GListModel        *applications);

const char *
bz_flathub_category_get_display_name (BzFlathubCategory *self);

const char *
bz_flathub_category_get_short_name (BzFlathubCategory *self);

const char *
bz_flathub_category_get_more_of_name (BzFlathubCategory *self);

const char *
bz_flathub_category_get_icon_name (BzFlathubCategory *self);

int
bz_flathub_category_get_total_entries (BzFlathubCategory *self);

void
bz_flathub_category_set_total_entries (BzFlathubCategory *self,
                                       int                total_entries);

gboolean
bz_flathub_category_get_is_spotlight (BzFlathubCategory *self);

void
bz_flathub_category_set_is_spotlight (BzFlathubCategory *self,
                                      gboolean           is_spotlight);

GListModel *
bz_flathub_category_list_from_appstream (GPtrArray *as_categories);

G_END_DECLS

/* End of bz-flathub-category.h */

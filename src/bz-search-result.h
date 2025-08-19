/* bz-search-result.h
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

#include "bz-entry-group.h"

G_BEGIN_DECLS

#define BZ_TYPE_SEARCH_RESULT (bz_search_result_get_type ())
G_DECLARE_FINAL_TYPE (BzSearchResult, bz_search_result, BZ, SEARCH_RESULT, GObject)

BzSearchResult *
bz_search_result_new (void);

BzEntryGroup *
bz_search_result_get_group (BzSearchResult *self);

guint
bz_search_result_get_original_index (BzSearchResult *self);

double
bz_search_result_get_score (BzSearchResult *self);

const char *
bz_search_result_get_title_markup (BzSearchResult *self);

void
bz_search_result_set_group (BzSearchResult *self,
                            BzEntryGroup   *group);

void
bz_search_result_set_original_index (BzSearchResult *self,
                                     guint           original_index);

void
bz_search_result_set_score (BzSearchResult *self,
                            double          score);

void
bz_search_result_set_title_markup (BzSearchResult *self,
                                   const char     *title_markup);

G_END_DECLS

/* End of bz-search-result.h */

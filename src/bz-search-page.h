/* bz-search-page.h
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

#include <adwaita.h>

#include "bz-state-info.h"

G_BEGIN_DECLS

#define BZ_TYPE_SEARCH_PAGE (bz_search_page_get_type ())
G_DECLARE_FINAL_TYPE (BzSearchPage, bz_search_page, BZ, SEARCH_PAGE, AdwBin)

GtkWidget *
bz_search_page_new (GListModel *model,
                    const char *initial);

void
bz_search_page_set_state (BzSearchPage *self,
                          BzStateInfo  *state);

BzStateInfo *
bz_search_page_get_state (BzSearchPage *self);

void
bz_search_page_set_text (BzSearchPage *self,
                         const char   *text);

const char *
bz_search_page_get_text (BzSearchPage *self);

BzEntryGroup *
bz_search_page_get_selected (BzSearchPage *self,
                             gboolean     *remove);

BzEntryGroup *
bz_search_page_get_previewing (BzSearchPage *self);

void
bz_search_page_refresh (BzSearchPage *self);

gboolean
bz_search_page_ensure_active (BzSearchPage *self,
                              const char   *initial);

G_END_DECLS

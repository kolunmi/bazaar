/* bz-library-page.h
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

#define BZ_TYPE_LIBRARY_PAGE (bz_library_page_get_type ())
G_DECLARE_FINAL_TYPE (BzLibraryPage, bz_library_page, BZ, LIBRARY_PAGE, AdwBin)

GtkWidget *
bz_library_page_new (void);

void
bz_library_page_set_model (BzLibraryPage *self,
                           GListModel    *model);

GListModel *
bz_library_page_get_model (BzLibraryPage *self);

void
bz_library_page_set_state (BzLibraryPage *self,
                           BzStateInfo   *state);

BzStateInfo *
bz_library_page_get_state (BzLibraryPage *self);

gboolean
bz_library_page_ensure_active (BzLibraryPage *self,
                               const char    *initial);

G_END_DECLS

/* bz-releases-list.h
 *
 * Copyright 2025 Alexander Vanhee, Adam Masciola
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

#define BZ_TYPE_RELEASES_LIST (bz_releases_list_get_type ())

G_DECLARE_FINAL_TYPE (BzReleasesList, bz_releases_list, BZ, RELEASES_LIST, AdwBin)

GtkWidget  *bz_releases_list_new (void);
void        bz_releases_list_set_version_history (BzReleasesList *self,
                                                  GListModel     *version_history);
GListModel *bz_releases_list_get_version_history (BzReleasesList *self);

G_END_DECLS

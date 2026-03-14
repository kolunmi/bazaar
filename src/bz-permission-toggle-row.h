/* bz-permission-toggle-row.h
 *
 * Copyright 2026 Alexander Vanhee
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
#define BZ_TYPE_PERMISSION_TOGGLE_ROW (bz_permission_toggle_row_get_type ())

G_DECLARE_FINAL_TYPE (BzPermissionToggleRow, bz_permission_toggle_row, BZ, PERMISSION_TOGGLE_ROW, AdwActionRow)

const char *
bz_permission_toggle_row_get_key (BzPermissionToggleRow *self);

const char *
bz_permission_toggle_row_get_group (BzPermissionToggleRow *self);

gboolean
bz_permission_toggle_row_get_active (BzPermissionToggleRow *self);

void
bz_permission_toggle_row_set_active (BzPermissionToggleRow *self,
                                     gboolean               active);

gboolean
bz_permission_toggle_row_get_is_modified (BzPermissionToggleRow *self);

void
bz_permission_toggle_row_set_default_value (BzPermissionToggleRow *self,
                                            gboolean               default_value);

void
bz_permission_toggle_row_reset (BzPermissionToggleRow *self);
G_END_DECLS

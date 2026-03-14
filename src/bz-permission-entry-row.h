/* bz-permission-entry-row.h
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

#define BZ_TYPE_PERMISSION_ENTRY_ROW (bz_permission_entry_row_get_type ())

G_DECLARE_FINAL_TYPE (BzPermissionEntryRow, bz_permission_entry_row, BZ, PERMISSION_ENTRY_ROW, AdwExpanderRow)

GtkWidget *
bz_permission_entry_row_new (const char *title,
                             const char *subtitle);

GStrv
bz_permission_entry_row_get_values (BzPermissionEntryRow *self);

void
bz_permission_entry_row_set_values (BzPermissionEntryRow *self,
                                    const char *const    *values);

void
bz_permission_entry_row_set_default_values (BzPermissionEntryRow *self,
                                            const char *const    *values);

GStrv
bz_permission_entry_row_get_default_values (BzPermissionEntryRow *self);

void
bz_permission_entry_row_set_regex (BzPermissionEntryRow *self,
                                   const char           *regex);

G_END_DECLS

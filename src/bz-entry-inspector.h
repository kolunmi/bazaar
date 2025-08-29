/* bz-entry-inspector.h
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

#include "bz-result.h"

G_BEGIN_DECLS

#define BZ_TYPE_ENTRY_INSPECTOR (bz_entry_inspector_get_type ())
G_DECLARE_FINAL_TYPE (BzEntryInspector, bz_entry_inspector, BZ, ENTRY_INSPECTOR, AdwWindow)

BzEntryInspector *
bz_entry_inspector_new (void);

BzResult *
bz_entry_inspector_get_result (BzEntryInspector *self);

void
bz_entry_inspector_set_result (BzEntryInspector *self,
                               BzResult         *result);

G_END_DECLS

/* End of bz-entry-inspector.h */

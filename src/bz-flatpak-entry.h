/* bz-flatpak-entry.h
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

#include "bz-entry.h"
#include "bz-flatpak-instance.h"

G_BEGIN_DECLS

#define BZ_TYPE_FLATPAK_ENTRY (bz_flatpak_entry_get_type ())
G_DECLARE_FINAL_TYPE (BzFlatpakEntry, bz_flatpak_entry, BZ, FLATPAK_ENTRY, BzEntry)

gboolean
bz_flatpak_entry_is_user (BzFlatpakEntry *self);

const char *
bz_flatpak_entry_get_flatpak_id (BzFlatpakEntry *self);

const char *
bz_flatpak_entry_get_application_name (BzFlatpakEntry *self);

const char *
bz_flatpak_entry_get_runtime_name (BzFlatpakEntry *self);

const char *
bz_flatpak_entry_get_addon_extension_of_ref (BzFlatpakEntry *self);

char *
bz_flatpak_entry_extract_id_from_unique_id (const char *unique_id);

gboolean
bz_flatpak_entry_launch (BzFlatpakEntry    *self,
                         BzFlatpakInstance *flatpak,
                         GError           **error);

G_END_DECLS

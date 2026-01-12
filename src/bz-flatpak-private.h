/* bz-flatpak-private.h
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

#include <appstream.h>
#include <flatpak.h>

#include "bz-flatpak-entry.h"
#include "bz-flatpak-instance.h"

G_BEGIN_DECLS

/* BzFlatpakInstance */

FlatpakInstallation *
bz_flatpak_instance_get_system_installation (BzFlatpakInstance *self);

FlatpakInstallation *
bz_flatpak_instance_get_user_installation (BzFlatpakInstance *self);

/* BzFlatpakEntry */

char *
bz_flatpak_ref_parts_format_unique (const char *origin,
                                    const char *fmt,
                                    gboolean    user);

char *
bz_flatpak_ref_format_unique (FlatpakRef *ref,
                              gboolean    user);

BzFlatpakEntry *
bz_flatpak_entry_new_for_ref (FlatpakRef    *ref,
                              FlatpakRemote *remote,
                              gboolean       user,
                              AsComponent   *component,
                              const char    *appstream_dir,
                              GError       **error);

FlatpakRef *
bz_flatpak_entry_get_ref (BzFlatpakEntry *self);

G_END_DECLS

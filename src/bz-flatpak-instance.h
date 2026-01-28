/* bz-flatpak-instance.h
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

#include <libdex.h>

G_BEGIN_DECLS

#define BZ_FLATPAK_ERROR (bz_flatpak_error_quark ())
GQuark bz_flatpak_error_quark (void);

typedef enum
{
  BZ_FLATPAK_ERROR_CANNOT_INITIALIZE = 0,
  BZ_FLATPAK_ERROR_LOCAL_SYNCHRONIZATION_FAILURE,
  BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
  BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
  BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
  BZ_FLATPAK_ERROR_APPSTREAM_FAILURE,
  BZ_FLATPAK_ERROR_GLYCIN_FAILURE,
} BzFlatpakError;

#define BZ_TYPE_FLATPAK_INSTANCE (bz_flatpak_instance_get_type ())
G_DECLARE_FINAL_TYPE (BzFlatpakInstance, bz_flatpak_instance, BZ, FLATPAK_INSTANCE, GObject)

DexFuture *
bz_flatpak_instance_new (void);

DexFuture *
bz_flatpak_instance_has_flathub (BzFlatpakInstance *self,
                                 GCancellable      *cancellable);

DexFuture *
bz_flatpak_instance_ensure_has_flathub (BzFlatpakInstance *self,
                                        GCancellable      *cancellable);

void
bz_flatpak_instance_trigger_external_change (BzFlatpakInstance *self);

G_END_DECLS

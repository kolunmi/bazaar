/* bz-preferences-dialog.h
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

#define BZ_TYPE_PREFERENCES_DIALOG (bz_preferences_dialog_get_type ())
G_DECLARE_FINAL_TYPE (BzPreferencesDialog, bz_preferences_dialog, BZ, PREFERENCES_DIALOG, AdwPreferencesDialog)

AdwDialog *
bz_preferences_dialog_new (BzStateInfo *state);

G_END_DECLS

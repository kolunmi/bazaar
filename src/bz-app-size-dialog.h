/* bz-app-size-dialog.h
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
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

#include "bz-entry-group.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_APP_SIZE_DIALOG (bz_app_size_dialog_get_type ())

G_DECLARE_FINAL_TYPE (BzAppSizeDialog, bz_app_size_dialog, BZ, APP_SIZE_DIALOG, AdwDialog)

AdwDialog *
bz_app_size_dialog_new (BzEntryGroup *group);

G_END_DECLS

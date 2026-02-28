/* bz-install-controls.h
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

#include "bz-entry-group.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_INSTALL_CONTROLS (bz_install_controls_get_type ())

G_DECLARE_FINAL_TYPE (BzInstallControls, bz_install_controls, BZ, INSTALL_CONTROLS, GtkBox)

GtkWidget *
bz_install_controls_new (void);

gboolean
bz_install_controls_get_wide (BzInstallControls *self);
void
bz_install_controls_set_wide (BzInstallControls *self,
                              gboolean           wide);

BzEntryGroup *
bz_install_controls_get_entry_group (BzInstallControls *self);
void

bz_install_controls_set_entry_group (BzInstallControls *self,
                                     BzEntryGroup      *group);

void
bz_install_controls_grab_focus_preferred (BzInstallControls *self);

G_END_DECLS

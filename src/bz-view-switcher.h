/* bz-view-switcher.h
 *
 * Copyright 2026 Eva M
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

#define BZ_TYPE_VIEW_SWITCHER (bz_view_switcher_get_type ())
G_DECLARE_FINAL_TYPE (BzViewSwitcher, bz_view_switcher, BZ, VIEW_SWITCHER, GtkWidget)

BzViewSwitcher *
bz_view_switcher_new (void);

AdwViewStack *
bz_view_switcher_get_stack (BzViewSwitcher *self);

void
bz_view_switcher_set_stack (BzViewSwitcher *self,
                            AdwViewStack   *stack);

G_END_DECLS

/* End of bz-view-switcher.h */

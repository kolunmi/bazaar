/* bz-window.h
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

#define BZ_TYPE_WINDOW (bz_window_get_type ())
G_DECLARE_FINAL_TYPE (BzWindow, bz_window, BZ, WINDOW, AdwApplicationWindow)

BzWindow *
bz_window_new (BzStateInfo *state);

void
bz_window_search (BzWindow   *self,
                  const char *text);

void
bz_window_toggle_transactions (BzWindow *self);

void
bz_window_push_update_dialog (BzWindow *self);

void
bz_window_show_entry (BzWindow *self,
                      BzEntry  *entry);

void
bz_window_show_group (BzWindow     *self,
                      BzEntryGroup *group);

void
bz_window_show_app_id (BzWindow     *self,
                       const char *app_id);

void
bz_window_push_page(BzWindow *self,
                    AdwNavigationPage *page);

void
bz_window_add_toast (BzWindow *self,
                     AdwToast *toast);

BzStateInfo *
bz_window_get_state_info (BzWindow *self);

G_END_DECLS

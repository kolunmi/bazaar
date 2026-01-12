/* bz-themed-entry-group-rect.h
 *
 * Copyright 2025 Eva M
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

#include <gtk/gtk.h>

#include "bz-entry-group.h"

G_BEGIN_DECLS

#define BZ_TYPE_THEMED_ENTRY_GROUP_RECT (bz_themed_entry_group_rect_get_type ())
G_DECLARE_FINAL_TYPE (BzThemedEntryGroupRect, bz_themed_entry_group_rect, BZ, THEMED_ENTRY_GROUP_RECT, GtkWidget)

GtkWidget *
bz_themed_entry_group_rect_new (void);

BzEntryGroup *
bz_themed_entry_group_rect_get_group (BzThemedEntryGroupRect *self);

void
bz_themed_entry_group_rect_set_group (BzThemedEntryGroupRect *self,
                                      BzEntryGroup           *group);

GtkWidget *
bz_themed_entry_group_rect_get_child (BzThemedEntryGroupRect *self);

void
bz_themed_entry_group_rect_set_child (BzThemedEntryGroupRect *self,
                                      GtkWidget              *child);

G_END_DECLS

/* End of bz-themed-entry-group-rect.h */

/* bz-entry-group.h
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

#include "bz-application-map-factory.h"
#include "bz-entry.h"
#include "bz-result.h"

G_BEGIN_DECLS

#define BZ_TYPE_ENTRY_GROUP (bz_entry_group_get_type ())
G_DECLARE_FINAL_TYPE (BzEntryGroup, bz_entry_group, BZ, ENTRY_GROUP, GObject)

BzEntryGroup *
bz_entry_group_new (BzApplicationMapFactory *factory);

GListModel *
bz_entry_group_get_model (BzEntryGroup *self);

const char *
bz_entry_group_get_id (BzEntryGroup *self);

const char *
bz_entry_group_get_title (BzEntryGroup *self);

const char *
bz_entry_group_get_developer (BzEntryGroup *self);

const char *
bz_entry_group_get_description (BzEntryGroup *self);

GdkPaintable *
bz_entry_group_get_icon_paintable (BzEntryGroup *self);

GIcon *
bz_entry_group_get_mini_icon (BzEntryGroup *self);

gboolean
bz_entry_group_get_is_floss (BzEntryGroup *self);

gboolean
bz_entry_group_get_is_flathub (BzEntryGroup *self);

GPtrArray *
bz_entry_group_get_search_tokens (BzEntryGroup *self);

BzResult *
bz_entry_group_dup_ui_entry (BzEntryGroup *self);

char *
bz_entry_group_dup_ui_entry_id (BzEntryGroup *self);

int
bz_entry_group_get_installable (BzEntryGroup *self);

int
bz_entry_group_get_updatable (BzEntryGroup *self);

int
bz_entry_group_get_removable (BzEntryGroup *self);

int
bz_entry_group_get_installable_and_available (BzEntryGroup *self);

int
bz_entry_group_get_updatable_and_available (BzEntryGroup *self);

int
bz_entry_group_get_removable_and_available (BzEntryGroup *self);

void
bz_entry_group_add (BzEntryGroup *self,
                    BzEntry      *entry);

DexFuture *
bz_entry_group_dup_all_into_model (BzEntryGroup *self);

G_END_DECLS

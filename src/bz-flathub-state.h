/* bz-flathub-state.h
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

#include <gtk/gtk.h>

#include "bz-application-map-factory.h"
#include "bz-entry-group.h"

G_BEGIN_DECLS

#define BZ_TYPE_FLATHUB_STATE (bz_flathub_state_get_type ())
G_DECLARE_FINAL_TYPE (BzFlathubState, bz_flathub_state, BZ, FLATHUB_STATE, GObject)

BzFlathubState *
bz_flathub_state_new (void);

DexFuture *
bz_flathub_state_set_for_day (BzFlathubState *self,
                              const char     *for_day);

void
bz_flathub_state_set_map_factory (BzFlathubState          *self,
                                  BzApplicationMapFactory *map_factory);

const char *
bz_flathub_state_get_for_day (BzFlathubState *self);

BzApplicationMapFactory *
bz_flathub_state_get_map_factory (BzFlathubState *self);

const char *
bz_flathub_state_get_app_of_the_day (BzFlathubState *self);

BzEntryGroup *
bz_flathub_state_dup_app_of_the_day_group (BzFlathubState *self);

GListModel *
bz_flathub_state_dup_apps_of_the_week (BzFlathubState *self);

GListModel *
bz_flathub_state_get_categories (BzFlathubState *self);

double
bz_flathub_state_lookup_app_score (BzFlathubState *self,
                                   const char     *app_id);

gboolean
bz_flathub_state_get_has_connection_error (BzFlathubState *self);

DexFuture *
bz_flathub_state_update_to_today (BzFlathubState *self);

DexFuture *
bz_flathub_state_search_collection (BzFlathubState *self,
                                    const char     *route);

G_END_DECLS

/* End of bz-flathub-state.h */

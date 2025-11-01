/* bz-search-widget.h
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

#include "bz-entry-group.h"
#include "bz-flathub-state.h"

G_BEGIN_DECLS

#define BZ_TYPE_SEARCH_WIDGET (bz_search_widget_get_type ())
G_DECLARE_FINAL_TYPE (BzSearchWidget, bz_search_widget, BZ, SEARCH_WIDGET, AdwBin)

GtkWidget *
bz_search_widget_new (GListModel *model,
                      const char *initial);

void
bz_search_widget_set_model (BzSearchWidget *self,
                            GListModel     *model);

GListModel *
bz_search_widget_get_model (BzSearchWidget *self);

void
bz_search_widget_set_text (BzSearchWidget *self,
                           const char     *text);

const char *
bz_search_widget_get_text (BzSearchWidget *self);

BzEntryGroup *
bz_search_widget_get_selected (BzSearchWidget *self,
                               gboolean       *remove);

BzEntryGroup *
bz_search_widget_get_previewing (BzSearchWidget *self);

void
bz_search_widget_refresh (BzSearchWidget *self);

void
bz_search_widget_set_flathub_state (BzSearchWidget *self,
                                    BzFlathubState *flathub_state);

BzFlathubState *
bz_search_widget_get_flathub_state (BzSearchWidget *self);

G_END_DECLS

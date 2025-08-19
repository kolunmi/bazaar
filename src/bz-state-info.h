/* bz-state-info.h
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
#include "bz-backend.h"
#include "bz-content-provider.h"
#include "bz-entry-cache-manager.h"
#include "bz-flathub-state.h"
#include "bz-search-engine.h"
#include "bz-transaction-manager.h"

G_BEGIN_DECLS

#define BZ_TYPE_STATE_INFO (bz_state_info_get_type ())
G_DECLARE_FINAL_TYPE (BzStateInfo, bz_state_info, BZ, STATE_INFO, GObject)

BzStateInfo *
bz_state_info_new (void);

GSettings *
bz_state_info_get_settings (BzStateInfo *self);

GListModel *
bz_state_info_get_blocklists (BzStateInfo *self);

GListModel *
bz_state_info_get_curated_configs (BzStateInfo *self);

BzBackend *
bz_state_info_get_backend (BzStateInfo *self);

BzEntryCacheManager *
bz_state_info_get_cache_manager (BzStateInfo *self);

BzTransactionManager *
bz_state_info_get_transaction_manager (BzStateInfo *self);

GListModel *
bz_state_info_get_available_updates (BzStateInfo *self);

BzApplicationMapFactory *
bz_state_info_get_entry_factory (BzStateInfo *self);

BzApplicationMapFactory *
bz_state_info_get_installed_factory (BzStateInfo *self);

BzApplicationMapFactory *
bz_state_info_get_application_factory (BzStateInfo *self);

GListModel *
bz_state_info_get_all_entries (BzStateInfo *self);

GListModel *
bz_state_info_get_all_installed_entries (BzStateInfo *self);

GListModel *
bz_state_info_get_all_entry_groups (BzStateInfo *self);

BzSearchEngine *
bz_state_info_get_search_engine (BzStateInfo *self);

BzContentProvider *
bz_state_info_get_curated_provider (BzStateInfo *self);

BzFlathubState *
bz_state_info_get_flathub (BzStateInfo *self);

gboolean
bz_state_info_get_busy (BzStateInfo *self);

const char *
bz_state_info_get_busy_label (BzStateInfo *self);

double
bz_state_info_get_busy_progress (BzStateInfo *self);

gboolean
bz_state_info_get_online (BzStateInfo *self);

gboolean
bz_state_info_get_checking_for_updates (BzStateInfo *self);

const char *
bz_state_info_get_background_task_label (BzStateInfo *self);

void
bz_state_info_set_settings (BzStateInfo *self,
                            GSettings   *settings);

void
bz_state_info_set_blocklists (BzStateInfo *self,
                              GListModel  *blocklists);

void
bz_state_info_set_curated_configs (BzStateInfo *self,
                                   GListModel  *curated_configs);

void
bz_state_info_set_backend (BzStateInfo *self,
                           BzBackend   *backend);

void
bz_state_info_set_cache_manager (BzStateInfo         *self,
                                 BzEntryCacheManager *cache_manager);

void
bz_state_info_set_transaction_manager (BzStateInfo          *self,
                                       BzTransactionManager *transaction_manager);

void
bz_state_info_set_available_updates (BzStateInfo *self,
                                     GListModel  *available_updates);

void
bz_state_info_set_entry_factory (BzStateInfo             *self,
                                 BzApplicationMapFactory *entry_factory);

void
bz_state_info_set_installed_factory (BzStateInfo             *self,
                                     BzApplicationMapFactory *installed_factory);

void
bz_state_info_set_application_factory (BzStateInfo             *self,
                                       BzApplicationMapFactory *application_factory);

void
bz_state_info_set_all_entries (BzStateInfo *self,
                               GListModel  *all_entries);

void
bz_state_info_set_all_installed_entries (BzStateInfo *self,
                                         GListModel  *all_installed_entries);

void
bz_state_info_set_all_entry_groups (BzStateInfo *self,
                                    GListModel  *all_entry_groups);

void
bz_state_info_set_search_engine (BzStateInfo    *self,
                                 BzSearchEngine *search_engine);

void
bz_state_info_set_curated_provider (BzStateInfo       *self,
                                    BzContentProvider *curated_provider);

void
bz_state_info_set_flathub (BzStateInfo    *self,
                           BzFlathubState *flathub);

void
bz_state_info_set_busy (BzStateInfo *self,
                        gboolean     busy);

void
bz_state_info_set_busy_label (BzStateInfo *self,
                              const char  *busy_label);

void
bz_state_info_set_busy_progress (BzStateInfo *self,
                                 double       busy_progress);

void
bz_state_info_set_online (BzStateInfo *self,
                          gboolean     online);

void
bz_state_info_set_checking_for_updates (BzStateInfo *self,
                                        gboolean     checking_for_updates);

void
bz_state_info_set_background_task_label (BzStateInfo *self,
                                         const char  *background_task_label);

G_END_DECLS

/* End of bz-state-info.h */

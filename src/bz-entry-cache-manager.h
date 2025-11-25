/* bz-entry-cache-manager.h
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

#include <libdex.h>

#include "bz-entry.h"

G_BEGIN_DECLS

#define BZ_ENTRY_CACHE_ERROR (bz_entry_cache_error_quark ())
GQuark bz_entry_cache_error_quark (void);

typedef enum
{
  BZ_ENTRY_CACHE_ERROR_CACHE_FAILED = 0,
  BZ_ENTRY_CACHE_ERROR_DECACHE_FAILED,
  BZ_ENTRY_CACHE_ERROR_ENUMERATE_FAILED,
} BzEntry_CacheError;

#define BZ_TYPE_ENTRY_CACHE_MANAGER (bz_entry_cache_manager_get_type ())
G_DECLARE_FINAL_TYPE (BzEntryCacheManager, bz_entry_cache_manager, BZ, ENTRY_CACHE_MANAGER, GObject)

BzEntryCacheManager *
bz_entry_cache_manager_new (void);

guint64
bz_entry_cache_manager_get_max_memory_usage (BzEntryCacheManager *self);

void
bz_entry_cache_manager_set_max_memory_usage (BzEntryCacheManager *self,
                                             guint64              max_memory_usage);

DexFuture *
bz_entry_cache_manager_add (BzEntryCacheManager *self,
                            BzEntry             *entry);

DexFuture *
bz_entry_cache_manager_get (BzEntryCacheManager *self,
                            const char          *unique_id);

DexFuture *
bz_entry_cache_manager_get_by_checksum (BzEntryCacheManager *self,
                                        const char          *unique_id_checksum);

DexFuture *
bz_entry_cache_manager_enumerate_disk (BzEntryCacheManager *self);

G_END_DECLS

/* End of bz-entry-cache-manager.h */

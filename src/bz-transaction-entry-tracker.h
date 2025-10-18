/* bz-transaction-entry-tracker.h
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

#include "bz-entry.h"

G_BEGIN_DECLS

typedef enum
{
  BZ_TRANSACTION_TYPE_INSTALL,
  BZ_TRANSACTION_TYPE_UPDATE,
  BZ_TRANSACTION_TYPE_REMOVAL
} BzTransactionType;

#define BZ_TYPE_TRANSACTION_ENTRY_TRACKER (bz_transaction_entry_tracker_get_type ())
G_DECLARE_FINAL_TYPE (BzTransactionEntryTracker, bz_transaction_entry_tracker, BZ, TRANSACTION_ENTRY_TRACKER, GObject)

BzTransactionEntryTracker *
bz_transaction_entry_tracker_new (void);

BzEntry *
bz_transaction_entry_tracker_get_entry (BzTransactionEntryTracker *self);

GListModel *
bz_transaction_entry_tracker_get_current_ops (BzTransactionEntryTracker *self);

GListModel *
bz_transaction_entry_tracker_get_finished_ops (BzTransactionEntryTracker *self);

BzTransactionType
bz_transaction_entry_tracker_get_type_enum (BzTransactionEntryTracker *self);

void
bz_transaction_entry_tracker_set_entry (BzTransactionEntryTracker *self,
                                        BzEntry                   *entry);

void
bz_transaction_entry_tracker_set_current_ops (BzTransactionEntryTracker *self,
                                              GListModel                *current_ops);

void
bz_transaction_entry_tracker_set_finished_ops (BzTransactionEntryTracker *self,
                                               GListModel                *finished_ops);

void
bz_transaction_entry_tracker_set_type_enum (BzTransactionEntryTracker *self,
                                            BzTransactionType          type);

G_END_DECLS
/* End of bz-transaction-entry-tracker.h */

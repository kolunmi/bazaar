/* bz-full-view.h
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
#include "bz-transaction-manager.h"

G_BEGIN_DECLS

#define BZ_TYPE_FULL_VIEW (bz_full_view_get_type ())
G_DECLARE_FINAL_TYPE (BzFullView, bz_full_view, BZ, FULL_VIEW, AdwBin)

GtkWidget *
bz_full_view_new (void);

void
bz_full_view_set_transaction_manager (BzFullView           *self,
                                      BzTransactionManager *group);

BzTransactionManager *
bz_full_view_get_transaction_manager (BzFullView *self);

void
bz_full_view_set_entry_group (BzFullView   *self,
                              BzEntryGroup *group);

BzEntryGroup *
bz_full_view_get_entry_group (BzFullView *self);

void
bz_full_view_set_debounce (BzFullView *self,
                           gboolean    debounce);

gboolean
bz_full_view_get_debounce (BzFullView *self);

G_END_DECLS

/* bz-transaction-view.h
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

#include "bz-transaction.h"

G_BEGIN_DECLS

#define BZ_TYPE_TRANSACTION_VIEW (bz_transaction_view_get_type ())
G_DECLARE_FINAL_TYPE (BzTransactionView, bz_transaction_view, BZ, TRANSACTION_VIEW, AdwBin)

BzTransactionView *
bz_transaction_view_new (void);

BzTransaction *
bz_transaction_view_get_transaction (BzTransactionView *self);

void
bz_transaction_view_set_transaction (BzTransactionView *self,
                                     BzTransaction     *transaction);

G_END_DECLS

/* End of bz-transaction-view.h */

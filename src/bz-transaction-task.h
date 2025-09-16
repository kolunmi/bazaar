/* bz-transaction-task.h
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

#include "bz-backend-transaction-op-payload.h"
#include "bz-backend-transaction-op-progress-payload.h"

G_BEGIN_DECLS

#define BZ_TYPE_TRANSACTION_TASK (bz_transaction_task_get_type ())
G_DECLARE_FINAL_TYPE (BzTransactionTask, bz_transaction_task, BZ, TRANSACTION_TASK, GObject)

BzTransactionTask *
bz_transaction_task_new (void);

BzBackendTransactionOpPayload *
bz_transaction_task_get_op (BzTransactionTask *self);

BzBackendTransactionOpProgressPayload *
bz_transaction_task_get_last_progress (BzTransactionTask *self);

const char *
bz_transaction_task_get_error (BzTransactionTask *self);

void
bz_transaction_task_set_op (BzTransactionTask             *self,
                            BzBackendTransactionOpPayload *op);

void
bz_transaction_task_set_last_progress (BzTransactionTask                     *self,
                                       BzBackendTransactionOpProgressPayload *last_progress);

void
bz_transaction_task_set_error (BzTransactionTask *self,
                               const char        *error);

G_END_DECLS

/* End of bz-transaction-task.h */

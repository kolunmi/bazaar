/* bz-backend-transaction-op-progress-payload.h
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

G_BEGIN_DECLS

#define BZ_TYPE_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (bz_backend_transaction_op_progress_payload_get_type ())
G_DECLARE_FINAL_TYPE (BzBackendTransactionOpProgressPayload, bz_backend_transaction_op_progress_payload, BZ, BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD, GObject)

BzBackendTransactionOpProgressPayload *
bz_backend_transaction_op_progress_payload_new (void);

BzBackendTransactionOpPayload *
bz_backend_transaction_op_progress_payload_get_op (BzBackendTransactionOpProgressPayload *self);

const char *
bz_backend_transaction_op_progress_payload_get_status (BzBackendTransactionOpProgressPayload *self);

gboolean
bz_backend_transaction_op_progress_payload_get_is_estimating (BzBackendTransactionOpProgressPayload *self);

double
bz_backend_transaction_op_progress_payload_get_progress (BzBackendTransactionOpProgressPayload *self);

double
bz_backend_transaction_op_progress_payload_get_total_progress (BzBackendTransactionOpProgressPayload *self);

guint64
bz_backend_transaction_op_progress_payload_get_bytes_transferred (BzBackendTransactionOpProgressPayload *self);

guint64
bz_backend_transaction_op_progress_payload_get_start_time (BzBackendTransactionOpProgressPayload *self);

void
bz_backend_transaction_op_progress_payload_set_op (BzBackendTransactionOpProgressPayload *self,
                                                   BzBackendTransactionOpPayload         *op);

void
bz_backend_transaction_op_progress_payload_set_status (BzBackendTransactionOpProgressPayload *self,
                                                       const char                            *status);

void
bz_backend_transaction_op_progress_payload_set_is_estimating (BzBackendTransactionOpProgressPayload *self,
                                                              gboolean                               is_estimating);

void
bz_backend_transaction_op_progress_payload_set_progress (BzBackendTransactionOpProgressPayload *self,
                                                         double                                 progress);

void
bz_backend_transaction_op_progress_payload_set_total_progress (BzBackendTransactionOpProgressPayload *self,
                                                               double                                 total_progress);

void
bz_backend_transaction_op_progress_payload_set_bytes_transferred (BzBackendTransactionOpProgressPayload *self,
                                                                  guint64                                bytes_transferred);

void
bz_backend_transaction_op_progress_payload_set_start_time (BzBackendTransactionOpProgressPayload *self,
                                                           guint64                                start_time);

G_END_DECLS

/* End of bz-backend-transaction-op-progress-payload.h */

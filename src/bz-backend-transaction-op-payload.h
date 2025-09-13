/* bz-backend-transaction-op-payload.h
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

#define BZ_TYPE_BACKEND_TRANSACTION_OP_PAYLOAD (bz_backend_transaction_op_payload_get_type ())
G_DECLARE_FINAL_TYPE (BzBackendTransactionOpPayload, bz_backend_transaction_op_payload, BZ, BACKEND_TRANSACTION_OP_PAYLOAD, GObject)

BzBackendTransactionOpPayload *
bz_backend_transaction_op_payload_new (void);

const char *
bz_backend_transaction_op_payload_get_name (BzBackendTransactionOpPayload *self);

BzEntry *
bz_backend_transaction_op_payload_get_entry (BzBackendTransactionOpPayload *self);

guint64
bz_backend_transaction_op_payload_get_download_size (BzBackendTransactionOpPayload *self);

guint64
bz_backend_transaction_op_payload_get_installed_size (BzBackendTransactionOpPayload *self);

void
bz_backend_transaction_op_payload_set_name (BzBackendTransactionOpPayload *self,
                                            const char                    *name);

void
bz_backend_transaction_op_payload_set_entry (BzBackendTransactionOpPayload *self,
                                             BzEntry                       *entry);

void
bz_backend_transaction_op_payload_set_download_size (BzBackendTransactionOpPayload *self,
                                                     guint64                        download_size);

void
bz_backend_transaction_op_payload_set_installed_size (BzBackendTransactionOpPayload *self,
                                                      guint64                        installed_size);

G_END_DECLS

/* End of bz-backend-transaction-op-payload.h */

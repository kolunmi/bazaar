/* bz-transaction-list-dialog.h
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

G_BEGIN_DECLS

#define BZ_TYPE_TRANSACTION_LIST_DIALOG (bz_transaction_list_dialog_get_type ())

G_DECLARE_FINAL_TYPE (BzTransactionListDialog, bz_transaction_list_dialog, BZ, TRANSACTION_LIST_DIALOG, AdwAlertDialog)

AdwDialog *
bz_transaction_list_dialog_new (GListModel  *entries,
                                const gchar *heading,
                                const gchar *body,
                                const gchar *body_no_apps,
                                const gchar *secondary_label,
                                const gchar *cancel_label,
                                const gchar *confirm_label);

gboolean
bz_transaction_list_dialog_was_confirmed (BzTransactionListDialog *self);

G_END_DECLS

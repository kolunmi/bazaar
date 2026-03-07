/* bz-transaction-dialog.h
 *
 * Copyright 2026 Alexander Vanhee
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

#include "bz-bulk-install-dialog-result.h"
#include "bz-entry-group.h"
#include "bz-transaction-dialog-result.h"

G_BEGIN_DECLS

DexFuture *bz_transaction_dialog_show (GtkWidget    *parent,
                                       BzEntry      *entry,
                                       BzEntryGroup *group,
                                       gboolean      remove,
                                       gboolean      auto_confirm);

DexFuture *bz_bulk_install_dialog_show (GtkWidget  *parent,
                                        GListModel *groups);

G_END_DECLS

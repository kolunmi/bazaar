/* bz-updates-card.h
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

#include "bz-state-info.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_UPDATES_CARD (bz_updates_card_get_type ())

G_DECLARE_FINAL_TYPE (BzUpdatesCard, bz_updates_card, BZ, UPDATES_CARD, AdwBin)

GtkWidget *
bz_updates_card_new (void);

void
bz_updates_card_set_state (BzUpdatesCard *self,
                           BzStateInfo   *state);

BzStateInfo *
bz_updates_card_get_state (BzUpdatesCard *self);

G_END_DECLS

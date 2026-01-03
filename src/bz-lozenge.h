/* bz-lozenge.h
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-context-row.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_LOZENGE (bz_lozenge_get_type ())

G_DECLARE_FINAL_TYPE (BzLozenge, bz_lozenge, BZ, LOZENGE, GtkBox)

GtkWidget *bz_lozenge_new (void);

void         bz_lozenge_set_title (BzLozenge   *self,
                                   const gchar *title);
const gchar *bz_lozenge_get_title (BzLozenge *self);

void         bz_lozenge_set_label (BzLozenge   *self,
                                   const gchar *label);
const gchar *bz_lozenge_get_label (BzLozenge *self);

void    bz_lozenge_set_icon_names (BzLozenge          *self,
                                   const gchar *const *icon_names);
gchar **bz_lozenge_get_icon_names (BzLozenge *self);

void         bz_lozenge_set_importance (BzLozenge   *self,
                                        BzImportance importance);
BzImportance bz_lozenge_get_importance (BzLozenge *self);

G_END_DECLS

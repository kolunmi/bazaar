/* bz-context-row.h
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

G_BEGIN_DECLS

typedef enum
{
  BZ_IMPORTANCE_UNIMPORTANT,
  BZ_IMPORTANCE_NEUTRAL,
  BZ_IMPORTANCE_INFORMATION,
  BZ_IMPORTANCE_WARNING,
  BZ_IMPORTANCE_IMPORTANT,
} BzImportance;

#define BZ_TYPE_IMPORTANCE (bz_importance_get_type ())
GType bz_importance_get_type (void) G_GNUC_CONST;

AdwActionRow *
bz_context_row_new (const gchar *icon_name,
                    BzImportance importance,
                    const gchar *title,
                    const gchar *subtitle);

const gchar *
bz_context_row_importance_to_css_class (BzImportance importance);

G_END_DECLS

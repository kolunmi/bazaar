/* bz-category-flags.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define BZ_TYPE_CATEGORY_FLAGS (bz_category_flags_get_type ())

typedef enum
{
  BZ_CATEGORY_FLAGS_NONE             = 0,
  BZ_CATEGORY_FLAGS_AUDIOVIDEO       = 1 << 0,
  BZ_CATEGORY_FLAGS_DEVELOPMENT      = 1 << 1,
  BZ_CATEGORY_FLAGS_EDUCATION        = 1 << 2,
  BZ_CATEGORY_FLAGS_GAME             = 1 << 3,
  BZ_CATEGORY_FLAGS_GRAPHICS         = 1 << 4,
  BZ_CATEGORY_FLAGS_NETWORK          = 1 << 5,
  BZ_CATEGORY_FLAGS_OFFICE           = 1 << 6,
  BZ_CATEGORY_FLAGS_SCIENCE          = 1 << 7,
  BZ_CATEGORY_FLAGS_SYSTEM           = 1 << 8,
  BZ_CATEGORY_FLAGS_UTILITY          = 1 << 9,
  BZ_CATEGORY_FLAGS_TRENDING         = 1 << 10,
  BZ_CATEGORY_FLAGS_POPULAR          = 1 << 11,
  BZ_CATEGORY_FLAGS_RECENTLY_ADDED   = 1 << 12,
  BZ_CATEGORY_FLAGS_RECENTLY_UPDATED = 1 << 13,
  BZ_CATEGORY_FLAGS_MOBILE           = 1 << 14,
  BZ_CATEGORY_FLAGS_ADWAITA          = 1 << 15,
  BZ_CATEGORY_FLAGS_KDE              = 1 << 16,
} BzCategoryFlags;

GType
bz_category_flags_get_type (void);

BzCategoryFlags
bz_category_flags_add (BzCategoryFlags flags,
                       const char     *name);

gboolean
bz_category_flags_has_name (BzCategoryFlags flags,
                            const char     *name);

BzCategoryFlags
bz_category_flags_from_name (const char *name);

G_END_DECLS

/* bz-category-flags.c
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

#include "config.h"

#include "bz-category-flags.h"

static const struct
{
  const char     *name;
  BzCategoryFlags flag;
} name_flag_map[] = {
  {       "audiovideo",       BZ_CATEGORY_FLAGS_AUDIOVIDEO },
  {      "development",      BZ_CATEGORY_FLAGS_DEVELOPMENT },
  {        "education",        BZ_CATEGORY_FLAGS_EDUCATION },
  {             "game",             BZ_CATEGORY_FLAGS_GAME },
  {         "graphics",         BZ_CATEGORY_FLAGS_GRAPHICS },
  {          "network",          BZ_CATEGORY_FLAGS_NETWORK },
  {           "office",           BZ_CATEGORY_FLAGS_OFFICE },
  {          "science",          BZ_CATEGORY_FLAGS_SCIENCE },
  {           "system",           BZ_CATEGORY_FLAGS_SYSTEM },
  {          "utility",          BZ_CATEGORY_FLAGS_UTILITY },
  {         "trending",         BZ_CATEGORY_FLAGS_TRENDING },
  {          "popular",          BZ_CATEGORY_FLAGS_POPULAR },
  {   "recently-added",   BZ_CATEGORY_FLAGS_RECENTLY_ADDED },
  { "recently-updated", BZ_CATEGORY_FLAGS_RECENTLY_UPDATED },
  {           "mobile",           BZ_CATEGORY_FLAGS_MOBILE },
  {          "adwaita",          BZ_CATEGORY_FLAGS_ADWAITA },
  {              "kde",              BZ_CATEGORY_FLAGS_KDE },
};

BzCategoryFlags
bz_category_flags_add (BzCategoryFlags flags,
                       const char     *name)
{
  g_autofree char *lower = NULL;

  if (name == NULL)
    return flags;

  lower = g_ascii_strdown (name, -1);
  for (gsize i = 0; i < G_N_ELEMENTS (name_flag_map); i++)
    {
      if (g_strcmp0 (name_flag_map[i].name, lower) == 0)
        return flags | name_flag_map[i].flag;
    }

  return flags;
}

gboolean
bz_category_flags_has_name (BzCategoryFlags flags,
                            const char     *name)
{
  for (gsize i = 0; i < G_N_ELEMENTS (name_flag_map); i++)
    {
      if (g_strcmp0 (name_flag_map[i].name, name) == 0)
        return (flags & name_flag_map[i].flag) != 0;
    }

  return FALSE;
}

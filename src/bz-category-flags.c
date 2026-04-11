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

GType
bz_category_flags_get_type (void)
{
  static gsize g_define_type_id = 0;

  if (g_once_init_enter (&g_define_type_id))
    {
      static const GFlagsValue values[] = {
        {             BZ_CATEGORY_FLAGS_NONE,             "BZ_CATEGORY_FLAGS_NONE",             "none" },
        {       BZ_CATEGORY_FLAGS_AUDIOVIDEO,       "BZ_CATEGORY_FLAGS_AUDIOVIDEO",       "audiovideo" },
        {      BZ_CATEGORY_FLAGS_DEVELOPMENT,      "BZ_CATEGORY_FLAGS_DEVELOPMENT",      "development" },
        {        BZ_CATEGORY_FLAGS_EDUCATION,        "BZ_CATEGORY_FLAGS_EDUCATION",        "education" },
        {             BZ_CATEGORY_FLAGS_GAME,             "BZ_CATEGORY_FLAGS_GAME",             "game" },
        {         BZ_CATEGORY_FLAGS_GRAPHICS,         "BZ_CATEGORY_FLAGS_GRAPHICS",         "graphics" },
        {          BZ_CATEGORY_FLAGS_NETWORK,          "BZ_CATEGORY_FLAGS_NETWORK",          "network" },
        {           BZ_CATEGORY_FLAGS_OFFICE,           "BZ_CATEGORY_FLAGS_OFFICE",           "office" },
        {          BZ_CATEGORY_FLAGS_SCIENCE,          "BZ_CATEGORY_FLAGS_SCIENCE",          "science" },
        {           BZ_CATEGORY_FLAGS_SYSTEM,           "BZ_CATEGORY_FLAGS_SYSTEM",           "system" },
        {          BZ_CATEGORY_FLAGS_UTILITY,          "BZ_CATEGORY_FLAGS_UTILITY",          "utility" },
        {         BZ_CATEGORY_FLAGS_TRENDING,         "BZ_CATEGORY_FLAGS_TRENDING",         "trending" },
        {          BZ_CATEGORY_FLAGS_POPULAR,          "BZ_CATEGORY_FLAGS_POPULAR",          "popular" },
        {   BZ_CATEGORY_FLAGS_RECENTLY_ADDED,   "BZ_CATEGORY_FLAGS_RECENTLY_ADDED",   "recently-added" },
        { BZ_CATEGORY_FLAGS_RECENTLY_UPDATED, "BZ_CATEGORY_FLAGS_RECENTLY_UPDATED", "recently-updated" },
        {           BZ_CATEGORY_FLAGS_MOBILE,           "BZ_CATEGORY_FLAGS_MOBILE",           "mobile" },
        {          BZ_CATEGORY_FLAGS_ADWAITA,          "BZ_CATEGORY_FLAGS_ADWAITA",          "adwaita" },
        {              BZ_CATEGORY_FLAGS_KDE,              "BZ_CATEGORY_FLAGS_KDE",              "kde" },
        {                                  0,                                 NULL,               NULL }
      };

      GType type = g_flags_register_static ("BzCategoryFlags", values);
      g_once_init_leave (&g_define_type_id, type);
    }

  return g_define_type_id;
}

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

BzCategoryFlags
bz_category_flags_from_name (const char *name)
{
  g_autofree char *lower = g_ascii_strdown (name, -1);

  for (gsize i = 0; i < G_N_ELEMENTS (name_flag_map); i++)
    if (g_strcmp0 (name_flag_map[i].name, lower) == 0)
      return name_flag_map[i].flag;

  return BZ_CATEGORY_FLAGS_NONE;
}

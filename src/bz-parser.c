/* bz-parser.c
 *
 * Copyright 2025 Eva M
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

#include "bz-parser.h"

G_DEFINE_INTERFACE (BzParser, bz_parser, G_TYPE_OBJECT)

static GHashTable *
bz_parser_real_process_bytes (BzParser *self,
                              GBytes   *bytes,
                              GError  **error)
{
  return NULL;
}

static void
bz_parser_default_init (BzParserInterface *iface)
{
  iface->process_bytes = bz_parser_real_process_bytes;
}

GHashTable *
bz_parser_process_bytes (BzParser *self,
                         GBytes   *bytes,
                         GError  **error)
{
  g_return_val_if_fail (BZ_IS_PARSER (self), NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  return BZ_PARSER_GET_IFACE (self)->process_bytes (
      self,
      bytes,
      error);
}

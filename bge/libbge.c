/* libbge.c
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

#define G_LOG_DOMAIN "BGE::CORE"

#include "bge.h"

#include "wdgt/fmt/parser.h"

/**
 * bge_init:
 *
 * Initializes BGE (And GTK).
 *
 * This function must be called before using any other BGE functions.
 */
void
bge_init (void)
{
  gtk_init ();

  g_type_ensure (BGE_TYPE_ANIMATION);
  g_type_ensure (BGE_TYPE_CAROUSEL);
  g_type_ensure (BGE_TYPE_MARKDOWN_RENDER);

  {
    g_autoptr (GError) local_error = NULL;
    gsize            length        = 0;
    g_autofree char *contents      = NULL;
    g_autoptr (BgeWdgtSpec) spec   = NULL;

    g_assert (g_file_get_contents ("/home/kol/Projects/bazaar/bge/test.wdgt", &contents, &length, NULL));

    g_type_ensure (GTK_TYPE_LABEL);
    spec = bge_wdgt_parse_string (contents, &local_error);
    if (spec == NULL)
      g_print ("Error!! %s\n", local_error->message);
  }
}

/* bz-env.c
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

#include <libdex.h>

#include "bz-env.h"

gsize
bz_get_dex_stack_size (void)
{
  static guint64 stack_size = 0;

  if (g_once_init_enter (&stack_size))
    {
      const char *envvar = NULL;
      guint64     value  = 0;

      value = MAX (4096 * 32, dex_get_min_stack_size ());

      envvar = g_getenv ("BAZAAR_DEX_STACK_SIZE");
      if (envvar != NULL)
        {
          g_autoptr (GError) local_error = NULL;
          g_autoptr (GVariant) variant   = NULL;

          variant = g_variant_parse (
              G_VARIANT_TYPE_UINT64, envvar,
              NULL, NULL, &local_error);
          if (variant != NULL)
            {
              guint64 parse_result = 0;

              parse_result = g_variant_get_uint64 (variant);
              if (parse_result < dex_get_min_stack_size ())
                g_critical ("BAZAAR_DEX_STACK_SIZE must be greater than %zu on this system",
                            dex_get_min_stack_size ());
              else
                value = parse_result;
            }
          else
            g_critical ("BAZAAR_DEX_STACK_SIZE is invalid: %s", local_error->message);
        }

      g_once_init_leave (&stack_size, value);
    }

  return stack_size;
}

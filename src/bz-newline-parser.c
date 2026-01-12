/* bz-newline-parser.c
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

#define G_LOG_DOMAIN "BAZAAR::NEWLINE-TXT-PARSER"

#include "config.h"

#include "bz-hash-table-object.h"
#include "bz-newline-parser.h"
#include "bz-parser.h"

struct _BzNewlineParser
{
  GObject parent_instance;

  gboolean comments;
  guint    max_lines;
};

static void
parser_iface_init (BzParserInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzNewlineParser,
    bz_newline_parser,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (BZ_TYPE_PARSER, parser_iface_init))

static void
destroy_gvalue (GValue *value);

static void
bz_newline_parser_dispose (GObject *object)
{
  BzNewlineParser *self = BZ_NEWLINE_PARSER (object);

  (void) self;

  G_OBJECT_CLASS (bz_newline_parser_parent_class)->dispose (object);
}

static void
bz_newline_parser_class_init (BzNewlineParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_newline_parser_dispose;
}

static void
bz_newline_parser_init (BzNewlineParser *self)
{
}

static GHashTable *
bz_newline_parser_real_process_bytes (BzParser *iface_self,
                                      GBytes   *bytes,
                                      GError  **error)
{
  BzNewlineParser *self             = BZ_NEWLINE_PARSER (iface_self);
  gsize            size             = 0;
  const guchar    *data             = NULL;
  g_autofree char *contents         = NULL;
  g_autoptr (GHashTable) set        = NULL;
  guint n_ids                       = 0;
  char *beg                         = NULL;
  char *end                         = NULL;
  g_autoptr (BzHashTableObject) obj = NULL;
  GValue *value                     = NULL;
  g_autoptr (GHashTable) ret        = NULL;

  g_return_val_if_fail (BZ_IS_NEWLINE_PARSER (self), NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  data           = g_bytes_get_data (bytes, &size);
  contents       = g_memdup2 (data, size + 1);
  contents[size] = '\0';

  set = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  for (beg = contents, end = g_utf8_strchr (beg, -1, '\n');
       beg != NULL && *beg != '\0';
       beg = end + 1, end = g_utf8_strchr (beg, -1, '\n'))
    {
      g_autofree char *line = NULL;

      if (end == NULL)
        g_warning ("Data has no terminating newline");
      if ((self->comments && g_str_has_prefix (beg, "#")) ||
          (end != NULL && end - beg <= 1) ||
          (end == NULL && *beg == '\0'))
        {
          if (end != NULL)
            continue;
          else
            break;
        }

      if (end != NULL)
        line = g_strndup (beg, end - beg);
      else
        line = g_strdup (beg);
      if (g_hash_table_contains (set, line))
        g_warning ("Duplicate line %s detected in data", line);
      else
        g_hash_table_replace (set, g_steal_pointer (&line), NULL);

      if (end == NULL)
        break;
      if (self->max_lines > 0 &&
          ++n_ids > self->max_lines)
        {
          g_warning ("Data has a lot of lines, automatically "
                     "truncating to %d",
                     self->max_lines);
          break;
        }
    }

  obj = bz_hash_table_object_new ();
  bz_hash_table_object_set_hash_table (obj, set);

  value = g_new0 (typeof (*value), 1);
  g_value_init (value, G_TYPE_OBJECT);
  g_value_set_object (value, obj);

  ret = g_hash_table_new_full (
      g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) destroy_gvalue);
  g_hash_table_replace (ret, g_strdup ("/"), g_steal_pointer (&value));

  return g_steal_pointer (&ret);
}

static void
parser_iface_init (BzParserInterface *iface)
{
  iface->process_bytes = bz_newline_parser_real_process_bytes;
}

BzNewlineParser *
bz_newline_parser_new (gboolean comments,
                       guint    max_lines)
{
  BzNewlineParser *parser = NULL;

  parser            = g_object_new (BZ_TYPE_NEWLINE_PARSER, NULL);
  parser->comments  = comments;
  parser->max_lines = max_lines;

  return parser;
}

static void
destroy_gvalue (GValue *value)
{
  g_value_unset (value);
  g_free (value);
}

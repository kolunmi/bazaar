/* bge-parser.c
 *
 * Copyright 2026 Eva M
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

#include "bge.h"

#include "parser.h"

#define STR_DEFWIDGET  "defwidget"
#define STR_CHILD      "child"
#define STR_STATE      "state"
#define STR_TRANSITION "transition"
#define STR_ALLOCATE   "%allocate"
#define STR_SNAPSHOT   "%snapshot"

enum
{
  BLK_TOPLEVEL = 0,
  BLK_DEFWIDGET,
  BLK_STATE,
  BLK_PROPERTIES,
  BLK_ALLOCATE_FUNC,
  BLK_SNAPSHOT_FUNC,
};

enum
{
  LEFT_ASSIGN = 0,
  RIGHT_ASSIGN,
};

#define IS_EOF(_p) ((_p) == NULL || *(_p) == '\0')

#define APPEND_ENUM_TO_ARRAY(_array, _enum) \
  G_STMT_START                              \
  {                                         \
    int _store = (_enum);                   \
    g_array_append_val ((_array), _store);  \
  }                                         \
  G_STMT_END

static char *
consume_token (const char **pp,
               gboolean     quoted,
               gboolean     allow_brackets,
               GError     **error);

gboolean
bge_wdgt_parse_string (const char *string,
                       GError    **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GArray) blocks      = NULL;

  g_return_val_if_fail (string != NULL, FALSE);

  blocks = g_array_new (FALSE, TRUE, sizeof (int));
  APPEND_ENUM_TO_ARRAY (blocks, BLK_TOPLEVEL);

#define RETURN_ERROR_UNLESS(_cond)       \
  G_STMT_START                           \
  {                                      \
    if (!(_cond))                        \
      {                                  \
        g_set_error (                    \
            error,                       \
            G_IO_ERROR,                  \
            G_IO_ERROR_UNKNOWN,          \
            "wdgt fmt parser error: %s", \
            local_error->message);       \
        return FALSE;                    \
      }                                  \
  }                                      \
  G_STMT_END

#define EXPECT_TOKEN(_string, _token)         \
  G_STMT_START                                \
  {                                           \
    if (g_strcmp0 ((_string), (_token)) != 0) \
      {                                       \
        g_set_error (                         \
            error,                            \
            G_IO_ERROR,                       \
            G_IO_ERROR_UNKNOWN,               \
            "wdgt fmt parser error: "         \
            "expected token %s, got \"%s\"",  \
            (_token), (_string));             \
        return FALSE;                         \
      }                                       \
  }                                           \
  G_STMT_END

  for (const char *p = string;
       p != NULL && *p != '\0';
       p = g_utf8_next_char (p))
    {
      g_autofree char *token0 = NULL;

      token0 = consume_token (&p, FALSE, FALSE, &local_error);
      RETURN_ERROR_UNLESS (token0 != NULL);
      EXPECT_TOKEN (token0, STR_DEFWIDGET);
      g_print ("%s\n", token0);
      g_clear_pointer (&token0, g_free);

      token0 = consume_token (&p, TRUE, FALSE, &local_error);
      RETURN_ERROR_UNLESS (token0 != NULL);
      g_print ("%s\n", token0);
      g_clear_pointer (&token0, g_free);

      token0 = consume_token (&p, FALSE, TRUE, &local_error);
      RETURN_ERROR_UNLESS (token0 != NULL);
      EXPECT_TOKEN (token0, "{");
      g_print ("%s\n", token0);
      g_clear_pointer (&token0, g_free);

      token0 = consume_token (&p, FALSE, FALSE, &local_error);
      RETURN_ERROR_UNLESS (token0 != NULL);
      EXPECT_TOKEN (token0, "atoken");
      g_print ("%s\n", token0);
      g_clear_pointer (&token0, g_free);

      break;
    }

#undef RETURN_ERROR_UNLESS
#undef EXPECT_TOKEN

  return TRUE;
}

static char *
consume_token (const char **pp,
               gboolean     quoted,
               gboolean     allow_brackets,
               GError     **error)
{
  const char *p             = *pp;
  gboolean    hit_non_space = FALSE;

#define UNEXPECTED_EOF      \
  G_STMT_START              \
  {                         \
    g_set_error (           \
        error,              \
        G_IO_ERROR,         \
        G_IO_ERROR_UNKNOWN, \
        "Unexpected EOF");  \
    return NULL;            \
  }                         \
  G_STMT_END

#define RETURN_TOKEN                 \
  G_STMT_START                       \
  {                                  \
    g_autofree char *_ret = NULL;    \
                                     \
    _ret = g_strndup (*pp, p - *pp); \
    *pp  = p;                        \
    return g_steal_pointer (&_ret);  \
  }                                  \
  G_STMT_END

#define RETURN_TOKEN_ADJUST_NEXT_CHAR \
  G_STMT_START                        \
  {                                   \
    g_autofree char *_ret = NULL;     \
                                      \
    _ret = g_strndup (*pp, p - *pp);  \
    *pp  = g_utf8_next_char (p);      \
    return g_steal_pointer (&_ret);   \
  }                                   \
  G_STMT_END

  if (IS_EOF (p))
    UNEXPECTED_EOF;

  for (; !IS_EOF (p); p = g_utf8_next_char (p))
    {
      gunichar ch            = 0;
      gboolean is_whitespace = FALSE;
      gboolean is_quotes     = FALSE;
      gboolean is_brackets   = FALSE;

      ch            = g_utf8_get_char (p);
      is_whitespace = ch == '\n' || g_unichar_isspace (ch);
      is_quotes     = ch == '"';
      is_brackets   = ch == '{' || ch == '}';

      if (is_whitespace)
        {
          if (!quoted && hit_non_space)
            RETURN_TOKEN;
        }
      else
        {
          if (is_quotes)
            {
              if (!quoted)
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "Unexpected quote");
                  return NULL;
                }
              if (hit_non_space)
                RETURN_TOKEN_ADJUST_NEXT_CHAR;
              else
                {
                  *pp = g_utf8_next_char (p);
                  if (IS_EOF (*pp))
                    UNEXPECTED_EOF;
                  hit_non_space = TRUE;
                }
            }
          else if (quoted)
            {
              if (!hit_non_space)
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "Expected quote");
                  return NULL;
                }
            }
          else if (is_brackets)
            {
              if (hit_non_space)
                RETURN_TOKEN;
              else if (allow_brackets)
                {
                  *pp = g_utf8_next_char (p);
                  if (ch == '}')
                    return g_strdup ("}");
                  else
                    return g_strdup ("{");
                }
              else
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "Expected token");
                  return NULL;
                }
            }

          if (!hit_non_space)
            {
              *pp           = p;
              hit_non_space = TRUE;
            }
        }
    }

  if (!quoted && hit_non_space)
    RETURN_TOKEN;

  UNEXPECTED_EOF;
#undef RETURN_TOKEN_ADJUST_NEXT_CHAR
#undef RETURN_TOKEN
#undef UNEXPECTED_EOF
}

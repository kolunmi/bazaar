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

#include "parser.h"

#define SINGLE_CHAR_TOKENS "{}=.;"

#define STR_DEFWIDGET     "defwidget"
#define STR_CHILD         "child"
#define STR_STATE         "state"
#define STR_DEFAULT_STATE "state@default"
#define STR_TRANSITION    "transition"
#define STR_ALLOCATE      "%allocate"
#define STR_SNAPSHOT      "%snapshot"

typedef enum
{
  TOKEN_PARSE_DEFAULT = 0,
  TOKEN_PARSE_QUOTED  = 1 << 0,
} TokenParseFlags;

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
consume_token (const char    **pp,
               const char     *single_chars,
               TokenParseFlags flags,
               GError        **error);

static char *
make_object_property_name (const char *object,
                           const char *property);

static char *
make_anon_name (guint n);

BgeWdgtSpec *
bge_wdgt_parse_string (const char *string,
                       GError    **error)
{
  gboolean result                = FALSE;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (BgeWdgtSpec) spec   = NULL;
  guint n_anon_vals              = 0;

  g_return_val_if_fail (string != NULL, FALSE);

  spec = bge_wdgt_spec_new ();

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
            local_error != NULL          \
                ? local_error->message   \
                : "???");                \
        return FALSE;                    \
      }                                  \
  }                                      \
  G_STMT_END

#define EXPECT_TOKEN(_string, _token)            \
  G_STMT_START                                   \
  {                                              \
    if (g_strcmp0 ((_string), (_token)) != 0)    \
      {                                          \
        g_set_error (                            \
            error,                               \
            G_IO_ERROR,                          \
            G_IO_ERROR_UNKNOWN,                  \
            "wdgt fmt parser error: "            \
            "expected token \"%s\", got \"%s\"", \
            (_token), (_string));                \
        return FALSE;                            \
      }                                          \
  }                                              \
  G_STMT_END

#define UNEXPECTED_TOKEN(_token)              \
  G_STMT_START                                \
  {                                           \
    g_set_error (                             \
        error,                                \
        G_IO_ERROR,                           \
        G_IO_ERROR_UNKNOWN,                   \
        "Unexpected token \"%s\"", (_token)); \
    return FALSE;                             \
  }                                           \
  G_STMT_END

  for (const char *p = string;
       p != NULL && *p != '\0';)
    {
      g_autofree char *token       = NULL;
      g_autofree char *widget_name = NULL;

#define GET_TOKEN(_token_out, _flags)            \
  G_STMT_START                                   \
  {                                              \
    g_clear_pointer ((_token_out), g_free);      \
    *(_token_out) = consume_token (              \
        &p,                                      \
        SINGLE_CHAR_TOKENS,                      \
        (_flags),                                \
        &local_error);                           \
    RETURN_ERROR_UNLESS (*(_token_out) != NULL); \
  }                                              \
  G_STMT_END

#define GET_TOKEN_EXPECT(_token_out, _flags, _expect) \
  G_STMT_START                                        \
  {                                                   \
    GET_TOKEN ((_token_out), (_flags));               \
    EXPECT_TOKEN (*(_token_out), (_expect));          \
  }                                                   \
  G_STMT_END

      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, STR_DEFWIDGET);
      GET_TOKEN (&widget_name, TOKEN_PARSE_QUOTED);
      bge_wdgt_spec_set_name (spec, widget_name);

      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
      for (;;)
        {
          GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
          if (g_strcmp0 (token, "}") == 0)
            break;
          else if (g_strcmp0 (token, STR_CHILD) == 0)
            {
              g_autofree char *name  = NULL;
              g_autofree char *gtype = NULL;

              GET_TOKEN (&name, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
              GET_TOKEN (&gtype, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");

              result = bge_wdgt_spec_add_child (spec, g_type_from_name (gtype), name, &local_error);
              RETURN_ERROR_UNLESS (result);
            }
          else if (g_strcmp0 (token, STR_STATE) == 0 ||
                   g_strcmp0 (token, STR_DEFAULT_STATE) == 0)
            {
              g_autofree char *state_name = NULL;

              GET_TOKEN (&state_name, TOKEN_PARSE_QUOTED);
              result = bge_wdgt_spec_add_state (spec, state_name, &local_error);
              RETURN_ERROR_UNLESS (result);

              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");

              for (;;)
                {
                  GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
                  if (g_strcmp0 (token, "}") == 0)
                    break;
                  else if (g_strcmp0 (token, STR_CHILD) == 0)
                    {
                      g_autofree char *child_name    = NULL;
                      g_autofree char *property_name = NULL;
                      g_autofree char *string_value  = NULL;
                      g_autofree char *dest_key      = NULL;
                      g_autofree char *src_key       = NULL;
                      GValue           value         = G_VALUE_INIT;

                      GET_TOKEN (&child_name, TOKEN_PARSE_QUOTED);
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ".");
                      GET_TOKEN (&property_name, TOKEN_PARSE_DEFAULT);
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
                      GET_TOKEN (&string_value, TOKEN_PARSE_QUOTED);
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");

                      dest_key = make_object_property_name (child_name, property_name);
                      result   = bge_wdgt_spec_add_property_value (spec, dest_key, child_name, property_name, &local_error);
                      RETURN_ERROR_UNLESS (result);

                      src_key = make_anon_name (n_anon_vals++);
                      g_value_init (&value, G_TYPE_STRING);
                      g_value_set_string (&value, string_value);
                      result = bge_wdgt_spec_add_constant_source_value (spec, src_key, &value, &local_error);
                      g_value_unset (&value);
                      RETURN_ERROR_UNLESS (result);

                      result = bge_wdgt_spec_set_value (spec, state_name, dest_key, src_key, &local_error);
                      RETURN_ERROR_UNLESS (result);
                    }
                  else
                    UNEXPECTED_TOKEN (token);
                }
            }
          else
            UNEXPECTED_TOKEN (token);
        }

      break;

#undef GET_TOKEN_EXPECT
#undef GET_TOKEN
    }

#undef UNEXPECTED_TOKEN
#undef EXPECT_TOKEN
#undef RETURN_ERROR_UNLESS

  return g_steal_pointer (&spec);
}

static char *
consume_token (const char    **pp,
               const char     *single_chars,
               TokenParseFlags flags,
               GError        **error)
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

      ch            = g_utf8_get_char (p);
      is_whitespace = ch == '\n' || g_unichar_isspace (ch);
      is_quotes     = ch == '"';

      if (is_whitespace)
        {
          if (!(flags & TOKEN_PARSE_QUOTED) &&
              hit_non_space)
            RETURN_TOKEN;
        }
      else
        {
          if (is_quotes)
            {
              if (!(flags & TOKEN_PARSE_QUOTED))
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "Unexpected quote");
                  return NULL;
                }
              else if (hit_non_space)
                RETURN_TOKEN_ADJUST_NEXT_CHAR;
              else
                {
                  *pp = g_utf8_next_char (p);
                  if (IS_EOF (*pp))
                    UNEXPECTED_EOF;
                  hit_non_space = TRUE;
                }
            }
          else if (flags & TOKEN_PARSE_QUOTED)
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
          else if (single_chars != NULL &&
                   g_utf8_strchr (single_chars, -1, ch) != NULL)
            {
              if (hit_non_space)
                RETURN_TOKEN;
              else
                {
                  char buf[16] = { 0 };

                  g_unichar_to_utf8 (ch, buf);
                  *pp = g_utf8_next_char (p);
                  return g_strdup (buf);
                }
            }

          if (!hit_non_space)
            {
              *pp           = p;
              hit_non_space = TRUE;
            }
        }
    }

  if (!(flags & TOKEN_PARSE_QUOTED) && hit_non_space)
    RETURN_TOKEN;

  UNEXPECTED_EOF;
#undef RETURN_TOKEN_ADJUST_NEXT_CHAR
#undef RETURN_TOKEN
#undef UNEXPECTED_EOF
}

static char *
make_object_property_name (const char *object,
                           const char *property)
{
  return g_strdup_printf ("prop@%s.%s", object, property);
}

static char *
make_anon_name (guint n)
{
  return g_strdup_printf ("anon@%u", n);
}

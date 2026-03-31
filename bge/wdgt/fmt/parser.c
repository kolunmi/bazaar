/* parser.c
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

#include <graphene-gobject.h>

#include "parser.h"

#define SINGLE_CHAR_TOKENS "{}()=:;,"

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

static const char *
parse_snapshot_block (const char  *p,
                      BgeWdgtSpec *spec,
                      const char  *state,
                      guint       *n_anon_vals,
                      GError     **error);

static const char *
parse_args (const char  *p,
            BgeWdgtSpec *spec,
            guint       *n_anon_vals,
            char      ***values_out,
            guint       *n_out,
            GError     **error);

static char *
parse_token_fundamental (const char  *token,
                         BgeWdgtSpec *spec,
                         guint       *n_anon_vals,
                         GError     **error);

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
        return NULL;                     \
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
        return NULL;                             \
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
    return NULL;                              \
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
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ":");
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
                  else if (g_strcmp0 (token, STR_SNAPSHOT) == 0)
                    {
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
                      p = parse_snapshot_block (p, spec, state_name, &n_anon_vals, &local_error);
                      RETURN_ERROR_UNLESS (p != NULL);
                    }
                  else
                    UNEXPECTED_TOKEN (token);
                }
            }
          else
            UNEXPECTED_TOKEN (token);
        }

      break;
    }

  return g_steal_pointer (&spec);
}

static const char *
parse_snapshot_block (const char  *p,
                      BgeWdgtSpec *spec,
                      const char  *state,
                      guint       *n_anon_vals,
                      GError     **error)
{
  g_autoptr (GError) local_error = NULL;
  gboolean         result        = FALSE;
  g_autofree char *token         = NULL;

  for (;;)
    {
      g_autofree char         *action = NULL;
      BgeWdgtSnapshotInstrKind kind   = 0;
      g_autofree char         *instr  = NULL;
      guint                    n_args = 0;
      g_auto (GStrv) args             = NULL;

      GET_TOKEN (&action, TOKEN_PARSE_DEFAULT);
      if (g_strcmp0 (action, "}") == 0)
        break;
      else if (g_strcmp0 (action, "save") == 0)
        kind = BGE_WDGT_SNAPSHOT_INSTR_SAVE;
      else if (g_strcmp0 (action, "with") == 0)
        kind = BGE_WDGT_SNAPSHOT_INSTR_PUSH;
      else if (g_strcmp0 (action, "add") == 0)
        kind = BGE_WDGT_SNAPSHOT_INSTR_APPEND;
      else if (g_strcmp0 (action, "move") == 0)
        kind = BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM;
      else
        UNEXPECTED_TOKEN (action);

      if (kind != BGE_WDGT_SNAPSHOT_INSTR_SAVE)
        {
          GET_TOKEN (&instr, TOKEN_PARSE_DEFAULT);
          p = parse_args (p, spec, n_anon_vals, &args, &n_args, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          result = bge_wdgt_spec_append_snapshot_instr (
              spec, state, kind,
              instr, (const char *const *) args, n_args, &local_error);
          RETURN_ERROR_UNLESS (result);
        }

      if (kind == BGE_WDGT_SNAPSHOT_INSTR_APPEND)
        GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");
      else
        {
          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
          p = parse_snapshot_block (p, spec, state, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (result);
        }
    }

  return p;
}

static const char *
parse_args (const char  *p,
            BgeWdgtSpec *spec,
            guint       *n_anon_vals,
            char      ***values_out,
            guint       *n_out,
            GError     **error)
{
  g_autoptr (GError) local_error   = NULL;
  gboolean         result          = FALSE;
  g_autofree char *token           = NULL;
  guint            n_args          = 0;
  g_autoptr (GStrvBuilder) builder = NULL;

  builder = g_strv_builder_new ();

  GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "(");
  for (gboolean need_comma = FALSE;;)
    {
      GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
      if (g_strcmp0 (token, ")") == 0)
        break;
      else if (need_comma)
        {
          if (g_strcmp0 (token, ",") == 0)
            {
              need_comma = FALSE;
              continue;
            }
          else
            {
              g_set_error (
                  error,
                  G_IO_ERROR,
                  G_IO_ERROR_UNKNOWN,
                  "Arguments must be comma-separated");
              return NULL;
            }
        }
      else if (g_strcmp0 (token, "#point") == 0)
        {
          g_auto (GStrv) point_args     = NULL;
          guint            n_point_args = 0;
          GType            point_type   = 0;
          g_autofree char *point_key    = NULL;

          p = parse_args (p, spec, n_anon_vals, &point_args, &n_point_args, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          switch (n_point_args)
            {
            case 2:
              point_type = GRAPHENE_TYPE_POINT;
              break;
            case 3:
              point_type = GRAPHENE_TYPE_POINT3D;
              break;
            default:
              g_set_error (
                  error,
                  G_IO_ERROR,
                  G_IO_ERROR_UNKNOWN,
                  "#point() specifier can have 2 or 3 arguments, got %u",
                  n_point_args);
              return NULL;
            }

          point_key = make_anon_name ((*n_anon_vals)++);
          result    = bge_wdgt_spec_add_component_source_value (
              spec, point_key, point_type, (const char *const *) point_args, n_point_args, &local_error);
          RETURN_ERROR_UNLESS (result);

          g_strv_builder_take (builder, g_steal_pointer (&point_key));
          n_args++;

          need_comma = TRUE;
        }
      else
        {
          g_autofree char *parsed = NULL;

          parsed = parse_token_fundamental (token, spec, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (parsed != NULL);
          g_strv_builder_take (builder, g_steal_pointer (&parsed));
          n_args++;

          need_comma = TRUE;
        }
    }

  if (n_out != NULL)
    *n_out = n_args;
  if (values_out != NULL)
    *values_out = g_strv_builder_end (builder);

  return p;
}

#undef GET_TOKEN_EXPECT
#undef GET_TOKEN
#undef UNEXPECTED_TOKEN
#undef EXPECT_TOKEN
#undef RETURN_ERROR_UNLESS

static char *
parse_token_fundamental (const char  *token,
                         BgeWdgtSpec *spec,
                         guint       *n_anon_vals,
                         GError     **error)
{
  g_autoptr (GError) local_error = NULL;
  gboolean         result        = FALSE;
  gunichar         ch            = 0;
  GValue           value         = { 0 };
  g_autofree char *key           = NULL;

  ch = g_utf8_get_char (token);
  if (ch == '-' ||
      g_unichar_isdigit (ch))
    {
      gboolean is_double           = FALSE;
      g_autoptr (GVariant) variant = NULL;

      is_double = g_utf8_strchr (token, -1, '.') != NULL;
      if (is_double)
        variant = g_variant_parse (G_VARIANT_TYPE_DOUBLE, token,
                                   NULL, NULL, &local_error);
      else
        variant = g_variant_parse (G_VARIANT_TYPE_INT64, token,
                                   NULL, NULL, &local_error);

      if (variant == NULL)
        {
          g_set_error (
              error,
              G_IO_ERROR,
              G_IO_ERROR_UNKNOWN,
              "Unable to parse number value '%s': %s",
              token, local_error->message);
          return FALSE;
        }

      if (is_double)
        g_value_set_float (g_value_init (&value, G_TYPE_FLOAT),
                           g_variant_get_double (variant));
      else
        g_value_set_int64 (g_value_init (&value, G_TYPE_INT64),
                           g_variant_get_int64 (variant));
    }
  else if (g_str_has_prefix (token, "rgba:"))
    {
      const char *color = NULL;
      GdkRGBA     rgba  = { 0 };

      color  = token + strlen ("rgba:");
      result = gdk_rgba_parse (&rgba, color);
      if (!result)
        {
          g_set_error (
              error,
              G_IO_ERROR,
              G_IO_ERROR_UNKNOWN,
              "Unable to parse color '%s'",
              color);
          return FALSE;
        }

      g_value_set_boxed (g_value_init (&value, GDK_TYPE_RGBA), &rgba);
    }
  else
    return g_strdup (token);

  if (key == NULL)
    key = make_anon_name ((*n_anon_vals)++);
  result = bge_wdgt_spec_add_constant_source_value (
      spec, key, &value, error);
  g_value_unset (&value);
  if (!result)
    return NULL;

  return g_steal_pointer (&key);
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

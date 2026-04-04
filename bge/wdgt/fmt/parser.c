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

#include "bge-marshalers.h"
#include "parser.h"
#include "util.h"

#define IS_EOF(_p) ((_p) == NULL || *(_p) == '\0')

#define SINGLE_CHAR_TOKENS      "{}()=:;,"
#define EVAL_SINGLE_CHAR_TOKENS "#(),+-*/^%"

#define STR_DEFWIDGET     "defwidget"
#define STR_CHILD         "child"
#define STR_VARIABLE      "var"
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

typedef enum
{
  OPERATOR_ADD = 0,
  OPERATOR_SUBTRACT,
  OPERATOR_MULTIPLY,
  OPERATOR_DIVIDE,
  OPERATOR_MODULUS,
  OPERATOR_POWER,
} Operator;

static const int operator_precedence[] = {
  [OPERATOR_ADD]      = 0,
  [OPERATOR_SUBTRACT] = 0,
  [OPERATOR_MULTIPLY] = 1,
  [OPERATOR_DIVIDE]   = 1,
  [OPERATOR_MODULUS]  = 1,
  [OPERATOR_POWER]    = 2,
};

typedef struct
{
  Operator op;
  guint    pos;
} EvalOperator;

BGE_DEFINE_DATA (
    eval_closure,
    EvalClosure,
    {
      GArray  *ops;
      gdouble *workbuf0;
      gdouble *workbuf1;
    },
    BGE_RELEASE_DATA (ops, g_array_unref);
    BGE_RELEASE_DATA (workbuf0, g_free);
    BGE_RELEASE_DATA (workbuf1, g_free));

static const char *
parse_snapshot_block (const char  *p,
                      BgeWdgtSpec *spec,
                      const char  *state,
                      guint       *n_anon_vals,
                      GError     **error);

static const char *
parse_eval (const char  *p,
            BgeWdgtSpec *spec,
            const char  *state,
            guint       *n_anon_vals,
            char       **value_out,
            GError     **error);

static const char *
parse_args (const char  *p,
            BgeWdgtSpec *spec,
            const char  *state,
            guint       *n_anon_vals,
            char      ***values_out,
            guint       *n_out,
            gboolean     is_right_assignment,
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
               gboolean       *was_quoted,
               GError        **error);

static gdouble
eval_closure (gpointer         this,
              guint            n_param_values,
              const GValue    *param_values,
              EvalClosureData *data);

static char *
make_object_property_name (const char *object,
                           const char *property);

static char *
make_anon_name (guint n);

static gint
cmp_operator (EvalOperator *a,
              EvalOperator *b);

static void
_marshal_DOUBLE__ARGS_DIRECT (GClosure                *closure,
                              GValue                  *return_value,
                              guint                    n_param_values,
                              const GValue            *param_values,
                              gpointer invocation_hint G_GNUC_UNUSED,
                              gpointer                 marshal_data);

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

#define GET_TOKEN_FULL(_token_out, _flags, _single_chars, _was_quoted) \
  G_STMT_START                                                         \
  {                                                                    \
    g_clear_pointer ((_token_out), g_free);                            \
    *(_token_out) = consume_token (                                    \
        &p,                                                            \
        (_single_chars),                                               \
        (_flags),                                                      \
        (_was_quoted),                                                 \
        &local_error);                                                 \
    RETURN_ERROR_UNLESS (*(_token_out) != NULL);                       \
  }                                                                    \
  G_STMT_END

#define GET_TOKEN(_token_out, _flags) \
  GET_TOKEN_FULL (_token_out, _flags, SINGLE_CHAR_TOKENS, NULL)

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

              GET_TOKEN (&name, TOKEN_PARSE_DEFAULT);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ":");
              GET_TOKEN (&gtype, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");

              result = bge_wdgt_spec_add_child (
                  spec, g_type_from_name (gtype), name, &local_error);
              RETURN_ERROR_UNLESS (result);
            }
          else if (g_strcmp0 (token, STR_VARIABLE) == 0)
            {
              g_autofree char *name  = NULL;
              g_autofree char *gtype = NULL;

              GET_TOKEN (&name, TOKEN_PARSE_DEFAULT);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ":");
              GET_TOKEN (&gtype, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");

              result = bge_wdgt_spec_add_variable_value (
                  spec, g_type_from_name (gtype), name, &local_error);
              RETURN_ERROR_UNLESS (result);
            }
          else if (g_strcmp0 (token, STR_STATE) == 0 ||
                   g_strcmp0 (token, STR_DEFAULT_STATE) == 0)
            {
              g_autofree char *state_name = NULL;

              GET_TOKEN (&state_name, TOKEN_PARSE_QUOTED);
              result = bge_wdgt_spec_add_state (
                  spec,
                  state_name,
                  g_strcmp0 (token, STR_DEFAULT_STATE) == 0,
                  &local_error);
              RETURN_ERROR_UNLESS (result);

              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
              GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");

              for (;;)
                {
                  GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
                  if (g_strcmp0 (token, "}") == 0)
                    break;
                  else if (g_strcmp0 (token, STR_SNAPSHOT) == 0)
                    {
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
                      GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
                      p = parse_snapshot_block (p, spec, state_name, &n_anon_vals, &local_error);
                      RETURN_ERROR_UNLESS (p != NULL);
                    }
                  else
                    {
                      g_autofree char *dest_name     = NULL;
                      g_autofree char *property_name = NULL;
                      g_autofree char *property_key  = NULL;
                      guint            n_src_values  = 0;
                      g_auto (GStrv) src_values      = NULL;

                      dest_name = g_steal_pointer (&token);

                      GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
                      if (g_strcmp0 (token, ":") == 0)
                        {
                          GET_TOKEN (&property_name, TOKEN_PARSE_DEFAULT);
                          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");
                        }
                      else if (g_strcmp0 (token, "=") != 0)
                        UNEXPECTED_TOKEN (token);

                      if (property_name != NULL)
                        {
                          property_key = make_object_property_name (dest_name, property_name);
                          result       = bge_wdgt_spec_add_property_value (spec, property_key, dest_name, property_name, &local_error);
                          RETURN_ERROR_UNLESS (result);
                        }

                      p = parse_args (p, spec, state_name, &n_anon_vals, &src_values,
                                      &n_src_values, TRUE, &local_error);
                      RETURN_ERROR_UNLESS (p != NULL);
                      if (n_src_values != 1)
                        {
                          g_set_error (
                              error,
                              G_IO_ERROR,
                              G_IO_ERROR_UNKNOWN,
                              "Right assignment needs a single argument");
                          return NULL;
                        }

                      result = bge_wdgt_spec_set_value (
                          spec,
                          state_name,
                          property_key != NULL
                              ? property_key
                              : dest_name,
                          src_values[0],
                          &local_error);
                      RETURN_ERROR_UNLESS (result);
                    }
                }
            }
          else
            UNEXPECTED_TOKEN (token);
        }

      break;
    }

  bge_wdgt_spec_mark_ready (spec);
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

      if (kind == BGE_WDGT_SNAPSHOT_INSTR_SAVE)
        {
          result = bge_wdgt_spec_append_snapshot_instr (
              spec, state, BGE_WDGT_SNAPSHOT_INSTR_SAVE,
              "save", NULL, 0, &local_error);
          RETURN_ERROR_UNLESS (result);
        }
      else
        {
          GET_TOKEN (&instr, TOKEN_PARSE_DEFAULT);
          p = parse_args (p, spec, state, n_anon_vals, &args,
                          &n_args, FALSE, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          result = bge_wdgt_spec_append_snapshot_instr (
              spec, state, kind,
              instr, (const char *const *) args, n_args, &local_error);
          RETURN_ERROR_UNLESS (result);
        }

      if (kind == BGE_WDGT_SNAPSHOT_INSTR_APPEND ||
          kind == BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM)
        GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ";");
      else if (kind == BGE_WDGT_SNAPSHOT_INSTR_PUSH)
        {
          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
          p = parse_snapshot_block (p, spec, state, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          result = bge_wdgt_spec_append_snapshot_instr (
              spec, state, BGE_WDGT_SNAPSHOT_INSTR_POP,
              "pop", NULL, 0, &local_error);
          RETURN_ERROR_UNLESS (result);
        }
      else if (kind == BGE_WDGT_SNAPSHOT_INSTR_SAVE)
        {
          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "{");
          p = parse_snapshot_block (p, spec, state, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          result = bge_wdgt_spec_append_snapshot_instr (
              spec, state, BGE_WDGT_SNAPSHOT_INSTR_RESTORE,
              "restore", NULL, 0, &local_error);
          RETURN_ERROR_UNLESS (result);
        }
    }

  return p;
}

static gdouble
sin_closure (gpointer this,
             gdouble  x,
             gpointer data)
{
  return sin (x);
}

static gdouble
cos_closure (gpointer this,
             gdouble  x,
             gpointer data)
{
  return cos (x);
}

static const char *
parse_eval (const char  *p,
            BgeWdgtSpec *spec,
            const char  *state,
            guint       *n_anon_vals,
            char       **value_out,
            GError     **error)
{
  g_autoptr (GError) local_error   = NULL;
  gboolean         result          = FALSE;
  g_autofree char *token           = NULL;
  g_autoptr (GPtrArray) values     = NULL;
  g_autoptr (GArray) ops           = NULL;
  g_autoptr (GArray) value_types   = NULL;
  g_autofree char *key             = NULL;
  g_autoptr (EvalClosureData) data = NULL;

  values      = g_ptr_array_new_with_free_func (g_free);
  ops         = g_array_new (FALSE, FALSE, sizeof (EvalOperator));
  value_types = g_array_new (FALSE, FALSE, sizeof (GType));

#define GET_TOKEN_EVAL(_token_out, _flags) \
  GET_TOKEN_FULL (_token_out, _flags, EVAL_SINGLE_CHAR_TOKENS, NULL)

#define GET_TOKEN_EVAL_EXPECT(_token_out, _flags, _expect) \
  G_STMT_START                                             \
  {                                                        \
    GET_TOKEN_EVAL ((_token_out), (_flags));               \
    EXPECT_TOKEN (*(_token_out), (_expect));               \
  }                                                        \
  G_STMT_END

  for (;;)
    {
      g_autofree char *value          = NULL;
      Operator         op             = -1;
      gboolean         expected_value = FALSE;

      GET_TOKEN_EVAL (&token, TOKEN_PARSE_DEFAULT);
      if (g_strcmp0 (token, ")") == 0)
        break;
      else if (g_strcmp0 (token, "(") == 0)
        {
          p = parse_eval (p, spec, state, n_anon_vals, &value, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);
        }
      else if (g_strcmp0 (token, "#") == 0)
        {
          g_auto (GStrv) escape_args = NULL;
          guint n_escape_args        = 0;

          p = parse_args (p, spec, state, n_anon_vals, &escape_args,
                          &n_escape_args, FALSE, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);
          if (n_escape_args != 1)
            {
              g_set_error (
                  error,
                  G_IO_ERROR,
                  G_IO_ERROR_UNKNOWN,
                  "evaluation escape "
                  "needs a single argument");
              return NULL;
            }

          value = g_strdup (escape_args[0]);
        }
      else if (g_strcmp0 (token, "sin") == 0 ||
               g_strcmp0 (token, "cos") == 0)
        {
          GCallback        cb            = NULL;
          g_autofree char *arg           = NULL;
          g_autofree char *math_func_key = NULL;

          if (g_strcmp0 (token, "sin") == 0)
            cb = G_CALLBACK (sin_closure);
          else if (g_strcmp0 (token, "cos") == 0)
            cb = G_CALLBACK (cos_closure);

          GET_TOKEN_EVAL_EXPECT (&token, TOKEN_PARSE_DEFAULT, "(");
          p = parse_eval (p, spec, state, n_anon_vals, &arg, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          math_func_key = make_anon_name ((*n_anon_vals)++);
          result        = bge_wdgt_spec_add_cclosure_source_value (
              spec,
              math_func_key,
              G_TYPE_DOUBLE,
              bge_marshal_DOUBLE__DOUBLE,
              cb,
              (const char *const[]){ arg },
              (GType[]){ G_TYPE_DOUBLE },
              1,
              NULL, NULL,
              &local_error);
          RETURN_ERROR_UNLESS (result);

          value = g_steal_pointer (&math_func_key);
        }
      else if (g_strcmp0 (token, "+") == 0)
        op = OPERATOR_ADD;
      else if (g_strcmp0 (token, "-") == 0)
        op = OPERATOR_SUBTRACT;
      else if (g_strcmp0 (token, "*") == 0)
        op = OPERATOR_MULTIPLY;
      else if (g_strcmp0 (token, "/") == 0)
        op = OPERATOR_DIVIDE;
      else if (g_strcmp0 (token, "%") == 0)
        op = OPERATOR_MODULUS;
      else if (g_strcmp0 (token, "^") == 0)
        op = OPERATOR_POWER;
      else
        {
          value = parse_token_fundamental (
              token, spec, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (value != NULL);
        }

      expected_value = values->len == ops->len;
      if (value != NULL)
        {
          GType type_double = G_TYPE_DOUBLE;

          if (!expected_value)
            {
              g_set_error (
                  error,
                  G_IO_ERROR,
                  G_IO_ERROR_UNKNOWN,
                  "Expected operator, got \"%s\"", token);
              return NULL;
            }
          g_ptr_array_add (values, g_steal_pointer (&value));
          g_array_append_val (value_types, type_double);
        }
      else if (op >= 0)
        {
          EvalOperator append = { 0 };

          if (expected_value)
            {
              g_set_error (
                  error,
                  G_IO_ERROR,
                  G_IO_ERROR_UNKNOWN,
                  "Expected value, got \"%s\"", token);
              return NULL;
            }

          append.op  = op;
          append.pos = ops->len;
          g_array_append_val (ops, append);
        }
    }

  if (values->len == 0)
    {
      g_set_error (
          error,
          G_IO_ERROR,
          G_IO_ERROR_UNKNOWN,
          "Empty evaluation block");
      return NULL;
    }
  if (ops->len != values->len - 1)
    {
      g_set_error (
          error,
          G_IO_ERROR,
          G_IO_ERROR_UNKNOWN,
          "Invalid syntax in evaluation block");
      return NULL;
    }

  if (values->len == 1)
    {
      *value_out = g_ptr_array_steal_index (values, 0);
      return p;
    }

  g_array_sort (ops, (GCompareFunc) cmp_operator);

  data           = eval_closure_data_new ();
  data->ops      = g_array_ref (ops);
  data->workbuf0 = g_malloc0_n (values->len, sizeof (gdouble));
  data->workbuf1 = g_malloc0_n (values->len, sizeof (gdouble));

  key    = make_anon_name ((*n_anon_vals)++);
  result = bge_wdgt_spec_add_cclosure_source_value (
      spec, key, G_TYPE_DOUBLE,
      _marshal_DOUBLE__ARGS_DIRECT,
      G_CALLBACK (eval_closure),
      (const char *const *) values->pdata,
      (const GType *) (void *) value_types->data,
      values->len,
      eval_closure_data_ref (data),
      eval_closure_data_unref,
      &local_error);
  RETURN_ERROR_UNLESS (result);

  *value_out = g_steal_pointer (&key);
  return p;
}

static const char *
parse_args (const char  *p,
            BgeWdgtSpec *spec,
            const char  *state,
            guint       *n_anon_vals,
            char      ***values_out,
            guint       *n_out,
            gboolean     is_right_assignment,
            GError     **error)
{
  g_autoptr (GError) local_error   = NULL;
  gboolean         result          = FALSE;
  g_autofree char *token           = NULL;
  guint            n_args          = 0;
  g_autoptr (GStrvBuilder) builder = NULL;

  builder = g_strv_builder_new ();

  if (!is_right_assignment)
    GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "(");
  for (gboolean need_comma = FALSE,
                get_token  = TRUE,
                was_quoted = FALSE;
       ;)
    {
      if (get_token)
        GET_TOKEN_FULL (&token, TOKEN_PARSE_DEFAULT, SINGLE_CHAR_TOKENS, &was_quoted);
      get_token = TRUE;
      if (was_quoted)
        {
          g_autofree char *tmp_token = NULL;
          g_autofree char *key       = NULL;
          GValue           value     = { 0 };

          g_value_set_string (g_value_init (&value, G_TYPE_STRING), token);

          key    = make_anon_name ((*n_anon_vals)++);
          result = bge_wdgt_spec_add_constant_source_value (
              spec, key, &value, &local_error);
          g_value_unset (&value);
          RETURN_ERROR_UNLESS (result);

          g_strv_builder_take (builder, g_steal_pointer (&key));
          n_args++;

          need_comma = TRUE;
        }
      else if ((is_right_assignment && g_strcmp0 (token, ";") == 0) ||
               (!is_right_assignment && g_strcmp0 (token, ")") == 0))
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
      else if (g_strcmp0 (token, "#eval") == 0)
        {
          g_autofree char *key = NULL;

          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "(");
          p = parse_eval (p, spec, state, n_anon_vals, &key, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          g_strv_builder_take (builder, g_steal_pointer (&key));
          n_args++;

          need_comma = TRUE;
        }
      else if (g_strcmp0 (token, "#i") == 0 ||
               g_strcmp0 (token, "#e") == 0 ||
               g_strcmp0 (token, "#enum") == 0 ||
               g_strcmp0 (token, "#rgba") == 0)
        {
          g_autofree char *tmp_token = NULL;
          g_autofree char *key       = NULL;
          GValue           value     = { 0 };

          if (g_strcmp0 (token, "#i") == 0)
            {
              g_autofree char    *int_type_name = NULL;
              GType               int_type      = 0;
              const GVariantType *variant_type  = NULL;
              g_autofree char    *int_string    = NULL;
              g_autoptr (GVariant) variant      = NULL;

              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, ":");
              GET_TOKEN (&int_type_name, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, "(");

              int_type = g_type_from_name (int_type_name);
              if (int_type == G_TYPE_INT)
                variant_type = G_VARIANT_TYPE_INT32;
              else if (int_type == G_TYPE_INT64)
                variant_type = G_VARIANT_TYPE_INT64;
              else if (int_type == G_TYPE_UINT)
                variant_type = G_VARIANT_TYPE_UINT32;
              else if (int_type == G_TYPE_UINT64)
                variant_type = G_VARIANT_TYPE_UINT64;
              else
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "'%s' not found to be a valid int type",
                      int_type_name);
                  return NULL;
                }

              GET_TOKEN (&int_string, TOKEN_PARSE_DEFAULT);
              variant = g_variant_parse (variant_type, int_string, NULL, NULL, &local_error);
              RETURN_ERROR_UNLESS (variant != NULL);

              if (int_type == G_TYPE_INT)
                g_value_set_int (g_value_init (&value, G_TYPE_INT),
                                 g_variant_get_int32 (variant));
              else if (int_type == G_TYPE_INT64)
                g_value_set_int64 (g_value_init (&value, G_TYPE_INT64),
                                   g_variant_get_int64 (variant));
              else if (int_type == G_TYPE_UINT)
                g_value_set_uint (g_value_init (&value, G_TYPE_UINT),
                                  g_variant_get_uint32 (variant));
              else if (int_type == G_TYPE_UINT64)
                g_value_set_uint64 (g_value_init (&value, G_TYPE_UINT64),
                                    g_variant_get_uint64 (variant));

              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, ")");
            }
          else if (g_strcmp0 (token, "#e") == 0 ||
                   g_strcmp0 (token, "#enum") == 0)
            {
              g_autofree char *enum_type_name   = NULL;
              g_autofree char *enum_nick        = NULL;
              GType            enum_type        = 0;
              g_autoptr (GEnumClass) enum_class = NULL;
              GEnumValue *enum_value            = NULL;

              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, ":");
              GET_TOKEN (&enum_type_name, TOKEN_PARSE_QUOTED);
              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, "(");

              enum_type = g_type_from_name (enum_type_name);
              if (!g_type_is_a (enum_type, G_TYPE_ENUM))
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "'%s' not found to be an enum type",
                      enum_type_name);
                  return NULL;
                }

              GET_TOKEN (&enum_nick, TOKEN_PARSE_DEFAULT);

              enum_class = g_type_class_ref (enum_type);
              enum_value = g_enum_get_value_by_nick (enum_class, enum_nick);
              if (enum_value == NULL)
                enum_value = g_enum_get_value_by_name (enum_class, enum_nick);
              if (enum_value == NULL)
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "'%s' not found in enum type %s",
                      enum_nick, enum_type_name);
                  return NULL;
                }

              g_value_set_enum (g_value_init (&value, enum_type), enum_value->value);
              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, ")");
            }
          else if (g_strcmp0 (token, "#rgba") == 0)
            {
              g_autofree char *string = NULL;
              GdkRGBA          rgba   = { 0 };

              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, "(");
              GET_TOKEN (&string, TOKEN_PARSE_QUOTED);

              result = gdk_rgba_parse (&rgba, string);
              if (!result)
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "#color() specifier failed to "
                      "parse color from string");
                  return NULL;
                }
              g_value_set_boxed (g_value_init (&value, GDK_TYPE_RGBA), &rgba);
              GET_TOKEN_EXPECT (&tmp_token, TOKEN_PARSE_DEFAULT, ")");
            }

          key    = make_anon_name ((*n_anon_vals)++);
          result = bge_wdgt_spec_add_constant_source_value (
              spec, key, &value, &local_error);
          g_value_unset (&value);
          RETURN_ERROR_UNLESS (result);

          g_strv_builder_take (builder, g_steal_pointer (&key));
          n_args++;

          need_comma = TRUE;
        }
      else if (g_strcmp0 (token, "#point") == 0 ||
               g_strcmp0 (token, "#size") == 0 ||
               g_strcmp0 (token, "#rect") == 0)
        {
          g_auto (GStrv) component_args     = NULL;
          guint            n_component_args = 0;
          GType            component_type   = 0;
          g_autofree char *component_key    = NULL;

          p = parse_args (p, spec, state, n_anon_vals, &component_args,
                          &n_component_args, FALSE, &local_error);
          RETURN_ERROR_UNLESS (p != NULL);

          if (g_strcmp0 (token, "#point") == 0)
            {
              switch (n_component_args)
                {
                case 2:
                  component_type = GRAPHENE_TYPE_POINT;
                  break;
                case 3:
                  component_type = GRAPHENE_TYPE_POINT3D;
                  break;
                default:
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "#point() specifier can have 2 or 3 arguments, got %u",
                      n_component_args);
                  return NULL;
                }
            }
          else if (g_strcmp0 (token, "#size") == 0)
            {
              switch (n_component_args)
                {
                case 2:
                  component_type = GRAPHENE_TYPE_SIZE;
                  break;
                default:
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "#size() specifier can have 2 arguments, got %u",
                      n_component_args);
                  return NULL;
                }
            }
          else if (g_strcmp0 (token, "#rect") == 0)
            {
              switch (n_component_args)
                {
                case 4:
                  component_type = GRAPHENE_TYPE_RECT;
                  break;
                default:
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "#rect() specifier can have 4 arguments, got %u",
                      n_component_args);
                  return NULL;
                }
            }

          component_key = make_anon_name ((*n_anon_vals)++);
          result        = bge_wdgt_spec_add_component_source_value (
              spec, component_key, component_type,
              (const char *const *) component_args,
              n_component_args, &local_error);
          RETURN_ERROR_UNLESS (result);

          g_strv_builder_take (builder, g_steal_pointer (&component_key));
          n_args++;

          need_comma = TRUE;
        }
      else if (g_strcmp0 (token, "#o") == 0 ||
               g_strcmp0 (token, "#obj") == 0 ||
               g_strcmp0 (token, "#object") == 0)
        {
          g_autofree char *object_type = NULL;
          g_autofree char *object_name = NULL;

          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, ":");
          GET_TOKEN (&object_type, TOKEN_PARSE_QUOTED);

          object_name = make_anon_name ((*n_anon_vals)++);
          result      = bge_wdgt_spec_add_instance_source_value (
              spec, object_name, g_type_from_name (object_type), &local_error);
          RETURN_ERROR_UNLESS (result);

          GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "(");
          for (;;)
            {
              g_autofree char *property_name   = NULL;
              gboolean         is_widget_child = FALSE;
              g_autofree char *key             = NULL;
              g_auto (GStrv) value_args        = NULL;
              guint n_value_args               = 0;

              GET_TOKEN (&property_name, TOKEN_PARSE_DEFAULT);
              if (g_strcmp0 (property_name, ")") == 0)
                break;

              is_widget_child = g_strcmp0 (property_name, "%child") == 0;
              if (is_widget_child)
                {
                  key    = make_anon_name ((*n_anon_vals)++);
                  result = bge_wdgt_spec_add_child_value (
                      spec, key, object_name, NULL, &local_error);
                  RETURN_ERROR_UNLESS (result);
                }
              else
                {
                  GET_TOKEN_EXPECT (&token, TOKEN_PARSE_DEFAULT, "=");

                  key    = make_object_property_name (object_name, property_name);
                  result = bge_wdgt_spec_add_property_value (
                      spec, key, object_name, property_name, &local_error);
                  RETURN_ERROR_UNLESS (result);
                }

              p = parse_args (p, spec, state, n_anon_vals, &value_args,
                              &n_value_args, TRUE, &local_error);
              RETURN_ERROR_UNLESS (p != NULL);

              if (n_value_args != 1)
                {
                  g_set_error (
                      error,
                      G_IO_ERROR,
                      G_IO_ERROR_UNKNOWN,
                      "property/child assignment "
                      "needs a single argument");
                  return NULL;
                }

              result = bge_wdgt_spec_set_value (spec, state, key,
                                                value_args[0], &local_error);
              RETURN_ERROR_UNLESS (result);
            }

          g_strv_builder_take (builder, g_steal_pointer (&object_name));
          n_args++;

          need_comma = TRUE;
        }
      else
        {
          g_autofree char *parsed = NULL;

          parsed = parse_token_fundamental (token, spec, n_anon_vals, &local_error);
          RETURN_ERROR_UNLESS (parsed != NULL);

          for (;;)
            {
              GET_TOKEN (&token, TOKEN_PARSE_DEFAULT);
              if (g_strcmp0 (token, ":") == 0)
                {
                  g_autofree char *property = NULL;
                  g_autofree char *name     = NULL;

                  GET_TOKEN (&property, TOKEN_PARSE_DEFAULT);
                  name = make_object_property_name (parsed, property);

                  result = bge_wdgt_spec_add_property_value (
                      spec, name, parsed, property, &local_error);
                  RETURN_ERROR_UNLESS (result);

                  g_clear_pointer (&parsed, g_free);
                  parsed = g_steal_pointer (&name);
                }
              else
                break;
            }
          get_token = FALSE;

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
        variant = g_variant_parse (G_VARIANT_TYPE_INT32, token,
                                   NULL, NULL, &local_error);

      if (variant == NULL)
        {
          g_set_error (
              error,
              G_IO_ERROR,
              G_IO_ERROR_UNKNOWN,
              "Unable to parse number value '%s': %s",
              token, local_error->message);
          return NULL;
        }

      if (is_double)
        g_value_set_double (g_value_init (&value, G_TYPE_DOUBLE),
                            g_variant_get_double (variant));
      else
        g_value_set_int (g_value_init (&value, G_TYPE_INT),
                         g_variant_get_int32 (variant));
    }
  else if (g_strcmp0 (token, "true") == 0)
    g_value_set_boolean (g_value_init (&value, G_TYPE_BOOLEAN),
                         TRUE);
  else if (g_strcmp0 (token, "false") == 0)
    g_value_set_boolean (g_value_init (&value, G_TYPE_BOOLEAN),
                         FALSE);
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
               gboolean       *was_quoted,
               GError        **error)
{
  const char *p             = *pp;
  gboolean    hit_non_space = FALSE;

  if (was_quoted != NULL)
    *was_quoted = FALSE;

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
              if (hit_non_space)
                {
                  if (was_quoted != NULL)
                    *was_quoted = TRUE;

                  if (flags & TOKEN_PARSE_QUOTED)
                    RETURN_TOKEN_ADJUST_NEXT_CHAR;
                  else
                    RETURN_TOKEN;
                }
              else
                {
                  flags |= TOKEN_PARSE_QUOTED;
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

static gdouble
eval_closure (gpointer         this,
              guint            n_param_values,
              const GValue    *param_values,
              EvalClosureData *data)
{
  GArray  *ops      = data->ops;
  gdouble *workbuf0 = data->workbuf0;
  gdouble *workbuf1 = data->workbuf1;
  gdouble  result   = 0.0;

  for (guint i = 0; i < n_param_values; i++)
    {
      workbuf0[i] = g_value_get_double (&param_values[i]);
      workbuf1[i] = 1.0;
    }

  for (guint i = 0; i < ops->len; i++)
    {
      EvalOperator *op        = NULL;
      guint         left_idx  = 0;
      guint         right_idx = 0;
      gdouble       left      = 0.0;
      gdouble       right     = 0.0;

      op = &g_array_index (ops, EvalOperator, i);

      left_idx = op->pos;
      while (workbuf1[left_idx] < 0.0)
        {
          left_idx--;
        }

      right_idx = op->pos + 1;
      while (workbuf1[right_idx] < 0.0)
        {
          right_idx++;
        }

      left  = workbuf0[left_idx];
      right = workbuf0[right_idx];

      switch (op->op)
        {
        case OPERATOR_ADD:
          result = left + right;
          break;
        case OPERATOR_SUBTRACT:
          result = left - right;
          break;
        case OPERATOR_MULTIPLY:
          result = left * right;
          break;
        case OPERATOR_DIVIDE:
          result = left / right;
          break;
        case OPERATOR_MODULUS:
          result = fmod (left, right);
          break;
        case OPERATOR_POWER:
          result = pow (left, right);
          break;
        default:
          g_assert_not_reached ();
        }

      workbuf0[left_idx]  = result;
      workbuf1[left_idx]  = 1.0;
      workbuf1[right_idx] = -1.0;
    }

  return result;
}

static char *
make_object_property_name (const char *object,
                           const char *property)
{
  return g_strdup_printf ("prop@(%s).%s", object, property);
}

static char *
make_anon_name (guint n)
{
  return g_strdup_printf ("anon@%u", n);
}

static gint
cmp_operator (EvalOperator *a,
              EvalOperator *b)
{
  int a_prec = 0;
  int b_prec = 0;

  a_prec = operator_precedence[a->op];
  b_prec = operator_precedence[b->op];

  return a_prec > b_prec ? -1 : 1;
}

static void
_marshal_DOUBLE__ARGS_DIRECT (GClosure                *closure,
                              GValue                  *return_value,
                              guint                    n_param_values,
                              const GValue            *param_values,
                              gpointer invocation_hint G_GNUC_UNUSED,
                              gpointer                 marshal_data)
{
  typedef gdouble (*GMarshalFunc_DOUBLE__ARGS_DIRECT) (gpointer      data1,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      data2);
  GCClosure                       *cc = (GCClosure *) closure;
  gpointer                         data1, data2;
  GMarshalFunc_DOUBLE__ARGS_DIRECT callback;
  gdouble                          v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values >= 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_DOUBLE__ARGS_DIRECT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       n_param_values - 1,
                       param_values + 1,
                       data2);

  g_value_set_double (return_value, v_return);
}

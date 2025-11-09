/* bz-yaml-parser.c
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

#define G_LOG_DOMAIN "BAZAAR::YAML"

#include "config.h"

#include <gtk/gtk.h>
#include <xmlb.h>
#include <yaml.h>

#include "bz-util.h"
#include "bz-yaml-parser.h"

/* clang-format off */
G_DEFINE_QUARK (bz-yaml-error-quark, bz_yaml_error);
/* clang-format on */

static void
deinit_schema_node (gpointer data);

BZ_DEFINE_DATA (
    schema_node,
    SchemaNode,
    {
      int kind;
      union
      {
        struct
        {
          char *vtype;
        } scalar;
        struct
        {
          GType       type;
          GHashTable *type_hints;
        } object;
        struct
        {
          SchemaNodeData *child;
        } list;
        struct
        {
          GHashTable *children;
        } mappings;
      };
    },
    deinit_schema_node (self);)

struct _BzYamlParser
{
  GObject parent_instance;

  SchemaNodeData *schema;
};

G_DEFINE_FINAL_TYPE (BzYamlParser, bz_yaml_parser, G_TYPE_OBJECT)

enum
{
  KIND_SCALAR,
  KIND_OBJECT,
  KIND_LIST,
  KIND_MAPPINGS,
};

static SchemaNodeData *
compile_schema (XbNode *node);

static gboolean
parse (BzYamlParser   *self,
       yaml_parser_t  *parser,
       yaml_event_t   *event,
       gboolean        parse_first,
       gboolean        toplevel,
       SchemaNodeData *schema,
       GHashTable     *output,
       GPtrArray      *path_stack,
       GError        **error);

static char *
join_path_stack (GPtrArray *path_stack);

static GValue *
parse_scalar (const GVariantType *vtype,
              const guchar       *data,
              yaml_event_t       *event,
              GError            **error);

static GObject *
parse_object (BzYamlParser  *self,
              yaml_parser_t *parser,
              yaml_event_t  *event,
              GType          object_gtype,
              GHashTable    *type_hints,
              const char    *prop_path,
              GError       **error);

static void
destroy_gvalue (GValue *value);

static void
bz_yaml_parser_dispose (GObject *object)
{
  BzYamlParser *self = BZ_YAML_PARSER (object);

  g_clear_pointer (&self->schema, schema_node_data_unref);

  G_OBJECT_CLASS (bz_yaml_parser_parent_class)->dispose (object);
}

static void
bz_yaml_parser_class_init (BzYamlParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_yaml_parser_dispose;

  g_type_ensure (GTK_TYPE_STRING_OBJECT);
}

static void
bz_yaml_parser_init (BzYamlParser *self)
{
}

BzYamlParser *
bz_yaml_parser_new_for_resource_schema (const char *path)
{
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GBytes) bytes          = NULL;
  const char *resource_data         = NULL;
  g_autoptr (XbSilo) silo           = NULL;
  g_autoptr (XbNode) root           = NULL;
  g_autoptr (SchemaNodeData) schema = NULL;
  g_autoptr (BzYamlParser) parser   = NULL;

  g_return_val_if_fail (path != NULL, NULL);

  bytes = g_resources_lookup_data (
      path, G_RESOURCE_LOOKUP_FLAGS_NONE, &local_error);
  if (bytes == NULL)
    g_critical ("Could not load internal resource: %s", local_error->message);
  g_assert (bytes != NULL);
  resource_data = g_bytes_get_data (bytes, NULL);

  silo = xb_silo_new_from_xml (resource_data, &local_error);
  if (silo == NULL)
    g_critical ("Could not parse internal xml resource: %s", local_error->message);
  g_assert (silo != NULL);

  root = xb_silo_get_root (silo);

  parser         = g_object_new (BZ_TYPE_YAML_PARSER, NULL);
  parser->schema = compile_schema (root);

  return g_steal_pointer (&parser);
}

GHashTable *
bz_yaml_parser_process_bytes (BzYamlParser *self,
                              GBytes       *bytes,
                              GError      **error)
{
  g_autoptr (GError) local_error   = NULL;
  gsize         bytes_size         = 0;
  const guchar *bytes_data         = NULL;
  yaml_parser_t parser             = { 0 };
  yaml_event_t  event              = { 0 };
  g_autoptr (GHashTable) output    = NULL;
  g_autoptr (GPtrArray) path_stack = NULL;
  gboolean result                  = FALSE;

  g_return_val_if_fail (BZ_IS_YAML_PARSER (self), NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  bytes_data = g_bytes_get_data (bytes, &bytes_size);

  yaml_parser_initialize (&parser);
  yaml_parser_set_input_string (&parser, bytes_data, bytes_size);

  output = g_hash_table_new_full (
      g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) destroy_gvalue);
  path_stack = g_ptr_array_new_with_free_func (g_free);

  result = parse (
      self,
      &parser,
      &event,
      TRUE,
      TRUE,
      self->schema,
      output,
      path_stack,
      &local_error);
  yaml_parser_delete (&parser);

  if (result)
    return g_steal_pointer (&output);
  else
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return NULL;
    }
}

static SchemaNodeData *
compile_schema (XbNode *node)
{
  const char *element               = NULL;
  g_autoptr (SchemaNodeData) schema = NULL;

  element = xb_node_get_element (node);
  schema  = schema_node_data_new ();

#define ERROR_OUT(...)                                   \
  G_STMT_START                                           \
  {                                                      \
    g_critical ("Fatal: internal schema: " __VA_ARGS__); \
    g_assert (FALSE);                                    \
  }                                                      \
  G_STMT_END

  if (g_strcmp0 (element, "scalar") == 0)
    {
      const char *type = NULL;

      type = xb_node_get_attr (node, "type");
      if (type == NULL)
        ERROR_OUT ("scalar must have a type");
      if (!g_variant_type_is_basic ((const GVariantType *) type))
        ERROR_OUT ("invalid variant type for scalar '%s'", type);

      schema->kind         = KIND_SCALAR;
      schema->scalar.vtype = g_strdup (type);
    }
  else if (g_strcmp0 (element, "object") == 0)
    {
      const char *class                  = NULL;
      GType       gtype                  = G_TYPE_INVALID;
      g_autoptr (GTypeClass) gtype_class = NULL;
      XbNode *child                      = NULL;

      class = xb_node_get_attr (node, "class");
      if (class == NULL)
        ERROR_OUT ("object must have a class");

      gtype = g_type_from_name (class);
      if (gtype == G_TYPE_INVALID || !g_type_is_a (gtype, G_TYPE_OBJECT))
        ERROR_OUT ("'%s' is not a valid object class", class);

      schema->kind              = KIND_OBJECT;
      schema->object.type       = gtype;
      schema->object.type_hints = g_hash_table_new_full (
          g_str_hash, g_str_equal, g_free, NULL);

      gtype_class = g_type_class_ref (gtype);

      child = xb_node_get_child (node);
      while (child != NULL)
        {
          const char *child_element  = NULL;
          const char *name           = NULL;
          GParamSpec *property       = NULL;
          const char *typehint_name  = NULL;
          GType       typehint_gtype = G_TYPE_INVALID;
          XbNode     *next           = NULL;

          child_element = xb_node_get_element (child);
          if (g_strcmp0 (child_element, "typehint") != 0)
            ERROR_OUT ("\"typehint\" is the only valid child element of \"object\"");

          name = xb_node_get_attr (child, "name");
          if (name == NULL)
            ERROR_OUT ("typehint must have a name");

          property = g_object_class_find_property (G_OBJECT_CLASS (gtype_class), name);
          if (property == NULL)
            ERROR_OUT ("typehint property '%s' is invalid", name);

          typehint_name = xb_node_get_attr (child, "type");
          if (typehint_name == NULL)
            ERROR_OUT ("typehint must have a type");

          typehint_gtype = g_type_from_name (typehint_name);
          if (typehint_gtype == G_TYPE_INVALID || !g_type_is_a (typehint_gtype, G_TYPE_OBJECT))
            ERROR_OUT ("'%s' is not a valid object class", typehint_name);

          g_hash_table_replace (
              schema->object.type_hints,
              g_strdup (name),
              GSIZE_TO_POINTER (typehint_gtype));

          next = xb_node_get_next (child);
          g_object_unref (child);
          child = next;
        }
    }
  else if (g_strcmp0 (element, "list") == 0)
    {
      g_autoptr (XbNode) child = NULL;

      child = xb_node_get_child (node);
      if (child == NULL)
        ERROR_OUT ("list must have a child");

      schema->kind       = KIND_LIST;
      schema->list.child = compile_schema (child);
    }
  else if (g_strcmp0 (element, "mappings") == 0)
    {
      XbNode *child = NULL;

      schema->kind              = KIND_MAPPINGS;
      schema->mappings.children = g_hash_table_new_full (
          g_str_hash, g_str_equal, g_free, schema_node_data_unref);

      child = xb_node_get_child (node);
      while (child != NULL)
        {
          const char *child_element      = NULL;
          const char *key                = NULL;
          g_autoptr (XbNode) child_child = NULL;
          XbNode *next                   = NULL;

          child_element = xb_node_get_element (child);
          if (g_strcmp0 (child_element, "mapping") != 0)
            ERROR_OUT ("\"mapping\" is the only valid child element of \"mappings\"");

          key = xb_node_get_attr (child, "key");
          if (key == NULL)
            ERROR_OUT ("mapping must have a key");

          child_child = xb_node_get_child (child);
          if (child == NULL)
            ERROR_OUT ("mapping must have a child");

          g_hash_table_replace (
              schema->mappings.children,
              g_strdup (key),
              compile_schema (child_child));

          next = xb_node_get_next (child);
          g_object_unref (child);
          child = next;
        }
    }
  else
    ERROR_OUT ("unrecognized element '%s'", element);

#undef ERROR_OUT

  return g_steal_pointer (&schema);
}

static gboolean
parse (BzYamlParser   *self,
       yaml_parser_t  *parser,
       yaml_event_t   *event,
       gboolean        parse_first,
       gboolean        toplevel,
       SchemaNodeData *schema,
       GHashTable     *output,
       GPtrArray      *path_stack,
       GError        **error)
{
  if (parse_first && !yaml_parser_parse (parser, event))
    {
      g_set_error (
          error,
          BZ_YAML_ERROR,
          BZ_YAML_ERROR_INVALID_YAML,
          "Failed to parse YAML at line %zu, column %zu: %s",
          parser->problem_mark.line,
          parser->problem_mark.column,
          parser->problem);
      return FALSE;
    }

#define NEXT_EVENT()                                            \
  G_STMT_START                                                  \
  {                                                             \
    yaml_event_delete (event);                                  \
    if (!yaml_parser_parse (parser, event))                     \
      {                                                         \
        g_set_error (                                           \
            error,                                              \
            BZ_YAML_ERROR,                                      \
            BZ_YAML_ERROR_INVALID_YAML,                         \
            "Failed to parse YAML at line %zu, column %zu: %s", \
            parser->problem_mark.line,                          \
            parser->problem_mark.column,                        \
            parser->problem);                                   \
        return FALSE;                                           \
      }                                                         \
  }                                                             \
  G_STMT_END

#define EXPECT(event_type, string_type)                                      \
  if (event->type != (event_type))                                           \
    {                                                                        \
      g_set_error (                                                          \
          error,                                                             \
          BZ_YAML_ERROR,                                                     \
          BZ_YAML_ERROR_DOES_NOT_CONFORM,                                    \
          "Failed to validate YAML against schema at line %zu, column %zu: " \
          "expected " string_type " here",                                   \
          event->start_mark.line,                                            \
          event->start_mark.column);                                         \
      yaml_event_delete (event);                                             \
      return FALSE;                                                          \
    }

  if (toplevel)
    {
      EXPECT (YAML_STREAM_START_EVENT, "start of stream");
      NEXT_EVENT ();
      EXPECT (YAML_DOCUMENT_START_EVENT, "start of document");
      NEXT_EVENT ();
    }

  switch (schema->kind)
    {
    case KIND_SCALAR:
      {
        GValue *value = NULL;

        EXPECT (YAML_SCALAR_EVENT, "scalar");

        value = parse_scalar (
            (const GVariantType *) schema->scalar.vtype,
            event->data.scalar.value,
            event,
            error);
        if (value == NULL)
          {
            yaml_event_delete (event);
            return FALSE;
          }

        g_hash_table_replace (output, join_path_stack (path_stack), value);
      }
      break;
    case KIND_OBJECT:
      {
        g_autoptr (GObject) object = NULL;
        GValue *value              = NULL;

        object = parse_object (
            self,
            parser,
            event,
            schema->object.type,
            schema->object.type_hints,
            NULL,
            error);
        if (object == NULL)
          /* event is already cleaned up */
          return FALSE;

        value = g_new0 (typeof (*value), 1);
        g_value_init (value, schema->object.type);
        g_value_set_object (value, object);
        g_hash_table_replace (output, join_path_stack (path_stack), value);
      }
      break;
    case KIND_LIST:
      {
        g_autoptr (GPtrArray) list = NULL;
        GValue *value              = NULL;

        EXPECT (YAML_SEQUENCE_START_EVENT, "list");

        list = g_ptr_array_new_with_free_func ((GDestroyNotify) destroy_gvalue);

        for (;;)
          {
            g_autoptr (GHashTable) list_output    = NULL;
            g_autoptr (GPtrArray) list_path_stack = NULL;
            gboolean result                       = FALSE;
            GValue  *append                       = NULL;

            NEXT_EVENT ();
            if (event->type == YAML_SEQUENCE_END_EVENT)
              break;

            list_output = g_hash_table_new_full (
                g_str_hash, g_str_equal,
                g_free, (GDestroyNotify) destroy_gvalue);
            list_path_stack = g_ptr_array_new_with_free_func (g_free);

            result = parse (
                self,
                parser,
                event,
                FALSE,
                FALSE,
                schema->list.child,
                list_output,
                list_path_stack,
                error);
            if (!result)
              /* event is already cleaned up */
              return FALSE;

            append = g_new0 (typeof (*append), 1);
            g_value_init (append, G_TYPE_HASH_TABLE);
            g_value_set_boxed (append, list_output);
            g_ptr_array_add (list, append);
          }

        value = g_new0 (typeof (*value), 1);
        g_value_init (value, G_TYPE_PTR_ARRAY);
        g_value_set_boxed (value, list);
        g_hash_table_replace (output, join_path_stack (path_stack), value);
      }
      break;
    case KIND_MAPPINGS:
      {
        EXPECT (YAML_MAPPING_START_EVENT, "mappings");

        for (;;)
          {
            g_autofree char *key        = NULL;
            SchemaNodeData  *map_schema = NULL;
            gboolean         result     = FALSE;

            NEXT_EVENT ();

            if (event->type == YAML_MAPPING_END_EVENT)
              break;
            EXPECT (YAML_SCALAR_EVENT, "scalar key");

            key        = g_strdup ((const char *) event->data.scalar.value);
            map_schema = g_hash_table_lookup (schema->mappings.children, key);
            if (map_schema == NULL)
              {
                g_autofree char *path = NULL;

                path = join_path_stack (path_stack);
                g_set_error (
                    error,
                    BZ_YAML_ERROR,
                    BZ_YAML_ERROR_DOES_NOT_CONFORM,
                    "Failed to validate YAML against schema at line %zu, column %zu: "
                    "key '%s' shouldn't exist at path %s",
                    event->start_mark.line,
                    event->start_mark.column,
                    key,
                    path);
                yaml_event_delete (event);
                return FALSE;
              }

            g_ptr_array_add (path_stack, g_steal_pointer (&key));

            result = parse (
                self,
                parser,
                event,
                TRUE,
                FALSE,
                map_schema,
                output,
                path_stack,
                error);
            if (!result)
              /* event is already cleaned up */
              return FALSE;

            g_ptr_array_set_size (path_stack, path_stack->len - 1);
          }
      }
      break;
    default:
      g_assert_not_reached ();
    }

  if (toplevel)
    {
      NEXT_EVENT ();
      EXPECT (YAML_DOCUMENT_END_EVENT, "end of document");
      NEXT_EVENT ();
      EXPECT (YAML_STREAM_END_EVENT, "end of stream");
    }

  yaml_event_delete (event);
  return TRUE;
}

static char *
join_path_stack (GPtrArray *path_stack)
{
  GString *string = NULL;

  if (path_stack->len == 0)
    return g_strdup ("/");

  string = g_string_new (NULL);
  for (guint i = 0; i < path_stack->len; i++)
    {
      const char *component = NULL;

      component = g_ptr_array_index (path_stack, i);
      g_string_append_printf (string, "/%s", component);
    }

  return g_string_free_and_steal (string);
}

static GValue *
parse_scalar (const GVariantType *vtype,
              const guchar       *data,
              yaml_event_t       *event,
              GError            **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GVariant) variant   = NULL;
  GValue *value                  = NULL;

  if (g_variant_type_equal (vtype, G_VARIANT_TYPE_STRING))
    variant = g_variant_new_string ((const char *) data);
  else
    {
      variant = g_variant_parse (
          vtype,
          (const char *) data,
          NULL,
          NULL,
          &local_error);
      if (variant == NULL)
        {
          g_set_error (
              error,
              BZ_YAML_ERROR,
              BZ_YAML_ERROR_BAD_SCALAR,
              "Failed to parse scalar variant at line %zu, column %zu: "
              "%s",
              event->start_mark.line,
              event->start_mark.column,
              local_error->message);
          return NULL;
        }
    }

  value = g_new0 (typeof (*value), 1);
  g_value_init (value, G_TYPE_VARIANT);
  g_value_set_variant (value, g_steal_pointer (&variant));

  return value;
}

static GObject *
parse_object (BzYamlParser  *self,
              yaml_parser_t *parser,
              yaml_event_t  *event,
              GType          object_gtype,
              GHashTable    *type_hints,
              const char    *prop_path,
              GError       **error)
{
  g_autoptr (GTypeClass) gtype_class = NULL;
  g_autoptr (GObject) object         = NULL;

  EXPECT (YAML_MAPPING_START_EVENT, "object mapping");

  gtype_class = g_type_class_ref (object_gtype);
  object      = g_object_new (object_gtype, NULL);

  for (;;)
    {
      g_autofree char *property = NULL;
      GParamSpec      *spec     = NULL;

      NEXT_EVENT ();

      if (event->type == YAML_MAPPING_END_EVENT)
        break;
      EXPECT (YAML_SCALAR_EVENT, "scalar key");

      property = g_strdup ((const char *) event->data.scalar.value);
      spec     = g_object_class_find_property (G_OBJECT_CLASS (gtype_class), property);
      if (spec == NULL)
        {
          g_set_error (
              error,
              BZ_YAML_ERROR,
              BZ_YAML_ERROR_DOES_NOT_CONFORM,
              "Failed to validate YAML against schema at line %zu, column %zu: "
              "property '%s' doesn't exist on type %s",
              event->start_mark.line,
              event->start_mark.column,
              property,
              g_type_name (object_gtype));
          yaml_event_delete (event);
          return FALSE;
        }

      NEXT_EVENT ();

      if (g_type_is_a (spec->value_type, G_TYPE_LIST_MODEL))
        {
          g_autofree char *replace_prop_path = NULL;
          GType            element_gtype     = 0;
          g_autoptr (GListModel) list        = NULL;

          EXPECT (YAML_SEQUENCE_START_EVENT, "sequence");

          if (prop_path != NULL)
            replace_prop_path = g_strdup_printf ("%s.%s", prop_path, property);

          element_gtype = GPOINTER_TO_SIZE (g_hash_table_lookup (
              type_hints, replace_prop_path != NULL ? replace_prop_path : property));

          if (element_gtype == GTK_TYPE_STRING_OBJECT)
            {
              list = (GListModel *) gtk_string_list_new (NULL);

              for (;;)
                {
                  NEXT_EVENT ();

                  if (event->type == YAML_SEQUENCE_END_EVENT)
                    break;
                  EXPECT (YAML_SCALAR_EVENT, "scalar list value");

                  gtk_string_list_append (
                      GTK_STRING_LIST (list),
                      (const char *) event->data.scalar.value);
                }
            }
          else
            {
              if (element_gtype == 0)
                element_gtype = G_TYPE_OBJECT;

              list = (GListModel *) g_list_store_new (element_gtype);

              for (;;)
                {
                  g_autoptr (GObject) child_object = NULL;

                  NEXT_EVENT ();
                  if (event->type == YAML_SEQUENCE_END_EVENT)
                    break;

                  child_object = parse_object (
                      self,
                      parser,
                      event,
                      element_gtype,
                      type_hints,
                      replace_prop_path != NULL ? replace_prop_path : property,
                      error);
                  if (child_object == NULL)
                    /* event is already cleaned up */
                    return FALSE;

                  g_list_store_append (G_LIST_STORE (list), child_object);
                }
            }

          g_object_set (object, property, list, NULL);
        }
      else if (g_type_is_a (spec->value_type, G_TYPE_OBJECT))
        {
          g_autofree char *replace_prop_path = NULL;
          g_autoptr (GObject) prop_object    = NULL;

          if (prop_path != NULL)
            replace_prop_path = g_strdup_printf ("%s.%s", prop_path, property);

          prop_object = parse_object (
              self,
              parser,
              event,
              spec->value_type,
              type_hints,
              replace_prop_path != NULL ? replace_prop_path : property,
              error);
          if (prop_object == NULL)
            /* event is already cleaned up */
            return FALSE;

          g_object_set (object, property, prop_object, NULL);
        }
      else if (g_type_is_a (spec->value_type, G_TYPE_ENUM))
        {
          g_autoptr (GEnumClass) class = NULL;
          GEnumValue *enum_value       = NULL;

          EXPECT (YAML_SCALAR_EVENT, "scalar enum value");

          class = g_type_class_ref (spec->value_type);

          enum_value = g_enum_get_value_by_nick (
              class, (const char *) event->data.scalar.value);
          if (enum_value == NULL)
            enum_value = g_enum_get_value_by_name (
                class, (const char *) event->data.scalar.value);

          if (enum_value == NULL)
            {
              g_set_error (
                  error,
                  BZ_YAML_ERROR,
                  BZ_YAML_ERROR_BAD_SCALAR,
                  "Failed to parse scalar enum at line %zu, column %zu: "
                  "'%s' does not exist in type %s",
                  event->start_mark.line,
                  event->start_mark.column,
                  (const char *) event->data.scalar.value,
                  g_type_name (spec->value_type));
              yaml_event_delete (event);
              return FALSE;
            }

          g_object_set (object, property, enum_value->value, NULL);
        }
      else
        {
          g_autoptr (GError) local_error = NULL;
          const GVariantType *vtype      = NULL;
          g_autoptr (GVariant) variant   = NULL;

          EXPECT (YAML_SCALAR_EVENT, "scalar value");

          switch (spec->value_type)
            {
            case G_TYPE_BOOLEAN:
              vtype = G_VARIANT_TYPE_BOOLEAN;
              break;
            case G_TYPE_INT:
              vtype = G_VARIANT_TYPE_INT32;
              break;
            case G_TYPE_INT64:
              vtype = G_VARIANT_TYPE_INT64;
              break;
            case G_TYPE_UINT:
              vtype = G_VARIANT_TYPE_UINT32;
              break;
            case G_TYPE_UINT64:
              vtype = G_VARIANT_TYPE_UINT64;
              break;
            case G_TYPE_DOUBLE:
            case G_TYPE_FLOAT:
              vtype = G_VARIANT_TYPE_DOUBLE;
              break;
            case G_TYPE_STRING:
            default:
              vtype = G_VARIANT_TYPE_STRING;
              break;
            }

          if (g_variant_type_equal (vtype, G_VARIANT_TYPE_STRING))
            variant = g_variant_new_string ((const char *) event->data.scalar.value);
          else
            {
              variant = g_variant_parse (
                  vtype,
                  (const char *) event->data.scalar.value,
                  NULL,
                  NULL,
                  &local_error);
              if (variant == NULL)
                {
                  g_set_error (
                      error,
                      BZ_YAML_ERROR,
                      BZ_YAML_ERROR_BAD_SCALAR,
                      "Failed to parse scalar variant at line %zu, column %zu: %s",
                      event->start_mark.line,
                      event->start_mark.column,
                      local_error->message);
                  return FALSE;
                }
            }

          switch (spec->value_type)
            {
            case G_TYPE_BOOLEAN:
              g_object_set (object, property, g_variant_get_boolean (variant), NULL);
              break;
            case G_TYPE_INT:
              g_object_set (object, property, g_variant_get_int32 (variant), NULL);
              break;
            case G_TYPE_INT64:
              g_object_set (object, property, g_variant_get_int64 (variant), NULL);
              break;
            case G_TYPE_UINT:
              g_object_set (object, property, g_variant_get_uint32 (variant), NULL);
              break;
            case G_TYPE_UINT64:
              g_object_set (object, property, g_variant_get_uint64 (variant), NULL);
              break;
            case G_TYPE_DOUBLE:
            case G_TYPE_FLOAT:
              g_object_set (object, property, g_variant_get_double (variant), NULL);
              break;
            case G_TYPE_STRING:
            default:
              g_object_set (object, property, g_variant_get_string (variant, NULL), NULL);
              break;
            }
        }
    }

  return g_steal_pointer (&object);
}

static void
deinit_schema_node (gpointer data)
{
  SchemaNodeData *self = data;

  switch (self->kind)
    {
    case KIND_SCALAR:
      g_clear_pointer (&self->scalar.vtype, g_free);
      break;
    case KIND_OBJECT:
      g_clear_pointer (&self->object.type_hints, g_hash_table_unref);
      break;
    case KIND_LIST:
      g_clear_pointer (&self->list.child, schema_node_data_unref);
      break;
    case KIND_MAPPINGS:
      g_clear_pointer (&self->mappings.children, g_hash_table_unref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
destroy_gvalue (GValue *value)
{
  g_value_unset (value);
  g_free (value);
}

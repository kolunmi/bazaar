/* bge-wdgt-spec.c
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

#include <gtk/gtk.h>

#include "bge.h"
#include "fmt/parser.h"
#include "graphene-gobject.h"
#include "util.h"

typedef enum
{
  VALUE_CONSTANT = 0,
  VALUE_COMPONENT,
  VALUE_SPECIAL,
  VALUE_PROPERTY,
} ValueKind;

typedef void (*SnapshotCallFunc) (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest);

BGE_DEFINE_DATA (
    child,
    Child,
    {
      char *name;
      GType type;
    },
    BGE_RELEASE_DATA (name, g_free))

BGE_DEFINE_DATA (
    value,
    Value,
    {
      char     *name;
      GType     type;
      ValueKind kind;
      // union
      struct
      {
        GValue              constant;
        GPtrArray          *component;
        BgeWdgtSpecialValue special;
        struct
        {
          ChildData  *child;
          char       *prop_name;
          GParamFlags pspec_flags;
        } property;
      };
      GPtrArray *notify;
    },
    g_value_unset (&self->constant);
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (component, g_ptr_array_unref);
    BGE_RELEASE_DATA (property.child, child_data_unref);
    BGE_RELEASE_DATA (property.prop_name, g_free);
    BGE_RELEASE_DATA (notify, g_ptr_array_unref))

BGE_DEFINE_DATA (
    snapshot_call,
    SnapshotCall,
    {
      BgeWdgtSnapshotInstrKind kind;
      SnapshotCallFunc         func;
      GPtrArray               *args;
      GPtrArray               *rest;
    },
    BGE_RELEASE_DATA (args, g_ptr_array_unref);
    BGE_RELEASE_DATA (rest, g_ptr_array_unref))

BGE_DEFINE_DATA (
    state,
    State,
    {
      char       *name;
      GHashTable *setters;
      GPtrArray  *snapshot;
    },
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (setters, g_hash_table_unref);
    BGE_RELEASE_DATA (snapshot, g_ptr_array_unref))

/* --------------------------- */
/* Spec Builder Implementation */
/* --------------------------- */

struct _BgeWdgtSpec
{
  GObject parent_instance;

  char *name;

  GHashTable *values;
  GHashTable *children;
  GHashTable *states;

  StateData *default_state;
  struct
  {
    ValueData *motion_x;
    ValueData *motion_y;
  } special_values;
};

G_DEFINE_FINAL_TYPE (BgeWdgtSpec, bge_wdgt_spec, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_NAME,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

typedef struct
{
  const char      *name;
  guint            n_args;
  guint            n_rest;
  GType            args[16];
  gpointer         func;
  SnapshotCallFunc call;
} SnapshotInstr;

static gboolean
lookup_snapshot_push_instr (const char    *lookup_name,
                            SnapshotInstr *out);
static gboolean
lookup_snapshot_transform_instr (const char    *lookup_name,
                                 SnapshotInstr *out);
static gboolean
lookup_snapshot_append_instr (const char    *lookup_name,
                              SnapshotInstr *out);

static void
resolve_value (BgeWdgtSpec *self,
               ValueData   *value,
               GValue      *out);

static void
bge_wdgt_spec_dispose (GObject *object)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  g_clear_pointer (&self->name, g_free);

  g_clear_pointer (&self->values, g_hash_table_unref);
  g_clear_pointer (&self->children, g_hash_table_unref);
  g_clear_pointer (&self->states, g_hash_table_unref);
  g_clear_pointer (&self->default_state, state_data_unref);

  G_OBJECT_CLASS (bge_wdgt_spec_parent_class)->dispose (object);
}

static void
bge_wdgt_spec_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, bge_wdgt_spec_get_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_spec_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  switch (prop_id)
    {
    case PROP_NAME:
      bge_wdgt_spec_set_name (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_spec_class_init (BgeWdgtSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bge_wdgt_spec_set_property;
  object_class->get_property = bge_wdgt_spec_get_property;
  object_class->dispose      = bge_wdgt_spec_dispose;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bge_wdgt_spec_init (BgeWdgtSpec *self)
{
  self->values   = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_data_unref);
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, child_data_unref);
  self->states   = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, state_data_unref);
}

BgeWdgtSpec *
bge_wdgt_spec_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_SPEC, NULL);
}

BgeWdgtSpec *
bge_wdgt_spec_new_for_string (const char *string,
                              GError    **error)
{
  g_return_val_if_fail (string != NULL, NULL);
  return bge_wdgt_parse_string (string, error);
}

BgeWdgtSpec *
bge_wdgt_spec_new_for_resource (const char *resource,
                                GError    **error)
{
  g_autoptr (GBytes) bytes = NULL;
  gsize         size       = 0;
  gconstpointer data       = NULL;

  g_return_val_if_fail (resource != NULL, NULL);

  bytes = g_resources_lookup_data (resource, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
  if (bytes == NULL)
    return NULL;

  data = g_bytes_get_data (bytes, &size);
  return bge_wdgt_parse_string ((const char *) data, error);
}

const char *
bge_wdgt_spec_get_name (BgeWdgtSpec *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), NULL);
  return self->name;
}

void
bge_wdgt_spec_set_name (BgeWdgtSpec *self,
                        const char  *name)
{
  g_return_if_fail (BGE_IS_WDGT_SPEC (self));

  if (name == self->name || (name != NULL && self->name != NULL && g_strcmp0 (name, self->name) == 0))
    return;

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

void
bge_wdgt_spec_set_name_take (BgeWdgtSpec *self,
                             char        *name)
{
  g_return_if_fail (BGE_IS_WDGT_SPEC (self));

  if (name != NULL && self->name != NULL && g_strcmp0 (name, self->name) == 0)
    {
      g_free (name);
      return;
    }

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = name;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

gboolean
bge_wdgt_spec_add_constant_source_value (BgeWdgtSpec  *self,
                                         const char   *name,
                                         const GValue *constant,
                                         GError      **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (constant != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  value       = value_data_new ();
  value->name = g_strdup (name);
  value->type = constant->g_type;
  value->kind = VALUE_CONSTANT;
  g_value_copy (constant, g_value_init (&value->constant, constant->g_type));
  value->notify = g_ptr_array_new_with_free_func (value_data_unref);

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_component_source_value (BgeWdgtSpec       *self,
                                          const char        *name,
                                          GType              type,
                                          const char *const *components,
                                          guint              n_components,
                                          GError           **error)
{
  GType expected_types[32]    = { 0 };
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (components != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  if (type == GRAPHENE_TYPE_POINT)
    {
      if (n_components != 2)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed point value needs 2 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
    }
  else if (type == GRAPHENE_TYPE_POINT3D)
    {
      if (n_components != 3)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed 3d point value needs 3 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
      expected_types[2] = G_TYPE_DOUBLE;
    }
  else if (type == GRAPHENE_TYPE_RECT)
    {
      if (n_components != 4)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "composed rectangle value needs 4 arguments");
          return FALSE;
        }
      expected_types[0] = G_TYPE_DOUBLE;
      expected_types[1] = G_TYPE_DOUBLE;
      expected_types[2] = G_TYPE_DOUBLE;
      expected_types[3] = G_TYPE_DOUBLE;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not elligible for a component value",
                   g_type_name (type));
      return FALSE;
    }

  value            = value_data_new ();
  value->name      = g_strdup (name);
  value->type      = type;
  value->kind      = VALUE_COMPONENT;
  value->component = g_ptr_array_new_with_free_func (value_data_unref);

  for (guint i = 0; i < n_components; i++)
    {
      ValueData *value_data = NULL;

      value_data = g_hash_table_lookup (self->values, components[i]);
      if (value_data == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' is undefined", components[i]);
          return FALSE;
        }
      if (!g_type_is_a (value_data->type, expected_types[i]))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "component %u for type %s "
                       "must be of type %s, got %s",
                       i, g_type_name (type),
                       g_type_name (expected_types[i]),
                       g_type_name (value_data->type));
          return FALSE;
        }

      g_ptr_array_add (value->component, value_data_ref (value_data));
    }

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_special_source_value (BgeWdgtSpec        *self,
                                        const char         *name,
                                        BgeWdgtSpecialValue kind,
                                        GError            **error)
{
  g_autoptr (ValueData) value = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  switch (kind)
    {
    case BGE_WDGT_SPECIAL_VALUE_MOTION_X:
      if (self->special_values.motion_x != NULL)
        value = value_data_ref (self->special_values.motion_x);
      break;
    case BGE_WDGT_SPECIAL_VALUE_MOTION_Y:
      if (self->special_values.motion_y != NULL)
        value = value_data_ref (self->special_values.motion_y);
      break;
    default:
      g_critical ("Invalid special value specified");
      return FALSE;
    }

  if (value == NULL)
    {
      value          = value_data_new ();
      value->name    = g_strdup (name);
      value->kind    = VALUE_SPECIAL;
      value->special = kind;
      value->notify  = g_ptr_array_new_with_free_func (value_data_unref);

      switch (kind)
        {
        case BGE_WDGT_SPECIAL_VALUE_MOTION_X:
          self->special_values.motion_x = value_data_ref (value);
          break;
        case BGE_WDGT_SPECIAL_VALUE_MOTION_Y:
          self->special_values.motion_y = value_data_ref (value);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_property_value (BgeWdgtSpec *self,
                                  const char  *name,
                                  const char  *child,
                                  const char  *property,
                                  GError     **error)
{
  ChildData *child_data             = NULL;
  ValueData *existing_value         = NULL;
  g_autoptr (GTypeClass) type_class = NULL;
  GParamSpec *pspec                 = NULL;
  g_autoptr (ValueData) value       = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  g_return_val_if_fail (property != NULL, FALSE);

  child_data = g_hash_table_lookup (self->children, child);
  if (child_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "child '%s' doesn't exist", child);
      return FALSE;
    }

  existing_value = g_hash_table_lookup (self->values, name);
  if (existing_value != NULL)
    {
      if (existing_value->kind == VALUE_PROPERTY &&
          existing_value->property.child == child_data &&
          g_strcmp0 (existing_value->property.prop_name, property) == 0)
        return TRUE;
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' already exists", name);
          return FALSE;
        }
    }

  type_class = g_type_class_ref (child_data->type);
  pspec      = g_object_class_find_property (G_OBJECT_CLASS (type_class), property);
  if (pspec == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property '%s' doesn't exist on type %s",
                   property, g_type_name (child_data->type));
      return FALSE;
    }

  value                       = value_data_new ();
  value->name                 = g_strdup (name);
  value->type                 = pspec->value_type;
  value->kind                 = VALUE_PROPERTY;
  value->property.child       = child_data_ref (child_data);
  value->property.prop_name   = g_strdup (property);
  value->property.pspec_flags = pspec->flags;
  value->notify               = g_ptr_array_new_with_free_func (value_data_unref);

  g_hash_table_replace (self->values, g_strdup (name), value_data_ref (value));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_child (BgeWdgtSpec *self,
                         GType        type,
                         const char  *name,
                         GError     **error)
{
  g_autoptr (ChildData) child = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->children, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "child '%s' already exists", name);
      return FALSE;
    }
  if (!g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' does not derive from GtkWidget",
                   g_type_name (type));
      return FALSE;
    }
  if (!G_TYPE_IS_INSTANTIATABLE (type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "type '%s' is not instantiable",
                   g_type_name (type));
      return FALSE;
    }

  child       = child_data_new ();
  child->type = type;
  child->name = g_strdup (name);

  g_hash_table_replace (self->children, g_strdup (name), child_data_ref (child));
  return TRUE;
}

gboolean
bge_wdgt_spec_add_state (BgeWdgtSpec *self,
                         const char  *name,
                         GError     **error)
{
  g_autoptr (StateData) state = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  if (g_hash_table_contains (self->states, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' already exists", name);
      return FALSE;
    }

  state          = state_data_new ();
  state->name    = g_strdup (name);
  state->setters = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      value_data_unref, value_data_unref);
  state->snapshot = g_ptr_array_new_with_free_func (
      snapshot_call_data_unref);

  g_hash_table_replace (self->states, g_strdup (name), state_data_ref (state));
  return TRUE;
}

gboolean
bge_wdgt_spec_set_value (BgeWdgtSpec *self,
                         const char  *state,
                         const char  *dest_value,
                         const char  *src_value,
                         GError     **error)
{
  StateData *state_data = NULL;
  ValueData *dest_data  = NULL;
  ValueData *src_data   = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (dest_value != NULL, FALSE);
  g_return_val_if_fail (src_value != NULL, FALSE);

  state_data = g_hash_table_lookup (self->states, state);
  dest_data  = g_hash_table_lookup (self->values, dest_value);
  src_data   = g_hash_table_lookup (self->values, src_value);

  if (state_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' is undefined", state);
      return FALSE;
    }
  if (dest_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is undefined", dest_value);
      return FALSE;
    }
  if (src_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' is undefined", src_value);
      return FALSE;
    }
  if (dest_data == src_data)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign '%s' to itself", src_value);
      return FALSE;
    }
  if (!g_type_is_a (src_data->type, dest_data->type))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "source type %s cannot be assigned to destination type %s",
                   g_type_name (src_data->type), g_type_name (dest_data->type));
      return FALSE;
    }
  if (dest_data->kind == VALUE_CONSTANT)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign to a constant source value");
      return FALSE;
    }
  if (dest_data->kind == VALUE_SPECIAL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "cannot assign to a special source value");
      return FALSE;
    }
  if (dest_data->kind == VALUE_PROPERTY &&
      !(dest_data->property.pspec_flags & G_PARAM_WRITABLE))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property %s on type %s is not writable",
                   dest_data->property.prop_name,
                   g_type_name (dest_data->type));
      return FALSE;
    }
  if (src_data->kind == VALUE_PROPERTY &&
      !(src_data->property.pspec_flags & G_PARAM_READABLE))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "property %s on type %s is not writable",
                   src_data->property.prop_name,
                   g_type_name (src_data->type));
      return FALSE;
    }

  g_hash_table_replace (
      state_data->setters,
      value_data_ref (dest_data),
      value_data_ref (src_data));
  return TRUE;
}

gboolean
bge_wdgt_spec_append_snapshot_instr (BgeWdgtSpec             *self,
                                     const char              *state,
                                     BgeWdgtSnapshotInstrKind kind,
                                     const char              *instr,
                                     const char *const       *args,
                                     guint                    n_args,
                                     GError                 **error)
{
  gboolean      result              = FALSE;
  StateData    *state_data          = NULL;
  SnapshotInstr match               = { 0 };
  g_autoptr (SnapshotCallData) call = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (instr != NULL, FALSE);
  g_return_val_if_fail (n_args == 0 || args != NULL, FALSE);

  state_data = g_hash_table_lookup (self->states, state);
  if (state_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "state '%s' is undefined", state);
      return FALSE;
    }

  switch (kind)
    {
    case BGE_WDGT_SNAPSHOT_INSTR_PUSH:
      result = lookup_snapshot_push_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot push instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM:
      result = lookup_snapshot_transform_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot transform instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_APPEND:
      result = lookup_snapshot_append_instr (instr, &match);
      if (!result)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "\"%s\" is not a valid snapshot append instruction",
                       instr);
          return FALSE;
        }
      break;
    case BGE_WDGT_SNAPSHOT_INSTR_POP:
    case BGE_WDGT_SNAPSHOT_INSTR_SAVE:
    case BGE_WDGT_SNAPSHOT_INSTR_RESTORE:
      call       = snapshot_call_data_new ();
      call->kind = kind;
      g_ptr_array_add (state_data->snapshot, snapshot_call_data_ref (call));
      return TRUE;
    default:
      g_critical ("invalid snapshot instruction kind specified");
      return FALSE;
    }

  if (n_args != match.n_args)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "snapshot instruction %s requires %u "
                   "arguments, got %u",
                   match.name, match.n_args, n_args);
      return FALSE;
    }

  call       = snapshot_call_data_new ();
  call->kind = kind;
  call->func = match.call;
  call->args = g_ptr_array_new_with_free_func (value_data_unref);
  call->rest = g_ptr_array_new_with_free_func (value_data_unref);

  for (guint i = 0; i < n_args; i++)
    {
      ValueData *value_data = NULL;

      value_data = g_hash_table_lookup (self->values, args[i]);
      if (value_data == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "value '%s' is undefined", args[i]);
          return FALSE;
        }
      if (!g_type_is_a (value_data->type, match.args[i]))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                       "argument %u for snapshot instruction %s "
                       "must be of type %s, got %s",
                       i, match.name,
                       g_type_name (match.args[i]),
                       g_type_name (value_data->type));
          return FALSE;
        }

      g_ptr_array_add (call->args, value_data_ref (value_data));
    }

  g_ptr_array_add (state_data->snapshot, snapshot_call_data_ref (call));
  return TRUE;
}

static void
snapshot_push_instr_opacity (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_push_opacity (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_push_instr_isolation (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_push_isolation (
      snapshot,
      g_value_get_flags (&args[0]));
}

static void
snapshot_push_instr_blur (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_blur (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_push_instr_color_matrix (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest)
{
  gtk_snapshot_push_color_matrix (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_component_transfer (GtkSnapshot *snapshot,
                                        const GValue args[],
                                        const GValue rest[],
                                        guint        n_rest)
{
  gtk_snapshot_push_component_transfer (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      g_value_get_boxed (&args[3]));
}

static void
snapshot_push_instr_repeat (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_push_repeat (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_clip (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_clip (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_push_instr_rounded_clip (GtkSnapshot *snapshot,
                                  const GValue args[],
                                  const GValue rest[],
                                  guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_push_rounded_clip (
      snapshot,
      &rrect);
}

static void
snapshot_push_instr_fill (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_fill (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_enum (&args[1]));
}

static void
snapshot_push_instr_stroke (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_push_stroke (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_push_instr_shadow (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  guint     n_shadows   = 0;
  GskShadow shadows[32] = { 0 };

  n_shadows = MIN (n_rest / 4, G_N_ELEMENTS (shadows));

  for (guint i = 0; i < n_shadows; i++)
    {
      shadows[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 4 + 0]);
      shadows[i].dx     = g_value_get_double (&rest[i * 4 + 1]);
      shadows[i].dy     = g_value_get_double (&rest[i * 4 + 2]);
      shadows[i].radius = g_value_get_double (&rest[i * 4 + 3]);
    }

  gtk_snapshot_push_shadow (
      snapshot,
      shadows,
      n_shadows);
}

static void
snapshot_push_instr_blend (GtkSnapshot *snapshot,
                           const GValue args[],
                           const GValue rest[],
                           guint        n_rest)
{
  gtk_snapshot_push_blend (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_mask (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_mask (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_copy (GtkSnapshot *snapshot,
                          const GValue args[],
                          const GValue rest[],
                          guint        n_rest)
{
  gtk_snapshot_push_copy (
      snapshot);
}

static void
snapshot_push_instr_composite (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_push_composite (
      snapshot,
      g_value_get_enum (&args[0]));
}

static void
snapshot_push_instr_cross_fade (GtkSnapshot *snapshot,
                                const GValue args[],
                                const GValue rest[],
                                guint        n_rest)
{
  gtk_snapshot_push_cross_fade (
      snapshot,
      g_value_get_double (&args[0]));
}

static gboolean
lookup_snapshot_push_instr (const char    *lookup_name,
                            SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "opacity",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_opacity,
     snapshot_push_instr_opacity,
     },
    {
     "isolation",
     1,
     0,
     {
     GSK_TYPE_ISOLATION,
     },
     gtk_snapshot_push_isolation,
     snapshot_push_instr_isolation,
     },
    {
     "blur",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_blur,
     snapshot_push_instr_blur,
     },
    {
     "color-matrix",
     2,
     0,
     {
     GRAPHENE_TYPE_MATRIX,
     GRAPHENE_TYPE_VEC4,
     },
     gtk_snapshot_push_color_matrix,
     snapshot_push_instr_color_matrix,
     },
    {
     "component-transfer",
     4,
     0,
     {
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     GSK_TYPE_COMPONENT_TRANSFER,
     },
     gtk_snapshot_push_component_transfer,
     snapshot_push_instr_component_transfer,
     },
    {
     "repeat",
     2,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_push_repeat,
     snapshot_push_instr_repeat,
     },
    {
     "clip",
     1,
     0,
     {
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_push_clip,
     snapshot_push_instr_clip,
     },
    {
     "rounded-clip",
     5,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     },
     gtk_snapshot_push_rounded_clip,
     snapshot_push_instr_rounded_clip,
     },
    {
     "fill",
     2,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_FILL_RULE,
     },
     gtk_snapshot_push_fill,
     snapshot_push_instr_fill,
     },
    {
     "stroke",
     2,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_STROKE,
     },
     gtk_snapshot_push_stroke,
     snapshot_push_instr_stroke,
     },
    {
     "shadow",
     4,
     4,
     {
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_shadow,
     snapshot_push_instr_shadow,
     },
    {
     "blend",
     1,
     0,
     {
     GSK_TYPE_BLEND_MODE,
     },
     gtk_snapshot_push_blend,
     snapshot_push_instr_blend,
     },
    {
     "mask",
     1,
     0,
     {
     GSK_TYPE_MASK_MODE,
     },
     gtk_snapshot_push_mask,
     snapshot_push_instr_mask,
     },
    {
     "copy",
     0,
     0,
     {},
     gtk_snapshot_push_copy,
     snapshot_push_instr_copy,
     },
    {
     "composite",
     1,
     0,
     {
     GSK_TYPE_PORTER_DUFF,
     },
     gtk_snapshot_push_composite,
     snapshot_push_instr_composite,
     },
    {
     "cross-fade",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_push_cross_fade,
     snapshot_push_instr_cross_fade,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static void
snapshot_transform_instr_transform (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_transform (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_transform_matrix (GtkSnapshot *snapshot,
                                           const GValue args[],
                                           const GValue rest[],
                                           guint        n_rest)
{
  gtk_snapshot_transform_matrix (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_translate (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_translate (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_translate_3d (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  gtk_snapshot_translate_3d (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_transform_instr_rotate (GtkSnapshot *snapshot,
                                 const GValue args[],
                                 const GValue rest[],
                                 guint        n_rest)
{
  gtk_snapshot_rotate (
      snapshot,
      g_value_get_double (&args[0]));
}

static void
snapshot_transform_instr_rotate_3d (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  gtk_snapshot_rotate_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_transform_instr_scale (GtkSnapshot *snapshot,
                                const GValue args[],
                                const GValue rest[],
                                guint        n_rest)
{
  gtk_snapshot_scale (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]));
}

static void
snapshot_transform_instr_scale_3d (GtkSnapshot *snapshot,
                                   const GValue args[],
                                   const GValue rest[],
                                   guint        n_rest)
{
  gtk_snapshot_scale_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]),
      g_value_get_double (&args[2]));
}

static void
snapshot_transform_instr_perspective (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  gtk_snapshot_scale_3d (
      snapshot,
      g_value_get_double (&args[0]),
      g_value_get_double (&args[1]),
      g_value_get_double (&args[2]));
}

static gboolean
lookup_snapshot_transform_instr (const char    *lookup_name,
                                 SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "transform",
     1,
     0,
     {
     GSK_TYPE_TRANSFORM,
     },
     gtk_snapshot_transform,
     snapshot_transform_instr_transform,
     },
    {
     "transform-matrix",
     1,
     0,
     {
     GRAPHENE_TYPE_MATRIX,
     },
     gtk_snapshot_transform_matrix,
     snapshot_transform_instr_transform_matrix,
     },
    {
     "translate",
     1,
     0,
     {
     GRAPHENE_TYPE_POINT,
     },
     gtk_snapshot_translate,
     snapshot_transform_instr_translate,
     },
    {
     "translate-3d",
     1,
     0,
     {
     GRAPHENE_TYPE_POINT3D,
     },
     gtk_snapshot_translate_3d,
     snapshot_transform_instr_translate_3d,
     },
    {
     "rotate",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_rotate,
     snapshot_transform_instr_rotate,
     },
    {
     "rotate-3d",
     2,
     0,
     {
     G_TYPE_DOUBLE,
     GRAPHENE_TYPE_VEC3,
     },
     gtk_snapshot_rotate_3d,
     snapshot_transform_instr_rotate_3d,
     },
    {
     "scale",
     2,
     0,
     {
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_scale,
     snapshot_transform_instr_scale,
     },
    {
     "scale-3d",
     3,
     0,
     {
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_scale_3d,
     snapshot_transform_instr_scale_3d,
     },
    {
     "perspective",
     1,
     0,
     {
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_perspective,
     snapshot_transform_instr_perspective,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static void
snapshot_append_instr_node (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_append_node (
      snapshot,
      g_value_get_boxed (&args[0]));
}

static void
snapshot_append_instr_texture (GtkSnapshot *snapshot,
                               const GValue args[],
                               const GValue rest[],
                               guint        n_rest)
{
  gtk_snapshot_append_texture (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_scaled_texture (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  gtk_snapshot_append_scaled_texture (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_enum (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_color (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_append_color (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_linear_gradient (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_linear_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_repeating_linear_gradient (GtkSnapshot *snapshot,
                                                 const GValue args[],
                                                 const GValue rest[],
                                                 guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_repeating_linear_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_radial_gradient (GtkSnapshot *snapshot,
                                       const GValue args[],
                                       const GValue rest[],
                                       guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_radial_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      g_value_get_double (&args[3]),
      g_value_get_double (&args[4]),
      g_value_get_double (&args[5]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_repeating_radial_gradient (GtkSnapshot *snapshot,
                                                 const GValue args[],
                                                 const GValue rest[],
                                                 guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_repeating_radial_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      g_value_get_double (&args[3]),
      g_value_get_double (&args[4]),
      g_value_get_double (&args[5]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_conic_gradient (GtkSnapshot *snapshot,
                                      const GValue args[],
                                      const GValue rest[],
                                      guint        n_rest)
{
  guint        n_stops   = 0;
  GskColorStop stops[32] = { 0 };

  n_stops = MIN (n_rest / 2, G_N_ELEMENTS (stops));
  for (guint i = 0; i < n_stops; i++)
    {
      stops[i].offset = g_value_get_double (&rest[i * 2 + 0]);
      stops[i].color  = *(GdkRGBA *) g_value_get_boxed (&rest[i * 2 + 1]);
    }

  gtk_snapshot_append_conic_gradient (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_double (&args[2]),
      stops,
      n_stops);
}

static void
snapshot_append_instr_border (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  GskRoundedRect rrect           = { 0 };
  float          border_width[4] = { 0 };
  GdkRGBA        border_color[4] = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  border_width[0] = g_value_get_double (&args[5]);
  border_width[1] = g_value_get_double (&args[6]);
  border_width[2] = g_value_get_double (&args[7]);
  border_width[3] = g_value_get_double (&args[8]);

  border_color[0] = *(GdkRGBA *) g_value_get_boxed (&args[9]);
  border_color[1] = *(GdkRGBA *) g_value_get_boxed (&args[10]);
  border_color[2] = *(GdkRGBA *) g_value_get_boxed (&args[11]);
  border_color[3] = *(GdkRGBA *) g_value_get_boxed (&args[12]);

  gtk_snapshot_append_border (
      snapshot,
      &rrect,
      border_width,
      border_color);
}

static void
snapshot_append_instr_inset_shadow (GtkSnapshot *snapshot,
                                    const GValue args[],
                                    const GValue rest[],
                                    guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_append_inset_shadow (
      snapshot,
      &rrect,
      g_value_get_boxed (&args[5]),
      g_value_get_double (&args[6]),
      g_value_get_double (&args[7]),
      g_value_get_double (&args[8]),
      g_value_get_double (&args[9]));
}

static void
snapshot_append_instr_outset_shadow (GtkSnapshot *snapshot,
                                     const GValue args[],
                                     const GValue rest[],
                                     guint        n_rest)
{
  GskRoundedRect rrect = { 0 };

  rrect.bounds    = *(graphene_rect_t *) g_value_get_boxed (&args[0]);
  rrect.corner[0] = *(graphene_size_t *) g_value_get_boxed (&args[1]);
  rrect.corner[1] = *(graphene_size_t *) g_value_get_boxed (&args[2]);
  rrect.corner[2] = *(graphene_size_t *) g_value_get_boxed (&args[3]);
  rrect.corner[3] = *(graphene_size_t *) g_value_get_boxed (&args[4]);

  gtk_snapshot_append_outset_shadow (
      snapshot,
      &rrect,
      g_value_get_boxed (&args[5]),
      g_value_get_double (&args[6]),
      g_value_get_double (&args[7]),
      g_value_get_double (&args[8]),
      g_value_get_double (&args[9]));
}

static void
snapshot_append_instr_layout (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  gtk_snapshot_append_layout (
      snapshot,
      g_value_get_object (&args[0]),
      g_value_get_boxed (&args[1]));
}

static void
snapshot_append_instr_fill (GtkSnapshot *snapshot,
                            const GValue args[],
                            const GValue rest[],
                            guint        n_rest)
{
  gtk_snapshot_append_fill (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_enum (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_stroke (GtkSnapshot *snapshot,
                              const GValue args[],
                              const GValue rest[],
                              guint        n_rest)
{
  gtk_snapshot_append_stroke (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_boxed (&args[1]),
      g_value_get_boxed (&args[2]));
}

static void
snapshot_append_instr_paste (GtkSnapshot *snapshot,
                             const GValue args[],
                             const GValue rest[],
                             guint        n_rest)
{
  gtk_snapshot_append_paste (
      snapshot,
      g_value_get_boxed (&args[0]),
      g_value_get_uint64 (&args[1]));
}

static gboolean
lookup_snapshot_append_instr (const char    *lookup_name,
                              SnapshotInstr *out)
{
  SnapshotInstr instrs[] = {
    {
     "node",
     1,
     0,
     {
     GSK_TYPE_RENDER_NODE,
     },
     gtk_snapshot_append_node,
     snapshot_append_instr_node,
     },
    {
     "texture",
     2,
     0,
     {
     GDK_TYPE_TEXTURE,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_texture,
     snapshot_append_instr_texture,
     },
    {
     "scaled-texture",
     3,
     0,
     {
     GDK_TYPE_TEXTURE,
     GSK_TYPE_SCALING_FILTER,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_scaled_texture,
     snapshot_append_instr_scaled_texture,
     },
    {
     "color",
     2,
     0,
     {
     GDK_TYPE_RGBA,
     GRAPHENE_TYPE_RECT,
     },
     gtk_snapshot_append_color,
     snapshot_append_instr_color,
     },
    {
     "linear-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_linear_gradient,
     snapshot_append_instr_linear_gradient,
     },
    {
     "repeating-linear-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_repeating_linear_gradient,
     snapshot_append_instr_repeating_linear_gradient,
     },
    {
     "radial-gradient",
     8,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_radial_gradient,
     snapshot_append_instr_radial_gradient,
     },
    {
     "repeating-radial-gradient",
     8,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_repeating_radial_gradient,
     snapshot_append_instr_repeating_radial_gradient,
     },
    {
     "conic-gradient",
     5,
     2,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_POINT,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_conic_gradient,
     snapshot_append_instr_conic_gradient,
     },
    {
     "border",
     13,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_border,
     snapshot_append_instr_border,
     },
    {
     "inset-shadow",
     10,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_append_inset_shadow,
     snapshot_append_instr_inset_shadow,
     },
    {
     "outset-shadow",
     10,
     0,
     {
     GRAPHENE_TYPE_RECT,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GRAPHENE_TYPE_SIZE,
     GDK_TYPE_RGBA,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     G_TYPE_DOUBLE,
     },
     gtk_snapshot_append_outset_shadow,
     snapshot_append_instr_outset_shadow,
     },
    {
     "layout",
     2,
     0,
     {
     PANGO_TYPE_LAYOUT,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_layout,
     snapshot_append_instr_layout,
     },
    {
     "fill",
     3,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_FILL_RULE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_fill,
     snapshot_append_instr_fill,
     },
    {
     "stroke",
     3,
     0,
     {
     GSK_TYPE_PATH,
     GSK_TYPE_STROKE,
     GDK_TYPE_RGBA,
     },
     gtk_snapshot_append_stroke,
     snapshot_append_instr_stroke,
     },
    {
     "paste",
     2,
     0,
     {
     GRAPHENE_TYPE_RECT,
     G_TYPE_UINT64,
     },
     gtk_snapshot_append_paste,
     snapshot_append_instr_paste,
     },
  };

  for (guint i = 0; i < G_N_ELEMENTS (instrs); i++)
    {
      if (g_strcmp0 (lookup_name, instrs[i].name) == 0)
        {
          *out = instrs[i];
          return TRUE;
        }
    }
  return FALSE;
}

static void
resolve_value (BgeWdgtSpec *self,
               ValueData   *value,
               GValue      *out)
{
  g_value_init (out, value->type);
  switch (value->kind)
    {
    case VALUE_CONSTANT:
      g_value_copy (&value->constant, out);
      break;
    case VALUE_COMPONENT:
      {
        GValue components[32] = { 0 };
        guint  n_components   = 0;

        n_components = MIN (value->component->len, G_N_ELEMENTS (components));
        for (guint i = 0; i < n_components; i++)
          {
            ValueData *component = NULL;

            component = g_ptr_array_index (value->component, i);
            resolve_value (self, component, &components[i]);
          }

        if (value->type == GRAPHENE_TYPE_POINT)
          {
            graphene_point_t point = { 0 };

            point = GRAPHENE_POINT_INIT (
                g_value_get_double (&components[0]),
                g_value_get_double (&components[1]));
            g_value_set_boxed (out, &point);
          }
        else if (value->type == GRAPHENE_TYPE_POINT)
          {
            graphene_point3d_t point = { 0 };

            point = GRAPHENE_POINT3D_INIT (
                g_value_get_double (&components[0]),
                g_value_get_double (&components[1]),
                g_value_get_double (&components[2]));
            g_value_set_boxed (out, &point);
          }
        else if (value->type == GRAPHENE_TYPE_RECT)
          {
            graphene_rect_t rect = { 0 };

            rect = GRAPHENE_RECT_INIT (
                g_value_get_double (&components[0]),
                g_value_get_double (&components[1]),
                g_value_get_double (&components[2]),
                g_value_get_double (&components[3]));
            g_value_set_boxed (out, &rect);
          }

        for (guint i = 0; i < n_components; i++)
          {
            g_value_unset (&components[i]);
          }
      }
      break;
    case VALUE_PROPERTY:
      {
        GObject *object = NULL;

        object = g_hash_table_lookup (
            self->children,
            value->property.child->name);
        g_assert (object != NULL);
        g_object_get_property (object, value->property.prop_name, out);
      }
      break;
    case VALUE_SPECIAL:
      /* TODO */
      break;
    default:
      break;
    }
}

/* ------------------------------ */
/* Widget Renderer Implementation */
/* ------------------------------ */

struct _BgeWdgtRenderer
{
  GtkWidget parent_instance;

  BgeWdgtSpec *spec;
  char        *state;
  GObject     *reference;
  GtkWidget   *child;

  StateData  *active_state;
  GHashTable *children;
  GPtrArray  *bindings;
};

G_DEFINE_FINAL_TYPE (BgeWdgtRenderer, bge_wdgt_renderer, GTK_TYPE_WIDGET);

enum
{
  RENDERER_PROP_0,

  RENDERER_PROP_SPEC,
  RENDERER_PROP_STATE,
  RENDERER_PROP_REFERENCE,
  RENDERER_PROP_CHILD,

  LAST_RENDERER_PROP
};
static GParamSpec *renderer_props[LAST_RENDERER_PROP] = { 0 };

static void
regenerate (BgeWdgtRenderer *self);

static void
apply_state (BgeWdgtRenderer *self);

static void
bge_wdgt_renderer_dispose (GObject *object)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  g_clear_pointer (&self->spec, g_object_unref);
  g_clear_pointer (&self->state, g_free);
  g_clear_pointer (&self->reference, g_object_unref);
  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_clear_pointer (&self->active_state, state_data_unref);
  g_clear_pointer (&self->children, g_hash_table_unref);
  g_clear_pointer (&self->bindings, g_ptr_array_unref);

  G_OBJECT_CLASS (bge_wdgt_renderer_parent_class)->dispose (object);
}

static void
bge_wdgt_renderer_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  switch (prop_id)
    {
    case RENDERER_PROP_SPEC:
      g_value_set_object (value, bge_wdgt_renderer_get_spec (self));
      break;
    case RENDERER_PROP_STATE:
      g_value_set_string (value, bge_wdgt_renderer_get_state (self));
      break;
    case RENDERER_PROP_REFERENCE:
      g_value_set_object (value, bge_wdgt_renderer_get_reference (self));
      break;
    case RENDERER_PROP_CHILD:
      g_value_set_object (value, bge_wdgt_renderer_get_child (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_renderer_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (object);

  switch (prop_id)
    {
    case RENDERER_PROP_SPEC:
      bge_wdgt_renderer_set_spec (self, g_value_get_object (value));
      break;
    case RENDERER_PROP_STATE:
      bge_wdgt_renderer_set_state (self, g_value_get_string (value));
      break;
    case RENDERER_PROP_REFERENCE:
      bge_wdgt_renderer_set_reference (self, g_value_get_object (value));
      break;
    case RENDERER_PROP_CHILD:
      bge_wdgt_renderer_set_child (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_renderer_measure (GtkWidget     *widget,
                           GtkOrientation orientation,
                           int            for_size,
                           int           *minimum,
                           int           *natural,
                           int           *minimum_baseline,
                           int           *natural_baseline)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (widget);

  if (self->child != NULL)
    gtk_widget_measure (
        GTK_WIDGET (self->child),
        orientation, for_size,
        minimum, natural,
        minimum_baseline, natural_baseline);
}

static void
bge_wdgt_renderer_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  BgeWdgtRenderer *self = BGE_WDGT_RENDERER (widget);
  GHashTableIter   iter = { 0 };

  if (self->child != NULL &&
      gtk_widget_should_layout (self->child))
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  g_hash_table_iter_init (&iter, self->children);
  for (;;)
    {
      char      *name  = NULL;
      GtkWidget *child = NULL;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &name,
              (gpointer *) &child))
        break;
      gtk_widget_allocate (child, width, height, baseline, NULL);
    }
}

static void
bge_wdgt_renderer_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  BgeWdgtRenderer *self       = BGE_WDGT_RENDERER (widget);
  StateData       *state_data = NULL;
  GHashTableIter   iter       = { 0 };

  state_data = self->active_state;

  if (self->child != NULL)
    gtk_widget_snapshot_child (GTK_WIDGET (self), self->child, snapshot);

  if (state_data != NULL)
    {
      for (guint i = 0; i < state_data->snapshot->len; i++)
        {
          SnapshotCallData *call = NULL;

          call = g_ptr_array_index (state_data->snapshot, i);
          switch (call->kind)
            {
            case BGE_WDGT_SNAPSHOT_INSTR_POP:
              gtk_snapshot_pop (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_SAVE:
              gtk_snapshot_save (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_RESTORE:
              gtk_snapshot_restore (snapshot);
              break;
            case BGE_WDGT_SNAPSHOT_INSTR_APPEND:
            case BGE_WDGT_SNAPSHOT_INSTR_PUSH:
            case BGE_WDGT_SNAPSHOT_INSTR_TRANSFORM:
              {
                GValue values[32] = { 0 };

                for (guint j = 0; j < call->args->len; j++)
                  {
                    ValueData *value = NULL;

                    value = g_ptr_array_index (call->args, j);
                    resolve_value (self->spec, value, &values[j]);
                  }
                call->func (snapshot, values, NULL, 0);

                for (guint j = 0; j < call->args->len; j++)
                  {
                    g_value_unset (&values[j]);
                  }
              }
              break;
            default:
              break;
            }
        }
    }

  g_hash_table_iter_init (&iter, self->children);
  for (;;)
    {
      char      *name  = NULL;
      GtkWidget *child = NULL;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &name,
              (gpointer *) &child))
        break;
      gtk_widget_snapshot_child (GTK_WIDGET (self), child, snapshot);
    }
}

static void
bge_wdgt_renderer_class_init (BgeWdgtRendererClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bge_wdgt_renderer_set_property;
  object_class->get_property = bge_wdgt_renderer_get_property;
  object_class->dispose      = bge_wdgt_renderer_dispose;

  renderer_props[RENDERER_PROP_SPEC] =
      g_param_spec_object (
          "spec",
          NULL, NULL,
          BGE_TYPE_WDGT_SPEC,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_STATE] =
      g_param_spec_string (
          "state",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_REFERENCE] =
      g_param_spec_object (
          "reference",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  renderer_props[RENDERER_PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_RENDERER_PROP, renderer_props);

  widget_class->measure       = bge_wdgt_renderer_measure;
  widget_class->size_allocate = bge_wdgt_renderer_size_allocate;
  widget_class->snapshot      = bge_wdgt_renderer_snapshot;
}

static void
bge_wdgt_renderer_init (BgeWdgtRenderer *self)
{
  self->children = g_hash_table_new_full (
      g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) gtk_widget_unparent);
  self->bindings = g_ptr_array_new_with_free_func (g_object_unref);
}

BgeWdgtRenderer *
bge_wdgt_renderer_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_RENDERER, NULL);
}

BgeWdgtSpec *
bge_wdgt_renderer_get_spec (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->spec;
}

const char *
bge_wdgt_renderer_get_state (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->state;
}

GObject *
bge_wdgt_renderer_get_reference (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->reference;
}

GtkWidget *
bge_wdgt_renderer_get_child (BgeWdgtRenderer *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_RENDERER (self), NULL);
  return self->child;
}

void
bge_wdgt_renderer_set_spec (BgeWdgtRenderer *self,
                            BgeWdgtSpec     *spec)
{
  GBinding *name_binding = NULL;

  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (spec == self->spec)
    return;

  g_clear_pointer (&self->spec, g_object_unref);
  if (spec != NULL)
    self->spec = g_object_ref (spec);

  regenerate (self);
  apply_state (self);

  if (spec != NULL)
    {
      name_binding = g_object_bind_property (spec, "name", self, "name", G_BINDING_SYNC_CREATE);
      g_ptr_array_add (self->bindings, name_binding);
    }
  else
    gtk_widget_set_name (GTK_WIDGET (self), NULL);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_SPEC]);
}

void
bge_wdgt_renderer_set_state (BgeWdgtRenderer *self,
                             const char      *state)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (state == self->state || (state != NULL && self->state != NULL && g_strcmp0 (state, self->state) == 0))
    return;

  g_clear_pointer (&self->state, g_free);
  if (state != NULL)
    self->state = g_strdup (state);

  apply_state (self);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_STATE]);
}

void
bge_wdgt_renderer_set_reference (BgeWdgtRenderer *self,
                                 GObject         *reference)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (reference == self->reference)
    return;

  g_clear_pointer (&self->reference, g_object_unref);
  if (reference != NULL)
    self->reference = g_object_ref (reference);

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_REFERENCE]);
}

void
bge_wdgt_renderer_set_child (BgeWdgtRenderer *self,
                             GtkWidget       *child)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (self->child == child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_CHILD]);
}

void
bge_wdgt_renderer_set_state_take (BgeWdgtRenderer *self,
                                  char            *state)
{
  g_return_if_fail (BGE_IS_WDGT_RENDERER (self));

  if (state != NULL && self->state != NULL && g_strcmp0 (state, self->state) == 0)
    {
      g_free (state);
      return;
    }

  g_clear_pointer (&self->state, g_free);
  if (state != NULL)
    self->state = state;

  g_object_notify_by_pspec (G_OBJECT (self), renderer_props[RENDERER_PROP_STATE]);
}

static void
regenerate (BgeWdgtRenderer *self)
{
  BgeWdgtSpec   *spec = self->spec;
  GHashTableIter iter = { 0 };

  g_hash_table_remove_all (self->children);

  if (self->spec == NULL)
    return;

  g_hash_table_iter_init (&iter, spec->children);
  for (;;)
    {
      char      *name   = NULL;
      ChildData *data   = NULL;
      GtkWidget *widget = NULL;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &name,
              (gpointer *) &data))
        break;

      widget = g_object_new (
          data->type,
          "name", name,
          NULL);
      gtk_widget_set_parent (widget, GTK_WIDGET (self));
      g_hash_table_replace (self->children, g_strdup (name), widget);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
apply_state (BgeWdgtRenderer *self)
{
  BgeWdgtSpec   *spec       = self->spec;
  StateData     *state_data = NULL;
  GHashTableIter iter       = { 0 };

  g_clear_pointer (&self->active_state, state_data_unref);
  g_ptr_array_set_size (self->bindings, 0);

  if (self->spec == NULL ||
      self->state == NULL)
    return;

  state_data = g_hash_table_lookup (spec->states, self->state);
  if (state_data == NULL)
    {
      g_critical ("state \"%s\" doesn't exist on spec \"%s\"",
                  self->state, spec->name);
      return;
    }

  self->active_state = state_data_ref (state_data);

  g_hash_table_iter_init (&iter, state_data->setters);
  for (;;)
    {
      ValueData  *dest_value_data = NULL;
      ValueData  *src_value_data  = NULL;
      GObject    *dest_object     = NULL;
      const char *dest_property   = NULL;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &dest_value_data,
              (gpointer *) &src_value_data))
        break;

      switch (dest_value_data->kind)
        {
        case VALUE_PROPERTY:
          dest_object = g_hash_table_lookup (
              self->children,
              dest_value_data->property.child->name);
          g_assert (dest_object != NULL);
          dest_property = dest_value_data->property.prop_name;
          break;
        case VALUE_COMPONENT:
        case VALUE_CONSTANT:
        case VALUE_SPECIAL:
        default:
          g_assert_not_reached ();
        }

      switch (src_value_data->kind)
        {
        case VALUE_CONSTANT:
          g_object_set_property (
              dest_object,
              dest_property,
              &src_value_data->constant);
          break;
        case VALUE_COMPONENT:
          /* TODO */
          break;
        case VALUE_PROPERTY:
          {
            GObject  *src_object = NULL;
            GBinding *binding    = NULL;

            src_object = g_hash_table_lookup (
                self->children,
                src_value_data->property.child->name);
            g_assert (src_object != NULL);

            binding = g_object_bind_property (
                src_object, src_value_data->property.prop_name,
                dest_object, dest_property,
                G_BINDING_SYNC_CREATE);
            if (binding != NULL)
              g_ptr_array_add (self->bindings, binding);
          }
          break;
        case VALUE_SPECIAL:
          /* TODO */
          break;
        default:
          break;
        }
    }
}

/* End of bge-wdgt-spec.c */

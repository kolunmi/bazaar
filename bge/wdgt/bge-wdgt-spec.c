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

#include "bge-wdgt-spec.h"
#include "util.h"

typedef enum
{
  VALUE_CONSTANT = 0,
  VALUE_SPECIAL,
  VALUE_PROPERTY,
} ValueKind;

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
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (property.prop_name, g_free);
    BGE_RELEASE_DATA (notify, g_ptr_array_unref))

BGE_DEFINE_DATA (
    state,
    State,
    {
      char       *name;
      GHashTable *setters;
    },
    BGE_RELEASE_DATA (name, g_free);
    BGE_RELEASE_DATA (setters, g_hash_table_unref))

struct _BgeWdgtSpec
{
  GObject parent_instance;

  char *name;

  GHashTable *values;
  GHashTable *connections;
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

static void
bge_wdgt_spec_dispose (GObject *object)
{
  BgeWdgtSpec *self = BGE_WDGT_SPEC (object);

  g_clear_pointer (&self->name, g_free);

  g_clear_pointer (&self->values, g_hash_table_unref);
  g_clear_pointer (&self->connections, g_hash_table_unref);
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
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->states   = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

BgeWdgtSpec *
bge_wdgt_spec_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_SPEC, NULL);
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
  value->kind = VALUE_CONSTANT;
  g_value_copy (constant, g_value_init (&value->constant, constant->g_type));
  value->notify = g_ptr_array_new_with_free_func (value_data_unref);

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
  g_autoptr (ChildData) child_data  = NULL;
  g_autoptr (GTypeClass) type_class = NULL;
  GParamSpec *pspec                 = NULL;
  g_autoptr (ValueData) value       = NULL;

  g_return_val_if_fail (BGE_IS_WDGT_SPEC (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  g_return_val_if_fail (property != NULL, FALSE);

  if (g_hash_table_contains (self->values, name))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "value '%s' already exists", name);
      return FALSE;
    }

  child_data = g_hash_table_lookup (self->children, child);
  if (child_data == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "child '%s' doesn't exist", child);
      return FALSE;
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

/* End of bge-wdgt-spec.c */

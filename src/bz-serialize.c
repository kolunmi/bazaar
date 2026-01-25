/* bz-serialize.c
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

#include "bz-serialize.h"

/* clang-format off */
G_DEFINE_QUARK (bz-serialize-typehint-quark, bz_serialize_typehint);
/* clang-format on */

void
bz_pspec_set_list_typehint (GParamSpec *pspec,
                            GType       type)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (g_type_is_a (type, G_TYPE_OBJECT));

  g_param_spec_set_qdata (pspec, BZ_SERIALIZE_TYPEHINT, GSIZE_TO_POINTER (type));
}

GVariant *
bz_serialize_object (GObject *object)
{
  g_autoptr (GVariantBuilder) builder = NULL;
  GType type                          = G_TYPE_NONE;
  g_autoptr (GTypeClass) class        = NULL;
  guint                   n_pspecs    = 0;
  g_autofree GParamSpec **pspecs      = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);

  type   = G_OBJECT_TYPE (object);
  class  = g_type_class_ref (type);
  pspecs = g_object_class_list_properties (G_OBJECT_CLASS (class), &n_pspecs);

  for (guint i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec            = NULL;
      GValue      value            = { 0 };
      g_autoptr (GVariant) variant = NULL;

      pspec = pspecs[i];
      g_object_get_property (object, pspec->name, &value);

      if (pspec->value_type == G_TYPE_CHAR ||
          pspec->value_type == G_TYPE_UCHAR)
        variant = g_variant_new_byte (g_value_get_uchar (&value));
      else if (pspec->value_type == G_TYPE_BOOLEAN)
        variant = g_variant_new_boolean (g_value_get_boolean (&value));
      else if (pspec->value_type == G_TYPE_INT)
        variant = g_variant_new_int32 (g_value_get_int (&value));
      else if (pspec->value_type == G_TYPE_UINT)
        variant = g_variant_new_uint32 (g_value_get_uint (&value));
      else if (pspec->value_type == G_TYPE_LONG ||
               pspec->value_type == G_TYPE_INT64)
        variant = g_variant_new_int64 (g_value_get_int64 (&value));
      else if (pspec->value_type == G_TYPE_ULONG ||
               pspec->value_type == G_TYPE_UINT64)
        variant = g_variant_new_uint64 (g_value_get_uint64 (&value));
      else if (pspec->value_type == G_TYPE_ENUM)
        variant = g_variant_new_int32 (g_value_get_enum (&value));
      else if (pspec->value_type == G_TYPE_FLAGS)
        variant = g_variant_new_uint32 (g_value_get_flags (&value));
      else if (pspec->value_type == G_TYPE_FLOAT)
        variant = g_variant_new_double (g_value_get_float (&value));
      else if (pspec->value_type == G_TYPE_DOUBLE)
        variant = g_variant_new_double (g_value_get_double (&value));
      else if (pspec->value_type == G_TYPE_STRING)
        variant = g_variant_new_string (g_value_get_string (&value));
      else if (pspec->value_type == G_TYPE_VARIANT)
        variant = g_value_dup_variant (&value);
      else if (g_type_is_a (pspec->value_type, G_TYPE_LIST_MODEL))
        {
          gpointer qdata = NULL;

          qdata = g_param_spec_get_qdata (pspec, BZ_SERIALIZE_TYPEHINT);
          if (qdata != NULL)
            {
              GType       typehint                     = G_TYPE_NONE;
              GListModel *model                        = NULL;
              guint       n_items                      = 0;
              g_autoptr (GVariantBuilder) list_builder = NULL;

              typehint = GPOINTER_TO_SIZE (qdata);
              model    = g_value_get_object (&value);
              if (model != NULL)
                n_items = g_list_model_get_n_items (model);

              if (typehint == GTK_TYPE_STRING_OBJECT)
                {
                  list_builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));

                  for (guint j = 0; j < n_items; j++)
                    {
                      g_autoptr (GtkStringObject) string = NULL;

                      string = g_list_model_get_item (model, j);
                      g_variant_builder_add (list_builder, "s", gtk_string_object_get_string (string));
                    }
                }
              else
                {
                  list_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

                  for (guint j = 0; j < n_items; j++)
                    {
                      g_autoptr (GObject) list_object = NULL;

                      list_object = g_list_model_get_item (model, j);
                      g_variant_builder_add (list_builder, "v", bz_serialize_object (list_object));
                    }
                }

              variant = g_variant_builder_end (list_builder);
            }
        }
      else if (g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
        {
          GObject *object_value = NULL;

          object_value = g_value_get_object (&value);
          variant      = bz_serialize_object (object_value);
        }

      if (variant != NULL)
        g_variant_builder_add (builder, "{sv}", pspec->name,
                               g_steal_pointer (&variant));
      else
        g_critical ("Can't serialize property \"%s\" on object type %s!",
                    pspec->name, g_type_name (type));

      g_value_unset (&value);
    }

  return g_variant_builder_end (builder);
}

gpointer
bz_deserialize_object (GType     type,
                       GVariant *import,
                       GError  **error)
{
  return NULL;
}

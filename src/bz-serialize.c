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

#include "bz-async-texture.h"
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
  GType type                          = G_TYPE_NONE;
  g_autoptr (GVariantBuilder) builder = NULL;
  g_autoptr (GTypeClass) class        = NULL;
  guint                   n_pspecs    = 0;
  g_autofree GParamSpec **pspecs      = NULL;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  type = G_OBJECT_TYPE (object);
  if (type == BZ_TYPE_ASYNC_TEXTURE)
    return g_variant_new (
        "(sms)",
        bz_async_texture_get_source_uri (BZ_ASYNC_TEXTURE (object)),
        bz_async_texture_get_cache_into_path (BZ_ASYNC_TEXTURE (object)));

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
  class   = g_type_class_ref (type);
  pspecs  = g_object_class_list_properties (G_OBJECT_CLASS (class), &n_pspecs);

  for (guint i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec            = NULL;
      GValue      value            = { 0 };
      g_autoptr (GVariant) variant = NULL;

      pspec = pspecs[i];
      if (!(pspec->flags & G_PARAM_WRITABLE))
        continue;

      g_object_get_property (object, pspec->name, g_value_init (&value, pspec->value_type));

      if (g_type_is_a (pspec->value_type, G_TYPE_CHAR) ||
          g_type_is_a (pspec->value_type, G_TYPE_UCHAR))
        variant = g_variant_new_byte (g_value_get_uchar (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_BOOLEAN))
        variant = g_variant_new_boolean (g_value_get_boolean (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_INT))
        variant = g_variant_new_int32 (g_value_get_int (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_UINT))
        variant = g_variant_new_uint32 (g_value_get_uint (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_LONG) ||
               g_type_is_a (pspec->value_type, G_TYPE_INT64))
        variant = g_variant_new_int64 (g_value_get_int64 (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_ULONG) ||
               g_type_is_a (pspec->value_type, G_TYPE_UINT64))
        variant = g_variant_new_uint64 (g_value_get_uint64 (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_ENUM))
        variant = g_variant_new_int32 (g_value_get_enum (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_FLAGS))
        variant = g_variant_new_uint32 (g_value_get_flags (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_FLOAT))
        variant = g_variant_new_double (g_value_get_float (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_DOUBLE))
        variant = g_variant_new_double (g_value_get_double (&value));
      else if (g_type_is_a (pspec->value_type, G_TYPE_STRING))
        {
          const char *string = NULL;

          string  = g_value_get_string (&value);
          variant = g_variant_new_string (string != NULL ? string : "");
        }
      else if (g_type_is_a (pspec->value_type, G_TYPE_VARIANT))
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
                  list_builder = g_variant_builder_new (G_VARIANT_TYPE ("av"));

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
          if (object_value != NULL)
            variant = bz_serialize_object (object_value);
        }

      if (variant != NULL)
        g_variant_builder_add (builder, "{sv}", pspec->name,
                               g_steal_pointer (&variant));

      g_value_unset (&value);
    }

  return g_variant_builder_end (builder);
}

gpointer
bz_deserialize_object (GType     type,
                       GVariant *import,
                       GError  **error)
{
  g_autoptr (GObject) object       = NULL;
  g_autoptr (GTypeClass) class     = NULL;
  guint                   n_pspecs = 0;
  g_autofree GParamSpec **pspecs   = NULL;
  g_autoptr (GVariantIter) iter    = NULL;

  g_return_val_if_fail (g_type_is_a (type, G_TYPE_OBJECT), NULL);
  g_return_val_if_fail (import != NULL, NULL);

  if (type == BZ_TYPE_ASYNC_TEXTURE)
    {
      g_autofree char *source_uri      = NULL;
      g_autofree char *cache_into_path = NULL;
      g_autoptr (GFile) source         = NULL;
      g_autoptr (GFile) cache_into     = NULL;

      g_variant_get (import, "(sms)", &source_uri, &cache_into_path);
      source = g_file_new_for_uri (source_uri);
      if (cache_into_path != NULL)
        cache_into = g_file_new_for_path (cache_into_path);

      return bz_async_texture_new_lazy (source, cache_into);
    }

  object = g_object_new (type, NULL);

  class  = g_type_class_ref (type);
  pspecs = g_object_class_list_properties (G_OBJECT_CLASS (class), &n_pspecs);

  iter = g_variant_iter_new (import);
  if (iter == NULL)
    return NULL;

  for (;;)
    {
      gboolean         success     = TRUE;
      g_autofree char *key         = NULL;
      g_autoptr (GVariant) variant = NULL;
      const GVariantType *vtype    = NULL;

      if (!g_variant_iter_next (iter, "{sv}", &key, &variant))
        break;
      vtype = g_variant_get_type (variant);

      for (guint i = 0; i < n_pspecs; i++)
        {
          GParamSpec *pspec = NULL;

          pspec = pspecs[i];
          if (!(pspec->flags & G_PARAM_WRITABLE))
            continue;

          if (g_strcmp0 (key, pspec->name) != 0)
            continue;

          if (g_type_is_a (pspec->value_type, G_TYPE_LIST_MODEL))
            {
              gpointer qdata = NULL;

              qdata = g_param_spec_get_qdata (pspec, BZ_SERIALIZE_TYPEHINT);
              if (qdata != NULL)
                {
                  GType typehint                     = G_TYPE_NONE;
                  g_autoptr (GListStore) store       = NULL;
                  g_autoptr (GVariantIter) list_iter = NULL;

                  typehint = GPOINTER_TO_SIZE (qdata);

                  if ((typehint == GTK_TYPE_STRING_OBJECT &&
                       !g_variant_type_equal (vtype, G_VARIANT_TYPE ("as"))) ||
                      (typehint != GTK_TYPE_STRING_OBJECT &&
                       !g_variant_type_equal (vtype, G_VARIANT_TYPE ("av"))))
                    {
                      success = FALSE;
                      break;
                    }

                  list_iter = g_variant_iter_new (variant);
                  if (typehint == GTK_TYPE_STRING_OBJECT)
                    {
                      for (;;)
                        {
                          g_autofree char *string                   = NULL;
                          g_autoptr (GtkStringObject) string_object = NULL;

                          if (!g_variant_iter_next (list_iter, "s", &string))
                            break;

                          string_object = gtk_string_object_new (string);
                          g_list_store_append (store, string_object);
                        }
                    }
                  else
                    {
                      for (;;)
                        {
                          g_autoptr (GVariant) list_variant = NULL;
                          g_autoptr (GObject) parsed        = NULL;

                          if (!g_variant_iter_next (list_iter, "v", &list_variant))
                            break;

                          parsed = bz_deserialize_object (typehint, list_variant, NULL);
                          g_list_store_append (store, parsed);
                        }
                    }
                }
            }
          else if (g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
            {
              g_autoptr (GObject) parsed = NULL;

              parsed = bz_deserialize_object (pspec->value_type, variant, NULL);
              g_object_set (object, pspec->name, parsed, NULL);
            }
          else
            {

#define HANDLE_TYPE(_gtype, _vtype, _get, ...)                                                   \
  G_STMT_START                                                                                   \
  {                                                                                              \
    if (g_type_is_a (pspec->value_type, (_gtype)))                                               \
      {                                                                                          \
        if (!g_variant_type_equal (vtype, (_vtype)))                                             \
          {                                                                                      \
            success = FALSE;                                                                     \
            break;                                                                               \
          }                                                                                      \
        g_object_set (object, pspec->name, g_variant_get_##_get (variant, ##__VA_ARGS__), NULL); \
        break;                                                                                   \
      }                                                                                          \
  }                                                                                              \
  G_STMT_END

              HANDLE_TYPE (G_TYPE_BOOLEAN, G_VARIANT_TYPE_BOOLEAN, boolean);
              HANDLE_TYPE (G_TYPE_INT, G_VARIANT_TYPE_INT32, int32);
              HANDLE_TYPE (G_TYPE_ENUM, G_VARIANT_TYPE_INT32, int32);
              HANDLE_TYPE (G_TYPE_INT64, G_VARIANT_TYPE_INT64, int64);
              HANDLE_TYPE (G_TYPE_UINT, G_VARIANT_TYPE_UINT32, uint32);
              HANDLE_TYPE (G_TYPE_FLAGS, G_VARIANT_TYPE_UINT32, uint32);
              HANDLE_TYPE (G_TYPE_UINT64, G_VARIANT_TYPE_UINT64, uint64);
              HANDLE_TYPE (G_TYPE_DOUBLE, G_VARIANT_TYPE_DOUBLE, double);
              HANDLE_TYPE (G_TYPE_FLOAT, G_VARIANT_TYPE_DOUBLE, double);
              HANDLE_TYPE (G_TYPE_STRING, G_VARIANT_TYPE_STRING, string, NULL);

#undef HANDLE_TYPE
            }

          break;
        }

      if (!success)
        g_warning ("Couldn't deserialize property \"%s\" into object of type %s",
                   key, g_type_name (type));
    }

  return g_steal_pointer (&object);
}

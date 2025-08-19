/* bz-lazy-async-texture-model.c
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

#include "bz-lazy-async-texture-model.h"
#include "bz-async-texture.h"

struct _BzLazyAsyncTextureModel
{
  GObject parent_instance;

  GListModel *model;
};

static void list_model_iface_init (GListModelInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzLazyAsyncTextureModel,
    bz_lazy_async_texture_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init));

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
items_changed (BzLazyAsyncTextureModel *self,
               guint                    position,
               guint                    removed,
               guint                    added,
               GListModel              *model);

static void
bz_lazy_async_texture_model_dispose (GObject *object)
{
  BzLazyAsyncTextureModel *self = BZ_LAZY_ASYNC_TEXTURE_MODEL (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_pointer (&self->model, g_object_unref);

  G_OBJECT_CLASS (bz_lazy_async_texture_model_parent_class)->dispose (object);
}

static void
bz_lazy_async_texture_model_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  BzLazyAsyncTextureModel *self = BZ_LAZY_ASYNC_TEXTURE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_lazy_async_texture_model_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_lazy_async_texture_model_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  BzLazyAsyncTextureModel *self = BZ_LAZY_ASYNC_TEXTURE_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_lazy_async_texture_model_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_lazy_async_texture_model_class_init (BzLazyAsyncTextureModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_lazy_async_texture_model_set_property;
  object_class->get_property = bz_lazy_async_texture_model_get_property;
  object_class->dispose      = bz_lazy_async_texture_model_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static GType
list_model_get_item_type (GListModel *list)
{
  return BZ_TYPE_ASYNC_TEXTURE;
}

static guint
list_model_get_n_items (GListModel *list)
{
  BzLazyAsyncTextureModel *self = BZ_LAZY_ASYNC_TEXTURE_MODEL (list);
  guint                    ret  = 0;

  if (self->model != NULL)
    ret = g_list_model_get_n_items (self->model);

  return ret;
}

static gpointer
list_model_get_item (GListModel *list,
                     guint       position)
{
  BzLazyAsyncTextureModel *self  = BZ_LAZY_ASYNC_TEXTURE_MODEL (list);
  g_autoptr (BzAsyncTexture) ret = NULL;

  if (self->model != NULL)
    {
      ret = g_list_model_get_item (self->model, position);
      bz_async_texture_ensure (ret);
    }

  return g_steal_pointer (&ret);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

static void
bz_lazy_async_texture_model_init (BzLazyAsyncTextureModel *self)
{
}

BzLazyAsyncTextureModel *
bz_lazy_async_texture_model_new (void)
{
  return g_object_new (BZ_TYPE_LAZY_ASYNC_TEXTURE_MODEL, NULL);
}

GListModel *
bz_lazy_async_texture_model_get_model (BzLazyAsyncTextureModel *self)
{
  g_return_val_if_fail (BZ_IS_LAZY_ASYNC_TEXTURE_MODEL (self), NULL);
  return self->model;
}

void
bz_lazy_async_texture_model_set_model (BzLazyAsyncTextureModel *self,
                                       GListModel              *model)
{
  guint had_n_items  = 0;
  guint have_n_items = 0;

  g_return_if_fail (BZ_IS_LAZY_ASYNC_TEXTURE_MODEL (self));

  if (model == self->model)
    return;

  if (self->model != NULL)
    {
      had_n_items = g_list_model_get_n_items (self->model);
      g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
    }
  g_clear_pointer (&self->model, g_object_unref);

  if (model != NULL)
    {
      self->model = g_object_ref (model);

      have_n_items = g_list_model_get_n_items (model);
      g_signal_connect_swapped (model, "items-changed", G_CALLBACK (items_changed), self);
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, had_n_items, have_n_items);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

static void
items_changed (BzLazyAsyncTextureModel *self,
               guint                    position,
               guint                    removed,
               guint                    added,
               GListModel              *model)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

/* End of bz-lazy-async-texture-model.c */

/* bz-async-texture.c
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

#include "config.h"

#include <glycin-gtk4.h>
#include <libdex.h>

#include "bz-async-texture.h"
#include "bz-env.h"
#include "bz-global-state.h"
#include "bz-util.h"

struct _BzAsyncTexture
{
  GObject parent_instance;

  GFile   *source;
  gboolean loaded;

  gboolean      lazy;
  DexFuture    *task;
  GdkPaintable *paintable;
};

static void paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
    BzAsyncTexture,
    bz_async_texture,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, paintable_iface_init))

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_LOADED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    load,
    Load,
    { GFile *file; },
    BZ_RELEASE_DATA (file, g_object_unref));
static DexFuture *
load_fiber (LoadData *data);
static DexFuture *
load_finally (DexFuture      *future,
              BzAsyncTexture *self);

static void
load (BzAsyncTexture *self);

static void
bz_async_texture_dispose (GObject *object)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  g_clear_object (&self->source);
  dex_clear (&self->task);
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_async_texture_parent_class)->dispose (object);
}

static void
bz_async_texture_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;
    case PROP_LOADED:
      g_value_set_boolean (value, self->loaded);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_async_texture_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  // BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
    case PROP_LOADED:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_async_texture_class_init (BzAsyncTextureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_async_texture_dispose;
  object_class->get_property = bz_async_texture_get_property;
  object_class->set_property = bz_async_texture_set_property;

  props[PROP_SOURCE] =
      g_param_spec_object (
          "source",
          NULL, NULL,
          G_TYPE_FILE,
          G_PARAM_READABLE);

  props[PROP_LOADED] =
      g_param_spec_boolean (
          "loaded",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_async_texture_init (BzAsyncTexture *self)
{
  self->paintable = gdk_paintable_new_empty (0, 0);
}

static void
paintable_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  return gdk_paintable_snapshot (self->paintable, snapshot, width, height);
}

static GdkPaintable *
paintable_get_current_image (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  return gdk_paintable_get_current_image (self->paintable);
}

static GdkPaintableFlags
paintable_get_flags (GdkPaintable *paintable)
{
  // BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);
  return 0;
}

static int
paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  if (self->loaded)
    return gdk_texture_get_width (GDK_TEXTURE (self->paintable));
  else
    return 0.0;
}

static int
paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  if (self->loaded)
    return gdk_texture_get_height (GDK_TEXTURE (self->paintable));
  else
    return 0.0;
}

static double
paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (paintable);

  if (self->lazy && !self->loaded && self->task == NULL)
    load (self);

  if (self->loaded)
    {
      int width  = 0;
      int height = 0;

      width  = gdk_texture_get_width (GDK_TEXTURE (self->paintable));
      height = gdk_texture_get_height (GDK_TEXTURE (self->paintable));

      return height > 0 ? (double) width / (double) height : 0.0;
    }
  else
    return 0.0;
}

static void
paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot                   = paintable_snapshot;
  iface->get_current_image          = paintable_get_current_image;
  iface->get_flags                  = paintable_get_flags;
  iface->get_intrinsic_width        = paintable_get_intrinsic_width;
  iface->get_intrinsic_height       = paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = paintable_get_intrinsic_aspect_ratio;
}

BzAsyncTexture *
bz_async_texture_new (GFile *source)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);

  self         = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source = g_object_ref (source);
  self->lazy   = FALSE;

  load (self);

  return self;
}

BzAsyncTexture *
bz_async_texture_new_lazy (GFile *source)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);

  self         = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source = g_object_ref (source);
  self->lazy   = TRUE;

  return self;
}

gboolean
bz_async_texture_get_loaded (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->loaded;
}

GdkTexture *
bz_async_texture_get_texture (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), NULL);
  g_return_val_if_fail (self->loaded, NULL);
  return GDK_TEXTURE (self->paintable);
}

static void
load (BzAsyncTexture *self)
{
  g_autoptr (LoadData) data    = NULL;
  g_autoptr (DexFuture) future = NULL;

  data       = load_data_new ();
  data->file = g_object_ref (self->source);

  future = dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) load_fiber,
      load_data_ref (data), load_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) load_finally,
      self, NULL);
  self->task = g_steal_pointer (&future);
}

static DexFuture *
load_fiber (LoadData *data)
{
  /* TODO: use `gly_loader_new_for_stream` with libsoup once glycin-2 releases */

  GFile *file                     = data->file;
  g_autoptr (GError) local_error  = NULL;
  g_autofree char *uri            = NULL;
  g_autofree char *uri_scheme     = NULL;
  gboolean         is_http        = FALSE;
  g_autoptr (GFileIOStream) io    = NULL;
  g_autoptr (GFile) dl_tmp_file   = NULL;
  GOutputStream *dl_tmp_file_dest = NULL;
  g_autoptr (SoupMessage) message = NULL;
  guint64  bytes_written          = 0;
  gboolean result                 = FALSE;
  g_autoptr (GlyLoader) loader    = NULL;
  g_autoptr (GlyImage) image      = NULL;
  g_autoptr (GlyFrame) frame      = NULL;
  g_autoptr (GdkTexture) texture  = NULL;

  uri        = g_file_get_uri (file);
  uri_scheme = g_file_get_uri_scheme (file);
  is_http    = g_str_has_prefix (uri_scheme, "http");

  if (is_http)
    {
      dl_tmp_file = g_file_new_tmp (NULL, &io, &local_error);
      if (dl_tmp_file == NULL)
        goto done;
      dl_tmp_file_dest = g_io_stream_get_output_stream (G_IO_STREAM (io));

      message       = soup_message_new (SOUP_METHOD_GET, uri);
      bytes_written = dex_await_uint64 (
          bz_send_with_global_http_session_then_splice_into (message, dl_tmp_file_dest),
          &local_error);
      if (local_error != NULL)
        goto done;

      result = g_io_stream_close (G_IO_STREAM (io), NULL, &local_error);
      if (!result)
        goto done;

      loader = gly_loader_new (dl_tmp_file);
    }
  else
    loader = gly_loader_new (file);

  image = gly_loader_load (loader, &local_error);
  if (image == NULL)
    goto done;

  frame = gly_image_next_frame (image, &local_error);
  if (frame == NULL)
    goto done;

  texture = gly_gtk_frame_get_texture (frame);

done:
  if (is_http)
    dex_future_disown (dex_file_delete (dl_tmp_file, G_PRIORITY_DEFAULT));

  if (texture != NULL)
    return dex_future_new_for_object (texture);
  else if (local_error != NULL)
    {
      g_critical (
          "Failed to download file with "
          "uri '%s' into texture: %s",
          uri, local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }
  else
    {
      g_critical (
          "Failed to download file with "
          "uri '%s' into texture: unknown cause",
          uri);
      return dex_future_new_reject (
          G_IO_ERROR,
          G_IO_ERROR_FAILED,
          "IO failed");
    }
}

static DexFuture *
load_finally (DexFuture      *future,
              BzAsyncTexture *self)
{
  const GValue *value = NULL;

  value = dex_future_get_value (future, NULL);
  if (value != NULL)
    {
      g_clear_object (&self->paintable);
      self->paintable = g_value_dup_object (value);

      self->loaded = TRUE;
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOADED]);
    }

  return NULL;
}

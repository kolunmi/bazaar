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

#define G_LOG_DOMAIN "BAZAAR::ASYNC-TEXTURE"

#define MAX_CONCURRENT_LOADS   32
#define CACHE_INVALID_AGE      (G_TIME_SPAN_DAY * 1)
#define HTTP_TIMEOUT_SECONDS   5
#define MAX_LOAD_RETRIES       3
#define RETRY_INTERVAL_SECONDS 5

#include "config.h"

#include <glycin-gtk4-2/glycin-gtk4.h>
#include <libdex.h>

#include "bz-async-texture.h"
#include "bz-download-worker.h"
#include "bz-env.h"
#include "bz-io.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    load,
    Load,
    {
      GFile        *source;
      char         *source_uri;
      GFile        *cache_into;
      char         *cache_into_path;
      GCancellable *cancellable;
      int           retries;
      GWeakRef      self;
    },
    BZ_RELEASE_DATA (source, g_object_unref);
    BZ_RELEASE_DATA (source_uri, g_free);
    BZ_RELEASE_DATA (cache_into, g_object_unref);
    BZ_RELEASE_DATA (cache_into_path, g_free);
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    g_weak_ref_clear (&self->self);)

struct _BzAsyncTexture
{
  GObject parent_instance;

  GFile   *source;
  char    *source_uri;
  GFile   *cache_into;
  char    *cache_into_path;
  gboolean lazy;

  DexFuture    *task;
  GCancellable *cancellable;

  int        retries;
  DexFuture *retry_future;

  GdkPaintable *paintable;
  GMutex        texture_mutex;
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
  PROP_CACHE_INTO,
  PROP_LOADED,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static DexFuture *
load_fiber_work (LoadData *data);

static DexFuture *
load_finally (DexFuture *future,
              LoadData  *data);

static void
maybe_load (BzAsyncTexture *self);

static DexFuture *
retry_cb (DexFuture *future,
          LoadData  *data);

static gboolean
idle_notify (BzAsyncTexture *self);

static void
bz_async_texture_dispose (GObject *object)
{
  BzAsyncTexture *self = BZ_ASYNC_TEXTURE (object);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  dex_clear (&self->task);
  g_clear_object (&self->cancellable);
  dex_clear (&self->retry_future);

  g_clear_object (&self->source);
  g_clear_pointer (&self->source_uri, g_free);
  g_clear_object (&self->cache_into);
  g_clear_pointer (&self->cache_into_path, g_free);
  g_clear_object (&self->paintable);
  g_mutex_clear (&self->texture_mutex);

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
      g_value_set_object (value, bz_async_texture_get_source (self));
      break;
    case PROP_CACHE_INTO:
      g_value_set_object (value, bz_async_texture_get_cache_into (self));
      break;
    case PROP_LOADED:
      g_value_set_boolean (value, bz_async_texture_get_loaded (self));
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
    case PROP_CACHE_INTO:
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

  props[PROP_CACHE_INTO] =
      g_param_spec_object (
          "cache-into",
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
  self->retries   = 0;
  self->paintable = NULL;
  g_mutex_init (&self->texture_mutex);
}

static void
paintable_snapshot (GdkPaintable *paintable,
                    GdkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->paintable != NULL)
    gdk_paintable_snapshot (self->paintable, snapshot, width, height);
}

static GdkPaintable *
paintable_get_current_image (GdkPaintable *paintable)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->paintable != NULL)
    return gdk_paintable_get_current_image (self->paintable);

  return NULL;
}

static GdkPaintableFlags
paintable_get_flags (GdkPaintable *paintable)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);
  return 0;
}

static int
paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->paintable != NULL)
    return gdk_paintable_get_intrinsic_width (self->paintable);

  return 0;
}

static int
paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->paintable != NULL)
    return gdk_paintable_get_intrinsic_height (self->paintable);

  return 0;
}

static double
paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  BzAsyncTexture *self            = BZ_ASYNC_TEXTURE (paintable);
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->paintable != NULL)
    return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

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
bz_async_texture_new (GFile *source,
                      GFile *cache_into)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);
  g_return_val_if_fail (cache_into == NULL || G_IS_FILE (cache_into), NULL);

  self                  = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source          = g_object_ref (source);
  self->source_uri      = g_file_get_uri (source);
  self->cache_into      = cache_into != NULL ? g_object_ref (cache_into) : NULL;
  self->cache_into_path = cache_into != NULL ? g_file_get_path (cache_into) : NULL;
  self->lazy            = FALSE;

  maybe_load (self);
  return self;
}

BzAsyncTexture *
bz_async_texture_new_lazy (GFile *source,
                           GFile *cache_into)
{
  BzAsyncTexture *self = NULL;

  g_return_val_if_fail (G_IS_FILE (source), NULL);
  g_return_val_if_fail (cache_into == NULL || G_IS_FILE (cache_into), NULL);

  self                  = g_object_new (BZ_TYPE_ASYNC_TEXTURE, NULL);
  self->source          = g_object_ref (source);
  self->source_uri      = g_file_get_uri (source);
  self->cache_into      = cache_into != NULL ? g_object_ref (cache_into) : NULL;
  self->cache_into_path = cache_into != NULL ? g_file_get_path (cache_into) : NULL;
  self->lazy            = TRUE;

  return self;
}

GFile *
bz_async_texture_get_source (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->source;
}

const char *
bz_async_texture_get_source_uri (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->source_uri;
}

GFile *
bz_async_texture_get_cache_into (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->cache_into;
}

const char *
bz_async_texture_get_cache_into_path (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->cache_into_path;
}

gboolean
bz_async_texture_get_loaded (BzAsyncTexture *self)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);

  locker = g_mutex_locker_new (&self->texture_mutex);
  return GDK_IS_TEXTURE (self->paintable);
}

GdkTexture *
bz_async_texture_dup_texture (BzAsyncTexture *self)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), NULL);

  locker = g_mutex_locker_new (&self->texture_mutex);
  if (GDK_IS_TEXTURE (self->paintable))
    return (GdkTexture *) g_object_ref (self->paintable);
  else
    return NULL;
}

DexFuture *
bz_async_texture_dup_future (BzAsyncTexture *self)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), NULL);

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);

  if (self->task != NULL)
    return dex_ref (self->task);
  else if (GDK_IS_TEXTURE (self->paintable))
    return dex_future_new_for_object (self->paintable);
  else
    return dex_future_new_reject (
        G_IO_ERROR,
        G_IO_ERROR_FAILED,
        "texture is in an invalid state");
}

void
bz_async_texture_ensure (BzAsyncTexture *self)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_if_fail (BZ_IS_ASYNC_TEXTURE (self));

  locker = g_mutex_locker_new (&self->texture_mutex);
  maybe_load (self);
}

void
bz_async_texture_cancel (BzAsyncTexture *self)
{
  g_return_if_fail (BZ_IS_ASYNC_TEXTURE (self));

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  dex_clear (&self->task);
  g_clear_object (&self->cancellable);
  self->retries = G_MAXINT;
}

gboolean
bz_async_texture_is_loading (BzAsyncTexture *self)
{
  g_return_val_if_fail (BZ_IS_ASYNC_TEXTURE (self), FALSE);
  return self->task != NULL && dex_future_is_pending (self->task);
}

static void
maybe_load (BzAsyncTexture *self)
{
  g_autoptr (LoadData) data    = NULL;
  g_autoptr (DexFuture) future = NULL;

  if (GDK_IS_TEXTURE (self->paintable) ||
      (self->task != NULL && dex_future_is_pending (self->task)) ||
      self->retries >= MAX_LOAD_RETRIES)
    return;

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  dex_clear (&self->task);
  g_clear_object (&self->cancellable);

  self->cancellable = g_cancellable_new ();

  data                  = load_data_new ();
  data->source          = g_object_ref (self->source);
  data->source_uri      = g_strdup (self->source_uri);
  data->cache_into      = self->cache_into != NULL ? g_object_ref (self->cache_into) : NULL;
  data->cache_into_path = self->cache_into_path != NULL ? g_strdup (self->cache_into_path) : NULL;
  data->cancellable     = g_object_ref (self->cancellable);
  data->retries         = self->retries;
  g_weak_ref_init (&data->self, self);

  future = dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) load_fiber_work,
      load_data_ref (data), load_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) load_finally,
      load_data_ref (data), load_data_unref);
  self->task = g_steal_pointer (&future);
}

static DexFuture *
load_fiber_work (LoadData *data)
{
  static GMutex   queueing_mutex                       = { 0 };
  static guint    concurrent_loads                     = 0;
  static guint    ongoing_queued[MAX_CONCURRENT_LOADS] = { 0 };
  static BzGuard *gates[MAX_CONCURRENT_LOADS]          = { 0 };
  static GMutex   mutexes[MAX_CONCURRENT_LOADS]        = { 0 };

  GFile        *source                  = data->source;
  char         *source_uri              = data->source_uri;
  GFile        *cache_into              = data->cache_into;
  char         *cache_into_path         = data->cache_into_path;
  GCancellable *cancellable             = data->cancellable;
  gboolean      result                  = FALSE;
  g_autoptr (GError) local_error        = NULL;
  g_autoptr (GMutexLocker) locker       = NULL;
  g_autoptr (BzGuard) slot_guard        = NULL;
  guint    slot_queued                  = G_MAXUINT;
  guint    slot_index                   = 0;
  gboolean is_http                      = FALSE;
  g_autoptr (GDateTime) now             = NULL;
  g_autofree char *async_tex_data_path  = NULL;
  g_autoptr (GFile) async_tex_data_file = NULL;
  g_autoptr (GdkTexture) texture        = NULL;
  g_autoptr (GlyFrame) frame            = NULL;

  locker = g_mutex_locker_new (&queueing_mutex);
  if (concurrent_loads == 0)
    {
      /* Ensure we don't overload the system with work; aim for # of logical
         processors divided by 2

        See:
          https://github.com/kolunmi/bazaar/issues/497
          https://docs.gtk.org/glib/func.get_num_processors.html

        Eva Thu, 23 Oct 2025 14:19:44 -0700
        */
      concurrent_loads = MIN (
          MAX_CONCURRENT_LOADS,
          MAX (1, g_get_num_processors () / 2));

      g_debug ("Allowing %d concurrent texture loads", concurrent_loads);
    }
  g_clear_pointer (&locker, g_mutex_locker_free);

#define RATE_LIMIT_BEGIN()                                  \
  G_STMT_START                                              \
  {                                                         \
    locker = g_mutex_locker_new (&queueing_mutex);          \
                                                            \
    /* Rate limiting to reduce competition for resources */ \
    for (guint i = 0; i < concurrent_loads; i++)            \
      {                                                     \
        if (ongoing_queued[i] < slot_queued)                \
          {                                                 \
            slot_queued = ongoing_queued[i];                \
            slot_index  = i;                                \
          }                                                 \
      }                                                     \
    ongoing_queued[slot_index]++;                           \
    g_clear_pointer (&locker, g_mutex_locker_free);         \
                                                            \
    BZ_BEGIN_GUARD_WITH_CONTEXT (&slot_guard,               \
                                 &mutexes[slot_index],      \
                                 &gates[slot_index]);       \
                                                            \
    locker = g_mutex_locker_new (&queueing_mutex);          \
    ongoing_queued[slot_index]--;                           \
    g_clear_pointer (&locker, g_mutex_locker_free);         \
  }                                                         \
  G_STMT_END

#define RATE_LIMIT_END() bz_clear_guard (&slot_guard)

  is_http = g_str_has_prefix (source_uri, "http");
  now     = g_date_time_new_now_utc ();
  if (cache_into != NULL)
    {
      async_tex_data_path = g_strdup_printf ("%s.bz-async-texture-data", cache_into_path);
      async_tex_data_file = g_file_new_for_path (async_tex_data_path);
    }

  if (cache_into != NULL &&
      g_file_query_exists (cache_into, NULL) &&
      g_file_query_exists (async_tex_data_file, NULL))
    {
      g_autoptr (GBytes) bytes     = NULL;
      g_autoptr (GVariant) variant = NULL;
      GTimeSpan age_span           = 0;
      g_autoptr (GlyLoader) loader = NULL;
      g_autoptr (GlyImage) image   = NULL;

      bytes = g_file_load_bytes (async_tex_data_file, NULL, NULL, &local_error);
      if (bytes != NULL)
        variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("a{sv}"), bytes, FALSE);
      if (variant != NULL)
        {
          gint64 birth_unix_stamp               = 0;
          g_autoptr (GDateTime) birth_date_time = NULL;

          if (g_variant_lookup (
                  variant,
                  "birth-unix-stamp", "x",
                  &birth_unix_stamp))
            {
              birth_date_time = g_date_time_new_from_unix_utc (birth_unix_stamp);
              age_span        = g_date_time_difference (now, birth_date_time);
            }
          else
            local_error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "key \"birth-unix-stamp\" was not found");
        }

      if (variant != NULL && age_span > 0)
        {
          if (age_span < CACHE_INVALID_AGE)
            {
              RATE_LIMIT_BEGIN ();

              loader = gly_loader_new (cache_into);
              /* We assume we exported this file, so uhhh it is safe to
                 not use sandboxing, since it is faster :-) */
              gly_loader_set_sandbox_selector (loader, GLY_SANDBOX_SELECTOR_NOT_SANDBOXED);

              image = gly_loader_load (loader, &local_error);
              if (image != NULL)
                frame = gly_image_next_frame (image, &local_error);

              RATE_LIMIT_END ();
            }
          else
            g_debug ("Metadata file %s for cached texture at %s indicates this resource is too old (GTimeSpan: %zu), "
                     "reaping and fetching from original source at %s instead",
                     async_tex_data_path, cache_into_path, age_span, source_uri);
        }
      else
        {
          g_warning ("Couldn't load associated metadata file %s for cached texture at %s, "
                     "reaping and fetching from original source at %s instead: %s",
                     async_tex_data_path, cache_into_path, source_uri, local_error->message);
          g_clear_pointer (&local_error, g_error_free);
        }

      if (frame == NULL)
        {
          if (local_error != NULL)
            g_warning ("An attempt to revive cached texture at %s has failed, "
                       "reaping and fetching from original source at %s instead: %s",
                       cache_into_path, source_uri, local_error->message);
          g_clear_pointer (&local_error, g_error_free);

          if (!g_file_delete (cache_into, NULL, &local_error))
            {
              g_critical ("Couldn't reap cached texture at %s, this "
                          "might lead to unexpected behavior: %s",
                          cache_into_path, local_error->message);
              g_clear_pointer (&local_error, g_error_free);
            }
        }
    }

  if (frame == NULL)
    {
      g_autoptr (GFile) load_file  = NULL;
      g_autoptr (GlyLoader) loader = NULL;
      g_autoptr (GlyImage) image   = NULL;

      if (cache_into != NULL)
        {
          g_autoptr (GFile) parent = NULL;
          gboolean reconstruct     = FALSE;

          parent = g_file_get_parent (cache_into);

          if (g_file_query_exists (parent, NULL))
            {
              GFileType parent_type = G_FILE_TYPE_UNKNOWN;

              parent_type = g_file_query_file_type (parent, G_FILE_QUERY_INFO_NONE, NULL);
              if (parent_type != G_FILE_TYPE_DIRECTORY)
                {
                  reconstruct = TRUE;

                  result = g_file_delete (parent, cancellable, &local_error);
                  if (!result)
                    return dex_future_new_for_error (g_steal_pointer (&local_error));
                }
            }
          else
            reconstruct = TRUE;

          if (reconstruct)
            {
              result = g_file_make_directory_with_parents (
                  parent, cancellable, &local_error);
              if (!result)
                {
                  if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                    g_clear_pointer (&local_error, g_error_free);
                  else
                    return dex_future_new_for_error (g_steal_pointer (&local_error));
                }
            }
        }

      if (is_http)
        {
          if (cache_into != NULL)
            load_file = g_object_ref (cache_into);
          else
            {
              g_autofree char *basename    = NULL;
              g_autofree char *tmpl        = NULL;
              g_autoptr (GFileIOStream) io = NULL;

              basename  = g_file_get_basename (source);
              tmpl      = g_strdup_printf ("XXXXXX-%s", basename);
              load_file = g_file_new_tmp (tmpl, &io, &local_error);
              if (load_file == NULL)
                return dex_future_new_for_error (g_steal_pointer (&local_error));
              g_io_stream_close (G_IO_STREAM (io), NULL, NULL);
            }

          result = dex_await (
              dex_future_first (
                  bz_download_worker_invoke (
                      bz_download_worker_get_default (),
                      source, load_file),
                  /* increase the timeout as more failures stack up */
                  dex_timeout_new_seconds ((data->retries + 1) * HTTP_TIMEOUT_SECONDS),
                  NULL),
              &local_error);
          if (!result)
            return dex_future_new_for_error (g_steal_pointer (&local_error));
        }
      else
        {
          if (cache_into != NULL)
            {
              result = g_file_copy (
                  source, cache_into,
                  G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA,
                  cancellable, NULL, NULL, &local_error);
              if (!result)
                return dex_future_new_for_error (g_steal_pointer (&local_error));

              load_file = g_object_ref (cache_into);
            }
          else
            load_file = g_object_ref (source);
        }

      RATE_LIMIT_BEGIN ();

      loader = gly_loader_new (load_file);
#ifdef SANDBOXED_LIBFLATPAK
      gly_loader_set_sandbox_selector (loader, GLY_SANDBOX_SELECTOR_NOT_SANDBOXED);
#endif

      image = gly_loader_load (loader, &local_error);
      if (is_http && cache_into == NULL)
        /* delete tmp file */
        g_file_delete (load_file, NULL, NULL);
      if (image == NULL || local_error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      frame = gly_image_next_frame (image, &local_error);
      if (frame == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      RATE_LIMIT_END ();

      if (async_tex_data_file != NULL)
        {
          g_autoptr (GVariantBuilder) builder  = NULL;
          g_autoptr (GVariant) variant         = NULL;
          g_autoptr (GBytes) bytes             = NULL;
          g_autoptr (GFileOutputStream) output = NULL;

          builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
          g_variant_builder_add (
              builder,
              "{sv}",
              "birth-unix-stamp",
              g_variant_new_int64 (g_date_time_to_unix (now)));

          variant = g_variant_builder_end (builder);
          bytes   = g_variant_get_data_as_bytes (variant);

          output = g_file_replace (
              async_tex_data_file,
              NULL,
              FALSE,
              G_FILE_CREATE_REPLACE_DESTINATION,
              NULL,
              &local_error);
          if (output != NULL)
            {
              gssize bytes_written = 0;

              bytes_written = g_output_stream_write_bytes (G_OUTPUT_STREAM (output), bytes, NULL, &local_error);
              if (bytes_written > 0)
                g_output_stream_close (G_OUTPUT_STREAM (output), NULL, &local_error);
            }

          if (local_error != NULL)
            g_critical ("Failed to write async-tex cache metadata to %s ;"
                        "The image will be fully reloaded next time: %s",
                        async_tex_data_path, local_error->message);
          g_clear_pointer (&local_error, g_error_free);
        }
    }

  texture = gly_gtk_frame_get_texture (frame);
  if (texture == NULL)
    return dex_future_new_reject (
        G_IO_ERROR,
        G_IO_ERROR_FAILED,
        "texture loading failed");

  return dex_future_new_for_object (texture);
}

static DexFuture *
load_finally (DexFuture *future,
              LoadData  *data)
{
  g_autoptr (GError) local_error  = NULL;
  g_autoptr (BzAsyncTexture) self = NULL;
  g_autoptr (GMutexLocker) locker = NULL;

  bz_weak_get_or_return_reject (self, &data->self);

  locker = g_mutex_locker_new (&self->texture_mutex);
  dex_clear (&self->task);

  if (dex_future_is_resolved (future))
    {
      g_clear_object (&self->paintable);
      self->paintable = g_value_dup_object (dex_future_get_value (future, NULL));

      g_idle_add_full (
          G_PRIORITY_DEFAULT_IDLE,
          (GSourceFunc) idle_notify,
          g_object_ref (self), g_object_unref);

      return dex_future_new_for_object (self->paintable);
    }
  else
    {
      dex_future_get_value (future, &local_error);
      if (self->retries < MAX_LOAD_RETRIES)
        {
          if (self->retries == MAX_LOAD_RETRIES - 1)
            g_warning ("Loading %s failed: %s. Retrying in %d seconds. This will "
                       "be the last retry, after which this texture will remain invalid",
                       self->source_uri,
                       local_error->message,
                       RETRY_INTERVAL_SECONDS);
          else
            g_warning ("Loading %s failed: %s. Retrying in %d seconds. Retries left: %d",
                       self->source_uri,
                       local_error->message,
                       RETRY_INTERVAL_SECONDS,
                       MAX_LOAD_RETRIES - self->retries);
          self->retries++;

          dex_clear (&self->retry_future);
          self->retry_future = dex_future_then (
              dex_timeout_new_seconds (RETRY_INTERVAL_SECONDS),
              (DexFutureCallback) retry_cb,
              load_data_ref (data), load_data_unref);
        }

      return dex_ref (future);
    }
}

static DexFuture *
retry_cb (DexFuture *future,
          LoadData  *data)
{
  g_autoptr (BzAsyncTexture) self = NULL;
  g_autoptr (GMutexLocker) locker = NULL;

  bz_weak_get_or_return_reject (self, &data->self);

  locker = g_mutex_locker_new (&self->texture_mutex);
  dex_clear (&self->retry_future);

  maybe_load (self);
  return NULL;
}

static gboolean
idle_notify (BzAsyncTexture *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOADED]);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));

  return G_SOURCE_REMOVE;
}

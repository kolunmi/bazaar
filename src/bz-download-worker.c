/* bz-download-worker.c
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

#include "bz-download-worker.h"
#include "bz-env.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    mutex_box,
    MutexBox,
    {
      GMutex     m;
      DexFuture *g;
    },
    BZ_RELEASE_DATA (g, dex_unref);
    g_mutex_clear (&self->m);)

struct _BzDownloadWorker
{
  GObject parent_instance;

  char *name;

  GSubprocess  *subprocess;
  MutexBoxData *stdin_mutex;
  GHashTable   *waiting;
  MutexBoxData *waiting_mutex;
  DexFuture    *task;
};

static void
initable_iface_init (GInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzDownloadWorker,
    bz_download_worker,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

enum
{
  PROP_0,

  PROP_NAME,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    monitor_worker,
    MonitorWorker,
    {
      GSubprocess  *subprocess;
      GHashTable   *waiting;
      MutexBoxData *waiting_mutex;
    },
    BZ_RELEASE_DATA (subprocess, g_object_unref);
    BZ_RELEASE_DATA (waiting, g_hash_table_unref);
    BZ_RELEASE_DATA (waiting_mutex, mutex_box_data_unref));
static DexFuture *
monitor_worker_fiber (MonitorWorkerData *data);

BZ_DEFINE_DATA (
    invoke_worker,
    InvokeWorker,
    {
      DexPromise   *promise;
      GFile        *src;
      GFile        *dest;
      GSubprocess  *subprocess;
      MutexBoxData *stdin_mutex;
      GHashTable   *waiting;
      MutexBoxData *waiting_mutex;
    },
    BZ_RELEASE_DATA (promise, dex_unref);
    BZ_RELEASE_DATA (src, g_object_unref);
    BZ_RELEASE_DATA (dest, g_object_unref);
    BZ_RELEASE_DATA (subprocess, g_object_unref);
    BZ_RELEASE_DATA (stdin_mutex, mutex_box_data_unref);
    BZ_RELEASE_DATA (waiting, g_hash_table_unref);
    BZ_RELEASE_DATA (waiting_mutex, mutex_box_data_unref));
static DexFuture *
invoke_worker_fiber (InvokeWorkerData *data);

static void
plumb_data_input_stream_read_line_async (GDataInputStream   *stream,
                                         GCancellable       *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer            user_data);

static char *
plumb_data_input_stream_read_line_finish (GDataInputStream *stream,
                                          GAsyncResult     *result,
                                          gpointer          user_data);

static void
bz_download_worker_dispose (GObject *object)
{
  BzDownloadWorker *self         = BZ_DOWNLOAD_WORKER (object);
  GHashTableIter    waiting_iter = { 0 };

  dex_clear (&self->task);
  g_clear_object (&self->subprocess);

  g_hash_table_iter_init (&waiting_iter, self->waiting);
  for (;;)
    {
      char       *dest_path = NULL;
      DexPromise *promise   = NULL;

      if (!g_hash_table_iter_next (
              &waiting_iter,
              (gpointer *) &dest_path,
              (gpointer *) &promise))
        break;

      dex_promise_reject (
          promise,
          g_error_new (G_IO_ERROR,
                       G_IO_ERROR_CANCELLED,
                       "The subprocess was terminated"));
    }

  g_clear_pointer (&self->waiting, g_hash_table_unref);
  g_clear_pointer (&self->waiting_mutex, mutex_box_data_unref);
  g_clear_pointer (&self->stdin_mutex, mutex_box_data_unref);

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (bz_download_worker_parent_class)->dispose (object);
}

static void
bz_download_worker_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzDownloadWorker *self = BZ_DOWNLOAD_WORKER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, bz_download_worker_get_name (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_download_worker_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzDownloadWorker *self = BZ_DOWNLOAD_WORKER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      bz_download_worker_set_name (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_download_worker_class_init (BzDownloadWorkerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_download_worker_set_property;
  object_class->get_property = bz_download_worker_get_property;
  object_class->dispose      = bz_download_worker_dispose;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_download_worker_init (BzDownloadWorker *self)
{
  self->stdin_mutex = mutex_box_data_new ();
  g_mutex_init (&self->stdin_mutex->m);

  self->waiting = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, dex_unref);

  self->waiting_mutex = mutex_box_data_new ();
  g_mutex_init (&self->waiting_mutex->m);
}

static gboolean
bz_download_worker_initable_init (GInitable    *initable,
                                  GCancellable *cancellable,
                                  GError      **error)
{
  BzDownloadWorker *self             = BZ_DOWNLOAD_WORKER (initable);
  g_autoptr (MonitorWorkerData) data = NULL;

  self->subprocess = g_subprocess_new (
      G_SUBPROCESS_FLAGS_STDIN_PIPE |
          G_SUBPROCESS_FLAGS_STDOUT_PIPE,
      error,
      DL_WORKER_BIN_NAME, NULL);
  if (self->subprocess == NULL)
    return FALSE;

  data                = monitor_worker_data_new ();
  data->subprocess    = g_object_ref (self->subprocess);
  data->waiting       = g_hash_table_ref (self->waiting);
  data->waiting_mutex = mutex_box_data_ref (self->waiting_mutex);

  self->task = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) monitor_worker_fiber,
      monitor_worker_data_ref (data),
      monitor_worker_data_unref);

  return TRUE;
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = bz_download_worker_initable_init;
}

BzDownloadWorker *
bz_download_worker_new (const char *name,
                        GError    **error)
{
  return g_initable_new (
      BZ_TYPE_DOWNLOAD_WORKER,
      NULL, error,
      "name", name,
      NULL);
}

const char *
bz_download_worker_get_name (BzDownloadWorker *self)
{
  g_return_val_if_fail (BZ_IS_DOWNLOAD_WORKER (self), NULL);
  return self->name;
}

void
bz_download_worker_set_name (BzDownloadWorker *self,
                             const char       *name)
{
  g_return_if_fail (BZ_IS_DOWNLOAD_WORKER (self));

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

DexFuture *
bz_download_worker_invoke (BzDownloadWorker *self,
                           GFile            *src,
                           GFile            *dest)
{
  g_autoptr (DexPromise) promise    = NULL;
  g_autoptr (InvokeWorkerData) data = NULL;

  dex_return_error_if_fail (BZ_IS_DOWNLOAD_WORKER (self));
  dex_return_error_if_fail (G_IS_FILE (src));
  dex_return_error_if_fail (G_IS_FILE (dest));

  promise = dex_promise_new ();

  data                = invoke_worker_data_new ();
  data->promise       = dex_ref (promise);
  data->src           = g_object_ref (src);
  data->dest          = g_object_ref (dest);
  data->subprocess    = g_object_ref (self->subprocess);
  data->stdin_mutex   = mutex_box_data_ref (self->stdin_mutex);
  data->waiting       = g_hash_table_ref (self->waiting);
  data->waiting_mutex = mutex_box_data_ref (self->waiting_mutex);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) invoke_worker_fiber,
      invoke_worker_data_ref (data),
      invoke_worker_data_unref));
  return DEX_FUTURE (g_steal_pointer (&promise));
}

BzDownloadWorker *
bz_download_worker_get_default (void)
{
  static GMutex     mutex         = { 0 };
  static GPtrArray *workers       = NULL;
  static guint      next          = 0;
  g_autoptr (GMutexLocker) locker = NULL;
  BzDownloadWorker *ret           = NULL;

  locker = g_mutex_locker_new (&mutex);

  if (workers == NULL)
    {
      workers = g_ptr_array_new_with_free_func (g_object_unref);

      /* TODO: make number of workers controllable with envvar */
#define N_WORKERS 5

      for (guint i = 0; i < N_WORKERS; i++)
        {
          g_autoptr (GError) local_error      = NULL;
          g_autoptr (BzDownloadWorker) worker = NULL;

          worker = bz_download_worker_new ("default", &local_error);
          if (worker == NULL)
            g_critical ("FATAL!!! The default download worker could not be spawned: %s",
                        local_error->message);
          g_assert (worker != NULL);

          g_ptr_array_add (workers, g_steal_pointer (&worker));
        }
    }

  /* Check if any of the subprocesses need to be recreated */
  for (guint i = 0; i < workers->len; i++)
    {
      BzDownloadWorker **loc = NULL;

      loc = (BzDownloadWorker **) &g_ptr_array_index (workers, i);
      if (g_subprocess_get_identifier ((*loc)->subprocess) == NULL)
        {
          g_autoptr (GError) local_error      = NULL;
          g_autoptr (BzDownloadWorker) worker = NULL;

          g_clear_object (loc);

          worker = bz_download_worker_new ("default", &local_error);
          if (worker == NULL)
            g_critical ("FATAL!!! The default download worker could not be spawned: %s",
                        local_error->message);
          g_assert (worker != NULL);

          *loc = g_steal_pointer (&worker);
        }
    }

  ret  = g_ptr_array_index (workers, next);
  next = (next + 1) % workers->len;

  return ret;
}

static DexFuture *
monitor_worker_fiber (MonitorWorkerData *data)
{
  GSubprocess  *subprocess                       = data->subprocess;
  GHashTable   *waiting                          = data->waiting;
  MutexBoxData *waiting_mutex                    = data->waiting_mutex;
  g_autoptr (GDataInputStream) subprocess_stdout = NULL;

  subprocess_stdout = g_data_input_stream_new (
      g_subprocess_get_stdout_pipe (subprocess));

  for (;;)
    {
      g_autoptr (GError) local_error = NULL;
      g_autoptr (BzGuard) guard      = NULL;
      g_autofree char *line          = NULL;
      g_autoptr (GVariant) variant   = NULL;
      g_autofree char *dest_path     = NULL;
      gboolean         success       = FALSE;
      DexPromise      *promise       = NULL;

      line = dex_await_string (
          dex_async_pair_new (
              subprocess_stdout,
              &DEX_ASYNC_PAIR_INFO_STRING (
                  plumb_data_input_stream_read_line_async,
                  plumb_data_input_stream_read_line_finish)),
          &local_error);
      if (line == NULL)
        {
          if (local_error != NULL)
            g_critical ("Could not read stdout from download worker subprocess: %s",
                        local_error->message);

          /* give up on this subprocess and wait to be disposed */
          return NULL;
        }

      variant = g_variant_parse (G_VARIANT_TYPE ("(sb)"),
                                 line, NULL, NULL, &local_error);
      if (variant == NULL)
        {
          g_critical ("Could not interpret stdout from download worker subprocess: %s",
                      local_error->message);
          continue;
        }
      g_variant_get (variant, "(sb)", &dest_path, &success);

      BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &waiting_mutex->m, &waiting_mutex->g);
      {
        promise = g_hash_table_lookup (waiting, dest_path);
        if (promise != NULL)
          {
            if (success)
              dex_promise_resolve_boolean (promise, TRUE);
            else
              dex_promise_reject (
                  promise,
                  g_error_new (G_IO_ERROR,
                               G_IO_ERROR_UNKNOWN,
                               "The subprocess reported an error downloading '%s'", dest_path));
            promise = NULL;
            g_hash_table_remove (waiting, dest_path);
          }
      }
      bz_clear_guard (&guard);
    }

  return NULL;
}

static DexFuture *
invoke_worker_fiber (InvokeWorkerData *data)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (BzGuard) guard      = NULL;
  DexPromise      *promise       = data->promise;
  GFile           *src           = data->src;
  GFile           *dest          = data->dest;
  GSubprocess     *subprocess    = data->subprocess;
  MutexBoxData    *stdin_mutex   = data->stdin_mutex;
  GHashTable      *waiting       = data->waiting;
  MutexBoxData    *waiting_mutex = data->waiting_mutex;
  g_autofree char *src_uri       = NULL;
  g_autofree char *dest_path     = NULL;
  DexPromise      *existing      = NULL;
  g_autoptr (GVariant) variant   = NULL;
  g_autoptr (GString) output     = NULL;
  GOutputStream *stdin_stream    = NULL;
  gint64         bytes_written   = -1;

  src_uri   = g_file_get_uri (src);
  dest_path = g_file_get_path (dest);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &waiting_mutex->m, &waiting_mutex->g);
  {
    existing = g_hash_table_lookup (waiting, dest_path);
    if (existing != NULL)
      {
        dex_promise_reject (
            existing,
            g_error_new (G_IO_ERROR,
                         G_IO_ERROR_CANCELLED,
                         "The operation was replaced"));
        existing = NULL;
      }
    g_hash_table_replace (waiting, g_strdup (dest_path), dex_ref (promise));
  }
  bz_clear_guard (&guard);

  variant = g_variant_new ("(ss)", src_uri, dest_path);
  output  = g_string_new (NULL);
  output  = g_variant_print_string (variant, g_steal_pointer (&output), TRUE);
  g_string_append_c (output, '\n');

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &stdin_mutex->m, &stdin_mutex->g);
  {
    stdin_stream  = g_subprocess_get_stdin_pipe (subprocess);
    bytes_written = dex_await_int64 (
        dex_output_stream_write (stdin_stream, output->str, output->len, G_PRIORITY_DEFAULT),
        &local_error);
  }
  bz_clear_guard (&guard);

  if (bytes_written < 0)
    {
      BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &waiting_mutex->m, &waiting_mutex->g);
      {
        g_hash_table_remove (waiting, dest_path);
      }
      bz_clear_guard (&guard);

      dex_promise_reject (promise, g_steal_pointer (&local_error));
    }

  return NULL;
}

static void
plumb_data_input_stream_read_line_async (GDataInputStream   *stream,
                                         GCancellable       *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer            user_data)
{
  g_data_input_stream_read_line_async (
      stream,
      G_PRIORITY_DEFAULT,
      cancellable,
      callback,
      user_data);
}

static char *
plumb_data_input_stream_read_line_finish (GDataInputStream *stream,
                                          GAsyncResult     *result,
                                          gpointer          user_data)
{
  return g_data_input_stream_read_line_finish (
      stream,
      result,
      NULL,
      user_data);
}

/* End of bz-download-worker.c */

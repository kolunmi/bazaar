/* bz-entry-cache-manager.c
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

#define G_LOG_DOMAIN  "BAZAAR::CACHE"
#define BAZAAR_MODULE "entry-cache"

#define MAX_CONCURRENT_WRITES             4
#define WATCH_CLEANUP_INTERVAL_MSEC       5000
#define WATCH_RECACHE_INTERVAL_SEC_DOUBLE 4.0

#include "bz-entry-cache-manager.h"
#include "bz-env.h"
#include "bz-flatpak-entry.h"
#include "bz-io.h"
#include "bz-serializable.h"
#include "bz-util.h"

/* clang-format off */
G_DEFINE_QUARK (bz-entry-cache-error-quark, bz_entry_cache_error);
/* clang-format on */

BZ_DEFINE_DATA (
    ongoing_task,
    OngoingTask,
    {
      DexScheduler *scheduler;
      DexPromise   *init;

      GHashTable *alive_hash;
      GHashTable *writing_hash;
      GHashTable *reading_hash;

      BzGuard *ongoing_gates[MAX_CONCURRENT_WRITES];
      GMutex   ongoing_mutexes[MAX_CONCURRENT_WRITES];
      guint    ongoing_queued[MAX_CONCURRENT_WRITES];
      GMutex   ongoing_queueing_mutex;

      BzGuard *alive_gate;
      GMutex   alive_mutex;
      BzGuard *reading_gate;
      GMutex   reading_mutex;
      BzGuard *writing_gate;
      GMutex   writing_mutex;
    },
    BZ_RELEASE_DATA (scheduler, dex_unref);
    BZ_RELEASE_DATA (init, dex_unref);
    BZ_RELEASE_DATA (alive_hash, g_hash_table_unref);
    BZ_RELEASE_DATA (writing_hash, g_hash_table_unref);
    BZ_RELEASE_DATA (reading_hash, g_hash_table_unref);
    for (guint i = 0; i < G_N_ELEMENTS (self->ongoing_gates); i++)
        BZ_RELEASE_DATA (ongoing_gates[i], bz_guard_destroy);
    for (guint i = 0; i < G_N_ELEMENTS (self->ongoing_mutexes); i++)
        g_mutex_clear (&self->ongoing_mutexes[i]);
    g_mutex_clear (&self->ongoing_queueing_mutex);
    BZ_RELEASE_DATA (alive_gate, bz_guard_destroy);
    BZ_RELEASE_DATA (reading_gate, bz_guard_destroy);
    BZ_RELEASE_DATA (writing_gate, bz_guard_destroy);
    g_mutex_clear (&self->alive_mutex);
    g_mutex_clear (&self->reading_mutex);
    g_mutex_clear (&self->writing_mutex););

struct _BzEntryCacheManager
{
  GObject parent_instance;

  guint64 max_memory_usage;

  DexScheduler *scheduler;
  guint64       memory_usage;

  OngoingTaskData *task_data;
  DexFuture       *watch_task;
};

G_DEFINE_FINAL_TYPE (BzEntryCacheManager, bz_entry_cache_manager, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MAX_MEMORY_USAGE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static DexFuture *
watch_fiber (OngoingTaskData *task_data);

BZ_DEFINE_DATA (
    living_entry,
    LivingEntry,
    {
      GWeakRef wr;
      BzGuard *gate;
      GMutex   mutex;
      GTimer  *cached;
    },
    BZ_RELEASE_DATA (gate, bz_guard_destroy);
    g_mutex_clear (&self->mutex);
    g_weak_ref_clear (&self->wr);
    BZ_RELEASE_DATA (cached, g_timer_destroy));

BZ_DEFINE_DATA (
    write_task,
    WriteTask,
    {
      OngoingTaskData *task_data;
      char            *unique_id_checksum;
      BzEntry         *entry;
    },
    BZ_RELEASE_DATA (task_data, ongoing_task_data_unref);
    BZ_RELEASE_DATA (unique_id_checksum, g_free);
    BZ_RELEASE_DATA (entry, g_object_unref);)
static DexFuture *
write_task_fiber (WriteTaskData *data);

BZ_DEFINE_DATA (
    read_task,
    ReadTask,
    {
      OngoingTaskData *task_data;
      char            *unique_id_checksum;
    },
    BZ_RELEASE_DATA (task_data, ongoing_task_data_unref);
    BZ_RELEASE_DATA (unique_id_checksum, g_free))
static DexFuture *
read_task_fiber (ReadTaskData *data);

static void
bz_entry_cache_manager_dispose (GObject *object)
{
  BzEntryCacheManager *self = BZ_ENTRY_CACHE_MANAGER (object);

  dex_clear (&self->scheduler);
  dex_clear (&self->watch_task);
  g_clear_pointer (&self->task_data, ongoing_task_data_unref);

  G_OBJECT_CLASS (bz_entry_cache_manager_parent_class)->dispose (object);
}

static void
bz_entry_cache_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BzEntryCacheManager *self = BZ_ENTRY_CACHE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_MAX_MEMORY_USAGE:
      g_value_set_uint64 (value, bz_entry_cache_manager_get_max_memory_usage (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_cache_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BzEntryCacheManager *self = BZ_ENTRY_CACHE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_MAX_MEMORY_USAGE:
      bz_entry_cache_manager_set_max_memory_usage (self, g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_cache_manager_class_init (BzEntryCacheManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_cache_manager_set_property;
  object_class->get_property = bz_entry_cache_manager_get_property;
  object_class->dispose      = bz_entry_cache_manager_dispose;

  props[PROP_MAX_MEMORY_USAGE] =
      g_param_spec_uint64 (
          "max-memory-usage",
          NULL, NULL,
          0, G_MAXUINT64, 0xccccccc,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_cache_manager_init (BzEntryCacheManager *self)
{
  static DexScheduler *global_scheduler = NULL;
  g_autoptr (OngoingTaskData) task_data = NULL;

  if (g_once_init_enter_pointer (&global_scheduler))
    g_once_init_leave_pointer (&global_scheduler, dex_thread_pool_scheduler_new ());

  self->scheduler    = dex_ref (global_scheduler);
  self->memory_usage = 0;

  task_data             = ongoing_task_data_new ();
  task_data->scheduler  = dex_ref (self->scheduler);
  task_data->init       = dex_promise_new ();
  task_data->alive_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, living_entry_data_unref);
  task_data->writing_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, dex_unref);
  task_data->reading_hash = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, dex_unref);
  for (guint i = 0; i < G_N_ELEMENTS (task_data->ongoing_mutexes); i++)
    g_mutex_init (&task_data->ongoing_mutexes[i]);
  g_mutex_init (&task_data->ongoing_queueing_mutex);
  g_mutex_init (&task_data->alive_mutex);
  g_mutex_init (&task_data->reading_mutex);
  g_mutex_init (&task_data->writing_mutex);
  self->task_data = g_steal_pointer (&task_data);

  self->watch_task = dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) watch_fiber,
      ongoing_task_data_ref (self->task_data),
      ongoing_task_data_unref);
}

BzEntryCacheManager *
bz_entry_cache_manager_new (void)
{
  return g_object_new (BZ_TYPE_ENTRY_CACHE_MANAGER, NULL);
}

guint64
bz_entry_cache_manager_get_max_memory_usage (BzEntryCacheManager *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_CACHE_MANAGER (self), 0);
  return self->max_memory_usage;
}

void
bz_entry_cache_manager_set_max_memory_usage (BzEntryCacheManager *self,
                                             guint64              max_memory_usage)
{
  g_return_if_fail (BZ_IS_ENTRY_CACHE_MANAGER (self));

  self->max_memory_usage = max_memory_usage;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAX_MEMORY_USAGE]);
}

DexFuture *
bz_entry_cache_manager_add (BzEntryCacheManager *self,
                            BzEntry             *entry)
{
  g_autoptr (WriteTaskData) data = NULL;
  g_autoptr (DexFuture) future   = NULL;

  dex_return_error_if_fail (BZ_IS_ENTRY_CACHE_MANAGER (self));
  dex_return_error_if_fail (BZ_IS_ENTRY (entry));
  dex_return_error_if_fail (!bz_entry_is_holding (entry));

  data                     = write_task_data_new ();
  data->task_data          = ongoing_task_data_ref (self->task_data);
  data->unique_id_checksum = g_strdup (bz_entry_get_unique_id_checksum (entry));
  data->entry              = g_object_ref (entry);

  future = dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) write_task_fiber,
      write_task_data_ref (data),
      write_task_data_unref);
  return g_steal_pointer (&future);
}

DexFuture *
bz_entry_cache_manager_get (BzEntryCacheManager *self,
                            const char          *unique_id)
{
  g_autoptr (ReadTaskData) data = NULL;
  g_autoptr (DexFuture) future  = NULL;

  dex_return_error_if_fail (BZ_IS_ENTRY_CACHE_MANAGER (self));
  dex_return_error_if_fail (unique_id != NULL);

  data                     = read_task_data_new ();
  data->task_data          = ongoing_task_data_ref (self->task_data);
  data->unique_id_checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, unique_id, -1);

  future = dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) read_task_fiber,
      read_task_data_ref (data),
      read_task_data_unref);
  return g_steal_pointer (&future);
}

static DexFuture *
write_task_fiber (WriteTaskData *data)
{
  OngoingTaskData *task_data           = data->task_data;
  char            *unique_id_checksum  = data->unique_id_checksum;
  BzEntry         *entry               = data->entry;
  g_autoptr (GError) local_error       = NULL;
  g_autoptr (BzGuard) slot_guard       = NULL;
  g_autoptr (BzGuard) other_guard      = NULL;
  g_autoptr (GMutexLocker) locker      = NULL;
  guint      slot_queued               = G_MAXUINT;
  guint      slot_index                = 0;
  DexFuture *writing_future            = NULL;
  g_autoptr (LivingEntryData) living   = NULL;
  g_autoptr (DexPromise) promise       = NULL;
  g_autoptr (GVariantBuilder) builder  = NULL;
  g_autoptr (GVariant) variant         = NULL;
  g_autoptr (GBytes) bytes             = NULL;
  g_autofree char *main_cache          = NULL;
  g_autoptr (GFile) parent_file        = NULL;
  g_autoptr (GFile) save_file          = NULL;
  g_autoptr (GFileOutputStream) output = NULL;
  gssize   bytes_written               = 0;
  gboolean result                      = FALSE;
  g_autoptr (GError) ret_error         = NULL;

  if (!BZ_IS_FLATPAK_ENTRY (entry))
    return dex_future_new_reject (
        BZ_ENTRY_CACHE_ERROR,
        BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
        "Entry with unique ID checksum '%s' cannot be "
        "cached because it is not a flatpak entry",
        unique_id_checksum);

  /* Rate limit to reduce competition for resources
   * when refresh triggers a flood of requests
   *
   * Here we make sure to pick the slot with the
   * least tasks waiting in line
   */
  locker = g_mutex_locker_new (&task_data->ongoing_queueing_mutex);
  for (guint i = 0; i < G_N_ELEMENTS (task_data->ongoing_gates); i++)
    {
      if (task_data->ongoing_queued[i] < slot_queued)
        {
          slot_queued = task_data->ongoing_queued[i];
          slot_index  = i;
        }
    }
  task_data->ongoing_queued[slot_index]++;
  g_clear_pointer (&locker, g_mutex_locker_free);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&slot_guard,
                               &task_data->ongoing_mutexes[slot_index],
                               &task_data->ongoing_gates[slot_index]);

  locker = g_mutex_locker_new (&task_data->ongoing_queueing_mutex);
  task_data->ongoing_queued[slot_index]--;
  g_clear_pointer (&locker, g_mutex_locker_free);

  dex_await (dex_ref (task_data->init), NULL);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&other_guard,
                               &task_data->writing_mutex,
                               &task_data->writing_gate);
  {
    writing_future = g_hash_table_lookup (task_data->writing_hash, unique_id_checksum);
    if (writing_future != NULL)
      dex_promise_reject (
          DEX_PROMISE (g_steal_pointer (&writing_future)),
          g_error_new (
              BZ_ENTRY_CACHE_ERROR,
              BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
              "Entry with unique ID '%s' is already being cached right now",
              unique_id_checksum));

    promise = dex_promise_new ();
    g_hash_table_replace (task_data->writing_hash,
                          g_strdup (unique_id_checksum),
                          dex_ref (promise));
  }
  bz_clear_guard (&other_guard);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&other_guard,
                               &task_data->alive_mutex,
                               &task_data->alive_gate);
  {
    living = g_hash_table_lookup (task_data->alive_hash, unique_id_checksum);
    if (living != NULL)
      living_entry_data_ref (living);
    else
      {
        living = living_entry_data_new ();
        g_weak_ref_init (&living->wr, NULL);
        g_mutex_init (&living->mutex);
        living->cached = g_timer_new ();
        g_hash_table_replace (task_data->alive_hash,
                              g_strdup (unique_id_checksum),
                              living_entry_data_ref (living));
      }
  }
  bz_clear_guard (&other_guard);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&other_guard,
                               &living->mutex,
                               &living->gate);
  {
    builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
    bz_serializable_serialize (BZ_SERIALIZABLE (entry), builder);
    variant = g_variant_builder_end (builder);
    bytes   = g_variant_get_data_as_bytes (variant);

    main_cache  = bz_dup_module_dir ();
    parent_file = g_file_new_for_path (main_cache);
    result      = g_file_make_directory_with_parents (parent_file, NULL, &local_error);
    if (!result)
      {
        if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
          g_clear_pointer (&local_error, g_error_free);
        else
          {
            ret_error = g_error_new (
                BZ_ENTRY_CACHE_ERROR,
                BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
                "Failed to make parent directory '%s' when caching '%s': %s",
                main_cache, unique_id_checksum, local_error->message);
            goto done;
          }
      }
    save_file = g_file_new_build_filename (main_cache, unique_id_checksum, NULL);

    output = g_file_replace (
        save_file,
        NULL,
        FALSE,
        G_FILE_CREATE_REPLACE_DESTINATION,
        NULL,
        &local_error);
    if (output == NULL)
      {
        ret_error = g_error_new (
            BZ_ENTRY_CACHE_ERROR,
            BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
            "Failed to open write stream when caching '%s': %s",
            unique_id_checksum, local_error->message);
        goto done;
      }

    bytes_written = g_output_stream_write_bytes (G_OUTPUT_STREAM (output), bytes, NULL, &local_error);
    if (bytes_written < 0)
      {
        ret_error = g_error_new (
            BZ_ENTRY_CACHE_ERROR,
            BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
            "Failed to write data to stream when caching '%s': %s",
            unique_id_checksum, local_error->message);
        goto done;
      }

    result = g_output_stream_close (G_OUTPUT_STREAM (output), NULL, &local_error);
    if (!result)
      {
        ret_error = g_error_new (
            BZ_ENTRY_CACHE_ERROR,
            BZ_ENTRY_CACHE_ERROR_CACHE_FAILED,
            "Failed to close stream when caching '%s': %s",
            unique_id_checksum, local_error->message);
        goto done;
      }

    g_timer_start (living->cached);
  }
done:
  bz_clear_guard (&other_guard);
  bz_clear_guard (&slot_guard);

  if (ret_error != NULL)
    dex_promise_reject (promise, g_error_copy (ret_error));
  else
    dex_promise_resolve_boolean (promise, TRUE);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&other_guard,
                               &task_data->writing_mutex,
                               &task_data->writing_gate);
  {
    g_hash_table_remove (task_data->writing_hash, unique_id_checksum);
  }
  bz_clear_guard (&other_guard);

  if (ret_error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&ret_error));
  else
    return dex_future_new_true ();
}

static DexFuture *
read_task_fiber (ReadTaskData *data)
{
  OngoingTaskData *task_data           = data->task_data;
  char            *unique_id_checksum  = data->unique_id_checksum;
  g_autoptr (GError) local_error       = NULL;
  g_autoptr (BzGuard) guard            = NULL;
  g_autoptr (GMutexLocker) locker      = NULL;
  g_autoptr (DexFuture) writing_future = NULL;
  g_autoptr (LivingEntryData) living   = NULL;
  DexFuture *reading_future            = NULL;
  g_autoptr (DexPromise) promise       = NULL;
  g_autofree char *main_cache          = NULL;
  g_autofree char *path                = NULL;
  g_autoptr (GFile) file               = NULL;
  g_autoptr (GBytes) bytes             = NULL;
  g_autoptr (GVariant) variant         = NULL;
  g_autoptr (BzFlatpakEntry) entry     = NULL;
  gboolean result                      = FALSE;
  g_autoptr (GError) ret_error         = NULL;

  dex_await (dex_ref (task_data->init), NULL);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard,
                               &task_data->writing_mutex,
                               &task_data->writing_gate);
  {
    writing_future = g_hash_table_lookup (task_data->writing_hash, unique_id_checksum);
    if (writing_future != NULL)
      {
        dex_ref (writing_future);
        bz_clear_guard (&guard);
        dex_await (g_steal_pointer (&writing_future), NULL);
      }
  }
  bz_clear_guard (&guard);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard,
                               &task_data->reading_mutex,
                               &task_data->reading_gate);
  {
    reading_future = g_hash_table_lookup (task_data->reading_hash, unique_id_checksum);
    if (reading_future != NULL)
      return dex_ref (reading_future);
    promise = dex_promise_new ();
    g_hash_table_replace (task_data->reading_hash,
                          g_strdup (unique_id_checksum),
                          dex_ref (promise));
  }
  bz_clear_guard (&guard);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard,
                               &task_data->alive_mutex,
                               &task_data->alive_gate);
  {
    living = g_hash_table_lookup (task_data->alive_hash, unique_id_checksum);
    if (living != NULL)
      {
        g_autoptr (BzEntry) living_entry = NULL;

        living_entry_data_ref (living);
        bz_clear_guard (&guard);

        BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &living->mutex, &living->gate);
        living_entry = g_weak_ref_get (&living->wr);
        if (living_entry != NULL)
          {
            dex_promise_resolve_object (promise, g_object_ref (living_entry));
            return dex_future_new_for_object (living_entry);
          }
      }
    else
      {
        living = living_entry_data_new ();
        g_weak_ref_init (&living->wr, NULL);
        g_mutex_init (&living->mutex);
        living->cached = g_timer_new ();

        g_hash_table_replace (task_data->alive_hash,
                              g_strdup (unique_id_checksum),
                              living_entry_data_ref (living));
        bz_clear_guard (&guard);

        BZ_BEGIN_GUARD_WITH_CONTEXT (&guard, &living->mutex, &living->gate);
      }
  }

  /* living data was guarded */

  main_cache = bz_dup_module_dir ();
  path       = g_build_filename (main_cache, unique_id_checksum, NULL);
  file       = g_file_new_for_path (path);

  bytes = g_file_load_bytes (file, NULL, NULL, &local_error);
  if (bytes == NULL)
    {
      ret_error = g_error_new (
          BZ_ENTRY_CACHE_ERROR,
          BZ_ENTRY_CACHE_ERROR_DECACHE_FAILED,
          "Failed to de-cache variant from file: %s",
          local_error->message);
      goto done;
    }

  variant = g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, FALSE);
  if (variant == NULL)
    {
      ret_error = g_error_new (
          BZ_ENTRY_CACHE_ERROR,
          BZ_ENTRY_CACHE_ERROR_DECACHE_FAILED,
          "Failed to interpret variant from %s: %s",
          path, local_error->message);
      goto done;
    }

  entry  = g_object_new (BZ_TYPE_FLATPAK_ENTRY, NULL);
  result = bz_serializable_deserialize (BZ_SERIALIZABLE (entry), variant, &local_error);
  if (!result)
    {
      ret_error = g_error_new (
          BZ_ENTRY_CACHE_ERROR,
          BZ_ENTRY_CACHE_ERROR_DECACHE_FAILED,
          "Failed to deserialize entry from %s: %s",
          path, local_error->message);
      goto done;
    }
  g_weak_ref_init (&living->wr, entry);

done:
  bz_clear_guard (&guard);

  BZ_BEGIN_GUARD_WITH_CONTEXT (&guard,
                               &task_data->reading_mutex,
                               &task_data->reading_gate);
  {
    g_hash_table_remove (task_data->reading_hash, unique_id_checksum);
  }
  bz_clear_guard (&guard);

  if (ret_error != NULL)
    {
      dex_promise_reject (promise, g_error_copy (ret_error));
      return dex_future_new_for_error (g_steal_pointer (&ret_error));
    }
  else
    {
      dex_promise_resolve_object (promise, g_object_ref (entry));
      return dex_future_new_for_object (entry);
    }
}

static DexFuture *
watch_fiber (OngoingTaskData *task_data)
{
  bz_discard_module_dir ();
  dex_promise_resolve_boolean (task_data->init, TRUE);

  for (;;)
    {
      g_autoptr (GError) local_error = NULL;
      g_autoptr (BzGuard) guard0     = NULL;
      GHashTableIter iter            = { 0 };
      g_autoptr (GTimer) timer       = NULL;
      guint total                    = 0;
      guint skipped                  = 0;
      guint written                  = 0;
      guint pruned                   = 0;

      if (!dex_await (dex_timeout_new_msec (WATCH_CLEANUP_INTERVAL_MSEC), &local_error) &&
          !g_error_matches (local_error, DEX_ERROR, DEX_ERROR_TIMED_OUT))
        {
          g_critical ("Cannot continue entry garbage collection: %s", local_error->message);
          return NULL;
        }

      timer = g_timer_new ();

      BZ_BEGIN_GUARD_WITH_CONTEXT (&guard0, &task_data->alive_mutex, &task_data->alive_gate);
      BZ_BEGIN_GUARD_WITH_CONTEXT (&guard0, &task_data->reading_mutex, &task_data->reading_gate);
      BZ_BEGIN_GUARD_WITH_CONTEXT (&guard0, &task_data->writing_mutex, &task_data->writing_gate);

      g_hash_table_iter_init (&iter, task_data->alive_hash);
      for (;;)
        {
          char *unique_id_checksum           = NULL;
          g_autoptr (LivingEntryData) living = NULL;
          g_autoptr (BzGuard) guard1         = NULL;
          g_autoptr (BzEntry) entry          = NULL;

          if (!g_hash_table_iter_next (&iter, (gpointer *) &unique_id_checksum, (gpointer *) &living))
            break;
          total++;
          living_entry_data_ref (living);

          if (g_hash_table_contains (task_data->reading_hash, unique_id_checksum) ||
              g_hash_table_contains (task_data->writing_hash, unique_id_checksum))
            {
              skipped++;
              continue;
            }

          BZ_BEGIN_GUARD_WITH_CONTEXT (&guard1, &living->mutex, &living->gate);

          entry = g_weak_ref_get (&living->wr);
          if (entry != NULL)
            {
              if (bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION) &&
                  g_timer_elapsed (living->cached, NULL) > WATCH_RECACHE_INTERVAL_SEC_DOUBLE)
                {
                  g_autoptr (WriteTaskData) data = NULL;

                  data                     = write_task_data_new ();
                  data->task_data          = ongoing_task_data_ref (task_data);
                  data->unique_id_checksum = g_strdup (unique_id_checksum);
                  data->entry              = g_object_ref (entry);

                  dex_future_disown (dex_scheduler_spawn (
                      task_data->scheduler,
                      bz_get_dex_stack_size (),
                      (DexFiberFunc) write_task_fiber,
                      write_task_data_ref (data),
                      write_task_data_unref));
                  written++;
                }
            }
          else
            {
              bz_clear_guard (&guard1);
              g_hash_table_iter_remove (&iter);
              pruned++;
            }
        }

      g_debug ("Sweep report: finished in %.4f seconds, including time to acquire guards\n"
               "  Out of a total of %d entries considered:\n"
               "    %d were skipped due to active tasks being associated with them\n"
               "    %d application entries were kept alive but written to back to disk\n"
               "    %d entries were forgotten by the application and were pruned\n"
               "  Another sweep will take place in %d msec",
               g_timer_elapsed (timer, NULL),
               total, skipped, written, pruned, WATCH_CLEANUP_INTERVAL_MSEC);
    }
}

/* End of bz-entry-cache-manager.c */

/* bz-io.c
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

#include "bz-io.h"
#include "bz-env.h"

static DexFuture *
reap_file_fiber (GFile *file);
static DexFuture *
reap_path_fiber (char *path);

static DexFuture *
get_directory_size_fiber (GFile *file);
static DexFuture *
get_user_data_size_fiber (char *app_id);
static DexFuture *
get_all_user_data_ids_fiber (void);

DexScheduler *
bz_get_io_scheduler (void)
{
  static DexScheduler *scheduler = NULL;

  if (g_once_init_enter_pointer (&scheduler))
    g_once_init_leave_pointer (&scheduler, dex_thread_pool_scheduler_new ());

  return scheduler;
}

void
bz_reap_file (GFile *file)
{
  g_autoptr (GError) local_error         = NULL;
  g_autofree gchar *uri                  = NULL;
  g_autoptr (GFileEnumerator) enumerator = NULL;
  gboolean result                        = FALSE;

  g_return_if_fail (G_IS_FILE (file));

  uri = g_file_get_uri (file);
  if (uri == NULL)
    uri = g_file_get_path (file);

  enumerator = g_file_enumerate_children (
      file,
      G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK
      "," G_FILE_ATTRIBUTE_STANDARD_NAME
      "," G_FILE_ATTRIBUTE_STANDARD_TYPE
      "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
      NULL,
      &local_error);
  if (enumerator == NULL)
    {
      if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
      g_clear_pointer (&local_error, g_error_free);
      return;
    }

  for (;;)
    {
      g_autoptr (GFileInfo) info = NULL;
      g_autoptr (GFile) child    = NULL;
      GFileType file_type        = G_FILE_TYPE_UNKNOWN;

      info = g_file_enumerator_next_file (enumerator, NULL, &local_error);
      if (info == NULL)
        {
          if (local_error != NULL)
            g_warning ("failed to enumerate cache directory '%s': %s", uri, local_error->message);
          g_clear_pointer (&local_error, g_error_free);
          break;
        }

      child     = g_file_enumerator_get_child (enumerator, info);
      file_type = g_file_info_get_file_type (info);

      if (!g_file_info_get_is_symlink (info) && file_type == G_FILE_TYPE_DIRECTORY)
        bz_reap_file (child);
      else
        {
          result = g_file_delete (child, NULL, &local_error);
          if (!result)
            {
              g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
              g_clear_pointer (&local_error, g_error_free);
            }
        }
    }

  result = g_file_enumerator_close (enumerator, NULL, &local_error);
  if (!result)
    {
      g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
      g_clear_pointer (&local_error, g_error_free);
    }

  result = g_file_delete (file, NULL, &local_error);
  if (!result)
    {
      g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
      g_clear_pointer (&local_error, g_error_free);
    }
}

DexFuture *
bz_reap_user_data_dex (const char *app_id)
{
  g_autofree char *user_data_path = NULL;

  dex_return_error_if_fail (app_id != NULL);

  user_data_path = g_build_filename (g_get_home_dir (), ".var", "app", app_id, NULL);
  return bz_reap_path_dex (g_steal_pointer (&user_data_path));
}

void
bz_reap_path (const char *path)
{
  g_autoptr (GFile) file = NULL;

  g_return_if_fail (path != NULL);

  file = g_file_new_for_path (path);
  bz_reap_file (file);
}

DexFuture *
bz_reap_file_dex (GFile *file)
{
  dex_return_error_if_fail (G_IS_FILE (file));
  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) reap_file_fiber,
      g_object_ref (file), g_object_unref);
}

DexFuture *
bz_reap_path_dex (const char *path)
{
  dex_return_error_if_fail (path != NULL);
  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) reap_path_fiber,
      g_strdup (path), g_free);
}

DexFuture *
bz_get_user_data_size_dex (const char *app_id)
{
  dex_return_error_if_fail (app_id != NULL);
  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) get_user_data_size_fiber,
      g_strdup (app_id), g_free);
}

DexFuture *
bz_get_user_data_ids_dex (void)
{
  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) get_all_user_data_ids_fiber,
      NULL, NULL);
}

static DexFuture *
get_user_data_size_fiber (char *app_id)
{
  g_autofree char *user_data_path = NULL;
  g_autoptr (GFile) file          = NULL;

  user_data_path = g_build_filename (g_get_home_dir (), ".var", "app", app_id, NULL);
  file           = g_file_new_for_path (user_data_path);

  return get_directory_size_fiber (file);
}

static DexFuture *
get_directory_size_fiber (GFile *file)
{
  g_autoptr (DexFuture) enumerator_future = NULL;
  g_autoptr (GFileEnumerator) enumerator  = NULL;
  g_autoptr (GError) error                = NULL;
  guint64 total_size                      = 0;

  enumerator_future = dex_file_enumerate_children (
      file,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
      G_PRIORITY_DEFAULT);

  enumerator = dex_await_object (dex_ref (enumerator_future), &error);
  if (enumerator == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        return dex_future_new_for_uint64 (0);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  for (;;)
    {
      g_autoptr (DexFuture) next_future = NULL;
      g_autoptr (GList) infos           = NULL;
      GList *l;

      next_future = dex_file_enumerator_next_files (enumerator, 10, G_PRIORITY_DEFAULT);
      infos       = dex_await_boxed (dex_ref (next_future), &error);

      if (infos == NULL)
        {
          if (error != NULL)
            return dex_future_new_for_error (g_steal_pointer (&error));
          break;
        }

      for (l = infos; l != NULL; l = l->next)
        {
          GFileInfo *info      = l->data;
          GFileType  file_type = g_file_info_get_file_type (info);

          if (file_type == G_FILE_TYPE_DIRECTORY)
            {
              g_autoptr (GFile) child           = g_file_enumerator_get_child (enumerator, info);
              g_autoptr (DexFuture) size_future = get_directory_size_fiber (child);
              guint64 child_size                = dex_await_uint64 (dex_ref (size_future), &error);

              if (error != NULL)
                return dex_future_new_for_error (g_steal_pointer (&error));

              total_size += child_size;
            }
          else
            {
              total_size += g_file_info_get_size (info);
            }
        }
    }

  return dex_future_new_for_uint64 (total_size);
}

static DexFuture *
get_all_user_data_ids_fiber (void)
{
  g_autofree char *var_app_path           = NULL;
  g_autoptr (GFile) var_app_dir           = NULL;
  g_autoptr (DexFuture) enumerator_future = NULL;
  g_autoptr (GFileEnumerator) enumerator  = NULL;
  g_autoptr (GError) error                = NULL;
  GHashTable *ids                         = NULL;

  var_app_path = g_build_filename (g_get_home_dir (), ".var", "app", NULL);
  var_app_dir  = g_file_new_for_path (var_app_path);

  ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  enumerator_future = dex_file_enumerate_children (
      var_app_dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
      G_PRIORITY_DEFAULT);

  enumerator = dex_await_object (dex_ref (enumerator_future), &error);
  if (enumerator == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        return dex_future_new_take_boxed (G_TYPE_HASH_TABLE, ids);
      g_hash_table_unref (ids);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  for (;;)
    {
      g_autoptr (DexFuture) next_future = NULL;
      g_autoptr (GList) infos           = NULL;
      GList *l;

      next_future = dex_file_enumerator_next_files (enumerator, 10, G_PRIORITY_DEFAULT);
      infos       = dex_await_boxed (dex_ref (next_future), &error);

      if (infos == NULL)
        {
          if (error != NULL)
            {
              g_hash_table_unref (ids);
              return dex_future_new_for_error (g_steal_pointer (&error));
            }
          break;
        }

      for (l = infos; l != NULL; l = l->next)
        {
          GFileInfo *info      = l->data;
          GFileType  file_type = g_file_info_get_file_type (info);

          if (file_type == G_FILE_TYPE_DIRECTORY)
            {
              const char *app_id = g_file_info_get_name (info);
              g_hash_table_insert (ids, g_strdup (app_id), NULL);
            }
        }
    }

  return dex_future_new_take_boxed (G_TYPE_HASH_TABLE, ids);
}

char *
bz_dup_root_cache_dir (void)
{
  const char *user_cache = NULL;
  const char *id         = NULL;

  user_cache = g_get_user_cache_dir ();

  id = g_application_get_application_id (g_application_get_default ());
  if (id == NULL)
    id = "Bazaar";

  return g_build_filename (user_cache, id, NULL);
}

char *
bz_dup_cache_dir (const char *submodule)
{
  g_autofree char *root_cache_dir = NULL;

  g_return_val_if_fail (submodule != NULL, NULL);

  root_cache_dir = bz_dup_root_cache_dir ();
  return g_build_filename (root_cache_dir, submodule, NULL);
}

static DexFuture *
reap_file_fiber (GFile *file)
{
  bz_reap_file (file);
  return dex_future_new_true ();
}

static DexFuture *
reap_path_fiber (char *path)
{
  bz_reap_path (path);
  return dex_future_new_true ();
}

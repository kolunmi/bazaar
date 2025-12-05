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

  uri        = g_file_get_uri (file);
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

      result = g_file_delete (child, NULL, &local_error);
      if (!result)
        {
          g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
          g_clear_pointer (&local_error, g_error_free);
        }
    }

  result = g_file_enumerator_close (enumerator, NULL, &local_error);
  if (!result)
    {
      g_warning ("failed to reap cache directory '%s': %s", uri, local_error->message);
      g_clear_pointer (&local_error, g_error_free);
    }
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

char *
bz_dup_cache_dir (const char *submodule)
{
  const char *user_cache = NULL;
  const char *id         = NULL;

  g_return_val_if_fail (submodule != NULL, NULL);

  user_cache = g_get_user_cache_dir ();

  id = g_application_get_application_id (g_application_get_default ());
  if (id == NULL)
    id = "Bazaar";

  return g_build_filename (user_cache, id, submodule, NULL);
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

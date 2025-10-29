/* dl-worker.c
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

#define G_LOG_DOMAIN "BAZAAR::DL-WORKER-SUBPROCESS"

#include <stdio.h>

#include "bz-env.h"
#include "bz-global-state.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    download,
    Download,
    {
      char *src;
      char *dest;
    },
    BZ_RELEASE_DATA (src, g_free);
    BZ_RELEASE_DATA (dest, g_free));

static DexFuture *
read_stdin (GMainLoop *loop);

static DexFuture *
download_fiber (DownloadData *data);

static DexFuture *
print_fiber (char *output);

int
main (int   argc,
      char *argv[])
{
  g_autoptr (GMainLoop) main_loop = NULL;

  g_log_writer_default_set_use_stderr (TRUE);
  dex_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_future_disown (dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) read_stdin,
      main_loop, NULL));
  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}

static DexFuture *
read_stdin (GMainLoop *loop)
{
  g_autoptr (GIOChannel) stdin_channel = NULL;

  stdin_channel = g_io_channel_unix_new (STDIN_FILENO);
  for (;;)
    {
      g_autoptr (GError) local_error = NULL;
      g_autofree char *string        = NULL;
      char            *newline       = NULL;
      g_autoptr (GVariant) variant   = NULL;
      g_autofree char *src_uri       = NULL;
      g_autofree char *dest_path     = NULL;
      g_autoptr (DownloadData) data  = NULL;

      g_io_channel_read_line (
          stdin_channel, &string, NULL, NULL, &local_error);
      if (string == NULL)
        {
          if (local_error != NULL)
            g_critical ("FATAL: Failure reading stdin channel: %s", local_error->message);
          g_main_loop_quit (loop);
          return NULL;
        }

      newline = g_utf8_strchr (string, -1, '\n');
      if (newline != NULL)
        *newline = '\0';

      variant = g_variant_parse (
          G_VARIANT_TYPE ("(ss)"),
          string, NULL, NULL,
          &local_error);
      if (variant == NULL)
        {
          g_critical ("Failure parsing variant text '%s' into structure: %s\n",
                      string, local_error->message);
          continue;
        }

      g_variant_get (variant, "(ss)", &src_uri, &dest_path);

      data       = download_data_new ();
      data->src  = g_steal_pointer (&src_uri);
      data->dest = g_steal_pointer (&dest_path);

      dex_future_disown (dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) download_fiber,
          download_data_ref (data), download_data_unref));
    }

  return NULL;
}

static DexFuture *
download_fiber (DownloadData *data)
{
  char *src                                 = data->src;
  char *dest                                = data->dest;
  g_autoptr (GError) local_error            = NULL;
  g_autoptr (GFile) dest_file               = NULL;
  g_autoptr (GFileOutputStream) dest_output = NULL;
  g_autoptr (SoupMessage) message           = NULL;
  gboolean success                          = FALSE;
  g_autoptr (GVariant) variant              = NULL;
  g_autofree char *output                   = NULL;

  dest_file   = g_file_new_for_path (dest);
  dest_output = g_file_replace (
      dest_file, NULL, FALSE,
      G_FILE_CREATE_REPLACE_DESTINATION,
      NULL, &local_error);
  if (dest_output == NULL)
    {
      g_critical ("%s", local_error->message);
      goto done;
    }

  message = soup_message_new (SOUP_METHOD_GET, src);
  success = dex_await (bz_send_with_global_http_session_then_splice_into (
                           message, G_OUTPUT_STREAM (dest_output)),
                       &local_error);
  if (!success)
    {
      g_critical ("%s", local_error->message);
      goto done;
    }

done:
  variant = g_variant_new ("(sb)", dest, success);
  output  = g_variant_print (variant, TRUE);

  dex_future_disown (dex_scheduler_spawn (
      /* ensure we only output on main thread */
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) print_fiber,
      g_steal_pointer (&output), g_free));

  return NULL;
}

static DexFuture *
print_fiber (char *output)
{
  fprintf (stdout, "%s\n", output);
  fflush (stdout);
  return NULL;
}

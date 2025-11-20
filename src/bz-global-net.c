/* bz-global-net.c
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

#define G_LOG_DOMAIN "BAZAAR::GLOBAL-NET"

#include <json-glib/json-glib.h>

#include "bz-env.h"
#include "bz-global-net.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    http_request,
    HttpRequest,
    {
      SoupMessage   *message;
      GOutputStream *splice_into;
      gboolean       close_output;
    },
    BZ_RELEASE_DATA (message, g_object_unref);
    BZ_RELEASE_DATA (splice_into, g_object_unref));
static DexFuture *
http_send_fiber (HttpRequestData *data);

static void
http_send_and_splice_finish (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data);

static DexFuture *
query_json_source_then (DexFuture     *future,
                        GOutputStream *output_stream);

static DexFuture *
send (SoupMessage   *message,
      GOutputStream *splice_into,
      gboolean       close_output);

DexFuture *
bz_send_with_global_http_session (SoupMessage *message)
{
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));
  return send (message, NULL, FALSE);
}

DexFuture *
bz_send_with_global_http_session_then_splice_into (SoupMessage   *message,
                                                   GOutputStream *output)
{
  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));
  dex_return_error_if_fail (G_IS_OUTPUT_STREAM (output));
  return send (message, output, TRUE);
}

DexFuture *
bz_https_query_json (const char *uri)
{
  g_autoptr (GError) local_error   = NULL;
  g_autoptr (SoupMessage) message  = NULL;
  SoupMessageHeaders *headers      = NULL;
  g_autoptr (GOutputStream) output = NULL;
  g_autoptr (DexFuture) future     = NULL;

  dex_return_error_if_fail (uri != NULL);

  message = soup_message_new (SOUP_METHOD_GET, uri);
  headers = soup_message_get_request_headers (message);
  /* Needed for github for some reason */
  soup_message_headers_append (headers, "User-Agent", "Bazaar");

  output = g_memory_output_stream_new_resizable ();

  future = send (message, output, TRUE);
  future = dex_future_then (
      future,
      (DexFutureCallback) query_json_source_then,
      g_object_ref (output), g_object_unref);
  return g_steal_pointer (&future);
}

DexFuture *
bz_query_flathub_v2_json (const char *request)
{
  g_autofree char *uri = NULL;

  dex_return_error_if_fail (request != NULL);

  uri = g_strdup_printf ("https://flathub.org/api/v2%s", request);
  return bz_https_query_json (uri);
}

DexFuture *
bz_query_flathub_v2_json_take (char *request)
{
  DexFuture *future = NULL;

  dex_return_error_if_fail (request != NULL);

  future = bz_query_flathub_v2_json (request);
  g_free (request);

  return future;
}

static DexFuture *
http_send_fiber (HttpRequestData *data)
{
  static SoupSession      *session      = NULL;
  SoupMessage             *message      = data->message;
  GOutputStream           *splice_into  = data->splice_into;
  gboolean                 close_output = data->close_output;
  GOutputStreamSpliceFlags splice_flags = G_OUTPUT_STREAM_SPLICE_NONE;
  g_autoptr (DexPromise) promise        = NULL;

  if (g_once_init_enter_pointer (&session))
    g_once_init_leave_pointer (&session, soup_session_new ());

  splice_flags = G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE;
  if (close_output)
    splice_flags |= G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET;

  promise = dex_promise_new_cancellable ();
  soup_session_send_and_splice_async (
      session,
      message,
      splice_into,
      splice_flags,
      G_PRIORITY_DEFAULT_IDLE,
      dex_promise_get_cancellable (promise),
      http_send_and_splice_finish,
      dex_ref (promise));

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static void
http_send_and_splice_finish (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  DexPromise *promise            = user_data;
  g_autoptr (GError) local_error = NULL;
  gssize bytes_written           = 0;

  g_assert (SOUP_IS_SESSION (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  bytes_written = soup_session_send_and_splice_finish (SOUP_SESSION (object), result, &local_error);
  if (bytes_written >= 0)
    {
      g_debug ("Spliced %zu bytes from http reply into output stream", bytes_written);
      dex_promise_resolve_uint64 (promise, bytes_written);
    }
  else
    {
      g_debug ("Could not splice http reply into output stream: %s", local_error->message);
      dex_promise_reject (promise, g_steal_pointer (&local_error));
    }

  dex_unref (promise);
}

static DexFuture *
query_json_source_then (DexFuture     *future,
                        GOutputStream *output_stream)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GBytes) bytes       = NULL;
  gsize         bytes_size       = 0;
  gconstpointer bytes_data       = NULL;
  g_autoptr (JsonParser) parser  = NULL;
  gboolean  result               = FALSE;
  JsonNode *node                 = NULL;

  bytes = g_memory_output_stream_steal_as_bytes (
      G_MEMORY_OUTPUT_STREAM (output_stream));
  bytes_data = g_bytes_get_data (bytes, &bytes_size);

  parser = json_parser_new_immutable ();
  result = json_parser_load_from_data (parser, bytes_data, bytes_size, &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  node = json_parser_get_root (parser);
  return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_ref (node));
}

static DexFuture *
send (SoupMessage   *message,
      GOutputStream *splice_into,
      gboolean       close_output)
{
  g_autoptr (HttpRequestData) data = NULL;
  g_autoptr (DexFuture) future     = NULL;

  data               = http_request_data_new ();
  data->message      = g_object_ref (message);
  data->splice_into  = splice_into != NULL ? g_object_ref (splice_into) : NULL;
  data->close_output = close_output;

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) http_send_fiber,
      http_request_data_ref (data),
      http_request_data_unref);
  return g_steal_pointer (&future);
}

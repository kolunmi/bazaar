/* bz-global-state.c
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

#define G_LOG_DOMAIN "BAZAAR::GLOBAL"

#include "bz-global-state.h"
#include "bz-env.h"
#include "bz-util.h"

BZ_DEFINE_DATA (
    http_send,
    HttpSend,
    {
      char          *uri;
      SoupMessage   *message;
      GOutputStream *output;
      gboolean       close_output;
      char          *content_type;
    },
    BZ_RELEASE_DATA (uri, g_free);
    BZ_RELEASE_DATA (message, g_object_unref);
    BZ_RELEASE_DATA (output, g_object_unref);
    BZ_RELEASE_DATA (content_type, g_free));

static DexFuture *
http_send_fiber (HttpSendData *data);

static void
http_send_finish (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data);

static void
splice_finish (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data);

static DexFuture *
query_flathub_then (DexFuture    *future,
                    HttpSendData *data);

SoupSession *
bz_get_global_http_session (void)
{
  static SoupSession *session = NULL;

  if (g_once_init_enter_pointer (&session))
    g_once_init_leave_pointer (&session, soup_session_new ());

  return session;
}

DexFuture *
bz_send_with_global_http_session (SoupMessage *message)
{
  g_autoptr (HttpSendData) data = NULL;

  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));

  data               = http_send_data_new ();
  data->message      = g_object_ref (message);
  data->output       = NULL;
  data->close_output = FALSE;
  data->content_type = NULL;

  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) http_send_fiber,
      g_steal_pointer (&data), http_send_data_unref);
}

DexFuture *
bz_send_with_global_http_session_then_splice_into (SoupMessage   *message,
                                                   GOutputStream *output)
{
  g_autoptr (HttpSendData) data = NULL;

  dex_return_error_if_fail (SOUP_IS_MESSAGE (message));
  dex_return_error_if_fail (G_IS_OUTPUT_STREAM (output));

  data               = http_send_data_new ();
  data->message      = g_object_ref (message);
  data->output       = g_object_ref (output);
  data->close_output = FALSE;
  data->content_type = NULL;

  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) http_send_fiber,
      g_steal_pointer (&data), http_send_data_unref);
}

DexFuture *
bz_query_flathub_v2_json (const char *request)
{
  g_autoptr (GError) local_error   = NULL;
  g_autofree char *uri             = NULL;
  g_autoptr (SoupMessage) message  = NULL;
  g_autoptr (GOutputStream) output = NULL;
  g_autoptr (HttpSendData) data    = NULL;
  g_autoptr (DexFuture) future     = NULL;

  dex_return_error_if_fail (request != NULL);

  uri     = g_strdup_printf ("https://flathub.org/api/v2%s", request);
  message = soup_message_new (SOUP_METHOD_GET, uri);
  output  = g_memory_output_stream_new_resizable ();

  g_debug ("Querying Flathub at URI %s ...", uri);

  data               = http_send_data_new ();
  data->uri          = g_steal_pointer (&uri);
  data->message      = g_object_ref (message);
  data->output       = g_object_ref (output);
  data->close_output = TRUE;
  data->content_type = g_strdup ("application/json");

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) http_send_fiber,
      http_send_data_ref (data), http_send_data_unref);
  future = dex_future_then (
      future,
      (DexFutureCallback) query_flathub_then,
      http_send_data_ref (data), http_send_data_unref);
  return g_steal_pointer (&future);
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
http_send_fiber (HttpSendData *data)
{
  g_autoptr (DexPromise) promise = NULL;

  if (data->uri != NULL)
    g_debug ("Sending message to uri %s now...", data->uri);

  promise = dex_promise_new_cancellable ();
  soup_session_send_async (
      bz_get_global_http_session (),
      data->message,
      G_PRIORITY_DEFAULT,
      dex_promise_get_cancellable (promise),
      http_send_finish,
      dex_ref (promise));

  if (data->output != NULL || data->content_type != NULL)
    {
      g_autoptr (GError) local_error    = NULL;
      g_autoptr (GInputStream) response = NULL;

      response = dex_await_object (DEX_FUTURE (g_steal_pointer (&promise)), &local_error);
      if (response == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      if (data->content_type != NULL)
        {
          SoupMessageHeaders *response_headers = NULL;
          const char         *content_type     = NULL;

          if (data->uri != NULL)
            g_debug ("Ensuring response from uri %s is of type '%s' as requested ...",
                     data->uri, data->content_type);

          response_headers = soup_message_get_response_headers (data->message);
          content_type     = soup_message_headers_get_content_type (response_headers, NULL);
          if (g_strcmp0 (content_type, data->content_type) != 0)
            return dex_future_new_reject (
                G_IO_ERROR,
                G_IO_ERROR_INVALID_DATA,
                "HTTP request cancelled: expected content type '%s', got '%s'",
                data->content_type,
                content_type);
        }

      if (data->output != NULL)
        {
          g_autoptr (DexPromise) splice  = NULL;
          GOutputStreamSpliceFlags flags = G_OUTPUT_STREAM_SPLICE_NONE;

          if (data->uri != NULL)
            g_debug ("Splicing response from uri %s into output stream as requested ...",
                     data->uri);

          splice = dex_promise_new_cancellable ();
          flags  = G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE;
          if (data->close_output)
            flags |= G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET;

          g_output_stream_splice_async (
              data->output,
              response,
              flags,
              G_PRIORITY_DEFAULT,
              dex_promise_get_cancellable (splice),
              splice_finish,
              dex_ref (splice));
          return DEX_FUTURE (g_steal_pointer (&splice));
        }
      else
        return dex_future_new_for_object (response);
    }
  else
    return DEX_FUTURE (g_steal_pointer (&promise));
}

static void
http_send_finish (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  DexPromise *promise             = user_data;
  g_autoptr (GError) local_error  = NULL;
  g_autoptr (GInputStream) stream = NULL;

  g_assert (SOUP_IS_SESSION (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  stream = soup_session_send_finish (SOUP_SESSION (object), result, &local_error);
  if (stream != NULL)
    dex_promise_resolve_object (promise, g_steal_pointer (&stream));
  else
    {
      g_debug ("Could not complete http operation: %s", local_error->message);
      dex_promise_reject (promise, g_steal_pointer (&local_error));
    }

  dex_unref (promise);
}

static void
splice_finish (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  DexPromise *promise            = user_data;
  g_autoptr (GError) local_error = NULL;
  gssize bytes_written           = 0;

  g_assert (G_IS_OUTPUT_STREAM (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_PROMISE (promise));

  bytes_written = g_output_stream_splice_finish (G_OUTPUT_STREAM (object), result, &local_error);
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
query_flathub_then (DexFuture    *future,
                    HttpSendData *data)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GBytes) bytes       = NULL;
  gsize         bytes_size       = 0;
  gconstpointer bytes_data       = NULL;
  g_autoptr (JsonParser) parser  = NULL;
  gboolean  result               = FALSE;
  JsonNode *node                 = NULL;

  bytes = g_memory_output_stream_steal_as_bytes (
      G_MEMORY_OUTPUT_STREAM (data->output));
  bytes_data = g_bytes_get_data (bytes, &bytes_size);

  g_debug ("Received %zu bytes back from Flathub", bytes_size);

  parser = json_parser_new_immutable ();
  result = json_parser_load_from_data (parser, bytes_data, bytes_size, &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  node = json_parser_get_root (parser);
  return dex_future_new_take_boxed (JSON_TYPE_NODE, json_node_ref (node));
}

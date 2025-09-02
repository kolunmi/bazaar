/* bz-error.c
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

#include <adwaita.h>
#include <glib/gi18n.h>

#include "bz-error.h"

static void
show_alert (GtkWidget  *widget,
            const char *title,
            const char *text,
            gboolean    markup);

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GtkWidget      *widget);

static void
await_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      DexPromise     *promise);

static void
unref_dex_closure (gpointer  data,
                   GClosure *closure);

void
bz_show_alert_for_widget (GtkWidget  *widget,
                          const char *title,
                          const char *text,
                          gboolean    markup)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (title != NULL);
  g_return_if_fail (text != NULL);

  show_alert (widget, title, text, markup);
}

void
bz_show_error_for_widget (GtkWidget  *widget,
                          const char *text)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (text != NULL);

  show_alert (widget, _ ("An Error Occurred"), text, FALSE);
}

static void
show_alert (GtkWidget  *widget,
            const char *title,
            const char *text,
            gboolean    markup)
{
  AdwDialog *alert = NULL;

  alert = adw_alert_dialog_new (NULL, NULL);
  adw_alert_dialog_set_prefer_wide_layout (ADW_ALERT_DIALOG (alert), TRUE);
  adw_alert_dialog_set_heading (
      ADW_ALERT_DIALOG (alert), title);
  adw_alert_dialog_set_body (
      ADW_ALERT_DIALOG (alert), text);
  adw_alert_dialog_set_body_use_markup (
      ADW_ALERT_DIALOG (alert), markup);
  adw_alert_dialog_add_responses (
      ADW_ALERT_DIALOG (alert),
      "close", _ ("Close"),
      "copy", _ ("Copy and Close"),
      NULL);
  adw_alert_dialog_set_response_appearance (
      ADW_ALERT_DIALOG (alert), "copy", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "close");

  g_signal_connect (alert, "response", G_CALLBACK (error_alert_response), widget);
  adw_dialog_present (alert, GTK_WIDGET (widget));
}

DexFuture *
bz_make_alert_dialog_future (AdwAlertDialog *dialog)
{
  g_autoptr (DexPromise) promise = NULL;

  dex_return_error_if_fail (ADW_IS_ALERT_DIALOG (dialog));

  promise = dex_promise_new ();
  g_signal_connect_data (
      dialog, "response",
      G_CALLBACK (await_alert_response),
      dex_ref (promise), unref_dex_closure,
      G_CONNECT_DEFAULT);

  return DEX_FUTURE (g_steal_pointer (&promise));
}

static void
error_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      GtkWidget      *widget)
{
  if (g_strcmp0 (response, "copy") == 0)
    {
      const char   *body = NULL;
      GdkClipboard *clipboard;

      body      = adw_alert_dialog_get_body (alert);
      clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

      gdk_clipboard_set_text (clipboard, body);
    }
}

static void
await_alert_response (AdwAlertDialog *alert,
                      gchar          *response,
                      DexPromise     *promise)
{
  dex_promise_resolve_string (promise, g_strdup (response));
}

static void
unref_dex_closure (gpointer  data,
                   GClosure *closure)
{
  DexPromise *promise = data;

  if (dex_future_is_pending (DEX_FUTURE (promise)))
    dex_promise_reject (
        promise,
        g_error_new (
            DEX_ERROR,
            DEX_ERROR_UNKNOWN,
            "The signal was disconnected"));

  dex_unref (promise);
}

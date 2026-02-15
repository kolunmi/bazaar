/* bz-error-dialog.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-error-dialog.h"

struct _BzErrorDialog
{
  AdwDialog parent_instance;

  AdwToastOverlay *toast_overlay;
  GtkLabel        *error_title;
  GtkTextView     *text_view;
};

G_DEFINE_FINAL_TYPE (BzErrorDialog, bz_error_dialog, ADW_TYPE_DIALOG)

static void
on_copy_button_clicked (GtkButton     *button,
                        BzErrorDialog *self)
{
  GtkTextBuffer   *buffer    = NULL;
  GtkTextIter      start     = { 0 };
  GtkTextIter      end       = { 0 };
  g_autofree char *text      = NULL;
  GdkClipboard    *clipboard = NULL;
  g_autoptr (AdwToast) toast = NULL;

  buffer = gtk_text_view_get_buffer (self->text_view);
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text      = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_text (clipboard, text);

  toast = adw_toast_new (_ ("Copied!"));
  adw_toast_set_timeout (toast, 2);
  adw_toast_overlay_add_toast (self->toast_overlay, g_steal_pointer (&toast));
}

static void
bz_error_dialog_class_init (BzErrorDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-error-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, BzErrorDialog, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzErrorDialog, error_title);
  gtk_widget_class_bind_template_child (widget_class, BzErrorDialog, text_view);
  gtk_widget_class_bind_template_callback (widget_class, on_copy_button_clicked);
}

static void
bz_error_dialog_init (BzErrorDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzErrorDialog *
bz_error_dialog_new (const char *title,
                     const char *text)
{
  BzErrorDialog *self;
  GtkTextBuffer *buffer;
  GtkTextIter    iter;

  g_return_val_if_fail (title != NULL, NULL);
  g_return_val_if_fail (text != NULL, NULL);

  self = g_object_new (BZ_TYPE_ERROR_DIALOG, NULL);

  gtk_label_set_text (self->error_title, title);

  buffer = gtk_text_view_get_buffer (self->text_view);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, text, -1);
  return self;
}

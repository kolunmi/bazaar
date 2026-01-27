/* bz-transaction-list-dialog.c
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

#include <glib/gi18n.h>

#include "bz-entry.h"
#include "bz-transaction-list-dialog.h"

struct _BzTransactionListDialog
{
  AdwAlertDialog parent_instance;

  GListModel         *entries;
  gboolean            confirmed;
  GtkFilterListModel *app_filter;

  /* Template widgets */
  GtkNoSelection *selection_model;
  GtkLabel       *secondary_label;
};

G_DEFINE_FINAL_TYPE (BzTransactionListDialog, bz_transaction_list_dialog, ADW_TYPE_ALERT_DIALOG)

static gboolean
match_for_app (BzEntry *item,
               gpointer user_data);

static void
on_response (AdwAlertDialog          *alert,
             gchar                   *response,
             BzTransactionListDialog *self);

static void
bz_transaction_list_dialog_dispose (GObject *object)
{
  BzTransactionListDialog *self = BZ_TRANSACTION_LIST_DIALOG (object);

  g_clear_object (&self->entries);
  g_clear_object (&self->app_filter);

  G_OBJECT_CLASS (bz_transaction_list_dialog_parent_class)->dispose (object);
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static void
bz_transaction_list_dialog_class_init (BzTransactionListDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_transaction_list_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-transaction-list-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, BzTransactionListDialog, selection_model);
  gtk_widget_class_bind_template_child (widget_class, BzTransactionListDialog, secondary_label);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
}

static void
bz_transaction_list_dialog_init (BzTransactionListDialog *self)
{
  GtkCustomFilter *filter = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));
  g_signal_connect (self, "response", G_CALLBACK (on_response), self);

  filter           = gtk_custom_filter_new ((GtkCustomFilterFunc) match_for_app, NULL, NULL);
  self->app_filter = gtk_filter_list_model_new (NULL, GTK_FILTER (filter));
  gtk_no_selection_set_model (self->selection_model, G_LIST_MODEL (self->app_filter));
}

static void
on_response (AdwAlertDialog          *alert,
             gchar                   *response,
             BzTransactionListDialog *self)
{
  self->confirmed = g_strcmp0 (response, "confirm") == 0;
}

AdwDialog *
bz_transaction_list_dialog_new (GListModel  *entries,
                                const gchar *heading,
                                const gchar *body,
                                const gchar *body_no_apps,
                                const gchar *secondary_label,
                                const gchar *cancel_label,
                                const gchar *confirm_label)
{
  BzTransactionListDialog *dialog  = NULL;
  guint                    n_total = 0;
  guint                    n_apps  = 0;
  guint                    n_other = 0;

  g_return_val_if_fail (G_IS_LIST_MODEL (entries), NULL);

  dialog          = g_object_new (BZ_TYPE_TRANSACTION_LIST_DIALOG, NULL);
  dialog->entries = g_object_ref (entries);

  gtk_filter_list_model_set_model (dialog->app_filter, entries);

  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel", cancel_label);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "confirm", confirm_label);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "confirm", ADW_RESPONSE_SUGGESTED);

  adw_alert_dialog_set_heading (ADW_ALERT_DIALOG (dialog), heading);

  n_total = g_list_model_get_n_items (entries);
  n_apps  = g_list_model_get_n_items (G_LIST_MODEL (dialog->app_filter));
  n_other = n_total - n_apps;

  if (n_total > 0)
    {
      if (n_apps == 0 && body_no_apps != NULL)
        {
          g_autofree char *formatted_body = NULL;

          formatted_body = g_strdup_printf (body_no_apps, n_other);
          adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), formatted_body);
          adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), NULL);
        }
      else
        {
          adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), body);

          if (n_other > 0 && secondary_label != NULL)
            {
              g_autofree char *formatted_label = NULL;

              formatted_label = g_strdup_printf (secondary_label, n_other);
              gtk_label_set_label (dialog->secondary_label, formatted_label);
              gtk_widget_set_visible (GTK_WIDGET (dialog->secondary_label), TRUE);
            }
        }
    }

  return ADW_DIALOG (dialog);
}

gboolean
bz_transaction_list_dialog_was_confirmed (BzTransactionListDialog *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_LIST_DIALOG (self), FALSE);

  return self->confirmed;
}

static gboolean
match_for_app (BzEntry *item,
               gpointer user_data)
{
  return bz_entry_is_of_kinds (item, BZ_ENTRY_KIND_APPLICATION);
}

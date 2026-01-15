/* bz-transaction-dialog.c
 *
 * Copyright 2026 Alexander Vanhee
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

#include "bz-error.h"
#include "bz-safety-calculator.h"
#include "bz-transaction-dialog.h"
#include "bz-transaction-list-dialog.h"
#include "bz-util.h"

BzTransactionDialogResult *
bz_transaction_dialog_result_new (void)
{
  return g_new0 (BzTransactionDialogResult, 1);
}

void
bz_transaction_dialog_result_free (BzTransactionDialogResult *result)
{
  if (result == NULL)
    return;

  g_clear_object (&result->selected_entry);
  g_free (result);
}

static gboolean
should_skip_entry (BzEntry *entry,
                   gboolean remove)
{
  gboolean is_installed;

  if (bz_entry_is_holding (entry))
    return TRUE;

  is_installed = bz_entry_is_installed (entry);

  return (!remove && is_installed) || (remove && !is_installed);
}

static GtkWidget *
create_entry_radio_button (BzEntry    *entry,
                           GtkWidget **out_radio)
{
  GtkWidget       *row;
  GtkWidget       *radio;
  g_autofree char *label;

  label = g_strdup (bz_entry_get_unique_id (entry));

  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), label);

  radio = gtk_check_button_new ();
  gtk_widget_set_valign (radio, GTK_ALIGN_CENTER);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), radio);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), radio);

  if (out_radio != NULL)
    *out_radio = radio;

  return row;
}

static GPtrArray *
create_entry_radio_buttons (AdwAlertDialog *alert,
                            GListStore     *store,
                            gboolean        remove)
{
  g_autoptr (GPtrArray) radios = NULL;
  GtkWidget *container         = NULL;

  container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

  radios = g_ptr_array_new ();
  if (store != NULL)
    {
      guint n_valid_entries = 0;

      for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store));)
        {
          g_autoptr (BzEntry) entry = NULL;

          entry = g_list_model_get_item (G_LIST_MODEL (store), i);
          if (should_skip_entry (entry, remove))
            {
              g_list_store_remove (store, i);
              continue;
            }
          n_valid_entries++;
          i++;
        }
      if (n_valid_entries > 1)
        {
          GtkWidget      *listbox           = NULL;
          GtkCheckButton *first_valid_radio = NULL;

          listbox = gtk_list_box_new ();
          gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
          gtk_widget_add_css_class (listbox, "boxed-list");

          for (guint i = 0; i < n_valid_entries; i++)
            {
              g_autoptr (BzEntry) entry = NULL;
              GtkWidget *row            = NULL;
              GtkWidget *radio          = NULL;

              entry = g_list_model_get_item (G_LIST_MODEL (store), i);
              row   = create_entry_radio_button (entry, &radio);
              g_ptr_array_add (radios, radio);

              if (first_valid_radio != NULL)
                gtk_check_button_set_group (GTK_CHECK_BUTTON (radio), first_valid_radio);
              else
                {
                  gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
                  first_valid_radio = (GtkCheckButton *) radio;
                }

              gtk_list_box_append (GTK_LIST_BOX (listbox), row);
            }

          gtk_box_append (GTK_BOX (container), listbox);
        }
    }

  if (remove)
    {
      GtkWidget *listbox         = NULL;
      GtkWidget *keep_data_row   = NULL;
      GtkWidget *delete_data_row = NULL;
      GtkWidget *keep_radio      = NULL;
      GtkWidget *delete_radio    = NULL;

      listbox = gtk_list_box_new ();
      gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
      gtk_widget_add_css_class (listbox, "boxed-list");

      keep_data_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (keep_data_row), _ ("Keep Data"));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (keep_data_row), _ ("Allow restoring settings and content"));
      keep_radio = gtk_check_button_new ();
      gtk_widget_set_valign (keep_radio, GTK_ALIGN_CENTER);
      gtk_check_button_set_active (GTK_CHECK_BUTTON (keep_radio), TRUE);
      adw_action_row_add_prefix (ADW_ACTION_ROW (keep_data_row), keep_radio);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (keep_data_row), keep_radio);
      gtk_list_box_append (GTK_LIST_BOX (listbox), keep_data_row);

      delete_data_row = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (delete_data_row), _ ("Delete Data"));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (delete_data_row), _ ("Permanently remove app data to save space"));
      delete_radio = gtk_check_button_new ();
      gtk_widget_set_valign (delete_radio, GTK_ALIGN_CENTER);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (delete_radio), GTK_CHECK_BUTTON (keep_radio));
      adw_action_row_add_prefix (ADW_ACTION_ROW (delete_data_row), delete_radio);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (delete_data_row), delete_radio);
      gtk_list_box_append (GTK_LIST_BOX (listbox), delete_data_row);

      g_ptr_array_add (radios, keep_radio);
      g_ptr_array_add (radios, delete_radio);
      gtk_box_append (GTK_BOX (container), listbox);
    }

  adw_alert_dialog_set_extra_child (alert, container);
  return g_steal_pointer (&radios);
}

static void
configure_install_dialog (AdwAlertDialog *alert,
                          const char     *title,
                          const char     *id)
{
  g_autofree char *heading = NULL;

  heading = g_strdup_printf (_ ("Install %s?"), title);

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, _ ("May install additional shared components"));

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("Cancel"),
                                  "install", _ ("Install"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "install", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (alert, "install");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static void
configure_remove_dialog (AdwAlertDialog *alert,
                         const char     *title,
                         const char     *id)
{
  g_autofree char *heading = NULL;

  heading = g_strdup_printf (_ ("Remove %s?"), title);

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (
      alert, g_strdup_printf (_ ("It will not be possible to use %s after it is uninstalled."), title));

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("Cancel"),
                                  "remove", _ ("Remove"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "remove", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (alert, "remove");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static void
configure_high_risk_warning_dialog (AdwAlertDialog *alert,
                                    const char     *title,
                                    BzHighRiskGroup risk_groups)
{
  g_autofree char *heading = NULL;
  g_autofree char *body    = NULL;

  heading = g_strdup_printf (_ ("“%s” is High Risk"), title);

  if (risk_groups & BZ_HIGH_RISK_GROUP_DISK)
    {
      body = g_strdup (_ ("This app has full access to your system, including all "
                          "<b>your files, browser history, saved passwords</b>, and "
                          "more. It also has access to the internet, meaning it "
                          "could send your data to outside parties.\n\n"
                          "Because the app is proprietary, it can not be audited "
                          "for what it does with these permissions."));
    }
  else if (risk_groups & BZ_HIGH_RISK_GROUP_X11)
    {
      body = g_strdup (_ ("This app uses the legacy X11 windowing system, which "
                          "allows it to <b>record all keystrokes, capture screenshots, "
                          "and monitor other applications</b>. It also has access "
                          "to the internet, meaning it could send your data to "
                          "outside parties.\n\n"
                          "Because the app is proprietary, it can not be audited "
                          "for what it does with these permissions."));
    }

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, body);
  adw_alert_dialog_set_body_use_markup (alert, TRUE);
  adw_alert_dialog_set_prefer_wide_layout (alert, TRUE);

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("Cancel"),
                                  "install", _ ("Install Anyway"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "install", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (alert, "cancel");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static BzHighRiskGroup
get_entry_high_risk_groups (BzEntry *entry)
{
  if (bz_entry_get_is_foss (entry))
    return BZ_HIGH_RISK_GROUP_NONE;

  return bz_safety_calculator_get_high_risk_groups (entry);
}

typedef struct
{
  GtkWidget    *parent;
  BzEntry      *entry;
  BzEntryGroup *group;
  gboolean      remove;
  gboolean      auto_confirm;
} ShowDialogData;

static void
show_dialog_data_free (ShowDialogData *data)
{
  g_clear_object (&data->entry);
  g_clear_object (&data->group);
  g_free (data);
}

static DexFuture *
show_dialog_fiber (ShowDialogData *data)
{
  g_autoptr (GError) local_error               = NULL;
  g_autoptr (GListStore) store                 = NULL;
  const char *title                            = NULL;
  const char *id                               = NULL;
  g_autoptr (AdwDialog) alert                  = NULL;
  g_autoptr (AdwDialog) risk_alert             = NULL;
  g_autoptr (GPtrArray) radios                 = NULL;
  g_autofree char *dialog_response             = NULL;
  g_autofree char *risk_response               = NULL;
  g_autoptr (BzTransactionDialogResult) result = NULL;
  g_autoptr (BzEntry) check_entry              = NULL;
  BzHighRiskGroup risk_groups                  = BZ_HIGH_RISK_GROUP_NONE;

  result = bz_transaction_dialog_result_new ();

  if (data->group != NULL)
    {
      store = dex_await_object (bz_entry_group_dup_all_into_store (data->group), &local_error);
      if (store == NULL)
        {
          bz_show_error_for_widget (data->parent, local_error->message);
          return dex_future_new_for_error (g_steal_pointer (&local_error));
        }
      title = bz_entry_group_get_title (data->group);
      id    = bz_entry_group_get_id (data->group);

      if (g_list_model_get_n_items (G_LIST_MODEL (store)) > 0)
        check_entry = g_list_model_get_item (G_LIST_MODEL (store), 0);
    }
  else
    {
      title       = bz_entry_get_title (data->entry);
      id          = bz_entry_get_id (data->entry);
      check_entry = g_object_ref (data->entry);
    }

  if (!data->remove && check_entry != NULL)
    {
      risk_groups = get_entry_high_risk_groups (check_entry);
    }

  if (risk_groups != BZ_HIGH_RISK_GROUP_NONE)
    {
      risk_alert = g_object_ref_sink (adw_alert_dialog_new (NULL, NULL));
      configure_high_risk_warning_dialog (ADW_ALERT_DIALOG (risk_alert), title, risk_groups);

      adw_dialog_present (risk_alert, data->parent);
      risk_response = dex_await_string (
          bz_make_alert_dialog_future (ADW_ALERT_DIALOG (risk_alert)),
          &local_error);

      if (risk_response == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      if (g_strcmp0 (risk_response, "install") != 0)
        {
          result->confirmed = FALSE;
          return dex_future_new_for_pointer (g_steal_pointer (&result));
        }
    }

  alert = g_object_ref_sink (adw_alert_dialog_new (NULL, NULL));
  if (data->remove)
    configure_remove_dialog (ADW_ALERT_DIALOG (alert), title, id);
  else
    configure_install_dialog (ADW_ALERT_DIALOG (alert), title, id);

  radios = create_entry_radio_buttons (ADW_ALERT_DIALOG (alert), store, data->remove);

  if (!data->remove && data->auto_confirm && radios->len <= 1 && risk_groups == BZ_HIGH_RISK_GROUP_NONE)
    {
      dialog_response = g_strdup ("install");
      g_ptr_array_set_size (radios, 0);
      g_clear_object (&alert);
    }
  else if (data->remove && data->auto_confirm && radios->len <= 1)
    {
      dialog_response = g_strdup ("remove");
      g_ptr_array_set_size (radios, 0);
      g_clear_object (&alert);
    }
  else
    {
      adw_dialog_present (alert, data->parent);
      dialog_response = dex_await_string (
          bz_make_alert_dialog_future (ADW_ALERT_DIALOG (alert)),
          &local_error);
      if (dialog_response == NULL)
        return dex_future_new_for_error (g_steal_pointer (&local_error));

      if (data->remove && radios->len >= 2)
        {
          GtkCheckButton *delete_radio = g_ptr_array_index (radios, radios->len - 1);
          result->delete_user_data     = gtk_check_button_get_active (delete_radio);
        }
    }

  result->confirmed = (g_strcmp0 (dialog_response, "install") == 0) ||
                      (g_strcmp0 (dialog_response, "remove") == 0);

  if (!result->confirmed)
    return dex_future_new_for_pointer (g_steal_pointer (&result));

  if (data->group != NULL)
    {
      guint n_entries = g_list_model_get_n_items (G_LIST_MODEL (store));

      for (guint i = 0; i < MIN (n_entries, radios->len); i++)
        {
          GtkCheckButton *check = g_ptr_array_index (radios, i);

          if (gtk_check_button_get_active (check))
            {
              result->selected_entry = g_list_model_get_item (G_LIST_MODEL (store), i);
              break;
            }
        }

      if (result->selected_entry == NULL && n_entries > 0)
        result->selected_entry = g_list_model_get_item (G_LIST_MODEL (store), 0);
    }
  else
    {
      result->selected_entry = g_object_ref (data->entry);
    }

  return dex_future_new_for_pointer (g_steal_pointer (&result));
}

DexFuture *
bz_transaction_dialog_show (GtkWidget    *parent,
                            BzEntry      *entry,
                            BzEntryGroup *group,
                            gboolean      remove,
                            gboolean      auto_confirm)
{
  ShowDialogData *data;

  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (entry != NULL || group != NULL, NULL);

  data               = g_new0 (ShowDialogData, 1);
  data->parent       = parent;
  data->entry        = entry ? g_object_ref (entry) : NULL;
  data->group        = group ? g_object_ref (group) : NULL;
  data->remove       = remove;
  data->auto_confirm = auto_confirm;

  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0,
      (DexFiberFunc) show_dialog_fiber,
      data,
      (GDestroyNotify) show_dialog_data_free);
}

BzBulkInstallDialogResult *
bz_bulk_install_dialog_result_new (void)
{
  BzBulkInstallDialogResult *result = g_new0 (BzBulkInstallDialogResult, 1);
  result->entries                   = g_ptr_array_new_with_free_func (g_object_unref);
  return result;
}

void
bz_bulk_install_dialog_result_free (BzBulkInstallDialogResult *result)
{
  if (result == NULL)
    return;

  g_clear_pointer (&result->entries, g_ptr_array_unref);
  g_free (result);
}

typedef struct
{
  GtkWidget  *parent;
  GListModel *groups;
} BulkInstallDialogData;

static void
bulk_install_dialog_data_free (BulkInstallDialogData *data)
{
  g_clear_object (&data->groups);
  g_free (data);
}

static DexFuture *
bulk_install_dialog_fiber (BulkInstallDialogData *data)
{
  g_autoptr (GError) local_error               = NULL;
  g_autoptr (BzBulkInstallDialogResult) result = NULL;
  g_autoptr (GPtrArray) resolved_entries       = NULL;
  g_autoptr (GListStore) entries_store         = NULL;
  AdwDialog       *dialog                      = NULL;
  g_autofree char *dialog_response             = NULL;
  g_autofree char *heading                     = NULL;
  guint            n_groups                    = 0;

  result           = bz_bulk_install_dialog_result_new ();
  resolved_entries = g_ptr_array_new_with_free_func (g_object_unref);

  if (data->groups == NULL)
    {
      result->confirmed = FALSE;
      return dex_future_new_for_pointer (g_steal_pointer (&result));
    }

  n_groups = g_list_model_get_n_items (data->groups);

  for (guint i = 0; i < n_groups; i++)
    {
      g_autoptr (BzEntryGroup) group = NULL;
      g_autoptr (GListStore) store   = NULL;
      g_autoptr (BzEntry) entry      = NULL;

      group = g_list_model_get_item (data->groups, i);

      if (bz_entry_group_get_removable (group) > 0)
        continue;

      store = dex_await_object (bz_entry_group_dup_all_into_store (group), &local_error);
      if (store == NULL || g_list_model_get_n_items (G_LIST_MODEL (store)) == 0)
        continue;

      entry = g_list_model_get_item (G_LIST_MODEL (store), 0);
      if (entry == NULL)
        continue;

      if (bz_entry_is_installed (entry) || bz_entry_is_holding (entry))
        continue;

      g_ptr_array_add (resolved_entries, g_object_ref (entry));
    }

  if (resolved_entries->len == 0)
    {
      g_autoptr (AdwDialog) info_alert = NULL;

      info_alert = g_object_ref_sink (adw_alert_dialog_new (
          _ ("All apps are already installed"), NULL));

      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (info_alert), "ok", _ ("OK"));
      adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (info_alert), "ok");
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (info_alert), "ok");

      adw_dialog_present (info_alert, data->parent);

      dex_await (bz_make_alert_dialog_future (ADW_ALERT_DIALOG (info_alert)), NULL);

      result->confirmed = FALSE;
      return dex_future_new_for_pointer (g_steal_pointer (&result));
    }

  entries_store = g_list_store_new (BZ_TYPE_ENTRY);
  for (guint i = 0; i < resolved_entries->len; i++)
    g_list_store_append (entries_store, g_ptr_array_index (resolved_entries, i));

  heading = g_strdup_printf (ngettext ("Install %u App?",
                                       "Install %u Apps?",
                                       resolved_entries->len),
                             resolved_entries->len);

  dialog = bz_transaction_list_dialog_new (
      G_LIST_MODEL (entries_store),
      heading,
      _ ("The following will be installed."),
      _ ("%d runtimes and/or addons will be installed."),
      _ ("Additionally, %d runtimes and/or addons will be installed."),
      _ ("Cancel"),
      _ ("Install All"));

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "confirm");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  adw_dialog_present (dialog, data->parent);

  dialog_response = dex_await_string (
      bz_make_alert_dialog_future (ADW_ALERT_DIALOG (dialog)),
      &local_error);

  if (dialog_response == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  result->confirmed = bz_transaction_list_dialog_was_confirmed (BZ_TRANSACTION_LIST_DIALOG (dialog));

  if (result->confirmed)
    {
      for (guint i = 0; i < resolved_entries->len; i++)
        {
          BzEntry *entry = g_ptr_array_index (resolved_entries, i);
          g_ptr_array_add (result->entries, g_object_ref (entry));
        }
    }

  return dex_future_new_for_pointer (g_steal_pointer (&result));
}

DexFuture *
bz_bulk_install_dialog_show (GtkWidget  *parent,
                             GListModel *groups)
{
  BulkInstallDialogData *data;

  g_return_val_if_fail (GTK_IS_WIDGET (parent), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (groups), NULL);

  data         = g_new0 (BulkInstallDialogData, 1);
  data->parent = parent;
  data->groups = g_object_ref (groups);

  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      0,
      (DexFiberFunc) bulk_install_dialog_fiber,
      data,
      (GDestroyNotify) bulk_install_dialog_data_free);
}

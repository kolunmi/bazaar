/* bz-addons-dialog.c
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

#include "bz-addons-dialog.h"
#include "bz-entry.h"
#include "bz-result.h"
#include <glib/gi18n.h>

struct _BzAddonsDialog
{
  AdwDialog parent_instance;

  BzResult   *entry;
  GListModel *model;

  /* Template widgets */
  AdwPreferencesGroup *addons_group;
};

G_DEFINE_FINAL_TYPE (BzAddonsDialog, bz_addons_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,

  PROP_ENTRY,
  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_TRANSACT,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
transact_cb (BzAddonsDialog *self,
             GtkButton      *button)
{
  BzEntry *entry = NULL;

  entry = g_object_get_data (G_OBJECT (button), "entry");
  if (entry == NULL)
    return;

  g_signal_emit (self, signals[SIGNAL_TRANSACT], 0, entry);
}

static void
update_button_for_entry (GtkButton *button,
                         BzEntry   *entry)
{
  gboolean    installed = FALSE;
  gboolean    holding   = FALSE;
  const char *icon_name;
  const char *tooltip_text;

  g_object_get (entry,
                "installed", &installed,
                "holding", &holding,
                NULL);

  if (installed)
    {
      icon_name    = "user-trash-symbolic";
      tooltip_text = _ ("Remove");
      gtk_widget_remove_css_class (GTK_WIDGET (button), "suggested-action");
      gtk_widget_add_css_class (GTK_WIDGET (button), "destructive-action");
    }
  else
    {
      icon_name    = "folder-download-symbolic";
      tooltip_text = _ ("Install");
      gtk_widget_remove_css_class (GTK_WIDGET (button), "destructive-action");
      gtk_widget_add_css_class (GTK_WIDGET (button), "suggested-action");
    }

  gtk_button_set_icon_name (button, icon_name);
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip_text);
  gtk_widget_set_sensitive (GTK_WIDGET (button), !holding);
}

static void
entry_notify_cb (BzEntry      *entry,
                 GParamSpec   *pspec,
                 AdwActionRow *action_row)
{
  g_autofree char *title       = NULL;
  g_autofree char *description = NULL;
  GtkButton       *action_button;

  action_button = g_object_get_data (G_OBJECT (action_row), "button");
  if (action_button == NULL)
    return;

  g_object_get (entry,
                "title", &title,
                "description", &description,
                NULL);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (action_row), title);
  adw_action_row_set_subtitle (action_row, description);

  update_button_for_entry (action_button, entry);
}

static void
update_action_row_from_result (AdwActionRow   *action_row,
                               BzResult       *result,
                               BzAddonsDialog *self)
{
  BzEntry         *entry;
  g_autofree char *title       = NULL;
  g_autofree char *description = NULL;
  GtkButton       *action_button;

  entry = bz_result_get_object (result);
  if (entry == NULL)
    return;

  g_object_get (entry,
                "title", &title,
                "description", &description,
                NULL);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (action_row), title);
  adw_action_row_set_subtitle (action_row, description);

  action_button = GTK_BUTTON (gtk_button_new ());
  gtk_widget_set_valign (GTK_WIDGET (action_button), GTK_ALIGN_CENTER);
  g_object_set_data_full (G_OBJECT (action_button), "entry", g_object_ref (entry), g_object_unref);
  g_signal_connect_swapped (action_button, "clicked",
                            G_CALLBACK (transact_cb), self);

  update_button_for_entry (action_button, entry);

  g_object_set_data (G_OBJECT (action_row), "button", action_button);

  adw_action_row_add_suffix (action_row, GTK_WIDGET (action_button));
  adw_action_row_set_activatable_widget (action_row, GTK_WIDGET (action_button));

  g_signal_connect_object (entry, "notify::installed",
                           G_CALLBACK (entry_notify_cb),
                           action_row, 0);
  g_signal_connect_object (entry, "notify::holding",
                           G_CALLBACK (entry_notify_cb),
                           action_row, 0);
}

static void
result_resolved_cb (BzResult   *result,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  AdwActionRow   *action_row;
  BzAddonsDialog *self;

  if (!bz_result_get_resolved (result))
    return;

  action_row = g_object_get_data (G_OBJECT (result), "action-row");
  self       = g_object_get_data (G_OBJECT (result), "dialog");

  if (action_row == NULL || self == NULL)
    return;

  update_action_row_from_result (action_row, result, self);
}

static AdwActionRow *
create_addon_action_row (BzAddonsDialog *self,
                         BzResult       *result)
{
  AdwActionRow *action_row;

  action_row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (action_row), FALSE);

  if (bz_result_get_resolved (result))
    {
      update_action_row_from_result (action_row, result, self);
    }
  else
    {
      g_object_set_data (G_OBJECT (result), "action-row", action_row);
      g_object_set_data (G_OBJECT (result), "dialog", self);
      g_signal_connect (result, "notify::resolved",
                        G_CALLBACK (result_resolved_cb),
                        NULL);
    }

  return action_row;
}

static void
populate_addons (BzAddonsDialog *self)
{
  guint n_items = 0;

  if (!self->model)
    return;

  if (!self->addons_group)
    return;

  n_items = g_list_model_get_n_items (self->model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzResult) result = NULL;
      AdwActionRow *action_row;

      result = g_list_model_get_item (self->model, i);
      if (!result)
        continue;

      action_row = create_addon_action_row (self, result);
      if (action_row)
        adw_preferences_group_add (self->addons_group, GTK_WIDGET (action_row));
    }
}

static void
bz_addons_dialog_dispose (GObject *object)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  g_clear_object (&self->entry);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (bz_addons_dialog_parent_class)->dispose (object);
}

static void
bz_addons_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, self->entry);
      break;
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_addons_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_clear_object (&self->entry);
      self->entry = g_value_dup_object (value);
      break;
    case PROP_MODEL:
      g_clear_object (&self->model);
      self->model = g_value_dup_object (value);
      populate_addons (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_addons_dialog_constructed (GObject *object)
{
  BzAddonsDialog *self = BZ_ADDONS_DIALOG (object);

  G_OBJECT_CLASS (bz_addons_dialog_parent_class)->constructed (object);

  if (self->model && self->addons_group)
    populate_addons (self);
}

static void
bz_addons_dialog_class_init (BzAddonsDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_addons_dialog_dispose;
  object_class->constructed  = bz_addons_dialog_constructed;
  object_class->get_property = bz_addons_dialog_get_property;
  object_class->set_property = bz_addons_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_TRANSACT] =
      g_signal_new (
          "transact",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_TRANSACT],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-addons-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, BzAddonsDialog, addons_group);
}

static void
bz_addons_dialog_init (BzAddonsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_addons_dialog_new (BzEntry    *entry,
                      GListModel *model)
{
  BzAddonsDialog *addons_dialog = NULL;

  addons_dialog = g_object_new (
      BZ_TYPE_ADDONS_DIALOG,
      "entry", entry,
      "model", model,
      NULL);

  return ADW_DIALOG (addons_dialog);
}

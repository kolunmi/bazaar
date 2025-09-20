/* bz-share-dialog.c
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

#include "bz-share-dialog.h"
#include "bz-entry.h"
#include "bz-url.h"
#include <glib/gi18n.h>

struct _BzShareDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;

  /* Template widgets */
  AdwToastOverlay     *toast_overlay;
  AdwPreferencesGroup *urls_group;
};

G_DEFINE_FINAL_TYPE (BzShareDialog, bz_share_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,

  PROP_ENTRY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
copy_cb (BzShareDialog *self,
         GtkButton     *button)
{
  const char   *link      = NULL;
  GdkClipboard *clipboard = NULL;
  AdwToast     *toast     = NULL;

  link = g_object_get_data (G_OBJECT (button), "url");

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_text (clipboard, link);

  toast = adw_toast_new (_ ("Copied!"));
  adw_toast_set_timeout (toast, 1);
  adw_toast_overlay_add_toast (self->toast_overlay, toast);
}

static void
follow_link_cb (BzShareDialog *self,
                GtkButton     *button)
{
  const char *link = NULL;

  link = g_object_get_data (G_OBJECT (button), "url");
  g_app_info_launch_default_for_uri (link, NULL, NULL);
}

static AdwActionRow *
create_url_action_row (BzShareDialog *self, BzUrl *url_item)
{
  g_autofree char *url_string = NULL;
  g_autofree char *url_title  = NULL;
  AdwActionRow    *action_row;
  GtkBox          *suffix_box;
  GtkButton       *copy_button;
  GtkButton       *open_button;
  GtkSeparator    *separator;

  g_object_get (url_item,
                "url", &url_string,
                "name", &url_title,
                NULL);

  /* `PreferenceGroup`s can't be constructed using the list widget
     framework, so we must do this manually. */

  action_row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (action_row),
                                 url_title ? url_title : url_string);
  adw_action_row_set_subtitle (action_row, url_string);

  suffix_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_widget_set_valign (GTK_WIDGET (suffix_box), GTK_ALIGN_CENTER);

  copy_button = GTK_BUTTON (gtk_button_new_from_icon_name ("edit-copy-symbolic"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (copy_button), _ ("Copy Link"));
  gtk_button_set_has_frame (copy_button, FALSE);
  g_object_set_data_full (G_OBJECT (copy_button), "url", g_strdup (url_string), g_free);
  g_signal_connect_swapped (copy_button, "clicked",
                            G_CALLBACK (copy_cb), self);

  separator = GTK_SEPARATOR (gtk_separator_new (GTK_ORIENTATION_VERTICAL));
  gtk_widget_set_margin_top (GTK_WIDGET (separator), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (separator), 6);

  open_button = GTK_BUTTON (gtk_button_new_from_icon_name ("external-link-symbolic"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (open_button), _ ("Open Link"));
  gtk_button_set_has_frame (open_button, FALSE);
  g_object_set_data_full (G_OBJECT (open_button), "url", g_strdup (url_string), g_free);
  g_signal_connect_swapped (open_button, "clicked",
                            G_CALLBACK (follow_link_cb), self);

  gtk_box_append (suffix_box, GTK_WIDGET (copy_button));
  gtk_box_append (suffix_box, GTK_WIDGET (separator));
  gtk_box_append (suffix_box, GTK_WIDGET (open_button));

  adw_action_row_add_suffix (action_row, GTK_WIDGET (suffix_box));
  adw_action_row_set_activatable_widget (action_row, GTK_WIDGET (open_button));

  return action_row;
}

static void
populate_urls (BzShareDialog *self)
{
  g_autoptr (GListModel) urls_model = NULL;
  guint n_items                     = 0;

  if (!self->entry)
    return;

  g_object_get (self->entry, "share-urls", &urls_model, NULL);
  if (!urls_model)
    return;

  n_items = g_list_model_get_n_items (urls_model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzUrl) url_item = NULL;
      AdwActionRow *action_row;

      url_item   = g_list_model_get_item (urls_model, i);
      action_row = create_url_action_row (self, url_item);
      adw_preferences_group_add (self->urls_group, GTK_WIDGET (action_row));
    }
}

static void
bz_share_dialog_dispose (GObject *object)
{
  BzShareDialog *self = BZ_SHARE_DIALOG (object);

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (bz_share_dialog_parent_class)->dispose (object);
}

static void
bz_share_dialog_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzShareDialog *self = BZ_SHARE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, self->entry);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_share_dialog_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzShareDialog *self = BZ_SHARE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_clear_object (&self->entry);
      self->entry = g_value_dup_object (value);
      populate_urls (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_share_dialog_class_init (BzShareDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_share_dialog_dispose;
  object_class->get_property = bz_share_dialog_get_property;
  object_class->set_property = bz_share_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_URL);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-share-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, BzShareDialog, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzShareDialog, urls_group);
}

static void
bz_share_dialog_init (BzShareDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_share_dialog_new (BzEntry *entry)
{
  BzShareDialog *share_dialog = NULL;

  share_dialog = g_object_new (
      BZ_TYPE_SHARE_DIALOG,
      "entry", entry,
      NULL);

  return ADW_DIALOG (share_dialog);
}

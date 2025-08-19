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

struct _BzShareDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;

  /* Template widgets */
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
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
copy_cb (GtkListItem *list_item,
         GtkButton   *button)
{
  BzUrl           *item = NULL;
  g_autofree char *link = NULL;
  GdkClipboard    *clipboard;

  item = gtk_list_item_get_item (list_item);
  g_object_get (item, "url", &link, NULL);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_text (clipboard, link);
}

static void
follow_link_cb (GtkListItem *list_item,
                GtkButton   *button)
{
  BzUrl           *item = NULL;
  g_autofree char *link = NULL;

  item = gtk_list_item_get_item (list_item);
  g_object_get (item, "url", &link, NULL);

  g_app_info_launch_default_for_uri (link, NULL, NULL);
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
  gtk_widget_class_bind_template_callback (widget_class, copy_cb);
  gtk_widget_class_bind_template_callback (widget_class, follow_link_cb);
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

/* bz-license-dialog.c
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

#include "bz-license-dialog.h"
#include "bz-entry.h"
#include "bz-spdx.h"
#include "bz-url.h"
#include <glib/gi18n.h>

struct _BzLicenseDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;
};

G_DEFINE_FINAL_TYPE (BzLicenseDialog, bz_license_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_ENTRY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_license_dialog_dispose (GObject *object)
{
  BzLicenseDialog *self = BZ_LICENSE_DIALOG (object);

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (bz_license_dialog_parent_class)->dispose (object);
}

static void
bz_license_dialog_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BzLicenseDialog *self = BZ_LICENSE_DIALOG (object);

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
bz_license_dialog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BzLicenseDialog *self = BZ_LICENSE_DIALOG (object);

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

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static char *
get_license_info (gpointer object,
                  BzEntry *entry)
{
  const char      *license      = NULL;
  gboolean         is_foss      = FALSE;
  g_autofree char *license_name = NULL;
  g_autofree char *license_url  = NULL;

  if (entry == NULL)
    return g_strdup ("");

  g_object_get (entry, "is-floss", &is_foss, NULL);

  if (!is_foss)
    {
      return g_strdup (_ ("This app is not developed in the open, so only its developers know how it works. "
                          "It may be insecure in ways that are hard to detect, and it may change without oversight.\n\n"
                          "You may not be able to contribute to this app."));
    }

  g_object_get (entry, "project-license", &license, NULL);

  if (license == NULL || *license == '\0')
    {
      return g_strdup (_ ("This app is developed in the open by an international community.\n\n"
                          "You can participate and help make it even better."));
    }

  license_name = bz_spdx_get_name (license);

  if (license_name == NULL || *license_name == '\0')
    license_name = g_strdup (license);

  license_url = bz_spdx_get_url (license);

  return g_strdup_printf (_ ("This app is developed in the open by an international community, "
                             "and released under the <a href=\"%s\">%s</a> license.\n\n"
                             "You can participate and help make it even better."),
                          license_url,
                          license_name);
}

static void
contribute_cb (BzLicenseDialog *self)
{
  GListModel *share_urls = NULL;
  BzUrl      *first_url  = NULL;
  const char *url        = NULL;
  GtkRoot    *root       = NULL;

  if (self->entry == NULL || !BZ_IS_ENTRY (self->entry))
    return;

  g_object_get (self->entry, "share-urls", &share_urls, NULL);

  if (share_urls == NULL || g_list_model_get_n_items (share_urls) < 1)
    {
      g_clear_object (&share_urls);
      return;
    }

  first_url = g_list_model_get_item (share_urls, 1);

  if (first_url != NULL)
    {
      url = bz_url_get_url (first_url);

      if (url != NULL && *url != '\0')
        {
          root = gtk_widget_get_root (GTK_WIDGET (self));
          gtk_show_uri (GTK_WINDOW (root), url, GDK_CURRENT_TIME);
        }

      g_object_unref (first_url);
    }

  g_clear_object (&share_urls);
}

static void
bz_license_dialog_class_init (BzLicenseDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_license_dialog_dispose;
  object_class->get_property = bz_license_dialog_get_property;
  object_class->set_property = bz_license_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (
      widget_class,
      "/io/github/kolunmi/Bazaar/bz-license-dialog.ui");

  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, get_license_info);
  gtk_widget_class_bind_template_callback (widget_class, contribute_cb);
}

static void
bz_license_dialog_init (BzLicenseDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_license_dialog_new (BzEntry *entry)
{
  return g_object_new (BZ_TYPE_LICENSE_DIALOG,
                       "entry", entry,
                       NULL);
}

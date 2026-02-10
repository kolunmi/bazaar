/* bz-donations-dialog.c
 *
 * Copyright 2026 Eva M
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

#include "bz-appstream-description-render.h"
#include "bz-donations-dialog.h"
#include "bz-template-callbacks.h"

struct _BzDonationsDialog
{
  AdwDialog parent_instance;

  BzStateInfo *state;

  /* Template widgets */
  GtkLabel *title;
};

G_DEFINE_FINAL_TYPE (BzDonationsDialog, bz_donations_dialog, ADW_TYPE_DIALOG);

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_donations_dialog_dispose (GObject *object)
{
  BzDonationsDialog *self = BZ_DONATIONS_DIALOG (object);

  g_clear_pointer (&self->state, g_object_unref);

  G_OBJECT_CLASS (bz_donations_dialog_parent_class)->dispose (object);
}

static void
bz_donations_dialog_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzDonationsDialog *self = BZ_DONATIONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_donations_dialog_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_donations_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzDonationsDialog *self = BZ_DONATIONS_DIALOG (object);

  switch (prop_id)
    {
    case PROP_STATE:
      bz_donations_dialog_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
donate_clicked (BzDonationsDialog *self,
                GtkButton         *button)
{
  g_app_info_launch_default_for_uri (
      DONATE_LINK, NULL, NULL);
}

static void
release_page_clicked (BzDonationsDialog *self,
                      GtkButton         *button)
{
  g_app_info_launch_default_for_uri (
      RELEASE_PAGE, NULL, NULL);
}

static void
bz_donations_dialog_class_init (BzDonationsDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_donations_dialog_set_property;
  object_class->get_property = bz_donations_dialog_get_property;
  object_class->dispose      = bz_donations_dialog_dispose;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_APPSTREAM_DESCRIPTION_RENDER);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-donations-dialog.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);
  gtk_widget_class_bind_template_child (widget_class, BzDonationsDialog, title);
  gtk_widget_class_bind_template_callback (widget_class, donate_clicked);
  gtk_widget_class_bind_template_callback (widget_class, release_page_clicked);
}

static void
bz_donations_dialog_init (BzDonationsDialog *self)
{
  g_autofree char *ui_version = NULL;
  char            *space      = NULL;
  g_autofree char *title_str  = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  ui_version = g_strdup (PACKAGE_VERSION);
  space      = g_utf8_strchr (ui_version, strlen (PACKAGE_VERSION), ' ');
  if (space != NULL)
    *space = '\0';

  /* Translators: the %s format specifier will be something along the lines of "0.7.6" etc */
  title_str = g_strdup_printf (_("What's New in Version %s?"), ui_version);
  gtk_label_set_label (self->title, title_str);
}

AdwDialog *
bz_donations_dialog_new (void)
{
  return g_object_new (BZ_TYPE_DONATIONS_DIALOG, NULL);
}

BzStateInfo *
bz_donations_dialog_get_state (BzDonationsDialog *self)
{
  g_return_val_if_fail (BZ_IS_DONATIONS_DIALOG (self), NULL);
  return self->state;
}

void
bz_donations_dialog_set_state (BzDonationsDialog *self,
                               BzStateInfo       *state)
{
  g_return_if_fail (BZ_IS_DONATIONS_DIALOG (self));
  g_return_if_fail (state == NULL || BZ_IS_STATE_INFO (state));

  if (state == self->state)
    return;

  g_clear_pointer (&self->state, g_object_unref);
  if (state != NULL)
    self->state = g_object_ref (state);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

/* End of bz-donations-dialog.c */

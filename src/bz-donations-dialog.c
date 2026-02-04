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

#include "bz-donations-dialog.h"
#include "bz-template-callbacks.h"

struct _BzDonationsDialog
{
  AdwDialog parent_instance;

  BzStateInfo *state;

  /* Template widgets */
  GtkCheckButton *disable_donations_banner_check;
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
banner_disable_toggled (BzDonationsDialog *self,
                        GtkCheckButton    *button)
{
  gboolean   disable  = FALSE;
  GSettings *settings = NULL;

  if (self->state == NULL)
    return;

  disable = gtk_check_button_get_active (button);

  settings = bz_state_info_get_settings (self->state);
  g_settings_set_boolean (settings, "disable-donations-banner", disable);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-donations-dialog.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);
  gtk_widget_class_bind_template_child (widget_class, BzDonationsDialog, disable_donations_banner_check);
  gtk_widget_class_bind_template_callback (widget_class, donate_clicked);
  gtk_widget_class_bind_template_callback (widget_class, banner_disable_toggled);
}

static void
bz_donations_dialog_init (BzDonationsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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

  if (state == self->state)
    return;

  g_clear_pointer (&self->state, g_object_unref);
  if (state != NULL)
    {
      GSettings *settings = NULL;

      self->state = g_object_ref (state);

      settings = bz_state_info_get_settings (state);
      gtk_check_button_set_active (
          self->disable_donations_banner_check,
          g_settings_get_boolean (settings, "disable-donations-banner"));
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

/* End of bz-donations-dialog.c */

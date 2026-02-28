/* bz-install-controls.c
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

#include <glib/gi18n.h>

#include "bz-install-controls.h"
#include "bz-template-callbacks.h"
#include "bz-util.h"

struct _BzInstallControls
{
  GtkBox parent_instance;

  BzEntryGroup *group;
  gboolean      wide;

  /* Template widgets */
  GtkWidget *open_button;
  GtkWidget *install_button;
};

G_DEFINE_FINAL_TYPE (BzInstallControls, bz_install_controls, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_WIDE,
  PROP_ENTRY_GROUP,
  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,
  SIGNAL_REMOVE,
  SIGNAL_RUN,
  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
install_cb (BzInstallControls *self,
            GtkButton         *button)
{
  g_signal_emit (self, signals[SIGNAL_INSTALL], 0);
}

static void
remove_cb (BzInstallControls *self,
           GtkButton         *button)
{
  g_signal_emit (self, signals[SIGNAL_REMOVE], 0);
}

static void
run_cb (BzInstallControls *self,
        GtkButton         *button)
{
  g_signal_emit (self, signals[SIGNAL_RUN], 0);
}

static void
bz_install_controls_dispose (GObject *object)
{
  BzInstallControls *self = BZ_INSTALL_CONTROLS (object);

  g_clear_object (&self->group);

  G_OBJECT_CLASS (bz_install_controls_parent_class)->dispose (object);
}

static void
bz_install_controls_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzInstallControls *self = BZ_INSTALL_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_WIDE:
      g_value_set_boolean (value, self->wide);
      break;
    case PROP_ENTRY_GROUP:
      g_value_set_object (value, self->group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_install_controls_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzInstallControls *self = BZ_INSTALL_CONTROLS (object);

  switch (prop_id)
    {
    case PROP_WIDE:
      bz_install_controls_set_wide (self, g_value_get_boolean (value));
      break;
    case PROP_ENTRY_GROUP:
      bz_install_controls_set_entry_group (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_install_controls_class_init (BzInstallControlsClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_install_controls_dispose;
  object_class->get_property = bz_install_controls_get_property;
  object_class->set_property = bz_install_controls_set_property;

  props[PROP_WIDE] =
      g_param_spec_boolean (
          "wide",
          NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ENTRY_GROUP] =
      g_param_spec_object (
          "entry-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 0);

  signals[SIGNAL_REMOVE] =
      g_signal_new (
          "remove",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 0);

  signals[SIGNAL_RUN] =
      g_signal_new (
          "run",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 0);

  g_type_ensure (BZ_TYPE_ENTRY_GROUP);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-install-controls.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzInstallControls, open_button);
  gtk_widget_class_bind_template_child (widget_class, BzInstallControls, install_button);

  gtk_widget_class_bind_template_callback (widget_class, install_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
}

static void
bz_install_controls_init (BzInstallControls *self)
{
  self->wide = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_install_controls_new (void)
{
  return g_object_new (BZ_TYPE_INSTALL_CONTROLS, NULL);
}

gboolean
bz_install_controls_get_wide (BzInstallControls *self)
{
  g_return_val_if_fail (BZ_IS_INSTALL_CONTROLS (self), FALSE);
  return self->wide;
}

void
bz_install_controls_set_wide (BzInstallControls *self,
                              gboolean           wide)
{
  g_return_if_fail (BZ_IS_INSTALL_CONTROLS (self));

  wide = !!wide;
  if (self->wide == wide)
    return;

  self->wide = wide;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_WIDE]);
}

BzEntryGroup *
bz_install_controls_get_entry_group (BzInstallControls *self)
{
  g_return_val_if_fail (BZ_IS_INSTALL_CONTROLS (self), NULL);
  return self->group;
}

void
bz_install_controls_set_entry_group (BzInstallControls *self,
                                     BzEntryGroup      *group)
{
  g_return_if_fail (BZ_IS_INSTALL_CONTROLS (self));

  if (self->group == group)
    return;

  g_clear_object (&self->group);
  if (group != NULL)
    self->group = g_object_ref (group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY_GROUP]);
}

void
bz_install_controls_grab_focus_preferred (BzInstallControls *self)
{
  g_return_if_fail (BZ_IS_INSTALL_CONTROLS (self));

  if (gtk_widget_get_visible (self->open_button))
    gtk_widget_grab_focus (self->open_button);
  else if (gtk_widget_get_visible (self->install_button))
    gtk_widget_grab_focus (self->install_button);
}

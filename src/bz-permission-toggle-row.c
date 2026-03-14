/* bz-permission-toggle-row.c
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

#include "bz-permission-toggle-row.h"

struct _BzPermissionToggleRow
{
  AdwActionRow parent_instance;

  char *key;
  char *group;

  gboolean default_value;
  gboolean active;

  GtkSwitch *toggle;
  GtkButton *reset_button;
};

G_DEFINE_FINAL_TYPE (BzPermissionToggleRow, bz_permission_toggle_row, ADW_TYPE_ACTION_ROW)

enum
{
  PROP_0,
  PROP_KEY,
  PROP_GROUP,
  PROP_ACTIVE,
  PROP_DEFAULT_VALUE,
  PROP_IS_MODIFIED,
  N_PROPS,
};

static GParamSpec *props[N_PROPS];

enum
{
  SIGNAL_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
bz_permission_toggle_row_update_modified (BzPermissionToggleRow *self)
{
  gboolean modified = self->active != self->default_value;

  gtk_widget_set_sensitive (GTK_WIDGET (self->reset_button), modified);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_MODIFIED]);
}

static void
toggle_active_changed_cb (GtkSwitch             *toggle,
                          GParamSpec            *pspec,
                          BzPermissionToggleRow *self)
{
  gboolean new_active = gtk_switch_get_active (toggle);

  if (new_active == self->active)
    return;

  self->active = new_active;

  bz_permission_toggle_row_update_modified (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

static void
reset_button_clicked_cb (GtkButton             *button,
                         BzPermissionToggleRow *self)
{
  bz_permission_toggle_row_reset (self);
}

static void
bz_permission_toggle_row_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  BzPermissionToggleRow *self = BZ_PERMISSION_TOGGLE_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;
    case PROP_GROUP:
      self->group = g_value_dup_string (value);
      break;
    case PROP_ACTIVE:
      bz_permission_toggle_row_set_active (self, g_value_get_boolean (value));
      break;
    case PROP_DEFAULT_VALUE:
      bz_permission_toggle_row_set_default_value (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_permission_toggle_row_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  BzPermissionToggleRow *self = BZ_PERMISSION_TOGGLE_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;
    case PROP_GROUP:
      g_value_set_string (value, self->group);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;
    case PROP_DEFAULT_VALUE:
      g_value_set_boolean (value, self->default_value);
      break;
    case PROP_IS_MODIFIED:
      g_value_set_boolean (value, bz_permission_toggle_row_get_is_modified (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_permission_toggle_row_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), BZ_TYPE_PERMISSION_TOGGLE_ROW);

  G_OBJECT_CLASS (bz_permission_toggle_row_parent_class)->dispose (object);
}

static void
bz_permission_toggle_row_finalize (GObject *object)
{
  BzPermissionToggleRow *self = BZ_PERMISSION_TOGGLE_ROW (object);

  g_free (self->key);
  g_free (self->group);

  G_OBJECT_CLASS (bz_permission_toggle_row_parent_class)->finalize (object);
}

static void
bz_permission_toggle_row_class_init (BzPermissionToggleRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_permission_toggle_row_set_property;
  object_class->get_property = bz_permission_toggle_row_get_property;
  object_class->dispose      = bz_permission_toggle_row_dispose;
  object_class->finalize     = bz_permission_toggle_row_finalize;

  props[PROP_KEY] =
      g_param_spec_string ("key", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_GROUP] =
      g_param_spec_string ("group", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_ACTIVE] =
      g_param_spec_boolean ("active", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_DEFAULT_VALUE] =
      g_param_spec_boolean ("default-value", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_IS_MODIFIED] =
      g_param_spec_boolean ("is-modified", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, props);

  signals[SIGNAL_CHANGED] =
      g_signal_new ("changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-permission-toggle-row.ui");

  gtk_widget_class_bind_template_child (widget_class, BzPermissionToggleRow, toggle);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionToggleRow, reset_button);

  gtk_widget_class_bind_template_callback (widget_class, toggle_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_button_clicked_cb);
}

static void
bz_permission_toggle_row_init (BzPermissionToggleRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

const char *
bz_permission_toggle_row_get_key (BzPermissionToggleRow *self)
{
  g_return_val_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self), NULL);
  return self->key;
}

const char *
bz_permission_toggle_row_get_group (BzPermissionToggleRow *self)
{
  g_return_val_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self), NULL);
  return self->group;
}

gboolean
bz_permission_toggle_row_get_active (BzPermissionToggleRow *self)
{
  g_return_val_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self), FALSE);
  return self->active;
}

void
bz_permission_toggle_row_set_active (BzPermissionToggleRow *self,
                                     gboolean               active)
{
  g_return_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self));

  active = !!active;
  if (self->active == active)
    return;

  self->active = active;
  gtk_switch_set_active (self->toggle, active);

  bz_permission_toggle_row_update_modified (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
}

void
bz_permission_toggle_row_set_default_value (BzPermissionToggleRow *self,
                                            gboolean               default_value)
{
  g_return_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self));

  self->default_value = !!default_value;

  bz_permission_toggle_row_update_modified (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_VALUE]);
}

gboolean
bz_permission_toggle_row_get_is_modified (BzPermissionToggleRow *self)
{
  g_return_val_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self), FALSE);
  return self->active != self->default_value;
}

void
bz_permission_toggle_row_reset (BzPermissionToggleRow *self)
{
  g_return_if_fail (BZ_IS_PERMISSION_TOGGLE_ROW (self));
  bz_permission_toggle_row_set_active (self, self->default_value);
}

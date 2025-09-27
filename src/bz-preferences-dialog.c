/* bz-preferences-dialog.c
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

#include "bz-preferences-dialog.h"

struct _BzPreferencesDialog
{
  AdwPreferencesDialog parent_instance;

  GSettings *settings;

  /* Template widgets */
  AdwSwitchRow *git_forge_star_counts_switch;
  AdwSwitchRow *search_only_foss_switch;
  AdwSwitchRow *search_only_flathub_switch;
  AdwSwitchRow *search_debounce_switch;
};

G_DEFINE_FINAL_TYPE (BzPreferencesDialog, bz_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

enum
{
  PROP_0,
  PROP_SETTINGS,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void bind_settings (BzPreferencesDialog *self);

static void
bz_preferences_dialog_dispose (GObject *object)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (bz_preferences_dialog_parent_class)->dispose (object);
}

static void
bz_preferences_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_preferences_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_clear_object (&self->settings);
      self->settings = g_value_dup_object (value);
      bind_settings (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bind_settings (BzPreferencesDialog *self)
{
  if (self->settings == NULL)
    return;

  /* Bind all boolean settings to their respective switches */
  g_settings_bind (self->settings, "show-git-forge-star-counts",
                  self->git_forge_star_counts_switch, "active",
                  G_SETTINGS_BIND_DEFAULT);
  
  g_settings_bind (self->settings, "search-only-foss",
                  self->search_only_foss_switch, "active",
                  G_SETTINGS_BIND_DEFAULT);
  
  g_settings_bind (self->settings, "search-only-flathub",
                  self->search_only_flathub_switch, "active",
                  G_SETTINGS_BIND_DEFAULT);
  
  g_settings_bind (self->settings, "search-debounce",
                  self->search_debounce_switch, "active",
                  G_SETTINGS_BIND_DEFAULT);
}

static void
bz_preferences_dialog_class_init (BzPreferencesDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_preferences_dialog_dispose;
  object_class->get_property = bz_preferences_dialog_get_property;
  object_class->set_property = bz_preferences_dialog_set_property;

  props[PROP_SETTINGS] =
      g_param_spec_object (
          "settings",
          NULL, NULL,
          G_TYPE_SETTINGS,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-preferences-dialog.ui");
  
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, git_forge_star_counts_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, search_only_foss_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, search_only_flathub_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, search_debounce_switch);
}

static void
bz_preferences_dialog_init (BzPreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_preferences_dialog_new (GSettings *settings)
{
  BzPreferencesDialog *dialog = NULL;

  dialog = g_object_new (
      BZ_TYPE_PREFERENCES_DIALOG,
      "settings", settings,
      NULL);

  return ADW_DIALOG (dialog);
}
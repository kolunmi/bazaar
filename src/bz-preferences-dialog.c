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
#include <glib/gi18n.h>

typedef struct
{
  const char *id;
  const char *style_class;
  const char *tooltip;
} BarTheme;

static const BarTheme bar_themes[] = {
  {                  "accent-color",                  "accent-color-theme",                              N_ ("Accent Color") },
  {            "pride-rainbow-flag",            "pride-rainbow-flag-theme",                              N_ ("Pride Colors") },
  { "pride-rainbow-flag-horizontal", "pride-rainbow-flag-horizontal-theme",                 N_ ("Pride Colors (Horizontal)") },
  {            "lesbian-pride-flag",            "lesbian-pride-flag-theme",                      N_ ("Lesbian Pride Colors") },
  { "lesbian-pride-flag-horizontal", "lesbian-pride-flag-horizontal-theme",         N_ ("Lesbian Pride Colors (Horizontal)") },
  {                "gay-pride-flag",                "gay-pride-flag-theme",              N_ ("Male Homosexual Pride Colors") },
  {     "gay-pride-flag-horizontal",     "gay-pride-flag-horizontal-theme", N_ ("Male Homosexual Pride Colors (Horizontal)") },
  {              "transgender-flag",              "transgender-flag-theme",                  N_ ("Transgender Pride Colors") },
  {   "transgender-flag-horizontal",   "transgender-flag-horizontal-theme",     N_ ("Transgender Pride Colors (Horizontal)") },
  {                "nonbinary-flag",                "nonbinary-flag-theme",                    N_ ("Nonbinary Pride Colors") },
  {     "nonbinary-flag-horizontal",     "nonbinary-flag-horizontal-theme",       N_ ("Nonbinary Pride Colors (Horizontal)") },
  {                 "bisexual-flag",                 "bisexual-flag-theme",                     N_ ("Bisexual Pride Colors") },
  {      "bisexual-flag-horizontal",      "bisexual-flag-horizontal-theme",        N_ ("Bisexual Pride Colors (Horizontal)") },
  {                  "asexual-flag",                  "asexual-flag-theme",                      N_ ("Asexual Pride Colors") },
  {       "asexual-flag-horizontal",       "asexual-flag-horizontal-theme",         N_ ("Asexual Pride Colors (Horizontal)") },
  {                "pansexual-flag",                "pansexual-flag-theme",                    N_ ("Pansexual Pride Colors") },
  {     "pansexual-flag-horizontal",     "pansexual-flag-horizontal-theme",       N_ ("Pansexual Pride Colors (Horizontal)") },
  {                "aromantic-flag",                "aromantic-flag-theme",                    N_ ("Aromantic Pride Colors") },
  {     "aromantic-flag-horizontal",     "aromantic-flag-horizontal-theme",       N_ ("Aromantic Pride Colors (Horizontal)") },
  {              "genderfluid-flag",              "genderfluid-flag-theme",                  N_ ("Genderfluid Pride Colors") },
  {   "genderfluid-flag-horizontal",   "genderfluid-flag-horizontal-theme",     N_ ("Genderfluid Pride Colors (Horizontal)") },
  {               "polysexual-flag",               "polysexual-flag-theme",                   N_ ("Polysexual Pride Colors") },
  {    "polysexual-flag-horizontal",    "polysexual-flag-horizontal-theme",      N_ ("Polysexual Pride Colors (Horizontal)") },
  {               "omnisexual-flag",               "omnisexual-flag-theme",                   N_ ("Omnisexual Pride Colors") },
  {    "omnisexual-flag-horizontal",    "omnisexual-flag-horizontal-theme",      N_ ("Omnisexual Pride Colors (Horizontal)") },
  {                   "aroace-flag",                   "aroace-flag-theme",                       N_ ("Aroace Pride Colors") },
  {        "aroace-flag-horizontal",        "aroace-flag-horizontal-theme",          N_ ("Aroace Pride Colors (Horizontal)") },
  {                  "agender-flag",                  "agender-flag-theme",                      N_ ("Agender Pride Colors") },
  {       "agender-flag-horizontal",       "agender-flag-horizontal-theme",         N_ ("Agender Pride Colors (Horizontal)") },
  {              "genderqueer-flag",              "genderqueer-flag-theme",                  N_ ("Genderqueer Pride Colors") },
  {   "genderqueer-flag-horizontal",   "genderqueer-flag-horizontal-theme",     N_ ("Genderqueer Pride Colors (Horizontal)") },
  {                 "intersex-flag",                 "intersex-flag-theme",                     N_ ("Intersex Pride Colors") },
  {      "intersex-flag-horizontal",      "intersex-flag-horizontal-theme",        N_ ("Intersex Pride Colors (Horizontal)") },
  {               "demigender-flag",               "demigender-flag-theme",                   N_ ("Demigender Pride Colors") },
  {    "demigender-flag-horizontal",    "demigender-flag-horizontal-theme",      N_ ("Demigender Pride Colors (Horizontal)") },
  {               "biromantic-flag",               "biromantic-flag-theme",                   N_ ("Biromantic Pride Colors") },
  {    "biromantic-flag-horizontal",    "biromantic-flag-horizontal-theme",      N_ ("Biromantic Pride Colors (Horizontal)") },
  {               "disability-flag",               "disability-flag-theme",                   N_ ("Disability Pride Colors") },
  {    "disability-flag-horizontal",    "disability-flag-horizontal-theme",      N_ ("Disability Pride Colors (Horizontal)") },
  {                   "femboy-flag",                   "femboy-flag-theme",                       N_ ("Femboy Pride Colors") },
  {        "femboy-flag-horizontal",        "femboy-flag-horizontal-theme",          N_ ("Femboy Pride Colors (Horizontal)") },
};

struct _BzPreferencesDialog
{
  AdwPreferencesDialog parent_instance;

  GSettings *settings;

  /* Template widgets */
  AdwSwitchRow *only_foss_switch;
  AdwSwitchRow *only_flathub_switch;
  AdwSwitchRow *only_verified_switch;
  AdwSwitchRow *search_debounce_switch;
  GtkFlowBox   *flag_buttons_box;
  AdwSwitchRow *hide_eol_switch;

  GtkToggleButton *flag_buttons[G_N_ELEMENTS (bar_themes)];
};

G_DEFINE_FINAL_TYPE (BzPreferencesDialog, bz_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static void bind_settings (BzPreferencesDialog *self);
static void create_flag_buttons (BzPreferencesDialog *self);

static void
bz_preferences_dialog_dispose (GObject *object)
{
  BzPreferencesDialog *self = BZ_PREFERENCES_DIALOG (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (bz_preferences_dialog_parent_class)->dispose (object);
}

static void
flag_button_toggled (GtkToggleButton     *button,
                     BzPreferencesDialog *self)
{
  const char *theme_id = NULL;

  if (!gtk_toggle_button_get_active (button))
    return;

  theme_id = g_object_get_data (G_OBJECT (button), "theme-id");
  if (theme_id != NULL)
    {
      g_settings_set_string (self->settings, "global-progress-bar-theme", theme_id);
    }
}

static void
global_progress_theme_settings_changed (BzPreferencesDialog *self,
                                        const char          *key,
                                        GSettings           *settings)
{
  const char *theme = NULL;

  theme = g_settings_get_string (self->settings, "global-progress-bar-theme");

  for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
    {
      if (g_strcmp0 (theme, bar_themes[i].id) == 0)
        {
          gtk_toggle_button_set_active (self->flag_buttons[i], TRUE);
          break;
        }
    }
}

static void
create_flag_buttons (BzPreferencesDialog *self)
{
  GtkToggleButton *first_button = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (bar_themes); i++)
    {
      GtkToggleButton *button = NULL;

      button = GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());

      gtk_widget_set_tooltip_text (GTK_WIDGET (button), _ (bar_themes[i].tooltip));
      gtk_widget_add_css_class (GTK_WIDGET (button), "accent-button");
      gtk_widget_add_css_class (GTK_WIDGET (button), bar_themes[i].style_class);

      g_object_set_data_full (G_OBJECT (button),
                              "theme-id",
                              g_strdup (bar_themes[i].id),
                              g_free);

      if (i == 0)
        {
          first_button = button;
        }
      else
        {
          gtk_toggle_button_set_group (button, first_button);
        }

      g_signal_connect (button, "toggled",
                        G_CALLBACK (flag_button_toggled), self);

      self->flag_buttons[i] = button;
      gtk_flow_box_append (self->flag_buttons_box, GTK_WIDGET (button));
    }
}

static void
bind_settings (BzPreferencesDialog *self)
{
  if (self->settings == NULL)
    return;

  /* Bind all boolean settings to their respective switches */
  g_settings_bind (self->settings, "show-only-foss",
                   self->only_foss_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "show-only-flathub",
                   self->only_flathub_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "show-only-verified",
                   self->only_verified_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "search-debounce",
                   self->search_debounce_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "hide-eol",
                   self->hide_eol_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (
      self->settings,
      "changed::global-progress-bar-theme",
      G_CALLBACK (global_progress_theme_settings_changed),
      self, G_CONNECT_SWAPPED);
  global_progress_theme_settings_changed (self, "global-progress-bar-theme", self->settings);
}

static void
bz_preferences_dialog_class_init (BzPreferencesDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_preferences_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-preferences-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_foss_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_flathub_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, only_verified_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, search_debounce_switch);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, flag_buttons_box);
  gtk_widget_class_bind_template_child (widget_class, BzPreferencesDialog, hide_eol_switch);
}

static void
bz_preferences_dialog_init (BzPreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  create_flag_buttons (self);
}

AdwDialog *
bz_preferences_dialog_new (GSettings *settings)
{
  BzPreferencesDialog *dialog = NULL;

  g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

  dialog           = g_object_new (BZ_TYPE_PREFERENCES_DIALOG, NULL);
  dialog->settings = g_object_ref (settings);
  bind_settings (dialog);

  return ADW_DIALOG (dialog);
}

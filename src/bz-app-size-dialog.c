/* bz-app-size-dialog.c
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
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

#include "bz-app-size-dialog.h"
#include "bz-entry.h"
#include <glib/gi18n.h>

struct _BzAppSizeDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;

  GtkLabel            *size_label;
  AdwPreferencesGroup *comparisons_group;
};

G_DEFINE_FINAL_TYPE (BzAppSizeDialog, bz_app_size_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,

  PROP_ENTRY,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

typedef struct
{
  const char *title;
  const char *subtitle;
  guint64     reference_size;
} SizeComparison;

static const SizeComparison comparisons[] = {
  {           N_ ("Of the size of human DNA"), N_ ("3 billion base pairs"),   750000000 },
  {                N_ ("Of the Linux Kernel"),    N_ ("linux-6.17.tar.xz"),   153382068 },
  { N_ ("Of the Apollo 11 guidance computer"),    N_ ("Total ROM and RAM"),       76800 },
  {   N_ ("Of the original Super Mario Bros"),           N_ ("On the NES"),       40976 },
  {           N_ ("Of the size of Wikipedia"),    N_ ("Without any media"), 25823490867 },
};

static void update_size_display (BzAppSizeDialog *self);
static void populate_comparisons (BzAppSizeDialog *self);

static void
bz_app_size_dialog_dispose (GObject *object)
{
  BzAppSizeDialog *self = BZ_APP_SIZE_DIALOG (object);

  g_clear_object (&self->entry);

  G_OBJECT_CLASS (bz_app_size_dialog_parent_class)->dispose (object);
}

static void
bz_app_size_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzAppSizeDialog *self = BZ_APP_SIZE_DIALOG (object);

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
bz_app_size_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzAppSizeDialog *self = BZ_APP_SIZE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_clear_object (&self->entry);
      self->entry = g_value_dup_object (value);
      update_size_display (self);
      populate_comparisons (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
format_size (gpointer object, guint64 value)
{
  g_autofree char *size_str = g_format_size (value);
  char            *space    = g_strrstr (size_str, "\xC2\xA0");

  if (space != NULL)
    {
      *space = '\0';
      return g_strdup_printf ("%s <span font_size='x-small'>%s</span>",
                              size_str, space + 2);
    }

  return g_strdup (size_str);
}

static char *
format_percentage (double percentage)
{
  int magnitude = (int) floor (log10 (fabs (percentage)));
  int decimals  = CLAMP (2 - magnitude, 0, 3);

  return g_strdup_printf ("%.*f<span font_size='x-small'>%%</span>", decimals, percentage);
}

static void
update_size_display (BzAppSizeDialog *self)
{
  guint64          app_size = 0;
  g_autofree char *size_str = NULL;

  if (self->entry == NULL)
    return;

  app_size = bz_entry_get_size (self->entry);
  size_str = format_size (NULL, app_size);

  gtk_label_set_markup (self->size_label, size_str);
}

static AdwActionRow *
create_comparison_row (const SizeComparison *comp,
                       double                percentage)
{
  AdwActionRow    *row            = NULL;
  GtkLabel        *prefix_label   = NULL;
  g_autofree char *percentage_str = NULL;

  row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _ (comp->title));
  adw_action_row_set_subtitle (row, _ (comp->subtitle));
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  percentage_str = format_percentage (percentage);
  prefix_label   = GTK_LABEL (gtk_label_new (NULL));
  gtk_label_set_markup (prefix_label, percentage_str);
  gtk_widget_set_valign (GTK_WIDGET (prefix_label), GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (prefix_label), "lozenge");
  gtk_widget_add_css_class (GTK_WIDGET (prefix_label), "title-4");
  gtk_widget_add_css_class (GTK_WIDGET (prefix_label), "grey");
  gtk_widget_set_size_request (GTK_WIDGET (prefix_label), 90, -1);

  adw_action_row_add_prefix (row, GTK_WIDGET (prefix_label));

  return row;
}

static void
populate_comparisons (BzAppSizeDialog *self)
{
  guint64 app_size = 0;

  if (self->entry == NULL)
    return;

  app_size = bz_entry_get_size (self->entry);

  if (app_size == 0)
    return;

  for (guint i = 0; i < G_N_ELEMENTS (comparisons); i++)
    {
      const SizeComparison *comp       = &comparisons[i];
      double                percentage = (double) app_size / (double) comp->reference_size * 100.0;
      AdwActionRow         *row        = NULL;

      row = create_comparison_row (comp, percentage);
      adw_preferences_group_add (self->comparisons_group, GTK_WIDGET (row));
    }
}

static void
bz_app_size_dialog_class_init (BzAppSizeDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_app_size_dialog_dispose;
  object_class->get_property = bz_app_size_dialog_get_property;
  object_class->set_property = bz_app_size_dialog_set_property;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-app-size-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, BzAppSizeDialog, size_label);
  gtk_widget_class_bind_template_child (widget_class, BzAppSizeDialog, comparisons_group);
}

static void
bz_app_size_dialog_init (BzAppSizeDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_app_size_dialog_new (BzEntry *entry)
{
  BzAppSizeDialog *app_size_dialog = NULL;

  app_size_dialog = g_object_new (
      BZ_TYPE_APP_SIZE_DIALOG,
      "entry", entry,
      NULL);

  return ADW_DIALOG (app_size_dialog);
}

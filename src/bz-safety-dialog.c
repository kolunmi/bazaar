/* bz-safety-dialog.c
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

#include <glib/gi18n.h>

#include "bz-app-permissions.h"
#include "bz-entry-group.h"
#include "bz-entry.h"
#include "bz-result.h"
#include "bz-safety-calculator.h"
#include "bz-safety-dialog.h"
#include "bz-safety-row.h"

struct _BzSafetyDialog
{
  AdwDialog parent_instance;

  BzEntryGroup *group;
  gulong        permissions_handler;

  GtkImage   *lozenge;
  GtkLabel   *title;
  GtkListBox *permissions_list;
};

G_DEFINE_FINAL_TYPE (BzSafetyDialog, bz_safety_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_GROUP,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static const char *get_css_class_for_rating (BzSafetyRating rating);
static AdwActionRow *create_permission_row (BzSafetyRow *row_data);
static void update_lozenge_css_class (GtkWidget *widget, const char *new_class);
static void update_permissions_list (BzSafetyDialog *self);
static void on_permissions_changed (BzSafetyDialog *self);
static gboolean invert_boolean (gpointer object, gboolean value);
static gboolean is_null (gpointer object, GObject *value);

static void
bz_safety_dialog_dispose (GObject *object)
{
  BzSafetyDialog *self;

  self = BZ_SAFETY_DIALOG (object);

  if (self->group != NULL && self->permissions_handler != 0)
    {
      g_signal_handler_disconnect (self->group, self->permissions_handler);
      self->permissions_handler = 0;
    }

  g_clear_object (&self->group);

  G_OBJECT_CLASS (bz_safety_dialog_parent_class)->dispose (object);
}

static void
bz_safety_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSafetyDialog *self;

  self = BZ_SAFETY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, self->group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_safety_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSafetyDialog *self;

  self = BZ_SAFETY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      if (self->group != NULL && self->permissions_handler != 0)
        {
          g_signal_handler_disconnect (self->group, self->permissions_handler);
          self->permissions_handler = 0;
        }

      g_clear_object (&self->group);
      self->group = g_value_dup_object (value);

      if (self->group != NULL)
        {
          self->permissions_handler = g_signal_connect_swapped (self->group, "notify::ui-entry",
                                                                G_CALLBACK (on_permissions_changed), self);
        }

      update_permissions_list (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_safety_dialog_class_init (BzSafetyDialogClass *klass)
{
  GObjectClass   *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_safety_dialog_dispose;
  object_class->get_property = bz_safety_dialog_get_property;
  object_class->set_property = bz_safety_dialog_set_property;

  props[PROP_GROUP] =
      g_param_spec_object ("group",
                           NULL, NULL,
                           BZ_TYPE_ENTRY_GROUP,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-safety-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, title);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, permissions_list);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
}

static void
bz_safety_dialog_init (BzSafetyDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwDialog *
bz_safety_dialog_new (BzEntryGroup *group)
{
  return g_object_new (BZ_TYPE_SAFETY_DIALOG,
                       "group", group,
                       NULL);
}

static const char *
get_css_class_for_rating (BzSafetyRating rating)
{
  switch (rating)
    {
    case BZ_SAFETY_RATING_SAFE:
      return "green";
    case BZ_SAFETY_RATING_NEUTRAL:
      return "grey";
    case BZ_SAFETY_RATING_PROBABLY_SAFE:
      return "yellow";
    case BZ_SAFETY_RATING_POTENTIALLY_UNSAFE:
      return "orange";
    case BZ_SAFETY_RATING_UNSAFE:
      return "red";
    default:
      return "grey";
    }
}

static AdwActionRow *
create_permission_row (BzSafetyRow *row_data)
{
  AdwActionRow  *row;
  GtkWidget     *icon;
  const char    *icon_name;
  const char    *title;
  const char    *subtitle;
  BzSafetyRating rating;

  icon_name = bz_safety_row_get_icon_name (row_data);
  title     = bz_safety_row_get_title (row_data);
  subtitle  = bz_safety_row_get_subtitle (row_data);
  rating    = bz_safety_row_get_rating (row_data);

  row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  if (subtitle != NULL)
    adw_action_row_set_subtitle (row, subtitle);

  icon = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (icon, "circular-lozenge");
  gtk_widget_add_css_class (icon, get_css_class_for_rating (rating));
  adw_action_row_add_prefix (row, icon);

  return row;
}

static void
update_lozenge_css_class (GtkWidget *widget, const char *new_class)
{
  static const char *css_classes[] = { "green", "yellow", "orange", "red", "grey" };

  for (size_t i = 0; i < G_N_ELEMENTS (css_classes); i++)
    gtk_widget_remove_css_class (widget, css_classes[i]);

  gtk_widget_add_css_class (widget, new_class);
}

static void
update_permissions_list (BzSafetyDialog *self)
{
  const char             *icon_name = NULL;
  const char             *css_class = NULL;
  const char             *app_name = NULL;
  g_autofree char        *title_text = NULL;
  BzSafetyRating          rating = BZ_SAFETY_RATING_SAFE;
  BzEntry                *entry = NULL;
  BzResult               *result = NULL;
  GtkWidget              *child;
  g_autoptr (GListModel)  model = NULL;
  guint                   n_items = 0;
  guint                   i = 0;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->permissions_list))) != NULL)
    gtk_list_box_remove (self->permissions_list, child);

  if (self->group == NULL)
    return;

  g_object_get (self->group, "ui-entry", &result, NULL);
  if (result == NULL)
    return;

  entry = bz_result_get_object (result);
  if (entry == NULL)
    {
      g_clear_object (&result);
      return;
    }

  app_name = bz_entry_get_title (entry);

  model  = bz_safety_calculator_analyze_entry (entry);
  rating = bz_safety_calculator_calculate_rating (entry);

  n_items = g_list_model_get_n_items (model);
  for (i = 0; i < n_items; i++)
    {
      g_autoptr (BzSafetyRow) row_data;
      AdwActionRow           *row;

      row_data = g_list_model_get_item (model, i);
      row      = create_permission_row (row_data);
      gtk_list_box_append (self->permissions_list, GTK_WIDGET (row));
    }

  switch (rating)
    {
    case BZ_SAFETY_RATING_NEUTRAL:
      icon_name  = "app-safety-ok-symbolic";
      title_text = g_strdup_printf (_ ("%s has no Unsafe Permissions"), app_name);
      css_class  = "grey";
      break;
    case BZ_SAFETY_RATING_SAFE:
      icon_name  = "app-safety-ok-symbolic";
      title_text = g_strdup_printf (_ ("%s is Safe"), app_name);
      css_class  = "green";
      break;
    case BZ_SAFETY_RATING_PROBABLY_SAFE:
      icon_name  = "app-safety-ok-symbolic";
      title_text = g_strdup_printf (_ ("%s is Probably Safe"), app_name);
      css_class  = "yellow";
      break;
    case BZ_SAFETY_RATING_POTENTIALLY_UNSAFE:
      icon_name  = "app-safety-unknown-symbolic";
      title_text = g_strdup_printf (_ ("%s is Possibly Unsafe"), app_name);
      css_class  = "orange";
      break;
    case BZ_SAFETY_RATING_UNSAFE:
      icon_name  = "app-safety-unsafe-symbolic";
      title_text = g_strdup_printf (_ ("%s is Unsafe"), app_name);
      css_class  = "red";
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_image_set_from_icon_name (self->lozenge, icon_name);
  gtk_label_set_text (self->title, title_text);
  update_lozenge_css_class (GTK_WIDGET (self->lozenge), css_class);

  g_clear_object (&result);
}

static void
on_permissions_changed (BzSafetyDialog *self)
{
  update_permissions_list (self);
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

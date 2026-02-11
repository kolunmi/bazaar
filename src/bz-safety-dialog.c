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
#include "bz-context-row.h"
#include "bz-entry-group.h"
#include "bz-entry.h"
#include "bz-lozenge.h"
#include "bz-popup.h"
#include "bz-result.h"
#include "bz-safety-calculator.h"
#include "bz-safety-dialog.h"
#include "bz-safety-row.h"

struct _BzSafetyDialog
{
  BzPopup parent_instance;

  BzEntryGroup *group;

  BzLozenge  *lozenge;
  GtkListBox *permissions_list;
};

G_DEFINE_FINAL_TYPE (BzSafetyDialog, bz_safety_dialog, BZ_TYPE_POPUP)

enum
{
  PROP_0,
  PROP_GROUP,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static AdwActionRow *create_permission_row (BzSafetyRow *row_data);
static void          update_permissions_list (BzSafetyDialog *self);
static gboolean      invert_boolean (gpointer object, gboolean value);
static gboolean      is_null (gpointer object, GObject *value);

static void
bz_safety_dialog_dispose (GObject *object)
{
  BzSafetyDialog *self = BZ_SAFETY_DIALOG (object);

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
      g_clear_object (&self->group);
      self->group = g_value_dup_object (value);
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

  g_type_ensure (BZ_TYPE_LOZENGE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-safety-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzSafetyDialog, permissions_list);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
}

static void
bz_safety_dialog_init (BzSafetyDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_safety_dialog_new (BzEntryGroup *group)
{
  return g_object_new (BZ_TYPE_SAFETY_DIALOG,
                       "group", group,
                       NULL);
}

static AdwActionRow *
create_permission_row (BzSafetyRow *row_data)
{
  return bz_context_row_new (bz_safety_row_get_icon_name (row_data),
                             bz_safety_row_get_importance (row_data),
                             bz_safety_row_get_title (row_data),
                             bz_safety_row_get_subtitle (row_data));
}

static void
update_permissions_list (BzSafetyDialog *self)
{
  const char      *icon_names[2];
  const char      *app_name   = NULL;
  g_autofree char *title_text = NULL;
  BzImportance     importance = BZ_IMPORTANCE_UNIMPORTANT;
  BzEntry         *entry      = NULL;
  BzResult        *result     = NULL;
  GtkWidget       *child;
  g_autoptr (GListModel) model = NULL;
  guint n_items                = 0;

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

  model      = bz_safety_calculator_analyze_entry (entry);
  importance = bz_safety_calculator_calculate_rating (entry);

  n_items = g_list_model_get_n_items (model);
  for (gint level = BZ_IMPORTANCE_IMPORTANT; level >= BZ_IMPORTANCE_UNIMPORTANT; level--)
    {
      for (gint j = 0; j < n_items; j++)
        {
          g_autoptr (BzSafetyRow) row_data;
          AdwActionRow *row;
          BzImportance  row_importance;
          row_data       = g_list_model_get_item (model, j);
          row_importance = bz_safety_row_get_importance (row_data);
          if (row_importance != level)
            continue;
          row = create_permission_row (row_data);
          gtk_list_box_append (self->permissions_list, GTK_WIDGET (row));
        }
    }

  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Safe"), app_name);
      break;
    case BZ_IMPORTANCE_NEUTRAL:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s has no Unsafe Permissions"), app_name);
      break;
    case BZ_IMPORTANCE_INFORMATION:
      icon_names[0] = "app-safety-ok-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Probably Safe"), app_name);
      break;
    case BZ_IMPORTANCE_WARNING:
      icon_names[0] = "app-safety-unknown-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Possibly Unsafe"), app_name);
      break;
    case BZ_IMPORTANCE_IMPORTANT:
      icon_names[0] = "app-safety-unsafe-symbolic";
      icon_names[1] = NULL;
      title_text    = g_strdup_printf (_ ("%s is Unsafe"), app_name);
      break;
    default:
      g_assert_not_reached ();
    }

  bz_lozenge_set_icon_names (self->lozenge, icon_names);
  bz_lozenge_set_title (self->lozenge, title_text);
  bz_lozenge_set_importance (self->lozenge, importance);

  g_clear_object (&result);
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

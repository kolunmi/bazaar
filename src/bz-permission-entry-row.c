/* bz-permission-entry-row.c
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

#include "bz-permission-entry-row.h"

enum
{
  SIGNAL_CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];

struct _BzPermissionEntryRow
{
  AdwExpanderRow parent_instance;

  GPtrArray *entries;
  GStrv      default_values;
  char      *regex;

  GtkWidget *add_button;
  GtkWidget *reset_button;
};

G_DEFINE_FINAL_TYPE (BzPermissionEntryRow, bz_permission_entry_row, ADW_TYPE_EXPANDER_ROW)

static void
update_expandability (BzPermissionEntryRow *self)
{
  if (self->entries->len == 0)
    {
      adw_expander_row_set_expanded (ADW_EXPANDER_ROW (self), FALSE);
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self), FALSE);
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (self), TRUE);
    }
}

static void
update_reset_visibility (BzPermissionEntryRow *self)
{
  g_auto (GStrv) current = bz_permission_entry_row_get_values (self);
  gboolean differs       = self->default_values != NULL && current != NULL &&
                     !g_strv_equal ((const char *const *) current,
                                    (const char *const *) self->default_values);

  gtk_widget_set_sensitive (self->reset_button, differs);
}

static void
validate_entry (BzPermissionEntryRow *self,
                AdwEntryRow          *row)
{
  const char *text   = NULL;
  GtkWidget  *prefix = NULL;

  prefix = g_object_get_data (G_OBJECT (row), "error-prefix");
  text   = gtk_editable_get_text (GTK_EDITABLE (row));

  if (prefix == NULL)
    return;

  if (self->regex == NULL || self->regex[0] == '\0' ||
      text == NULL || text[0] == '\0')
    {
      gtk_widget_set_visible (prefix, FALSE);
      return;
    }

  gtk_widget_set_visible (prefix, !g_regex_match_simple (self->regex, text, 0, 0));
}

static void
validate_all_entries (BzPermissionEntryRow *self)
{
  for (guint i = 0; i < self->entries->len; i++)
    {
      AdwEntryRow *row = g_ptr_array_index (self->entries, i);
      validate_entry (self, row);
    }
}

static void
remove_cb (GtkButton            *button,
           BzPermissionEntryRow *self)
{
  GtkWidget *row = NULL;
  row            = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_ENTRY_ROW);

  if (row != NULL)
    {
      g_ptr_array_remove (self->entries, row);
      adw_expander_row_remove (ADW_EXPANDER_ROW (self), row);
      update_expandability (self);
      update_reset_visibility (self);
      g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
    }
}

static void
focus_leave_cb (GtkEventControllerFocus *controller,
                BzPermissionEntryRow    *self)
{
  update_reset_visibility (self);
  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

static void
entry_text_changed_cb (GtkEditable          *editable,
                       GParamSpec           *pspec,
                       BzPermissionEntryRow *self)
{
  validate_entry (self, ADW_ENTRY_ROW (editable));
  update_reset_visibility (self);
}

static AdwEntryRow *
add_entry (BzPermissionEntryRow *self,
           const char           *text)
{
  AdwEntryRow             *row        = NULL;
  GtkButton               *button     = NULL;
  GtkEventControllerFocus *focus_ctrl = NULL;
  GtkWidget               *error_icon = NULL;

  row = ADW_ENTRY_ROW (adw_entry_row_new ());
  gtk_widget_add_css_class (GTK_WIDGET (row), "monospace");

  if (text != NULL)
    gtk_editable_set_text (GTK_EDITABLE (row), text);

  error_icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic");
  gtk_widget_add_css_class (error_icon, "error");
  gtk_widget_set_tooltip_text(error_icon, _("Invalid Format"));
  gtk_widget_set_visible (error_icon, FALSE);
  adw_entry_row_add_prefix (row, error_icon);
  g_object_set_data (G_OBJECT (row), "error-prefix", error_icon);

  button = GTK_BUTTON (gtk_button_new_from_icon_name ("cross-small-circle-outline-symbolic"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), _ ("Remove"));
  gtk_widget_add_css_class (GTK_WIDGET (button), "error");
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (button), "flat");
  adw_entry_row_add_suffix (row, GTK_WIDGET (button));

  g_signal_connect (button, "clicked",
                    G_CALLBACK (remove_cb), self);

  focus_ctrl = GTK_EVENT_CONTROLLER_FOCUS (gtk_event_controller_focus_new ());
  g_signal_connect (focus_ctrl, "leave",
                    G_CALLBACK (focus_leave_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (row),
                             GTK_EVENT_CONTROLLER (focus_ctrl));

  g_signal_connect (row, "notify::text",
                    G_CALLBACK (entry_text_changed_cb), self);

  adw_expander_row_add_row (ADW_EXPANDER_ROW (self), GTK_WIDGET (row));
  // this cursed line sets first -> second -> first widget to invisible to properly hide the title...
  gtk_widget_set_visible (gtk_widget_get_next_sibling (gtk_widget_get_first_child (gtk_widget_get_next_sibling (gtk_widget_get_first_child (gtk_widget_get_first_child (GTK_WIDGET (row)))))), FALSE);

  g_ptr_array_add (self->entries, row);
  update_expandability (self);

  validate_entry (self, row);

  return row;
}

static void
add_cb (GtkButton            *button,
        BzPermissionEntryRow *self)
{
  AdwEntryRow *row = add_entry (self, NULL);
  adw_expander_row_set_expanded (ADW_EXPANDER_ROW (self), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (row));
  update_reset_visibility (self);
}

static void
reset_cb (GtkButton            *button,
          BzPermissionEntryRow *self)
{
  bz_permission_entry_row_set_values (self, (const char *const *) self->default_values);
  update_reset_visibility (self);
  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

static void
bz_permission_entry_row_dispose (GObject *object)
{
  BzPermissionEntryRow *self = BZ_PERMISSION_ENTRY_ROW (object);

  g_clear_pointer (&self->entries, g_ptr_array_unref);
  g_clear_pointer (&self->default_values, g_strfreev);
  g_clear_pointer (&self->regex, g_free);
  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_PERMISSION_ENTRY_ROW);

  G_OBJECT_CLASS (bz_permission_entry_row_parent_class)->dispose (object);
}

static void
bz_permission_entry_row_class_init (BzPermissionEntryRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_permission_entry_row_dispose;

  signals[SIGNAL_CHANGED] =
      g_signal_new ("changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-permission-entry-row.ui");

  gtk_widget_class_bind_template_child (widget_class, BzPermissionEntryRow, reset_button);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionEntryRow, add_button);

  gtk_widget_class_bind_template_callback (widget_class, reset_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_cb);
}

static void
bz_permission_entry_row_init (BzPermissionEntryRow *self)
{
  self->entries = g_ptr_array_new ();
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_permission_entry_row_new (const char *title,
                             const char *subtitle)
{
  BzPermissionEntryRow *self = NULL;

  self = g_object_new (BZ_TYPE_PERMISSION_ENTRY_ROW, NULL);

  if (title != NULL)
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);

  if (subtitle != NULL)
    adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (self), subtitle);

  return GTK_WIDGET (self);
}

GStrv
bz_permission_entry_row_get_values (BzPermissionEntryRow *self)
{
  g_autoptr (GStrvBuilder) builder = NULL;

  g_return_val_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self), NULL);

  builder = g_strv_builder_new ();
  for (guint i = 0; i < self->entries->len; i++)
    {
      AdwEntryRow *row  = g_ptr_array_index (self->entries, i);
      const char  *text = gtk_editable_get_text (GTK_EDITABLE (row));

      if (text != NULL && text[0] != '\0')
        g_strv_builder_add (builder, text);
    }

  return g_strv_builder_end (builder);
}

void
bz_permission_entry_row_set_values (BzPermissionEntryRow *self,
                                    const char *const    *values)
{
  g_return_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self));

  for (guint i = self->entries->len; i > 0; i--)
    {
      GtkWidget *row = g_ptr_array_index (self->entries, i - 1);
      adw_expander_row_remove (ADW_EXPANDER_ROW (self), row);
    }

  g_ptr_array_set_size (self->entries, 0);

  if (values != NULL)
    {
      for (guint i = 0; values[i] != NULL; i++)
        add_entry (self, values[i]);
    }

  update_expandability (self);
  update_reset_visibility (self);
}

void
bz_permission_entry_row_set_default_values (BzPermissionEntryRow *self,
                                            const char *const    *values)
{
  g_return_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self));

  g_clear_pointer (&self->default_values, g_strfreev);
  self->default_values = g_strdupv ((char **) values);

  update_reset_visibility (self);
}

GStrv
bz_permission_entry_row_get_default_values (BzPermissionEntryRow *self)
{
  g_return_val_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self), NULL);

  return self->default_values;
}

void
bz_permission_entry_row_set_regex (BzPermissionEntryRow *self,
                                   const char           *regex)
{
  g_return_if_fail (BZ_IS_PERMISSION_ENTRY_ROW (self));

  if (g_set_str (&self->regex, regex))
    validate_all_entries (self);
}

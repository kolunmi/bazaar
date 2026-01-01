/* bz-hardware-support-dialog.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-hardware-support-dialog.h"
#include <glib/gi18n.h>

struct _BzHardwareSupportDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;
  gulong   entry_notify_handler;

  /* Template widgets */
  GtkWidget  *lozenge;
  GtkLabel   *title;
  GtkListBox *list;
};

G_DEFINE_FINAL_TYPE (BzHardwareSupportDialog, bz_hardware_support_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_ENTRY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

typedef enum
{
  RELATION_NONE = 0,
  RELATION_SUPPORTS,
  RELATION_RECOMMENDS,
  RELATION_REQUIRES
} RelationType;

typedef struct
{
  const gchar  *icon_name;
  const gchar  *title;
  BzControlType control_flag;
  const gchar  *required_subtitle;
  const gchar  *recommended_subtitle;
  const gchar  *supported_subtitle;
  const gchar  *unsupported_subtitle;
} ControlInfo;

static const ControlInfo control_infos[] = {
  {       "input-keyboard-symbolic",
   N_ ("Keyboard support"),
   BZ_CONTROL_KEYBOARD,
   N_ ("Requires keyboards"),
   N_ ("Recommends keyboards"),
   N_ ("Supports keyboards"),
   N_ ("Unknown support for keyboards")               },
  {          "input-mouse-symbolic",
   N_ ("Mouse support"),
   BZ_CONTROL_POINTING,
   N_ ("Requires mice or pointing devices"),
   N_ ("Recommends mice or pointing devices"),
   N_ ("Supports mice or pointing devices"),
   N_ ("Unknown support for mice or pointing devices") },
  { "device-support-touch-symbolic",
   N_ ("Touchscreen support"),
   BZ_CONTROL_TOUCH,
   N_ ("Requires touchscreens"),
   N_ ("Recommends touchscreens"),
   N_ ("Supports touchscreens"),
   N_ ("Unknown support for touchscreens")            }
};

static AdwActionRow *
create_support_row (const gchar *icon_name,
                    const gchar *title_text,
                    const gchar *subtitle,
                    gboolean     is_supported)
{
  AdwActionRow *row;
  GtkWidget    *icon;

  row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title_text);

  if (subtitle != NULL)
    adw_action_row_set_subtitle (row, subtitle);

  icon = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (icon, "circular-lozenge");
  gtk_widget_add_css_class (icon, is_supported ? "green" : "grey");

  adw_action_row_add_prefix (row, icon);

  return row;
}

static RelationType
get_control_relation (guint         required_controls,
                      guint         recommended_controls,
                      guint         supported_controls,
                      BzControlType control_flag)
{
  if (required_controls & control_flag)
    return RELATION_REQUIRES;
  else if (recommended_controls & control_flag)
    return RELATION_RECOMMENDS;
  else if (supported_controls & control_flag)
    return RELATION_SUPPORTS;
  else
    return RELATION_NONE;
}

static const gchar *
get_subtitle_for_relation (const ControlInfo *info,
                           RelationType       relation)
{
  switch (relation)
    {
    case RELATION_REQUIRES:
      return _ (info->required_subtitle);
    case RELATION_RECOMMENDS:
      return _ (info->recommended_subtitle);
    case RELATION_SUPPORTS:
      return _ (info->supported_subtitle);
    case RELATION_NONE:
      return _ (info->unsupported_subtitle);
    default:
      return _ (info->unsupported_subtitle);
    }
}

static void
add_control_row (BzHardwareSupportDialog *self,
                 const ControlInfo       *info,
                 RelationType             relation)
{
  AdwActionRow *row;
  gboolean      is_supported;
  const gchar  *subtitle;

  is_supported = (relation != RELATION_NONE);
  subtitle     = get_subtitle_for_relation (info, relation);

  row = create_support_row (info->icon_name,
                            _ (info->title),
                            subtitle,
                            is_supported);
  gtk_list_box_append (self->list, GTK_WIDGET (row));
}

static void
update_list (BzHardwareSupportDialog *self)
{
  GtkWidget    *child;
  AdwActionRow *row;
  guint         required_controls;
  guint         recommended_controls;
  guint         supported_controls;
  gboolean      is_mobile_friendly;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->list))) != NULL)
    gtk_list_box_remove (self->list, child);

  if (self->entry == NULL)
    return;

  required_controls    = bz_entry_get_required_controls (self->entry);
  recommended_controls = bz_entry_get_recommended_controls (self->entry);
  supported_controls   = bz_entry_get_supported_controls (self->entry);
  is_mobile_friendly   = bz_entry_get_is_mobile_friendly (self->entry);

  row = create_support_row ("phone-symbolic",
                            _ ("Mobile support"),
                            is_mobile_friendly ? _ ("Works on mobile devices") : _ ("May not work well on mobile devices"),
                            is_mobile_friendly);
  gtk_list_box_append (self->list, GTK_WIDGET (row));

  row = create_support_row ("device-support-desktop-symbolic",
                            _ ("Desktop support"),
                            _ ("Works well on large screens"),
                            TRUE);
  gtk_list_box_append (self->list, GTK_WIDGET (row));

  for (gsize i = 0; i < G_N_ELEMENTS (control_infos); i++)
    {
      RelationType relation;

      relation = get_control_relation (required_controls,
                                       recommended_controls,
                                       supported_controls,
                                       control_infos[i].control_flag);
      add_control_row (self, &control_infos[i], relation);
    }
}

static void
update_header (BzHardwareSupportDialog *self)
{
  const gchar      *icon_name;
  g_autofree gchar *title_text = NULL;
  const gchar      *css_class;
  guint             required_controls;
  gboolean          is_mobile_friendly;

  if (self->entry == NULL)
    return;

  required_controls  = bz_entry_get_required_controls (self->entry);
  is_mobile_friendly = bz_entry_get_is_mobile_friendly (self->entry);

  if (required_controls != BZ_CONTROL_NONE || !is_mobile_friendly)
    {
      icon_name  = "dialog-warning-symbolic";
      title_text = g_strdup_printf (_ ("%s works best on specific hardware"),
                                    bz_entry_get_title (self->entry));
      css_class  = "grey";
    }
  else
    {
      icon_name  = "device-supported-symbolic";
      title_text = g_strdup_printf (_ ("%s works on most devices"),
                                    bz_entry_get_title (self->entry));
      css_class  = "green";
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->lozenge), icon_name);
  gtk_label_set_text (self->title, title_text);

  gtk_widget_remove_css_class (self->lozenge, "green");
  gtk_widget_remove_css_class (self->lozenge, "grey");
  gtk_widget_add_css_class (self->lozenge, css_class);
}

static void
update_ui (BzHardwareSupportDialog *self)
{
  update_list (self);
  update_header (self);
}

static void
entry_notify_cb (GObject    *obj,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  BzHardwareSupportDialog *self = BZ_HARDWARE_SUPPORT_DIALOG (user_data);
  update_ui (self);
}

static void
bz_hardware_support_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BzHardwareSupportDialog *self = BZ_HARDWARE_SUPPORT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      self->entry = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_hardware_support_dialog_constructed (GObject *object)
{
  BzHardwareSupportDialog *self = BZ_HARDWARE_SUPPORT_DIALOG (object);

  G_OBJECT_CLASS (bz_hardware_support_dialog_parent_class)->constructed (object);

  if (self->entry != NULL)
    {
      self->entry_notify_handler = g_signal_connect (self->entry, "notify",
                                                     G_CALLBACK (entry_notify_cb), self);
      update_ui (self);
    }
}

static void
bz_hardware_support_dialog_dispose (GObject *object)
{
  BzHardwareSupportDialog *self = BZ_HARDWARE_SUPPORT_DIALOG (object);

  g_clear_signal_handler (&self->entry_notify_handler, self->entry);
  g_clear_object (&self->entry);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_HARDWARE_SUPPORT_DIALOG);

  G_OBJECT_CLASS (bz_hardware_support_dialog_parent_class)->dispose (object);
}

static void
bz_hardware_support_dialog_class_init (BzHardwareSupportDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_hardware_support_dialog_set_property;
  object_class->constructed  = bz_hardware_support_dialog_constructed;
  object_class->dispose      = bz_hardware_support_dialog_dispose;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-hardware-support-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzHardwareSupportDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzHardwareSupportDialog, title);
  gtk_widget_class_bind_template_child (widget_class, BzHardwareSupportDialog, list);
}

static void
bz_hardware_support_dialog_init (BzHardwareSupportDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzHardwareSupportDialog *
bz_hardware_support_dialog_new (BzEntry *entry)
{
  return g_object_new (BZ_TYPE_HARDWARE_SUPPORT_DIALOG,
                       "entry", entry,
                       NULL);
}

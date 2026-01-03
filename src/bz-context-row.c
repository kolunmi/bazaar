/* bz-context-row.c
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

#include "bz-context-row.h"

GType
bz_importance_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GEnumValue values[] = {
        { BZ_IMPORTANCE_UNIMPORTANT, "BZ_IMPORTANCE_UNIMPORTANT", "unimportant" },
        {     BZ_IMPORTANCE_NEUTRAL,     "BZ_IMPORTANCE_NEUTRAL",     "neutral" },
        { BZ_IMPORTANCE_INFORMATION, "BZ_IMPORTANCE_INFORMATION", "information" },
        {     BZ_IMPORTANCE_WARNING,     "BZ_IMPORTANCE_WARNING",     "warning" },
        {   BZ_IMPORTANCE_IMPORTANT,   "BZ_IMPORTANCE_IMPORTANT",   "important" },
        {                         0,                        NULL,          NULL }
      };
      type = g_enum_register_static ("BzImportance", values);
    }

  return type;
}

const gchar *
bz_context_row_importance_to_css_class (BzImportance importance)
{
  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
      return "green";
    case BZ_IMPORTANCE_NEUTRAL:
      return "grey";
    case BZ_IMPORTANCE_INFORMATION:
      return "yellow";
    case BZ_IMPORTANCE_WARNING:
      return "orange";
    case BZ_IMPORTANCE_IMPORTANT:
      return "red";
    default:
      return "grey";
    }
}

AdwActionRow *
bz_context_row_new (const gchar *icon_name,
                    BzImportance importance,
                    const gchar *title,
                    const gchar *subtitle)
{
  AdwActionRow *row;
  GtkWidget    *icon;
  const gchar  *css_class;

  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);

  if (subtitle != NULL)
    adw_action_row_set_subtitle (row, subtitle);

  icon = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (icon, "circular-lozenge");

  css_class = bz_context_row_importance_to_css_class (importance);
  gtk_widget_add_css_class (icon, css_class);

  adw_action_row_add_prefix (row, icon);

  return row;
}

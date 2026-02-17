/* bz-search-pill-list.c
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

#include "bz-search-pill-list.h"

struct _BzSearchPillList
{
  GtkBox parent_instance;
};

G_DEFINE_FINAL_TYPE (BzSearchPillList, bz_search_pill_list, GTK_TYPE_BOX)

enum
{
  SIGNAL_ACTIVATED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
pill_button_clicked_cb (BzSearchPillList *self,
                        GtkButton        *button);

static void
bz_search_pill_list_class_init (BzSearchPillListClass *klass)
{
  signals[SIGNAL_ACTIVATED] =
      g_signal_new ("activated",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__STRING,
                    G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
bz_search_pill_list_init (BzSearchPillList *self)
{
  GtkLayoutManager *layout;
  const char *pills[] = {
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Browser"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Video"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Music"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Office"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("PDF"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Calendar"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Messaging"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Steam"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Paint"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("VPN"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Torrent"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Minecraft"),
    /* Translators: Search suggestion: please use keywords that give good results */
    N_("Emulator"),
    NULL
  };

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  layout = GTK_LAYOUT_MANAGER (adw_wrap_layout_new ());
  adw_wrap_layout_set_child_spacing (ADW_WRAP_LAYOUT (layout), 10);
  adw_wrap_layout_set_line_spacing (ADW_WRAP_LAYOUT (layout), 8);
  gtk_widget_set_layout_manager (GTK_WIDGET (self), layout);

  for (guint i = 0; pills[i] != NULL; i++)
    {
      GtkWidget *button;

      button = gtk_button_new_with_label (_ (pills[i]));
      gtk_widget_add_css_class (button, "small-pill");
      gtk_widget_add_css_class (button, "search-pill");
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK (pill_button_clicked_cb), self);
      gtk_box_append (GTK_BOX (self), button);
    }
}

GtkWidget *
bz_search_pill_list_new (void)
{
  return g_object_new (BZ_TYPE_SEARCH_PILL_LIST, NULL);
}

static void
pill_button_clicked_cb (BzSearchPillList *self,
                        GtkButton        *button)
{
  const char *label;

  g_return_if_fail (BZ_IS_SEARCH_PILL_LIST (self));
  g_return_if_fail (GTK_IS_BUTTON (button));

  label = gtk_button_get_label (button);
  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0, label);
}

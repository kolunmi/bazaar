/* bz-view-switcher.c
 *
 * Copyright 2026 Eva M
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

#include "bz-view-switcher.h"
#include "bz-view-switcher-button.h"

struct _BzViewSwitcher
{
  GtkWidget parent_instance;

  GtkWidget *box;

  AdwViewStack *stack;

  GtkSelectionModel *pages;
  GPtrArray         *buttons;
};

G_DEFINE_FINAL_TYPE (BzViewSwitcher, bz_view_switcher, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_STACK,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
pages_changed (BzViewSwitcher *self,
               guint           position,
               guint           removed,
               guint           added,
               GListModel     *model);

static void
selection_changed (BzViewSwitcher    *self,
                   guint              position,
                   guint              n_items,
                   GtkSelectionModel *model);

static void
button_clicked (GtkButton *toggle,
                gpointer   idx_p);

static void
refresh_selection (BzViewSwitcher *self);

static void
bz_view_switcher_dispose (GObject *object)
{
  BzViewSwitcher *self = BZ_VIEW_SWITCHER (object);

  if (self->pages != NULL)
    {
      g_signal_handlers_disconnect_by_func (
          self->pages, pages_changed, self);
      g_signal_handlers_disconnect_by_func (
          self->pages, selection_changed, self);
    }

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_pointer (&self->stack, g_object_unref);
  g_clear_pointer (&self->pages, g_object_unref);
  g_clear_pointer (&self->buttons, g_ptr_array_unref);

  G_OBJECT_CLASS (bz_view_switcher_parent_class)->dispose (object);
}

static void
bz_view_switcher_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzViewSwitcher *self = BZ_VIEW_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, bz_view_switcher_get_stack (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_view_switcher_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzViewSwitcher *self = BZ_VIEW_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      bz_view_switcher_set_stack (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_view_switcher_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  BzViewSwitcher *self = BZ_VIEW_SWITCHER (widget);

  gtk_widget_allocate (self->box, width, height, baseline, NULL);
}

static void
bz_view_switcher_measure (GtkWidget     *widget,
                          GtkOrientation orientation,
                          int            for_size,
                          int           *minimum,
                          int           *natural,
                          int           *minimum_baseline,
                          int           *natural_baseline)
{
  BzViewSwitcher *self = BZ_VIEW_SWITCHER (widget);

  gtk_widget_measure (
      self->box, orientation,
      for_size, minimum, natural,
      minimum_baseline, natural_baseline);
}

static void
bz_view_switcher_class_init (BzViewSwitcherClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_view_switcher_set_property;
  object_class->get_property = bz_view_switcher_get_property;
  object_class->dispose      = bz_view_switcher_dispose;

  props[PROP_STACK] =
      g_param_spec_object (
          "stack",
          NULL, NULL,
          ADW_TYPE_VIEW_STACK,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure       = bz_view_switcher_measure;
  widget_class->size_allocate = bz_view_switcher_size_allocate;

  g_type_ensure (BZ_TYPE_VIEW_SWITCHER_BUTTON);
}

static void
bz_view_switcher_init (BzViewSwitcher *self)
{
  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_parent (self->box, GTK_WIDGET (self));

  self->buttons = g_ptr_array_new ();
}

BzViewSwitcher *
bz_view_switcher_new (void)
{
  return g_object_new (BZ_TYPE_VIEW_SWITCHER, NULL);
}

AdwViewStack *
bz_view_switcher_get_stack (BzViewSwitcher *self)
{
  g_return_val_if_fail (BZ_IS_VIEW_SWITCHER (self), NULL);
  return self->stack;
}

void
bz_view_switcher_set_stack (BzViewSwitcher *self,
                            AdwViewStack   *stack)
{
  g_return_if_fail (BZ_IS_VIEW_SWITCHER (self));

  if (stack == self->stack)
    return;

  if (self->pages != NULL)
    {
      guint n_items = 0;

      g_signal_handlers_disconnect_by_func (
          self->pages, pages_changed, self);
      g_signal_handlers_disconnect_by_func (
          self->pages, selection_changed, self);

      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->pages));
      pages_changed (self, 0, n_items, 0, G_LIST_MODEL (self->pages));
    }

  for (guint i = 0; i < self->buttons->len; i++)
    {
      GtkWidget *button = NULL;

      button = g_ptr_array_index (self->buttons, i);
      gtk_widget_unparent (button);
    }
  g_ptr_array_set_size (self->buttons, 0);

  g_clear_pointer (&self->stack, g_object_unref);
  g_clear_pointer (&self->pages, g_object_unref);
  if (stack != NULL)
    {
      guint n_items = 0;

      self->stack = g_object_ref (stack);
      self->pages = adw_view_stack_get_pages (stack);

      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->pages));
      pages_changed (self, 0, 0, n_items, G_LIST_MODEL (self->pages));
      refresh_selection (self);

      g_signal_connect_swapped (
          self->pages,
          "items-changed",
          G_CALLBACK (pages_changed),
          self);
      g_signal_connect_swapped (
          self->pages,
          "selection-changed",
          G_CALLBACK (selection_changed),
          self);
    }
  else
    refresh_selection (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STACK]);
}

static void
pages_changed (BzViewSwitcher *self,
               guint           position,
               guint           removed,
               guint           added,
               GListModel     *model)
{
  for (guint i = 0; i < removed; i++)
    {
      GtkWidget *button = NULL;

      button = g_ptr_array_index (self->buttons, position + i);
      gtk_widget_unparent (button);
    }
  if (removed > 0)
    g_ptr_array_remove_range (self->buttons, position, removed);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (AdwViewStackPage) page = NULL;
      GtkWidget *button                 = NULL;
      GtkButton *toggle                 = NULL;
      GtkWidget *sibling                = NULL;

      page = g_list_model_get_item (G_LIST_MODEL (self->pages), position + i);

      button = bz_view_switcher_button_new ();
      bz_view_switcher_button_set_page (BZ_VIEW_SWITCHER_BUTTON (button), page);

      toggle = bz_view_switcher_button_get_toggle (BZ_VIEW_SWITCHER_BUTTON (button));
      g_signal_connect (
          toggle,
          "clicked",
          G_CALLBACK (button_clicked),
          GUINT_TO_POINTER (position + i));

      if (position + i > 0)
        sibling = g_ptr_array_index (self->buttons, position + i - 1);
      gtk_box_insert_child_after (GTK_BOX (self->box), button, sibling);

      g_ptr_array_insert (self->buttons, position + i, button);
    }
}

static void
selection_changed (BzViewSwitcher    *self,
                   guint              position,
                   guint              n_items,
                   GtkSelectionModel *model)
{
  refresh_selection (self);
}

static void
button_clicked (GtkButton *toggle,
                gpointer   idx_p)
{
  guint           idx  = GPOINTER_TO_UINT (idx_p);
  BzViewSwitcher *self = NULL;

  if (!gtk_widget_has_css_class (GTK_WIDGET (toggle), "flat"))
    return;

  self = (BzViewSwitcher *) gtk_widget_get_ancestor (
      GTK_WIDGET (toggle), BZ_TYPE_VIEW_SWITCHER);
  g_assert (self != NULL);

  gtk_selection_model_select_item (self->pages, idx, TRUE);
}

static void
refresh_selection (BzViewSwitcher *self)
{
  g_autoptr (GtkBitset) bitset = NULL;
  guint selected_idx           = 0;

  g_assert (self->pages != NULL);

  bitset       = gtk_selection_model_get_selection (self->pages);
  selected_idx = gtk_bitset_get_nth (bitset, 0);

  for (guint i = 0; i < self->buttons->len; i++)
    {
      BzViewSwitcherButton *button = NULL;
      GtkButton            *toggle = NULL;

      button = g_ptr_array_index (self->buttons, i);
      toggle = bz_view_switcher_button_get_toggle (button);
      if (i == selected_idx)
        gtk_widget_remove_css_class (GTK_WIDGET (toggle), "flat");
      else
        gtk_widget_add_css_class (GTK_WIDGET (toggle), "flat");
    }
}

/* End of bz-view-switcher.c */

/* bz-view-switcher-button.c
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

#include "bz-view-switcher-button.h"

struct _BzViewSwitcherButton
{
  AdwBin parent_instance;

  AdwViewStackPage *page;

  GtkButton *toggle;
};

G_DEFINE_FINAL_TYPE (BzViewSwitcherButton, bz_view_switcher_button, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_PAGE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_view_switcher_button_dispose (GObject *object)
{
  BzViewSwitcherButton *self = BZ_VIEW_SWITCHER_BUTTON (object);

  g_clear_pointer (&self->page, g_object_unref);

  G_OBJECT_CLASS (bz_view_switcher_button_parent_class)->dispose (object);
}

static void
bz_view_switcher_button_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzViewSwitcherButton *self = BZ_VIEW_SWITCHER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      g_value_set_object (value, bz_view_switcher_button_get_page (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_view_switcher_button_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzViewSwitcherButton *self = BZ_VIEW_SWITCHER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      bz_view_switcher_button_set_page (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_view_switcher_button_class_init (BzViewSwitcherButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_view_switcher_button_set_property;
  object_class->get_property = bz_view_switcher_button_get_property;
  object_class->dispose      = bz_view_switcher_button_dispose;

  props[PROP_PAGE] =
      g_param_spec_object (
          "page",
          NULL, NULL,
          ADW_TYPE_VIEW_STACK_PAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-view-switcher-button.ui");
  gtk_widget_class_bind_template_child (widget_class, BzViewSwitcherButton, toggle);
}

static void
bz_view_switcher_button_init (BzViewSwitcherButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_view_switcher_button_new (void)
{
  return g_object_new (BZ_TYPE_VIEW_SWITCHER_BUTTON, NULL);
}

GtkButton *
bz_view_switcher_button_get_toggle (BzViewSwitcherButton *self)
{
  g_return_val_if_fail (BZ_IS_VIEW_SWITCHER_BUTTON (self), NULL);
  return self->toggle;
}

AdwViewStackPage *
bz_view_switcher_button_get_page (BzViewSwitcherButton *self)
{
  g_return_val_if_fail (BZ_IS_VIEW_SWITCHER_BUTTON (self), NULL);
  return self->page;
}

void
bz_view_switcher_button_set_page (BzViewSwitcherButton *self,
                                  AdwViewStackPage     *page)
{
  g_return_if_fail (BZ_IS_VIEW_SWITCHER_BUTTON (self));

  if (page == self->page)
    return;

  g_clear_pointer (&self->page, g_object_unref);
  if (page != NULL)
    self->page = g_object_ref (page);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAGE]);
}

/* End of bz-view-switcher-button.c */

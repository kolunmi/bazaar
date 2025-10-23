/* bz-screenshots-carousel.c
 *
 * Copyright 2025 Alexander Vanhee
 *
 * Adapted from gs-screenshot-carousel.c
 *
 * Copyright (C) 2013-2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2015-2019 Kalev Lember <klember@redhat.com>
 * Copyright (C) 2019 Joaquim Rocha <jrocha@endlessm.com>
 * Copyright (C) 2021 Adrien Plazas <adrien.plazas@puri.sm>
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

#include "bz-screenshots-carousel.h"
#include "bz-decorated-screenshot.h"
#include "bz-screenshot-dialog.h"
#include <adwaita.h>

struct _BzScreenshotsCarousel
{
  GtkWidget parent_instance;

  AdwCarousel *carousel;
  GtkWidget   *carousel_indicator;
  GtkButton   *prev_button;
  GtkWidget   *prev_button_revealer;
  GtkButton   *next_button;
  GtkWidget   *next_button_revealer;

  GListModel *model;
  gulong      items_changed_id;
};

G_DEFINE_FINAL_TYPE (BzScreenshotsCarousel, bz_screenshots_carousel, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
update_button_visibility (BzScreenshotsCarousel *self)
{
  gdouble position;
  guint   n_pages;

  if (!self->carousel)
    return;

  position = adw_carousel_get_position (self->carousel);
  n_pages  = adw_carousel_get_n_pages (self->carousel);

  gtk_widget_set_visible (self->carousel_indicator, n_pages > 1);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->prev_button_revealer), position >= 0.5);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->next_button_revealer), position < n_pages - 1.5);
}

static void
carousel_navigate (AdwCarousel *carousel, AdwNavigationDirection direction)
{
  g_autoptr (GList) children = NULL;
  GtkWidget *child;
  gdouble    position;
  guint      n_children;

  n_children = 0;
  for (child = gtk_widget_get_first_child (GTK_WIDGET (carousel));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      children = g_list_prepend (children, child);
      n_children++;
    }
  children = g_list_reverse (children);

  position = adw_carousel_get_position (carousel);
  position += (direction == ADW_NAVIGATION_DIRECTION_BACK) ? -1 : 1;
  position = round (position);
  position = MIN (position, n_children - 1);
  position = MAX (0, position);

  child = g_list_nth_data (children, position);
  if (child)
    adw_carousel_scroll_to (carousel, child, TRUE);
}

static void
on_prev_clicked (BzScreenshotsCarousel *self)
{
  if (!self->carousel)
    return;

  carousel_navigate (self->carousel, ADW_NAVIGATION_DIRECTION_BACK);
}

static void
on_next_clicked (BzScreenshotsCarousel *self)
{
  if (!self->carousel)
    return;

  carousel_navigate (self->carousel, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static void
on_notify_position (BzScreenshotsCarousel *self)
{
  update_button_visibility (self);
}

static void
on_notify_n_pages (BzScreenshotsCarousel *self)
{
  update_button_visibility (self);
}

static void
on_screenshot_clicked (BzDecoratedScreenshot *screenshot, BzScreenshotsCarousel *self)
{
  BzAsyncTexture *async_texture = NULL;
  AdwDialog      *dialog        = NULL;
  guint           index         = 0;
  guint           n_items       = 0;

  if (!self->carousel || !self->model)
    return;

  async_texture = bz_decorated_screenshot_get_async_texture (screenshot);
  if (async_texture == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzAsyncTexture) item = NULL;

      item = g_list_model_get_item (self->model, i);
      if (item == async_texture)
        {
          index = i;
          break;
        }
    }

  dialog = bz_screenshot_dialog_new (self->model, index);
  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

static void
clear_carousel (BzScreenshotsCarousel *self)
{
  GtkWidget *child;

  if (!self->carousel)
    return;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->carousel))) != NULL)
    adw_carousel_remove (self->carousel, child);
}

static void
populate_carousel (BzScreenshotsCarousel *self)
{
  guint i;
  guint n_items;

  clear_carousel (self);

  if (!self->carousel || self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr (GdkPaintable) paintable = NULL;
      GtkWidget *screenshot;

      paintable = g_list_model_get_item (self->model, i);

      screenshot = g_object_new (BZ_TYPE_DECORATED_SCREENSHOT,
                                 "async-texture", paintable,
                                 NULL);

      g_signal_connect (screenshot, "clicked",
                        G_CALLBACK (on_screenshot_clicked), self);

      adw_carousel_append (self->carousel, screenshot);
      gtk_widget_set_visible (screenshot, TRUE);
    }

  update_button_visibility (self);
}

static void
on_model_items_changed (GListModel            *model,
                        guint                  position,
                        guint                  removed,
                        guint                  added,
                        BzScreenshotsCarousel *self)
{
  populate_carousel (self);
}

static void
bz_screenshots_carousel_dispose (GObject *object)
{
  BzScreenshotsCarousel *self = BZ_SCREENSHOTS_CAROUSEL (object);

  if (self->model && self->items_changed_id)
    {
      g_signal_handler_disconnect (self->model, self->items_changed_id);
      self->items_changed_id = 0;
    }

  g_clear_object (&self->model);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_SCREENSHOTS_CAROUSEL);

  G_OBJECT_CLASS (bz_screenshots_carousel_parent_class)->dispose (object);
}

static void
bz_screenshots_carousel_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzScreenshotsCarousel *self = BZ_SCREENSHOTS_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_screenshots_carousel_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzScreenshotsCarousel *self = BZ_SCREENSHOTS_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_screenshots_carousel_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_screenshots_carousel_class_init (BzScreenshotsCarouselClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_screenshots_carousel_dispose;
  object_class->get_property = bz_screenshots_carousel_get_property;
  object_class->set_property = bz_screenshots_carousel_set_property;

  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           NULL,
                           NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-screenshots-carousel.ui");
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, carousel);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, carousel_indicator);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, prev_button);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, prev_button_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, next_button);
  gtk_widget_class_bind_template_child (widget_class, BzScreenshotsCarousel, next_button_revealer);
  gtk_widget_class_bind_template_callback (widget_class, on_prev_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_next_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_notify_position);
  gtk_widget_class_bind_template_callback (widget_class, on_notify_n_pages);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "screenshot-carousel");
}

static void
bz_screenshots_carousel_init (BzScreenshotsCarousel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (self->carousel)
    adw_carousel_set_allow_scroll_wheel (self->carousel, FALSE);
}

GtkWidget *
bz_screenshots_carousel_new (void)
{
  return g_object_new (BZ_TYPE_SCREENSHOTS_CAROUSEL, NULL);
}

void
bz_screenshots_carousel_set_model (BzScreenshotsCarousel *self, GListModel *model)
{
  g_return_if_fail (BZ_IS_SCREENSHOTS_CAROUSEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  if (self->model && self->items_changed_id)
    {
      g_signal_handler_disconnect (self->model, self->items_changed_id);
      self->items_changed_id = 0;
    }

  g_clear_object (&self->model);

  if (model)
    {
      self->model            = g_object_ref (model);
      self->items_changed_id = g_signal_connect (self->model, "items-changed",
                                                 G_CALLBACK (on_model_items_changed), self);
    }

  populate_carousel (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

GListModel *
bz_screenshots_carousel_get_model (BzScreenshotsCarousel *self)
{
  g_return_val_if_fail (BZ_IS_SCREENSHOTS_CAROUSEL (self), NULL);
  return self->model;
}

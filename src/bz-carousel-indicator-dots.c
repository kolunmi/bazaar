/* bz-carousel-indicator-dots.c
 *
 * Copyright (C) 2020 Alice Mikhaylenko <alicem@gnome.org>
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "bz-carousel-indicator-dots.h"
#include "config.h"
#include <math.h>

#define DOTS_RADIUS           3
#define DOTS_RADIUS_SELECTED  4
#define DOTS_OPACITY          0.3
#define DOTS_OPACITY_SELECTED 0.9
#define DOTS_SPACING          7
#define DOTS_MARGIN           6

struct _BzCarouselIndicatorDots
{
  GtkWidget      parent_instance;
  BzCarousel    *carousel;
  GtkOrientation orientation;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (BzCarouselIndicatorDots, bz_carousel_indicator_dots, GTK_TYPE_WIDGET, G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

enum
{
  PROP_0,
  PROP_CAROUSEL,
  PROP_ORIENTATION,
  LAST_PROP = PROP_CAROUSEL + 1,
};

static GParamSpec *props[LAST_PROP];

static double
lerp (double a, double b, double t)
{
  return a + (b - a) * t;
}

static void
snapshot_dots (GtkWidget     *widget,
               GtkSnapshot   *snapshot,
               GtkOrientation orientation,
               double         position,
               guint          n_pages)
{
  GdkRGBA         color;
  int             i, widget_length, widget_thickness;
  double          x, y, indicator_length, dot_size, full_size;
  double          current_position, remaining_progress;
  graphene_rect_t rect;

  gtk_widget_get_color (widget, &color);
  dot_size         = 2 * DOTS_RADIUS_SELECTED + DOTS_SPACING;
  indicator_length = n_pages * dot_size - DOTS_SPACING;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      widget_length    = gtk_widget_get_width (widget);
      widget_thickness = gtk_widget_get_height (widget);
    }
  else
    {
      widget_length    = gtk_widget_get_height (widget);
      widget_thickness = gtk_widget_get_width (widget);
    }

  full_size = round (indicator_length / dot_size) * dot_size;
  if ((widget_length - (int) full_size) % 2 == 0)
    widget_length--;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      x = (widget_length - indicator_length) / 2.0;
      y = widget_thickness / 2;
    }
  else
    {
      x = widget_thickness / 2;
      y = (widget_length - indicator_length) / 2.0;
    }

  current_position   = 0;
  remaining_progress = 1;

  graphene_rect_init (&rect, -DOTS_RADIUS, -DOTS_RADIUS, DOTS_RADIUS * 2, DOTS_RADIUS * 2);

  for (i = 0; i < n_pages; i++)
    {
      double         progress, radius, opacity;
      GskRoundedRect clip;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        x += dot_size / 2.0;
      else
        y += dot_size / 2.0;

      current_position += 1;

      progress = CLAMP (current_position - position, 0, remaining_progress);
      remaining_progress -= progress;

      radius  = lerp (DOTS_RADIUS, DOTS_RADIUS_SELECTED, progress);
      opacity = lerp (DOTS_OPACITY, DOTS_OPACITY_SELECTED, progress);

      gsk_rounded_rect_init_from_rect (&clip, &rect, radius);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
      gtk_snapshot_scale (snapshot, radius / DOTS_RADIUS, radius / DOTS_RADIUS);

      gtk_snapshot_push_rounded_clip (snapshot, &clip);
      gtk_snapshot_push_opacity (snapshot, opacity);

      gtk_snapshot_append_color (snapshot, &color, &rect);

      gtk_snapshot_pop (snapshot);
      gtk_snapshot_pop (snapshot);

      gtk_snapshot_restore (snapshot);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        x += dot_size / 2.0;
      else
        y += dot_size / 2.0;
    }
}

static void
bz_carousel_indicator_dots_measure (GtkWidget     *widget,
                                    GtkOrientation orientation,
                                    int            for_size,
                                    int           *minimum,
                                    int           *natural,
                                    int           *minimum_baseline,
                                    int           *natural_baseline)
{
  BzCarouselIndicatorDots *self = BZ_CAROUSEL_INDICATOR_DOTS (widget);
  int                      size = 0;

  if (orientation == self->orientation)
    {
      guint  n_pages = 0;
      double indicator_length, dot_size;

      if (self->carousel)
        g_object_get (self->carousel, "n-pages", &n_pages, NULL);

      dot_size         = 2 * DOTS_RADIUS_SELECTED + DOTS_SPACING;
      indicator_length = n_pages * dot_size - DOTS_SPACING;
      size             = ceil (indicator_length);
    }
  else
    {
      size = 2 * DOTS_RADIUS_SELECTED;
    }

  size += 2 * DOTS_MARGIN;

  if (minimum)
    *minimum = size;
  if (natural)
    *natural = size;
  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;
}

static void
bz_carousel_indicator_dots_snapshot (GtkWidget   *widget,
                                     GtkSnapshot *snapshot)
{
  BzCarouselIndicatorDots *self = BZ_CAROUSEL_INDICATOR_DOTS (widget);
  guint                    n_pages;
  double                   position;

  if (!self->carousel)
    return;

  g_object_get (self->carousel, "n-pages", &n_pages, "position", &position, NULL);

  if (n_pages < 2)
    return;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    position = n_pages - 1 - position;

  snapshot_dots (widget, snapshot, self->orientation, position, n_pages);
}

static void
bz_carousel_indicator_dots_dispose (GObject *object)
{
  BzCarouselIndicatorDots *self = BZ_CAROUSEL_INDICATOR_DOTS (object);

  bz_carousel_indicator_dots_set_carousel (self, NULL);

  G_OBJECT_CLASS (bz_carousel_indicator_dots_parent_class)->dispose (object);
}

static void
bz_carousel_indicator_dots_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  BzCarouselIndicatorDots *self = BZ_CAROUSEL_INDICATOR_DOTS (object);

  switch (prop_id)
    {
    case PROP_CAROUSEL:
      g_value_set_object (value, bz_carousel_indicator_dots_get_carousel (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_carousel_indicator_dots_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BzCarouselIndicatorDots *self = BZ_CAROUSEL_INDICATOR_DOTS (object);

  switch (prop_id)
    {
    case PROP_CAROUSEL:
      bz_carousel_indicator_dots_set_carousel (self, g_value_get_object (value));
      break;
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (orientation != self->orientation)
          {
            self->orientation = orientation;
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify (G_OBJECT (self), "orientation");
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_carousel_indicator_dots_class_init (BzCarouselIndicatorDotsClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_carousel_indicator_dots_dispose;
  object_class->get_property = bz_carousel_indicator_dots_get_property;
  object_class->set_property = bz_carousel_indicator_dots_set_property;

  widget_class->measure  = bz_carousel_indicator_dots_measure;
  widget_class->snapshot = bz_carousel_indicator_dots_snapshot;

  props[PROP_CAROUSEL] =
      g_param_spec_object ("carousel", NULL, NULL,
                           BZ_TYPE_CAROUSEL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");
  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "carouselindicatordots");
}

static void
bz_carousel_indicator_dots_init (BzCarouselIndicatorDots *self)
{
}

GtkWidget *
bz_carousel_indicator_dots_new (void)
{
  return g_object_new (BZ_TYPE_CAROUSEL_INDICATOR_DOTS, NULL);
}

BzCarousel *
bz_carousel_indicator_dots_get_carousel (BzCarouselIndicatorDots *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL_INDICATOR_DOTS (self), NULL);

  return self->carousel;
}

void
bz_carousel_indicator_dots_set_carousel (BzCarouselIndicatorDots *self,
                                         BzCarousel              *carousel)
{
  g_return_if_fail (BZ_IS_CAROUSEL_INDICATOR_DOTS (self));
  g_return_if_fail (carousel == NULL || BZ_IS_CAROUSEL (carousel));

  if (self->carousel == carousel)
    return;

  if (self->carousel)
    {
      g_signal_handlers_disconnect_by_func (self->carousel, gtk_widget_queue_draw, self);
    }

  g_set_object (&self->carousel, carousel);

  if (self->carousel)
    {
      g_signal_connect_object (self->carousel, "notify::position",
                               G_CALLBACK (gtk_widget_queue_draw), self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->carousel, "notify::n-pages",
                               G_CALLBACK (gtk_widget_queue_resize), self,
                               G_CONNECT_SWAPPED);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CAROUSEL]);
}

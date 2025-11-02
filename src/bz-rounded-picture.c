/* bz-rounded-picture.c
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

#include "bz-rounded-picture.h"

struct _BzRoundedPicture
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  double        radius;
};

G_DEFINE_FINAL_TYPE (BzRoundedPicture, bz_rounded_picture, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_RADIUS,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL };

static void
invalidate_contents (BzRoundedPicture *self,
                     GdkPaintable     *paintable)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
invalidate_size (BzRoundedPicture *self,
                 GdkPaintable     *paintable)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
bz_rounded_picture_dispose (GObject *object)
{
  BzRoundedPicture *self = BZ_ROUNDED_PICTURE (object);

  if (self->paintable != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_size, self);
    }
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_rounded_picture_parent_class)->dispose (object);
}

static void
bz_rounded_picture_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzRoundedPicture *self = BZ_ROUNDED_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;
    case PROP_RADIUS:
      g_value_set_double (value, self->radius);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_rounded_picture_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzRoundedPicture *self = BZ_ROUNDED_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      bz_rounded_picture_set_paintable (self, g_value_get_object (value));
      break;
    case PROP_RADIUS:
      bz_rounded_picture_set_radius (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_rounded_picture_measure (GtkWidget     *widget,
                            GtkOrientation orientation,
                            int            for_size,
                            int           *minimum,
                            int           *natural,
                            int           *minimum_baseline,
                            int           *natural_baseline)
{
  BzRoundedPicture *self = BZ_ROUNDED_PICTURE (widget);

  *minimum = 0;
  *natural = 0;

  if (self->paintable == NULL)
    return;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *natural = gdk_paintable_get_intrinsic_width (self->paintable);
    }
  else
    {
      *natural = gdk_paintable_get_intrinsic_height (self->paintable);
    }
}

static void
bz_rounded_picture_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  BzRoundedPicture *self = BZ_ROUNDED_PICTURE (widget);
  int               widget_width;
  int               widget_height;
  double            paintable_width;
  double            paintable_height;
  double            paintable_aspect;
  double            widget_aspect;
  double            scale;
  double            draw_width, draw_height;
  double            x, y;
  GskRoundedRect    rect;
  GskShadow         shadow;

  if (self->paintable == NULL)
    return;

  widget_width  = gtk_widget_get_width (widget);
  widget_height = gtk_widget_get_height (widget);

  if (widget_width <= 0 || widget_height <= 0)
    return;

  paintable_width  = gdk_paintable_get_intrinsic_width (self->paintable);
  paintable_height = gdk_paintable_get_intrinsic_height (self->paintable);

  if (paintable_width <= 0 || paintable_height <= 0)
    {
      paintable_width  = widget_width;
      paintable_height = widget_height;
    }

  paintable_aspect = paintable_width / paintable_height;
  widget_aspect    = (double) widget_width / (double) widget_height;

  if (widget_aspect > paintable_aspect)
    {
      scale       = (double) widget_height / paintable_height;
      draw_height = widget_height;
      draw_width  = paintable_width * scale;
    }
  else
    {
      scale       = (double) widget_width / paintable_width;
      draw_width  = widget_width;
      draw_height = paintable_height * scale;
    }

  x = (widget_width - draw_width) / 2.0;
  y = (widget_height - draw_height) / 2.0;

  shadow.color.red   = 0.0;
  shadow.color.green = 0.0;
  shadow.color.blue  = 0.0;
  shadow.color.alpha = 0.35;
  shadow.dx          = 0.0;
  shadow.dy          = 2.0;
  shadow.radius      = 16.0;

  gtk_snapshot_push_shadow (snapshot, &shadow, 1);

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));

  gsk_rounded_rect_init_from_rect (&rect,
                                   &GRAPHENE_RECT_INIT (0, 0, draw_width, draw_height),
                                   self->radius);
  gtk_snapshot_push_rounded_clip (snapshot, &rect);

  gdk_paintable_snapshot (self->paintable, snapshot, draw_width, draw_height);

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_pop (snapshot);
}

static void
bz_rounded_picture_class_init (BzRoundedPictureClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_rounded_picture_dispose;
  object_class->get_property = bz_rounded_picture_get_property;
  object_class->set_property = bz_rounded_picture_set_property;

  widget_class->measure  = bz_rounded_picture_measure;
  widget_class->snapshot = bz_rounded_picture_snapshot;

  props[PROP_PAINTABLE] =
      g_param_spec_object ("paintable",
                           NULL, NULL,
                           GDK_TYPE_PAINTABLE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RADIUS] =
      g_param_spec_double ("radius",
                           NULL, NULL,
                           0.0, G_MAXDOUBLE, 12.0,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_rounded_picture_init (BzRoundedPicture *self)
{
  self->radius = 12.0;
}

GtkWidget *
bz_rounded_picture_new (void)
{
  return g_object_new (BZ_TYPE_ROUNDED_PICTURE, NULL);
}

void
bz_rounded_picture_set_paintable (BzRoundedPicture *self,
                                  GdkPaintable     *paintable)
{
  g_return_if_fail (BZ_IS_ROUNDED_PICTURE (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (self->paintable == paintable)
    return;

  if (self->paintable != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_size, self);
    }

  g_set_object (&self->paintable, paintable);

  if (self->paintable != NULL)
    {
      g_signal_connect_swapped (self->paintable, "invalidate-contents",
                                G_CALLBACK (invalidate_contents), self);
      g_signal_connect_swapped (self->paintable, "invalidate-size",
                                G_CALLBACK (invalidate_size), self);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAINTABLE]);
}

GdkPaintable *
bz_rounded_picture_get_paintable (BzRoundedPicture *self)
{
  g_return_val_if_fail (BZ_IS_ROUNDED_PICTURE (self), NULL);
  return self->paintable;
}

void
bz_rounded_picture_set_radius (BzRoundedPicture *self,
                               double            radius)
{
  g_return_if_fail (BZ_IS_ROUNDED_PICTURE (self));

  if (self->radius == radius)
    return;

  self->radius = radius;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RADIUS]);
}

double
bz_rounded_picture_get_radius (BzRoundedPicture *self)
{
  g_return_val_if_fail (BZ_IS_ROUNDED_PICTURE (self), 0.0);
  return self->radius;
}

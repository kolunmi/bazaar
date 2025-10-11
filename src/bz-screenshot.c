/* bz-screenshot.c
 *
 * Copyright 2025 Adam Masciola
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

#include "bz-screenshot.h"
#include "bz-async-texture.h"

struct _BzScreenshot
{
  GtkWidget parent_instance;

  GdkPaintable *paintable;
  double        focus_x;
  double        focus_y;
  gboolean      rounded_corners;
};

G_DEFINE_FINAL_TYPE (BzScreenshot, bz_screenshot, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_PAINTABLE,
  PROP_FOCUS_X,
  PROP_FOCUS_Y,
  PROP_ROUNDED_CORNERS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
invalidate_contents (BzScreenshot *self,
                     GdkPaintable *paintable);

static void
invalidate_size (BzScreenshot *self,
                 GdkPaintable *paintable);

static void
async_loaded (BzScreenshot   *self,
              GParamSpec     *pspec,
              BzAsyncTexture *texture);

static void
bz_screenshot_dispose (GObject *object)
{
  BzScreenshot *self = BZ_SCREENSHOT (object);

  if (self->paintable != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_size, self);
      g_signal_handlers_disconnect_by_func (self->paintable, async_loaded, self);
    }
  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (bz_screenshot_parent_class)->dispose (object);
}

static void
bz_screenshot_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BzScreenshot *self = BZ_SCREENSHOT (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, bz_screenshot_get_paintable (self));
      break;
    case PROP_FOCUS_X:
      g_value_set_double (value, bz_screenshot_get_focus_x (self));
      break;
    case PROP_FOCUS_Y:
      g_value_set_double (value, bz_screenshot_get_focus_y (self));
      break;
    case PROP_ROUNDED_CORNERS:
      g_value_set_boolean (value, bz_screenshot_get_rounded_corners (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_screenshot_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BzScreenshot *self = BZ_SCREENSHOT (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      bz_screenshot_set_paintable (self, g_value_get_object (value));
      break;
    case PROP_FOCUS_X:
      bz_screenshot_set_focus_x (self, g_value_get_double (value));
      break;
    case PROP_FOCUS_Y:
      bz_screenshot_set_focus_y (self, g_value_get_double (value));
      break;
    case PROP_ROUNDED_CORNERS:
      bz_screenshot_set_rounded_corners (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
bz_screenshot_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
bz_screenshot_measure (GtkWidget     *widget,
                       GtkOrientation orientation,
                       int            for_size,
                       int           *minimum,
                       int           *natural,
                       int           *minimum_baseline,
                       int           *natural_baseline)
{
  BzScreenshot *self = BZ_SCREENSHOT (widget);

  if (self->paintable == NULL)
    return;

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      int    intrinsic_height;
      double intrinsic_aspect_rato;

      intrinsic_height      = gdk_paintable_get_intrinsic_height (self->paintable);
      intrinsic_aspect_rato = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

      if (for_size >= 0 && intrinsic_aspect_rato > 0.0)
        {
          double result;

          result = ceil ((double) for_size / intrinsic_aspect_rato);

          *minimum = (int) MIN (intrinsic_height, result);
          *natural = (int) MIN (intrinsic_height, result);
        }
      else
        {
          *minimum = 0;
          *natural = intrinsic_height;
        }
    }
  else
    {
      *minimum = 0;
      *natural = gdk_paintable_get_intrinsic_width (self->paintable);
    }
}

static void
bz_screenshot_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  BzScreenshot  *self;
  int            widget_width;
  int            widget_height;
  double         paintable_aspect;
  double         scaled_w, scaled_h;
  double         x, y;
  GskRoundedRect rect;

  self = BZ_SCREENSHOT (widget);

  if (self->paintable == NULL)
    return;

  widget_width  = gtk_widget_get_width (widget);
  widget_height = gtk_widget_get_height (widget);

  paintable_aspect = gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

  if (paintable_aspect > 0.0)
    {
      scaled_w = widget_width;
      scaled_h = scaled_w / paintable_aspect;

      if (scaled_h > widget_height)
        {
          scaled_h = widget_height;
          scaled_w = scaled_h * paintable_aspect;
        }
    }
  else
    {
      scaled_w = widget_width;
      scaled_h = widget_height;
    }

  x = (widget_width - scaled_w) / 2.0;
  y = (widget_height - scaled_h) / 2.0;

  rect.bounds = GRAPHENE_RECT_INIT (0, 0, widget_width, widget_height);

  if (self->rounded_corners)
    {
      rect.corner[0].width  = 10.0;
      rect.corner[0].height = 10.0;
      rect.corner[1].width  = 10.0;
      rect.corner[1].height = 10.0;
      rect.corner[2].width  = 10.0;
      rect.corner[2].height = 10.0;
      rect.corner[3].width  = 10.0;
      rect.corner[3].height = 10.0;

      gtk_snapshot_push_rounded_clip (snapshot, &rect);
    }

  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
  gdk_paintable_snapshot (self->paintable, snapshot, scaled_w, scaled_h);

  if (self->rounded_corners)
    gtk_snapshot_pop (snapshot);
}

static void
bz_screenshot_class_init (BzScreenshotClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_screenshot_dispose;
  object_class->get_property = bz_screenshot_get_property;
  object_class->set_property = bz_screenshot_set_property;

  props[PROP_PAINTABLE] =
      g_param_spec_object (
          "paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FOCUS_X] =
      g_param_spec_double (
          "focus-x",
          NULL, NULL,
          -1.0, G_MAXDOUBLE, -1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FOCUS_Y] =
      g_param_spec_double (
          "focus-y",
          NULL, NULL,
          -1.0, G_MAXDOUBLE, -1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ROUNDED_CORNERS] =
      g_param_spec_boolean (
          "rounded-corners",
          NULL, NULL,
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->get_request_mode = bz_screenshot_get_request_mode;
  widget_class->measure          = bz_screenshot_measure;
  widget_class->snapshot         = bz_screenshot_snapshot;
}

static void
bz_screenshot_init (BzScreenshot *self)
{
  self->focus_x         = -1.0;
  self->focus_y         = -1.0;
  self->rounded_corners = TRUE;
}

GtkWidget *
bz_screenshot_new (void)
{
  return g_object_new (BZ_TYPE_SCREENSHOT, NULL);
}

void
bz_screenshot_set_paintable (BzScreenshot *self,
                             GdkPaintable *paintable)
{
  g_return_if_fail (BZ_IS_SCREENSHOT (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  if (self->paintable != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->paintable, invalidate_size, self);
      g_signal_handlers_disconnect_by_func (self->paintable, async_loaded, self);
    }
  g_clear_object (&self->paintable);

  if (paintable != NULL)
    {
      self->paintable = g_object_ref (paintable);
      g_signal_connect_swapped (paintable, "invalidate-contents",
                                G_CALLBACK (invalidate_contents), self);
      g_signal_connect_swapped (paintable, "invalidate-size",
                                G_CALLBACK (invalidate_size), self);
      if (BZ_IS_ASYNC_TEXTURE (paintable))
        g_signal_connect_swapped (paintable, "notify::loaded",
                                  G_CALLBACK (async_loaded), self);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAINTABLE]);
}

GdkPaintable *
bz_screenshot_get_paintable (BzScreenshot *self)
{
  g_return_val_if_fail (BZ_IS_SCREENSHOT (self), NULL);
  return self->paintable;
}

void
bz_screenshot_set_focus_x (BzScreenshot *self,
                           double        focus_x)
{
  g_return_if_fail (BZ_IS_SCREENSHOT (self));

  self->focus_x = focus_x;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FOCUS_X]);
}

double
bz_screenshot_get_focus_x (BzScreenshot *self)
{
  g_return_val_if_fail (BZ_IS_SCREENSHOT (self), 0.0);
  return self->focus_x;
}

void
bz_screenshot_set_focus_y (BzScreenshot *self,
                           double        focus_y)
{
  g_return_if_fail (BZ_IS_SCREENSHOT (self));

  self->focus_y = focus_y;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FOCUS_Y]);
}

double
bz_screenshot_get_focus_y (BzScreenshot *self)
{
  g_return_val_if_fail (BZ_IS_SCREENSHOT (self), 0.0);
  return self->focus_y;
}

void
bz_screenshot_set_rounded_corners (BzScreenshot *self,
                                   gboolean      rounded_corners)
{
  g_return_if_fail (BZ_IS_SCREENSHOT (self));

  if (self->rounded_corners == rounded_corners)
    return;

  self->rounded_corners = rounded_corners;
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ROUNDED_CORNERS]);
}

gboolean
bz_screenshot_get_rounded_corners (BzScreenshot *self)
{
  g_return_val_if_fail (BZ_IS_SCREENSHOT (self), TRUE);
  return self->rounded_corners;
}

static void
invalidate_contents (BzScreenshot *self,
                     GdkPaintable *paintable)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
invalidate_size (BzScreenshot *self,
                 GdkPaintable *paintable)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
async_loaded (BzScreenshot   *self,
              GParamSpec     *pspec,
              BzAsyncTexture *texture)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

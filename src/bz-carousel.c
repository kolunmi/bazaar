/* bz-carousel.c
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

#include <adwaita.h>

#include "bz-animation.h"
#include "bz-carousel.h"
#include "bz-marshalers.h"
#include "bz-util.h"

#define RAISE_FACTOR 0.025

struct _BzCarousel
{
  GtkWidget parent_instance;

  GtkEventController *motion;
  double              motion_x;
  double              motion_y;
  GtkEventController *scroll;
  GtkGesture         *drag;
  gboolean            dragging;
  BzAnimation        *animation;

  gboolean            auto_scroll;
  gboolean            allow_long_swipes;
  gboolean            allow_mouse_drag;
  gboolean            allow_scroll_wheel;
  gboolean            allow_raise;
  gboolean            raised;
  GtkSingleSelection *model;

  GPtrArray *mirror;
  GPtrArray *widgets;
};

G_DEFINE_FINAL_TYPE (BzCarousel, bz_carousel, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_AUTO_SCROLL,
  PROP_ALLOW_LONG_SWIPES,
  PROP_ALLOW_MOUSE_DRAG,
  PROP_ALLOW_SCROLL_WHEEL,
  PROP_ALLOW_RAISE,
  PROP_RAISED,
  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_BIND_WIDGET,
  SIGNAL_UNBIND_WIDGET,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

BZ_DEFINE_DATA (
    carousel_widget,
    CarouselWidget,
    {
      GtkWidget *widget;

      /* x/y interpreted as pixel units, width/height interpreted as percentages of
         the widget width/height */
      graphene_rect_t rect;
      graphene_rect_t target;

      gboolean raised;
    },
    BZ_RELEASE_DATA (widget, gtk_widget_unparent))

static void
items_changed (BzCarousel *self,
               guint       position,
               guint       removed,
               guint       added,
               GListModel *model);

static void
model_selected_changed (BzCarousel         *self,
                        GParamSpec         *pspec,
                        GtkSingleSelection *selection);

static void
move_to_idx (BzCarousel *self,
             guint       idx,
             /* damping_ratio <= 0.0 means no animation */
             double damping_ratio);

static void
animate (BzCarousel         *self,
         const char         *key,
         double              value,
         CarouselWidgetData *data);

static void
ensure_viewport (BzCarousel         *self,
                 GtkSingleSelection *model,
                 gboolean            animate);

static void
motion_enter (BzCarousel               *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller);

static void
motion_event (BzCarousel               *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller);

static void
motion_leave (BzCarousel               *self,
              GtkEventControllerMotion *controller);

static void
update_motion (BzCarousel *self,
               gdouble     x,
               gdouble     y);

static gboolean
scroll (BzCarousel               *self,
        gdouble                   dx,
        gdouble                   dy,
        GtkEventControllerScroll *controller);

static void
drag_begin (BzCarousel     *self,
            gdouble         start_x,
            gdouble         start_y,
            GtkGestureDrag *gesture);

static void
drag_end (BzCarousel     *self,
          gdouble         offset_x,
          gdouble         offset_y,
          GtkGestureDrag *gesture);

static void
drag_update (BzCarousel     *self,
             gdouble         offset_x,
             gdouble         offset_y,
             GtkGestureDrag *gesture);

static void
cancel_drag (BzCarousel *self);

static void
bz_carousel_dispose (GObject *object)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  g_clear_pointer (&self->animation, g_object_unref);

  g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_signal_handlers_disconnect_by_func (self->model, model_selected_changed, self);
  items_changed (
      self,
      0,
      g_list_model_get_n_items (G_LIST_MODEL (self->model)),
      0,
      G_LIST_MODEL (self->model));
  g_clear_pointer (&self->model, g_object_unref);

  g_clear_pointer (&self->mirror, g_ptr_array_unref);
  g_clear_pointer (&self->widgets, g_ptr_array_unref);

  G_OBJECT_CLASS (bz_carousel_parent_class)->dispose (object);
}

static void
bz_carousel_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_AUTO_SCROLL:
      g_value_set_boolean (value, bz_carousel_get_auto_scroll (self));
      break;
    case PROP_ALLOW_LONG_SWIPES:
      g_value_set_boolean (value, bz_carousel_get_allow_long_swipes (self));
      break;
    case PROP_ALLOW_MOUSE_DRAG:
      g_value_set_boolean (value, bz_carousel_get_allow_mouse_drag (self));
      break;
    case PROP_ALLOW_SCROLL_WHEEL:
      g_value_set_boolean (value, bz_carousel_get_allow_scroll_wheel (self));
      break;
    case PROP_ALLOW_RAISE:
      g_value_set_boolean (value, bz_carousel_get_allow_raise (self));
      break;
    case PROP_RAISED:
      g_value_set_boolean (value, bz_carousel_get_raised (self));
      break;
    case PROP_MODEL:
      g_value_set_object (value, bz_carousel_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_carousel_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_AUTO_SCROLL:
      bz_carousel_set_auto_scroll (self, g_value_get_boolean (value));
      break;
    case PROP_ALLOW_LONG_SWIPES:
      bz_carousel_set_allow_long_swipes (self, g_value_get_boolean (value));
      break;
    case PROP_ALLOW_MOUSE_DRAG:
      bz_carousel_set_allow_mouse_drag (self, g_value_get_boolean (value));
      break;
    case PROP_ALLOW_SCROLL_WHEEL:
      bz_carousel_set_allow_scroll_wheel (self, g_value_get_boolean (value));
      break;
    case PROP_ALLOW_RAISE:
      bz_carousel_set_allow_raise (self, g_value_get_boolean (value));
      break;
    case PROP_RAISED:
      bz_carousel_set_raised (self, g_value_get_boolean (value));
      break;
    case PROP_MODEL:
      bz_carousel_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_carousel_measure (GtkWidget     *widget,
                     GtkOrientation orientation,
                     int            for_size,
                     int           *minimum,
                     int           *natural,
                     int           *minimum_baseline,
                     int           *natural_baseline)
{
  BzCarousel *self = BZ_CAROUSEL (widget);

  for (guint i = 0; i < self->widgets->len; i++)
    {
      CarouselWidgetData *child                = NULL;
      int                 tmp_minimum          = 0;
      int                 tmp_natural          = 0;
      int                 tmp_minimum_baseline = 0;
      int                 tmp_natural_baseline = 0;

      child = g_ptr_array_index (self->widgets, i);

      gtk_widget_measure (
          child->widget,
          orientation,
          for_size,
          &tmp_minimum,
          &tmp_natural,
          &tmp_minimum_baseline,
          &tmp_natural_baseline);

      if (tmp_minimum > 0 && tmp_minimum < *minimum)
        *minimum = tmp_minimum;
      if (tmp_natural > 0 && tmp_natural > *natural)
        *natural = tmp_natural;
      if (tmp_minimum_baseline > 0 && tmp_minimum_baseline < *minimum_baseline)
        *minimum_baseline = tmp_minimum_baseline;
      if (tmp_natural_baseline > 0 && tmp_natural_baseline > *natural_baseline)
        *natural_baseline = tmp_natural_baseline;
    }
}

static void
bz_carousel_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  BzCarousel *self = BZ_CAROUSEL (widget);

  ensure_viewport (self, self->model, FALSE);

  for (guint i = 0; i < self->widgets->len; i++)
    {
      CarouselWidgetData *child          = NULL;
      g_autoptr (GskTransform) transform = NULL;

      child     = g_ptr_array_index (self->widgets, i);
      transform = gsk_transform_translate (
          gsk_transform_new (),
          &child->rect.origin);

      gtk_widget_allocate (
          child->widget,
          child->rect.size.width,
          child->rect.size.height,
          baseline,
          g_steal_pointer (&transform));
    }
}

static void
bz_carousel_class_init (BzCarouselClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_carousel_set_property;
  object_class->get_property = bz_carousel_get_property;
  object_class->dispose      = bz_carousel_dispose;

  props[PROP_AUTO_SCROLL] =
      g_param_spec_boolean (
          "auto-scroll",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_LONG_SWIPES] =
      g_param_spec_boolean (
          "allow-long-swipes",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_MOUSE_DRAG] =
      g_param_spec_boolean (
          "allow-mouse-drag",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_SCROLL_WHEEL] =
      g_param_spec_boolean (
          "allow-scroll-wheel",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALLOW_RAISE] =
      g_param_spec_boolean (
          "allow-raise",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RAISED] =
      g_param_spec_boolean (
          "raised",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          GTK_TYPE_SINGLE_SELECTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_BIND_WIDGET] =
      g_signal_new (
          "bind-widget",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          bz_marshal_VOID__OBJECT_OBJECT,
          G_TYPE_NONE,
          1,
          GTK_TYPE_WIDGET);
  g_signal_set_va_marshaller (
      signals[SIGNAL_BIND_WIDGET],
      G_TYPE_FROM_CLASS (klass),
      bz_marshal_VOID__OBJECT_OBJECTv);

  signals[SIGNAL_UNBIND_WIDGET] =
      g_signal_new (
          "unbind-widget",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          bz_marshal_VOID__OBJECT_OBJECT,
          G_TYPE_NONE,
          1,
          GTK_TYPE_WIDGET);
  g_signal_set_va_marshaller (
      signals[SIGNAL_UNBIND_WIDGET],
      G_TYPE_FROM_CLASS (klass),
      bz_marshal_VOID__OBJECT_OBJECTv);

  widget_class->measure       = bz_carousel_measure;
  widget_class->size_allocate = bz_carousel_size_allocate;
}

static void
bz_carousel_init (BzCarousel *self)
{
  self->animation = bz_animation_new (GTK_WIDGET (self));

  self->mirror = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_object_unref);
  self->widgets = g_ptr_array_new_with_free_func (
      carousel_widget_data_unref);

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

  self->motion = gtk_event_controller_motion_new ();
  g_signal_connect_swapped (self->motion, "enter", G_CALLBACK (motion_enter), self);
  g_signal_connect_swapped (self->motion, "motion", G_CALLBACK (motion_event), self);
  g_signal_connect_swapped (self->motion, "leave", G_CALLBACK (motion_leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->motion);

  self->scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL);
  g_signal_connect_swapped (self->scroll, "scroll", G_CALLBACK (scroll), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->scroll);

  self->drag = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->drag), GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (self->drag, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect_swapped (self->drag, "drag-end", G_CALLBACK (drag_end), self);
  g_signal_connect_swapped (self->drag, "drag-update", G_CALLBACK (drag_update), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->drag));
}

GtkWidget *
bz_carousel_new (void)
{
  return g_object_new (BZ_TYPE_CAROUSEL, NULL);
}

gboolean
bz_carousel_get_auto_scroll (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->auto_scroll;
}

gboolean
bz_carousel_get_allow_long_swipes (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->allow_long_swipes;
}

gboolean
bz_carousel_get_allow_mouse_drag (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->allow_mouse_drag;
}

gboolean
bz_carousel_get_allow_scroll_wheel (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->allow_scroll_wheel;
}

gboolean
bz_carousel_get_allow_raise (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->allow_raise;
}

gboolean
bz_carousel_get_raised (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), FALSE);
  return self->raised;
}

GtkSingleSelection *
bz_carousel_get_model (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), NULL);
  return self->model;
}

void
bz_carousel_set_auto_scroll (BzCarousel *self,
                             gboolean    auto_scroll)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!auto_scroll == !!self->auto_scroll)
    return;

  self->auto_scroll = auto_scroll;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AUTO_SCROLL]);
}

void
bz_carousel_set_allow_long_swipes (BzCarousel *self,
                                   gboolean    allow_long_swipes)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!allow_long_swipes == !!self->allow_long_swipes)
    return;

  self->allow_long_swipes = allow_long_swipes;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_LONG_SWIPES]);
}

void
bz_carousel_set_allow_mouse_drag (BzCarousel *self,
                                  gboolean    allow_mouse_drag)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!allow_mouse_drag == !!self->allow_mouse_drag)
    return;

  self->allow_mouse_drag = allow_mouse_drag;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_MOUSE_DRAG]);
}

void
bz_carousel_set_allow_scroll_wheel (BzCarousel *self,
                                    gboolean    allow_scroll_wheel)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!allow_scroll_wheel == !!self->allow_scroll_wheel)
    return;

  self->allow_scroll_wheel = allow_scroll_wheel;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_SCROLL_WHEEL]);
}

void
bz_carousel_set_allow_raise (BzCarousel *self,
                             gboolean    allow_raise)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!allow_raise == !!self->allow_raise)
    return;

  self->allow_raise = allow_raise;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALLOW_RAISE]);
}

void
bz_carousel_set_raised (BzCarousel *self,
                        gboolean    raised)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));

  if (!!raised == !!self->raised)
    return;

  self->raised = raised;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RAISED]);
}

void
bz_carousel_set_model (BzCarousel         *self,
                       GtkSingleSelection *model)
{
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (model == NULL || GTK_IS_SINGLE_SELECTION (model));

  if (model == self->model)
    return;

  if (self->model != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
      g_signal_handlers_disconnect_by_func (self->model, model_selected_changed, self);

      items_changed (
          self,
          0,
          g_list_model_get_n_items (G_LIST_MODEL (self->model)),
          0,
          G_LIST_MODEL (self->model));
    }
  g_clear_pointer (&self->model, g_object_unref);

  if (model != NULL)
    {
      self->model = g_object_ref (model);
      items_changed (
          self,
          0,
          0,
          g_list_model_get_n_items (G_LIST_MODEL (model)),
          G_LIST_MODEL (model));

      g_signal_connect_swapped (
          model,
          "items-changed",
          G_CALLBACK (items_changed),
          self);
      g_signal_connect_swapped (
          model,
          "notify::selected",
          G_CALLBACK (model_selected_changed),
          self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

static void
items_changed (BzCarousel *self,
               guint       position,
               guint       removed,
               guint       added,
               GListModel *model)
{
  for (guint i = 0; i < removed; i++)
    {
      GObject            *object = NULL;
      CarouselWidgetData *child  = NULL;

      object = g_ptr_array_index (self->mirror, position + i);
      child  = g_ptr_array_index (self->widgets, position + i);

      g_signal_emit (self, signals[SIGNAL_UNBIND_WIDGET], 0, child->widget, object);
    }
  if (removed > 0)
    {
      g_ptr_array_remove_range (self->mirror, position, removed);
      g_ptr_array_remove_range (self->widgets, position, removed);
    }

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (GObject) object = NULL;
      GtkWidget          *child  = NULL;
      CarouselWidgetData *data   = NULL;

      object = g_list_model_get_item (model, position + i);
      child  = adw_bin_new ();

      if (position + i == 0)
        gtk_widget_set_parent (child, GTK_WIDGET (self));
      else
        {
          CarouselWidgetData *prev = NULL;

          prev = g_ptr_array_index (self->widgets, position + i - 1);
          gtk_widget_insert_after (child, GTK_WIDGET (self), prev->widget);
        }
      g_signal_emit (self, signals[SIGNAL_BIND_WIDGET], 0, ADW_BIN (child), object);

      data         = carousel_widget_data_new ();
      data->widget = child;

      g_ptr_array_insert (self->mirror, position + i, g_object_ref (object));
      g_ptr_array_insert (self->widgets, position + i, data);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
model_selected_changed (BzCarousel         *self,
                        GParamSpec         *pspec,
                        GtkSingleSelection *selection)
{
  guint idx = 0;

  idx = gtk_single_selection_get_selected (selection);
  if (idx != G_MAXUINT)
    move_to_idx (self, idx, 1.0);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
move_to_idx (BzCarousel *self,
             guint       idx,
             /* damping_ratio <= 0.0 means no animation */
             double damping_ratio)
{
  int width  = 0;
  int height = 0;
  int offset = 0;

  width  = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));
  offset = width / 2;
  if (self->dragging)
    {
      gboolean result = FALSE;
      double   drag_x = 0;
      double   drag_y = 0;

      result = gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (self->drag), &drag_x, &drag_y);
      if (result)
        offset += drag_x;
    }

  for (guint i = 0; i <= idx; i++)
    {
      CarouselWidgetData *child       = NULL;
      int                 hminimum    = 0;
      int                 hnatural    = 0;
      int                 unused      = 0;
      int                 child_width = 0;

      child = g_ptr_array_index (self->widgets, i);

      gtk_widget_measure (
          child->widget,
          GTK_ORIENTATION_HORIZONTAL,
          height,
          &hminimum,
          &hnatural,
          &unused,
          &unused);
      child_width = CLAMP (hnatural, hminimum, width);

      if (i == idx)
        offset -= child_width / 2;
      else
        offset -= child_width;
    }

  for (guint i = 0; i < self->widgets->len; i++)
    {
      CarouselWidgetData *child        = NULL;
      int                 hminimum     = 0;
      int                 hnatural     = 0;
      int                 unused       = 0;
      int                 rect_width   = 0;
      int                 child_width  = 0;
      int                 child_height = 0;
      int                 child_x      = 0;
      int                 child_y      = 0;
      graphene_rect_t     target       = { 0 };

      child = g_ptr_array_index (self->widgets, i);

      gtk_widget_measure (
          child->widget,
          GTK_ORIENTATION_HORIZONTAL,
          height,
          &hminimum,
          &hnatural,
          &unused,
          &unused);
      rect_width = CLAMP (hnatural, hminimum, width);

      if (child->raised)
        {
          child_width  = rect_width;
          child_height = height;
          child_x      = offset;
          child_y      = 0;
        }
      else
        {
          child_height = round ((double) height * (1.0 - RAISE_FACTOR));

          gtk_widget_measure (
              child->widget,
              GTK_ORIENTATION_HORIZONTAL,
              child_height,
              &hminimum,
              &hnatural,
              &unused,
              &unused);
          child_width = CLAMP (hnatural, hminimum, width);

          child_x = offset + round ((double) (rect_width - child_width) * 0.5);
          child_y = round ((double) height * (0.5 * RAISE_FACTOR));
        }

      target = GRAPHENE_RECT_INIT (child_x, child_y, child_width, child_height);
      if (graphene_rect_equal (&target, &child->target))
        child->target = target;
      else if (damping_ratio < 0.0 ||
               graphene_rect_equal (graphene_rect_zero (), &child->rect))
        {
          char buf[64] = { 0 };

          child->rect   = target;
          child->target = target;

          g_snprintf (buf, sizeof (buf), "x%p", child);
          bz_animation_cancel (self->animation, buf);

          g_snprintf (buf, sizeof (buf), "y%p", child);
          bz_animation_cancel (self->animation, buf);

          g_snprintf (buf, sizeof (buf), "w%p", child);
          bz_animation_cancel (self->animation, buf);

          g_snprintf (buf, sizeof (buf), "h%p", child);
          bz_animation_cancel (self->animation, buf);
        }
      else
        {
          char buf[64] = { 0 };

#define MASS      1.0
#define STIFFNESS 0.16

          /* pointer is to ensure a unique identifier so as not to overwrite any
             other child's key */
          g_snprintf (buf, sizeof (buf), "x%p", child);
          bz_animation_add_spring (
              self->animation, buf,
              child->rect.origin.x, target.origin.x,
              damping_ratio, MASS, STIFFNESS,
              (BzAnimationCallback) animate,
              carousel_widget_data_ref (child),
              carousel_widget_data_unref);

          g_snprintf (buf, sizeof (buf), "y%p", child);
          bz_animation_add_spring (
              self->animation, buf,
              child->rect.origin.y, target.origin.y,
              damping_ratio, MASS, STIFFNESS,
              (BzAnimationCallback) animate,
              carousel_widget_data_ref (child),
              carousel_widget_data_unref);

          g_snprintf (buf, sizeof (buf), "w%p", child);
          bz_animation_add_spring (
              self->animation, buf,
              child->rect.size.width, target.size.width,
              damping_ratio, MASS, STIFFNESS,
              (BzAnimationCallback) animate,
              carousel_widget_data_ref (child),
              carousel_widget_data_unref);

          g_snprintf (buf, sizeof (buf), "h%p", child);
          bz_animation_add_spring (
              self->animation, buf,
              child->rect.size.height, target.size.height,
              damping_ratio, MASS, STIFFNESS,
              (BzAnimationCallback) animate,
              carousel_widget_data_ref (child),
              carousel_widget_data_unref);

#undef STIFFNESS
#undef MASS

          child->target = target;
        }

      offset += rect_width;
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
animate (BzCarousel         *self,
         const char         *key,
         double              value,
         CarouselWidgetData *data)
{
  switch (*key)
    {
    case 'x':
      data->rect.origin.x = value;
      break;
    case 'y':
      data->rect.origin.y = value;
      break;
    case 'w':
      data->rect.size.width = value;
      break;
    case 'h':
      data->rect.size.height = value;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
ensure_viewport (BzCarousel         *self,
                 GtkSingleSelection *model,
                 gboolean            animate)
{
  guint n_items = 0;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  if (n_items > 0)
    {
      guint selected = 0;

      selected = gtk_single_selection_get_selected (model);
      if (selected == G_MAXUINT)
        {
          gtk_single_selection_set_selected (model, 0);
          move_to_idx (self, 0, animate ? 1.0 : -1.0);
        }
      else
        move_to_idx (self, selected, animate ? 1.0 : -1.0);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
motion_enter (BzCarousel               *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller)
{
  self->motion_x = x;
  self->motion_y = y;
  update_motion (self, x, y);
}

static void
motion_event (BzCarousel               *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller)
{
  self->motion_x = x;
  self->motion_y = y;
  update_motion (self, x, y);
}

static void
motion_leave (BzCarousel               *self,
              GtkEventControllerMotion *controller)
{
  if (self->dragging)
    return;
  self->motion_x = -1.0;
  self->motion_y = -1.0;
  update_motion (self, -1.0, -1.0);
}

static void
update_motion (BzCarousel *self,
               gdouble     x,
               gdouble     y)
{
  graphene_point_t point  = { 0 };
  gboolean         ensure = FALSE;

  point = GRAPHENE_POINT_INIT (x, y);

  for (guint i = 0; i < self->widgets->len; i++)
    {
      CarouselWidgetData *child     = NULL;
      gboolean            contained = FALSE;

      child     = g_ptr_array_index (self->widgets, i);
      contained = graphene_rect_contains_point (&child->target, &point);
      if (!!contained != !!child->raised)
        {
          child->raised = contained;
          ensure        = TRUE;
        }
    }

  if (self->dragging)
    return;
  if (ensure)
    ensure_viewport (self, self->model, TRUE);
}

static gboolean
scroll (BzCarousel               *self,
        gdouble                   dx,
        gdouble                   dy,
        GtkEventControllerScroll *controller)
{
  guint n_items      = 0;
  guint selected     = 0;
  guint new_selected = 0;

  if (self->model == NULL)
    return FALSE;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  if (n_items == 0)
    return FALSE;

  selected = gtk_single_selection_get_selected (self->model);

  if (dx > 0)
    new_selected = MIN (selected + 1, n_items - 1);
  else
    {
      if (selected == 0)
        new_selected = 0;
      else
        new_selected = selected - 1;
    }
  gtk_single_selection_set_selected (self->model, new_selected);

  return TRUE;
}

static void
drag_begin (BzCarousel     *self,
            gdouble         start_x,
            gdouble         start_y,
            GtkGestureDrag *gesture)
{
  self->dragging = TRUE;
  if (self->model == NULL)
    return;

  ensure_viewport (self, self->model, TRUE);
}

static void
drag_end (BzCarousel     *self,
          gdouble         offset_x,
          gdouble         offset_y,
          GtkGestureDrag *gesture)
{
  self->dragging = FALSE;
  cancel_drag (self);

  if (offset_x < -3 ||
      offset_x > 3 ||
      offset_y < -3 ||
      offset_y > 3)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
drag_update (BzCarousel     *self,
             gdouble         offset_x,
             gdouble         offset_y,
             GtkGestureDrag *gesture)
{
  ensure_viewport (self, self->model, FALSE);
}

static void
cancel_drag (BzCarousel *self)
{
  guint  selected     = 0;
  double width        = 0.0;
  guint  new_selected = G_MAXUINT;
  int    min_distance = G_MAXINT;

  if (self->model == NULL)
    return;

  selected = gtk_single_selection_get_selected (self->model);

  width = gtk_widget_get_width (GTK_WIDGET (self));
  for (guint i = 0; i < self->widgets->len; i++)
    {
      CarouselWidgetData *child                = NULL;
      int                 left_distance        = 0;
      int                 right_distance       = 0;
      int                 distance_from_center = 0;

      child = g_ptr_array_index (self->widgets, i);

      if (child->rect.origin.x > width / 2.0)
        left_distance = child->rect.origin.x - width / 2.0;
      else
        left_distance = width / 2.0 - child->rect.origin.x;

      if ((child->rect.origin.x + child->rect.size.width) > width / 2.0)
        right_distance = (child->rect.origin.x + child->rect.size.width) - width / 2.0;
      else
        right_distance = width / 2.0 - (child->rect.origin.x + child->rect.size.width);

      distance_from_center = MIN (left_distance, right_distance);
      if (distance_from_center < min_distance)
        {
          new_selected = i;
          min_distance = distance_from_center;
        }
    }

  if (new_selected == G_MAXUINT ||
      new_selected == selected)
    ensure_viewport (self, self->model, TRUE);
  else
    gtk_single_selection_set_selected (self->model, new_selected);
}

/* End of bz-carousel.c */

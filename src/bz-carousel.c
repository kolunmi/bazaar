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

struct _BzCarousel
{
  GtkWidget parent_instance;

  BzAnimation *animation;

  gboolean            auto_scroll;
  gboolean            allow_long_swipes;
  gboolean            allow_mouse_drag;
  gboolean            allow_scroll_wheel;
  gboolean            allow_raise;
  gboolean            raised;
  GtkSingleSelection *model;

  GPtrArray *mirror;
  GPtrArray *carousel_widgets;

  /* x interpreted as progress between child widths, y/width/height interpreted
    as percentages of the widget height/width/height */
  graphene_rect_t viewport;
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
             gboolean    raised,
             double      damping_ratio);

static void
animate (GtkWidget  *widget,
         const char *key,
         double      value,
         BzCarousel *self);

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
  g_clear_pointer (&self->carousel_widgets, g_ptr_array_unref);

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

  for (guint i = 0; i < self->carousel_widgets->len; i++)
    {
      GtkWidget *carousel_widget      = NULL;
      int        tmp_minimum          = 0;
      int        tmp_natural          = 0;
      int        tmp_minimum_baseline = 0;
      int        tmp_natural_baseline = 0;

      carousel_widget = g_ptr_array_index (self->carousel_widgets, i);

      gtk_widget_measure (
          carousel_widget,
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
  BzCarousel *self                        = BZ_CAROUSEL (widget);
  int         hoffset                     = 0;
  g_autoptr (GskTransform) base_transform = NULL;

  if (graphene_rect_equal (
          &self->viewport,
          graphene_rect_zero ()))
    self->viewport = GRAPHENE_RECT_INIT (
        0.0, 0.0, (double) width, (double) height);

  for (guint i = 0; i < self->carousel_widgets->len;)
    {
      GtkWidget *child                   = NULL;
      int        hminimum                = 0;
      int        hnatural                = 0;
      int        vminimum                = 0;
      int        vnatural                = 0;
      int        unused                  = 0;
      int        child_width             = 0;
      int        child_height            = 0;
      g_autoptr (GskTransform) transform = NULL;

      child = g_ptr_array_index (self->carousel_widgets, i);

      gtk_widget_measure (
          child,
          GTK_ORIENTATION_HORIZONTAL,
          height,
          &hminimum,
          &hnatural,
          &unused,
          &unused);
      gtk_widget_measure (
          child,
          GTK_ORIENTATION_VERTICAL,
          width,
          &vminimum,
          &vnatural,
          &unused,
          &unused);

      child_width  = CLAMP (hnatural, hminimum, width);
      child_height = CLAMP (vnatural, vminimum, height);

      if (base_transform != NULL)
        {
          transform = gsk_transform_translate (
              gsk_transform_ref (base_transform),
              &GRAPHENE_POINT_INIT (hoffset, 0.0));

          gtk_widget_allocate (
              child,
              child_width,
              height,
              baseline,
              g_steal_pointer (&transform));

          i++;
          hoffset += child_width;
        }
      else if (self->viewport.origin.x <= 0.0 ||
               ((double) i < self->viewport.origin.x &&
                (double) i > self->viewport.origin.x - 1.0))
        {
          base_transform = gsk_transform_translate (
              gsk_transform_new (),
              &GRAPHENE_POINT_INIT (
                  -hoffset - (double) child_width * (self->viewport.origin.x - (double) i),
                  -self->viewport.origin.y * (double) height));
          i       = 0;
          hoffset = 0;
        }
      else
        {
          i++;
          hoffset += child_width;
        }
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
  self->carousel_widgets = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gtk_widget_unparent);

  self->viewport = (graphene_rect_t) { 0 };

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
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
      GObject   *object = NULL;
      GtkWidget *child  = NULL;

      object = g_ptr_array_index (self->mirror, position + i);
      child  = g_ptr_array_index (self->carousel_widgets, position + i);

      g_signal_emit (self, signals[SIGNAL_UNBIND_WIDGET], 0, child, object);
    }
  if (removed > 0)
    {
      g_ptr_array_remove_range (self->mirror, position, removed);
      g_ptr_array_remove_range (self->carousel_widgets, position, removed);
    }

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (GObject) object = NULL;
      GtkWidget *child           = NULL;

      object = g_list_model_get_item (model, position + i);
      child  = adw_bin_new ();

      gtk_widget_set_parent (child, GTK_WIDGET (self));
      g_signal_emit (self, signals[SIGNAL_BIND_WIDGET], 0, ADW_BIN (child), object);

      g_ptr_array_insert (self->mirror, position + i, g_object_ref (object));
      g_ptr_array_insert (self->carousel_widgets, position + i, child);
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
    move_to_idx (self, idx, FALSE, 1.2);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
move_to_idx (BzCarousel *self,
             guint       idx,
             gboolean    raised,
             double      damping_ratio)
{
  graphene_rect_t target = { 0 };

  target = GRAPHENE_RECT_INIT (
      (double) idx - 0.5, 0.0, 1.5, 1.0);
  if (raised)
    graphene_rect_inset (&target, -0.1, -0.1);

#define MASS      1.0
#define STIFFNESS 0.08

  bz_animation_add_spring (
      self->animation, "x",
      self->viewport.origin.x, target.origin.x,
      damping_ratio, MASS, STIFFNESS,
      (BzAnimationCallback) animate,
      self, NULL);
  bz_animation_add_spring (
      self->animation, "y",
      self->viewport.origin.y, target.origin.y,
      damping_ratio, MASS, STIFFNESS,
      (BzAnimationCallback) animate,
      self, NULL);
  bz_animation_add_spring (
      self->animation, "w",
      self->viewport.size.width, target.size.width,
      damping_ratio, MASS, STIFFNESS,
      (BzAnimationCallback) animate,
      self, NULL);
  bz_animation_add_spring (
      self->animation, "h",
      self->viewport.size.height, target.size.height,
      damping_ratio, MASS, STIFFNESS,
      (BzAnimationCallback) animate,
      self, NULL);

#undef STIFFNESS
#undef MASS
}

static void
animate (GtkWidget  *widget,
         const char *key,
         double      value,
         BzCarousel *self)
{
  switch (*key)
    {
    case 'x':
      self->viewport.origin.x = value;
      break;
    case 'y':
      self->viewport.origin.y = value;
      break;
    case 'w':
      self->viewport.size.width = value;
      break;
    case 'h':
      self->viewport.size.height = value;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

/* End of bz-carousel.c */

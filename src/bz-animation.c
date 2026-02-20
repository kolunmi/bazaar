/* bz-animation.c
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

/* This animation implementation was originally from my other project,
   libpastry: https://github.com/kolunmi/libpastry */

/**
 * BzAnimation:
 *
 * Manages animations for a widget. Individual value animations are tracked in a
 * hash map with string keys, allowing them to be easily restarted or replaced.
 */

#define G_LOG_DOMAIN "BZ::ANIMATION"

#define DELTA   0.001
#define EPSILON 0.00001

#include "config.h"

#include "bz-animation.h"

enum
{
  PROP_0,

  PROP_WIDGET,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

struct _BzAnimation
{
  GObject parent_instance;

  GtkWidget *widget;
  GWeakRef   wr;

  guint       tag;
  GHashTable *data;
};
G_DEFINE_FINAL_TYPE (BzAnimation, bz_animation, G_TYPE_OBJECT)

typedef struct
{
  double              from;
  double              to;
  double              damping_ratio;
  double              mass;
  double              stiffness;
  gboolean            clamp;
  BzAnimationCallback cb;
  gpointer            user_data;
  GDestroyNotify      destroy_data;
  double              est_duration;
  GTimer             *timer;
  double              velocity;
} SpringData;

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         GWeakRef      *wr);

/* Copied with modifications from libadwaita */
static double
oscillate (SpringData *data,
           double      time,
           double     *velocity);

/* Copied with modifications from libadwaita */
static double
get_first_zero (SpringData *data);

/* Copied with modifications from libadwaita */
static double
calculate_duration (SpringData *data);

static void
destroy_spring_data (gpointer ptr);

static void
destroy_wr (gpointer ptr);

static gboolean
should_animate (GtkWidget *widget);

static void
dispose (GObject *object)
{
  BzAnimation *self            = BZ_ANIMATION (object);
  g_autoptr (GtkWidget) widget = NULL;

  widget = g_weak_ref_get (&self->wr);
  if (widget != NULL)
    {
      gtk_widget_remove_tick_callback (widget, self->tag);
      self->tag = 0;
    }
  g_clear_object (&widget);
  g_weak_ref_clear (&self->wr);

  g_clear_object (&self->widget);
  g_clear_pointer (&self->data, g_hash_table_unref);

  G_OBJECT_CLASS (bz_animation_parent_class)->dispose (object);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  BzAnimation *self = BZ_ANIMATION (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_take_object (value, bz_animation_dup_widget (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  BzAnimation *self = BZ_ANIMATION (object);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_clear_object (&self->widget);
      self->widget = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
constructed (GObject *object)
{
  BzAnimation *self = BZ_ANIMATION (object);

  if (GTK_IS_WIDGET (self->widget))
    {
      GWeakRef *wr = NULL;

      wr = g_new0 (typeof (*wr), 1);
      g_weak_ref_init (wr, self);

      self->tag = gtk_widget_add_tick_callback (
          self->widget,
          (GtkTickCallback) tick_cb,
          wr, destroy_wr);
    }
  g_weak_ref_init (&self->wr, self->widget);
  g_clear_object (&self->widget);
}

static void
bz_animation_class_init (BzAnimationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = constructed;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose      = dispose;

  /**
   * BzAnimation:widget:
   *
   * The widget on which this animation is attached.
   */
  props[PROP_WIDGET] =
      g_param_spec_object (
          "widget",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_animation_init (BzAnimation *self)
{
  g_weak_ref_init (&self->wr, NULL);
  self->data = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, destroy_spring_data);
}

/**
 * bz_animation_new:
 * @widget: The widget onto which to attach the tick callback
 *
 * Creates a new `BzAnimation` object.
 *
 * Returns: The newly created `BzAnimation` object.
 */
BzAnimation *
bz_animation_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  return g_object_new (
      BZ_TYPE_ANIMATION,
      "widget", widget,
      NULL);
}

/**
 * bz_animation_dup_widget:
 * @self: a `BzAnimation`
 *
 * Gets the widget on which @self is attached.
 *
 * Returns: (nullable) (transfer full): the widget for @self
 */
GtkWidget *
bz_animation_dup_widget (BzAnimation *self)
{
  g_return_val_if_fail (BZ_IS_ANIMATION (self), NULL);
  return g_weak_ref_get (&self->wr);
}

/**
 * bz_animation_add_spring:
 * @self: a `BzAnimation`
 * @key: a string ID to replace
 * @from: the start value
 * @to: the end value
 * @damping_ratio: the damping ratio
 * @mass: the mass
 * @stiffness: the stiffness
 * @cb: a tick callback
 * @user_data: the user data pointer to be passed to @cb
 * @destroy_data: the destruction function for @user_data
 *
 * Adds a one shot spring animation to @self. If @key is already running in
 * @self, then the old animation is replaced, maintaining the current velocity.
 */
void
bz_animation_add_spring (BzAnimation        *self,
                         const char         *key,
                         double              from,
                         double              to,
                         double              damping_ratio,
                         double              mass,
                         double              stiffness,
                         BzAnimationCallback cb,
                         gpointer            user_data,
                         GDestroyNotify      destroy_data)
{
  g_autoptr (GtkWidget) widget = NULL;

  g_return_if_fail (BZ_IS_ANIMATION (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (cb != NULL);

  widget = g_weak_ref_get (&self->wr);
  if (widget != NULL)
    {
      if (should_animate (widget))
        {
          SpringData *data = NULL;

          /* reuse old data if possible */
          data = g_hash_table_lookup (self->data, key);
          if (data != NULL)
            {
              if (data->user_data != NULL &&
                  data->destroy_data != NULL)
                /* we are going to overwrite this */
                data->destroy_data (data->user_data);

              g_clear_pointer (&data->timer, g_timer_destroy);

              /* old velocity is retained */
            }
          else
            {
              data = g_new0 (typeof (*data), 1);
              g_hash_table_replace (self->data, g_strdup (key), data);
            }

          data->from          = from;
          data->to            = to;
          data->damping_ratio = damping_ratio;
          data->mass          = mass;
          data->stiffness     = stiffness;
          data->cb            = cb;
          data->user_data     = user_data;
          data->destroy_data  = destroy_data;

          /* We'll fill this in on the first iteration */
          data->timer = NULL;

          data->est_duration = calculate_duration (data);

          cb (widget, key, from, user_data);
        }
      else
        /* If we shouldn't animate, just invoke the callback at the final
           value */
        {
          cb (widget, key, to, user_data);
          if (user_data != NULL &&
              destroy_data != NULL)
            destroy_data (user_data);
        }
    }
  else if (user_data != NULL &&
           destroy_data != NULL)
    destroy_data (user_data);
}

/**
 * bz_animation_cancel:
 * @self: a `BzAnimation`
 * @key: a string ID to remove
 *
 * If @key exists on @self, cancel the associated animation.
 */
void
bz_animation_cancel (BzAnimation *self,
                     const char  *key)
{
  g_autoptr (GtkWidget) widget = NULL;

  g_return_if_fail (BZ_IS_ANIMATION (self));
  g_return_if_fail (key != NULL);

  if (!g_hash_table_contains (self->data, key))
    return;

  widget = g_weak_ref_get (&self->wr);
  if (widget != NULL)
    {
      SpringData *data = NULL;

      data = g_hash_table_lookup (self->data, key);
      data->cb (widget, key, data->to, data->user_data);
    }

  g_hash_table_remove (self->data, key);
}

/**
 * bz_animation_cancel_all:
 * @self: a `BzAnimation`
 *
 * Cancel all animations on @self.
 */
void
bz_animation_cancel_all (BzAnimation *self)
{
  g_autoptr (GtkWidget) widget = NULL;

  g_return_if_fail (BZ_IS_ANIMATION (self));

  widget = g_weak_ref_get (&self->wr);
  if (widget != NULL)
    {
      GHashTableIter iter = { 0 };

      g_hash_table_iter_init (&iter, self->data);

      for (;;)
        {
          char       *key  = NULL;
          SpringData *data = NULL;

          if (!g_hash_table_iter_next (
                  &iter,
                  (gpointer *) &key,
                  (gpointer *) &data))
            break;

          data->cb (widget, key, data->to, data->user_data);
          g_hash_table_iter_remove (&iter);
        }
    }
  else
    g_hash_table_remove_all (self->data);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         GWeakRef      *wr)
{
  g_autoptr (BzAnimation) self = NULL;
  gboolean       cancel        = FALSE;
  GHashTableIter iter          = { 0 };

  self = g_weak_ref_get (wr);
  if (self == NULL)
    return G_SOURCE_REMOVE;

  cancel = !should_animate (widget);

  g_hash_table_iter_init (&iter, self->data);
  for (;;)
    {
      char       *key      = NULL;
      SpringData *data     = NULL;
      double      value    = 0.0;
      gboolean    finished = FALSE;

      if (!g_hash_table_iter_next (
              &iter,
              (gpointer *) &key,
              (gpointer *) &data))
        break;

      if (cancel)
        finished = TRUE;
      else
        {
          double elapsed = 0.0;

          if (data->timer == NULL)
            {
              data->timer = g_timer_new ();
              value       = data->from;
            }
          else
            {
              elapsed = g_timer_elapsed (data->timer, NULL);
              value   = oscillate (data, elapsed, &data->velocity);
            }

          finished = elapsed > data->est_duration ||
                     (data->damping_ratio >= 1.0 &&
                      (G_APPROX_VALUE (value, data->to, EPSILON) ||
                       (data->from > data->to && value < data->to) ||
                       (data->from < data->to && value > data->to)));
        }
      if (finished)
        value = data->to;

      data->cb (widget, key, value, data->user_data);

      if (finished)
        g_hash_table_iter_remove (&iter);
    }

  return G_SOURCE_CONTINUE;
}

/* COPIED FROM LIBADWAITA */

/* Based on RBBSpringAnimation from RBBAnimation, MIT license.
 * https://github.com/robb/RBBAnimation/blob/master/RBBAnimation/RBBSpringAnimation.m
 *
 * @offset: Starting value of the spring simulation. Use -1 for regular animations,
 * as the formulas are tailored to rest at 0 and the resulting evolution between
 * -1 and 0 will be lerped to the desired range afterwards. Otherwise use 0 for in-place
 * animations which already start at equilibrium
 */
static double
oscillate (SpringData *data,
           double      time,
           double     *velocity)
{
  double t        = time * 100.0; // ?
  double b        = data->damping_ratio;
  double m        = data->mass;
  double k        = data->stiffness;
  double v0       = 0.0;
  double beta     = 0.0;
  double omega0   = 0.0;
  double x0       = 0.0;
  double envelope = 0.0;

  beta     = b / (2 * m);
  omega0   = sqrt (k / m);
  x0       = data->from - data->to;
  envelope = exp (-beta * t);

  /*
   * Solutions of the form C1*e^(lambda1*x) + C2*e^(lambda2*x)
   * for the differential equation m*ẍ+b*ẋ+kx = 0
   */

  /* Critically damped */
  /* DBL_EPSILON is too small for this specific comparison, so we use
   * FLT_EPSILON even though it's doubles */
  if (G_APPROX_VALUE (beta, omega0, FLT_EPSILON))
    {
      if (velocity != NULL)
        *velocity = envelope *
                    (-beta * t * v0 -
                     beta * beta * t * x0 +
                     v0);

      return data->to + envelope *
                            (x0 + (beta * x0 + v0) * t);
    }

  /* Underdamped */
  if (beta < omega0)
    {
      double omega1 = 0.0;

      omega1 = sqrt ((omega0 * omega0) - (beta * beta));

      if (velocity != NULL)
        *velocity = envelope *
                    (v0 * cos (omega1 * t) -
                     (x0 * omega1 +
                      (beta * beta * x0 + beta * v0) /
                          (omega1)) *
                         sin (omega1 * t));

      return data->to + envelope *
                            (x0 * cos (omega1 * t) +
                             ((beta * x0 + v0) /
                              omega1) *
                                 sin (omega1 * t));
    }

  /* Overdamped */
  if (beta > omega0)
    {
      double omega2 = 0.0;

      omega2 = sqrt ((beta * beta) - (omega0 * omega0));

      if (velocity != NULL)
        *velocity = envelope *
                    (v0 * coshl (omega2 * t) +
                     (omega2 * x0 -
                      (beta * beta * x0 + beta * v0) /
                          omega2) *
                         sinhl (omega2 * t));

      return data->to + envelope *
                            (x0 * coshl (omega2 * t) +
                             ((beta * x0 + v0) / omega2) *
                                 sinhl (omega2 * t));
    }

  g_assert_not_reached ();
}

static double
get_first_zero (SpringData *data)
{
  /* The first frame is not that important and we avoid finding the trivial 0
   * for in-place animations. */
  double i = 0.0;
  double y = 0.0;

  i = 0.001;
  y = oscillate (data, i, NULL);

  while ((data->to - data->from > DBL_EPSILON && data->to - y > EPSILON) ||
         (data->from - data->to > DBL_EPSILON && y - data->to > EPSILON))
    {
      if (i > 2.0)
        return 0.0;

      i += 0.001;
      y = oscillate (data, i, NULL);
    }

  return i;
}

static double
calculate_duration (SpringData *data)
{
  double beta   = 0.0;
  double omega0 = 0.0;
  double x0     = 0.0;
  double y0     = 0.0;
  double x1     = 0.0;
  double y1     = 0.0;
  double m      = 0.0;
  double i      = 0.0;

  beta = data->damping_ratio / (2 * data->mass);

  if (G_APPROX_VALUE (beta, 0, DBL_EPSILON) ||
      beta < 0)
    return G_MAXDOUBLE;

  if (data->clamp)
    {
      if (G_APPROX_VALUE (data->to, data->from, DBL_EPSILON))
        return 0;
      return get_first_zero (data);
    }

  omega0 = sqrt (data->stiffness / data->mass);

  /*
   * As first ansatz for the overdamped solution,
   * and general estimation for the oscillating ones
   * we take the value of the envelope when it's < epsilon
   */
  x0 = -log (EPSILON) / beta;

  /* DBL_EPSILON is too small for this specific comparison, so we use
   * FLT_EPSILON even though it's doubles */
  if (G_APPROX_VALUE (beta, omega0, FLT_EPSILON) ||
      beta < omega0)
    return x0;

  /*
   * Since the overdamped solution decays way slower than the envelope
   * we need to use the value of the oscillation itself.
   * Newton's root finding method is a good candidate in this particular case:
   * https://en.wikipedia.org/wiki/Newton%27s_method
   */
  y0 = oscillate (data, x0, NULL);
  m  = (oscillate (data, (x0 + DELTA), NULL) - y0) / DELTA;

  x1 = (data->to - y0 + m * x0) / m;
  y1 = oscillate (data, x1, NULL);

  while (ABS (data->to - y1) > EPSILON)
    {
      if (i > 1.0)
        return 0.0;

      x0 = x1;
      y0 = y1;

      m = (oscillate (data, x0 + DELTA, NULL) - y0) / DELTA;

      x1 = (data->to - y0 + m * x0) / m;
      y1 = oscillate (data, x1, NULL);
      i += 0.001;
    }

  return x1;
}

/* ///COPIED FROM LIBADWAITA */

static void
destroy_spring_data (gpointer ptr)
{
  SpringData *data = ptr;

  if (data->destroy_data != NULL &&
      data->user_data != NULL)
    data->destroy_data (data->user_data);
  g_clear_pointer (&data->timer, g_timer_destroy);
  g_free (ptr);
}

static void
destroy_wr (gpointer ptr)
{
  GWeakRef *wr = ptr;

  g_weak_ref_clear (wr);
  g_free (ptr);
}

static gboolean
should_animate (GtkWidget *widget)
{
  GtkSettings *settings          = NULL;
  gboolean     enable_animations = FALSE;

  if (!gtk_widget_get_mapped (widget))
    return FALSE;

  settings = gtk_widget_get_settings (widget);
  g_object_get (
      settings,
      "gtk-enable-animations", &enable_animations,
      NULL);

  return enable_animations;
}

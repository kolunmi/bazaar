/* TAKEN FROM LIBADWAITA */

/*
 * Copyright (C) 2021 Manuel Genovés <manuel.genoves@gmail.com>
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "bge.h"

G_DEFINE_ENUM_TYPE (
    BgeEasing,
    bge_easing,
    G_DEFINE_ENUM_VALUE (BGE_LINEAR, "linear"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_QUAD, "in-quad"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_QUAD, "out-quad"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_QUAD, "in-out-quad"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_CUBIC, "in-cubic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_CUBIC, "out-cubic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_CUBIC, "in-out-cubic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_QUART, "in-quart"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_QUART, "out-quart"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_QUART, "in-out-quart"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_QUINT, "in-quint"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_QUINT, "out-quint"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_QUINT, "in-out-quint"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_SINE, "in-sine"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_SINE, "out-sine"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_SINE, "in-out-sine"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_EXPO, "in-expo"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_EXPO, "out-expo"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_EXPO, "in-out-expo"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_CIRC, "in-circ"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_CIRC, "out-circ"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_CIRC, "in-out-circ"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_ELASTIC, "in-elastic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_ELASTIC, "out-elastic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_ELASTIC, "in-out-elastic"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_BACK, "in-back"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_BACK, "out-back"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_BACK, "in-out-back"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_BOUNCE, "in-bounce"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT_BOUNCE, "out-bounce"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT_BOUNCE, "in-out-bounce"),
    G_DEFINE_ENUM_VALUE (BGE_EASE, "ease"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN, "ease-in"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_OUT, "ease-out"),
    G_DEFINE_ENUM_VALUE (BGE_EASE_IN_OUT, "ease-in-out"));

#include <math.h>

/*
 * Copied from:
 *   https://gitlab.gnome.org/GNOME/clutter/-/blob/a236494ea7f31848b4a459dad41330f225137832/clutter/clutter-easing.c
 *   https://gitlab.gnome.org/GNOME/clutter/-/blob/a236494ea7f31848b4a459dad41330f225137832/clutter/clutter-enums.h
 *
 * Copyright (C) 2011  Intel Corporation
 */

/**
 * BgeEasing:
 * @BGE_LINEAR: Linear tweening.
 * @BGE_EASE_IN_QUAD: Quadratic tweening.
 * @BGE_EASE_OUT_QUAD: Quadratic tweening, inverse of
 *   [enum@Bge.Easing.ease-in-quad].
 * @BGE_EASE_IN_OUT_QUAD: Quadratic tweening, combining
 *   [enum@Bge.Easing.ease-in-quad] and [enum@Bge.Easing.ease-out-quad].
 * @BGE_EASE_IN_CUBIC: Cubic tweening.
 * @BGE_EASE_OUT_CUBIC: Cubic tweening, inverse of
 *   [enum@Bge.Easing.ease-in-cubic].
 * @BGE_EASE_IN_OUT_CUBIC: Cubic tweening, combining
 *   [enum@Bge.Easing.ease-in-cubic] and [enum@Bge.Easing.ease-out-cubic].
 * @BGE_EASE_IN_QUART: Quartic tweening.
 * @BGE_EASE_OUT_QUART: Quartic tweening, inverse of
 *   [enum@Bge.Easing.ease-in-quart].
 * @BGE_EASE_IN_OUT_QUART: Quartic tweening, combining
 *   [enum@Bge.Easing.ease-in-quart] and [enum@Bge.Easing.ease-out-quart].
 * @BGE_EASE_IN_QUINT: Quintic tweening.
 * @BGE_EASE_OUT_QUINT: Quintic tweening, inverse of
 *   [enum@Bge.Easing.ease-in-quint].
 * @BGE_EASE_IN_OUT_QUINT: Quintic tweening, combining
 *   [enum@Bge.Easing.ease-in-quint] and [enum@Bge.Easing.ease-out-quint].
 * @BGE_EASE_IN_SINE: Sine wave tweening.
 * @BGE_EASE_OUT_SINE: Sine wave tweening, inverse of
 *   [enum@Bge.Easing.ease-in-sine].
 * @BGE_EASE_IN_OUT_SINE: Sine wave tweening, combining
 *   [enum@Bge.Easing.ease-in-sine] and [enum@Bge.Easing.ease-out-sine].
 * @BGE_EASE_IN_EXPO: Exponential tweening.
 * @BGE_EASE_OUT_EXPO: Exponential tweening, inverse of
 *   [enum@Bge.Easing.ease-in-expo].
 * @BGE_EASE_IN_OUT_EXPO: Exponential tweening, combining
 *   [enum@Bge.Easing.ease-in-expo] and [enum@Bge.Easing.ease-out-expo].
 * @BGE_EASE_IN_CIRC: Circular tweening.
 * @BGE_EASE_OUT_CIRC: Circular tweening, inverse of
 *   [enum@Bge.Easing.ease-in-circ].
 * @BGE_EASE_IN_OUT_CIRC: Circular tweening, combining
 *   [enum@Bge.Easing.ease-in-circ] and [enum@Bge.Easing.ease-out-circ].
 * @BGE_EASE_IN_ELASTIC: Elastic tweening, with offshoot on start.
 * @BGE_EASE_OUT_ELASTIC: Elastic tweening, with offshoot on end, inverse of
 *   [enum@Bge.Easing.ease-in-elastic].
 * @BGE_EASE_IN_OUT_ELASTIC: Elastic tweening, with offshoot on both ends,
 *   combining [enum@Bge.Easing.ease-in-elastic] and
 *   [enum@Bge.Easing.ease-out-elastic].
 * @BGE_EASE_IN_BACK: Overshooting cubic tweening, with backtracking on start.
 * @BGE_EASE_OUT_BACK: Overshooting cubic tweening, with backtracking on end,
 *   inverse of [enum@Bge.Easing.ease-in-back].
 * @BGE_EASE_IN_OUT_BACK: Overshooting cubic tweening, with backtracking on both
 *   ends, combining [enum@Bge.Easing.ease-in-back] and
 *   [enum@Bge.Easing.ease-out-back].
 * @BGE_EASE_IN_BOUNCE: Exponentially decaying parabolic (bounce) tweening,
 *   on start.
 * @BGE_EASE_OUT_BOUNCE: Exponentially decaying parabolic (bounce) tweening,
 *   with bounce on end, inverse of [enum@Bge.Easing.ease-in-bounce].
 * @BGE_EASE_IN_OUT_BOUNCE: Exponentially decaying parabolic (bounce) tweening,
 *   with bounce on both ends, combining [enum@Bge.Easing.ease-in-bounce] and
 *   [enum@Bge.Easing.ease-out-bounce].
 *
 * Describes the available easing functions for use with
 * [class@TimedAnimation].
 *
 * New values may be added to this enumeration over time.
 */

/**
 * BGE_EASE:
 *
 * Cubic bezier tweening, with control points in (0.25, 0.1) and (0.25, 1.0).
 *
 * Increases in velocity towards the middle of the animation, slowing back down
 * at the end.
 *
 * Since: 1.7
 */

/**
 * BGE_EASE_IN:
 *
 * Cubic bezier tweening, with control points in (0.42, 0.0) and (1.0, 1.0).
 *
 * Starts off slowly, with the speed of the animation increasing until complete.
 *
 * Since: 1.7
 */

/**
 * BGE_EASE_OUT:
 *
 * Cubic bezier tweening, with control points in (0.0, 0.0) and (0.58, 1.0).
 *
 * Starts quickly, slowing down the animation until complete.
 *
 * Since: 1.7
 */

/**
 * BGE_EASE_IN_OUT:
 *
 * Cubic bezier tweening, with control points in (0.42, 0.0) and (0.58, 1.0).
 *
 * Starts off slowly, speeds up in the middle, and then slows down again.
 *
 * Since: 1.7
 */

static inline double
linear (double t,
        double d)
{
  return t / d;
}

static inline double
ease_in_quad (double t,
              double d)
{
  double p = t / d;

  return p * p;
}

static inline double
ease_out_quad (double t,
               double d)
{
  double p = t / d;

  return -1.0 * p * (p - 2);
}

static inline double
ease_in_out_quad (double t,
                  double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p;

  p -= 1;

  return -0.5 * (p * (p - 2) - 1);
}

static inline double
ease_in_cubic (double t,
               double d)
{
  double p = t / d;

  return p * p * p;
}

static inline double
ease_out_cubic (double t,
                double d)
{
  double p = t / d - 1;

  return p * p * p + 1;
}

static inline double
ease_in_out_cubic (double t,
                   double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p + 2);
}

static inline double
ease_in_quart (double t,
               double d)
{
  double p = t / d;

  return p * p * p * p;
}

static inline double
ease_out_quart (double t,
                double d)
{
  double p = t / d - 1;

  return -1.0 * (p * p * p * p - 1);
}

static inline double
ease_in_out_quart (double t,
                   double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p;

  p -= 2;

  return -0.5 * (p * p * p * p - 2);
}

static inline double
ease_in_quint (double t,
               double d)
{
  double p = t / d;

  return p * p * p * p * p;
}

static inline double
ease_out_quint (double t,
                double d)
{
  double p = t / d - 1;

  return p * p * p * p * p + 1;
}

static inline double
ease_in_out_quint (double t,
                   double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p * p * p + 2);
}

static inline double
ease_in_sine (double t,
              double d)
{
  return -1.0 * cos (t / d * G_PI_2) + 1.0;
}

static inline double
ease_out_sine (double t,
               double d)
{
  return sin (t / d * G_PI_2);
}

static inline double
ease_in_out_sine (double t,
                  double d)
{
  return -0.5 * (cos (G_PI * t / d) - 1);
}

static inline double
ease_in_expo (double t,
              double d)
{
  return G_APPROX_VALUE (t, 0, DBL_EPSILON) ? 0.0 : pow (2, 10 * (t / d - 1));
}

static double
ease_out_expo (double t,
               double d)
{
  return G_APPROX_VALUE (t, d, DBL_EPSILON) ? 1.0 : -pow (2, -10 * t / d) + 1;
}

static inline double
ease_in_out_expo (double t,
                  double d)
{
  double p;

  if (G_APPROX_VALUE (t, 0, DBL_EPSILON))
    return 0.0;

  if (G_APPROX_VALUE (t, d, DBL_EPSILON))
    return 1.0;

  p = t / (d / 2);

  if (p < 1)
    return 0.5 * pow (2, 10 * (p - 1));

  p -= 1;

  return 0.5 * (-pow (2, -10 * p) + 2);
}

static inline double
ease_in_circ (double t,
              double d)
{
  double p = t / d;

  return -1.0 * (sqrt (1 - p * p) - 1);
}

static inline double
ease_out_circ (double t,
               double d)
{
  double p = t / d - 1;

  return sqrt (1 - p * p);
}

static inline double
ease_in_out_circ (double t,
                  double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return -0.5 * (sqrt (1 - p * p) - 1);

  p -= 2;

  return 0.5 * (sqrt (1 - p * p) + 1);
}

static inline double
ease_in_elastic (double t,
                 double d)
{
  double p = d * .3;
  double s = p / 4;
  double q = t / d;

  if (G_APPROX_VALUE (q, 1, DBL_EPSILON))
    return 1.0;

  q -= 1;

  return -(pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
}

static inline double
ease_out_elastic (double t,
                  double d)
{
  double p = d * .3;
  double s = p / 4;
  double q = t / d;

  if (G_APPROX_VALUE (q, 1, DBL_EPSILON))
    return 1.0;

  return pow (2, -10 * q) * sin ((q * d - s) * (2 * G_PI) / p) + 1.0;
}

static inline double
ease_in_out_elastic (double t,
                     double d)
{
  double p = d * (.3 * 1.5);
  double s = p / 4;
  double q = t / (d / 2);

  if (G_APPROX_VALUE (q, 2, DBL_EPSILON))
    return 1.0;

  if (q < 1)
    {
      q -= 1;

      return -.5 * (pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
    }
  else
    {
      q -= 1;

      return pow (2, -10 * q) * sin ((q * d - s) * (2 * G_PI) / p) * .5 + 1.0;
    }
}

static inline double
ease_in_back (double t,
              double d)
{
  double p = t / d;

  return p * p * ((1.70158 + 1) * p - 1.70158);
}

static inline double
ease_out_back (double t,
               double d)
{
  double p = t / d - 1;

  return p * p * ((1.70158 + 1) * p + 1.70158) + 1;
}

static inline double
ease_in_out_back (double t,
                  double d)
{
  double p = t / (d / 2);
  double s = 1.70158 * 1.525;

  if (p < 1)
    return 0.5 * (p * p * ((s + 1) * p - s));

  p -= 2;

  return 0.5 * (p * p * ((s + 1) * p + s) + 2);
}

static inline double
ease_out_bounce (double t,
                 double d)
{
  double p = t / d;

  if (p < (1 / 2.75))
    {
      return 7.5625 * p * p;
    }
  else if (p < (2 / 2.75))
    {
      p -= (1.5 / 2.75);

      return 7.5625 * p * p + .75;
    }
  else if (p < (2.5 / 2.75))
    {
      p -= (2.25 / 2.75);

      return 7.5625 * p * p + .9375;
    }
  else
    {
      p -= (2.625 / 2.75);

      return 7.5625 * p * p + .984375;
    }
}

static inline double
ease_in_bounce (double t,
                double d)
{
  return 1.0 - ease_out_bounce (d - t, d);
}

static inline double
ease_in_out_bounce (double t,
                    double d)
{
  if (t < d / 2)
    return ease_in_bounce (t * 2, d) * 0.5;
  else
    return ease_out_bounce (t * 2 - d, d) * 0.5 + 1.0 * 0.5;
}

static inline double
x_for_t (double t,
         double x_1,
         double x_2)
{
  double omt = 1.0 - t;

  return 3.0 * omt * omt * t * x_1 + 3.0 * omt * t * t * x_2 + t * t * t;
}

static inline double
y_for_t (double t,
         double y_1,
         double y_2)
{
  double omt = 1.0 - t;

  return 3.0 * omt * omt * t * y_1 + 3.0 * omt * t * t * y_2 + t * t * t;
}

static inline double
t_for_x (double x,
         double x_1,
         double x_2)
{
  double min_t = 0, max_t = 1;
  int    i;

  for (i = 0; i < 30; ++i)
    {
      double guess_t = (min_t + max_t) / 2.0;
      double guess_x = x_for_t (guess_t, x_1, x_2);

      if (x < guess_x)
        max_t = guess_t;
      else
        min_t = guess_t;
    }

  return (min_t + max_t) / 2.0;
}

static double
ease_cubic_bezier (double t,
                   double d,
                   double x_1,
                   double y_1,
                   double x_2,
                   double y_2)
{
  double p = t / d;

  if (G_APPROX_VALUE (p, 0.0, DBL_EPSILON))
    return 0.0;

  if (G_APPROX_VALUE (p, 1.0, DBL_EPSILON))
    return 1.0;

  return y_for_t (t_for_x (p, x_1, x_2), y_1, y_2);
}

/**
 * bge_easing_ease:
 * @self: an easing value
 * @value: a value to ease
 *
 * Computes easing with @easing for @value.
 *
 * @value should generally be in the [0, 1] range.
 *
 * Returns: the easing for @value
 */
double
bge_easing_ease (BgeEasing self,
                 double    value)
{
  switch (self)
    {
    case BGE_LINEAR:
      return linear (value, 1);
    case BGE_EASE_IN_QUAD:
      return ease_in_quad (value, 1);
    case BGE_EASE_OUT_QUAD:
      return ease_out_quad (value, 1);
    case BGE_EASE_IN_OUT_QUAD:
      return ease_in_out_quad (value, 1);
    case BGE_EASE_IN_CUBIC:
      return ease_in_cubic (value, 1);
    case BGE_EASE_OUT_CUBIC:
      return ease_out_cubic (value, 1);
    case BGE_EASE_IN_OUT_CUBIC:
      return ease_in_out_cubic (value, 1);
    case BGE_EASE_IN_QUART:
      return ease_in_quart (value, 1);
    case BGE_EASE_OUT_QUART:
      return ease_out_quart (value, 1);
    case BGE_EASE_IN_OUT_QUART:
      return ease_in_out_quart (value, 1);
    case BGE_EASE_IN_QUINT:
      return ease_in_quint (value, 1);
    case BGE_EASE_OUT_QUINT:
      return ease_out_quint (value, 1);
    case BGE_EASE_IN_OUT_QUINT:
      return ease_in_out_quint (value, 1);
    case BGE_EASE_IN_SINE:
      return ease_in_sine (value, 1);
    case BGE_EASE_OUT_SINE:
      return ease_out_sine (value, 1);
    case BGE_EASE_IN_OUT_SINE:
      return ease_in_out_sine (value, 1);
    case BGE_EASE_IN_EXPO:
      return ease_in_expo (value, 1);
    case BGE_EASE_OUT_EXPO:
      return ease_out_expo (value, 1);
    case BGE_EASE_IN_OUT_EXPO:
      return ease_in_out_expo (value, 1);
    case BGE_EASE_IN_CIRC:
      return ease_in_circ (value, 1);
    case BGE_EASE_OUT_CIRC:
      return ease_out_circ (value, 1);
    case BGE_EASE_IN_OUT_CIRC:
      return ease_in_out_circ (value, 1);
    case BGE_EASE_IN_ELASTIC:
      return ease_in_elastic (value, 1);
    case BGE_EASE_OUT_ELASTIC:
      return ease_out_elastic (value, 1);
    case BGE_EASE_IN_OUT_ELASTIC:
      return ease_in_out_elastic (value, 1);
    case BGE_EASE_IN_BACK:
      return ease_in_back (value, 1);
    case BGE_EASE_OUT_BACK:
      return ease_out_back (value, 1);
    case BGE_EASE_IN_OUT_BACK:
      return ease_in_out_back (value, 1);
    case BGE_EASE_IN_BOUNCE:
      return ease_in_bounce (value, 1);
    case BGE_EASE_OUT_BOUNCE:
      return ease_out_bounce (value, 1);
    case BGE_EASE_IN_OUT_BOUNCE:
      return ease_in_out_bounce (value, 1);
    case BGE_EASE:
      return ease_cubic_bezier (value, 1, 0.25, 0.1, 0.25, 1.0);
    case BGE_EASE_IN:
      return ease_cubic_bezier (value, 1, 0.42, 0.0, 1.0, 1.0);
    case BGE_EASE_OUT:
      return ease_cubic_bezier (value, 1, 0.0, 0.0, 0.58, 1.0);
    case BGE_EASE_IN_OUT:
      return ease_cubic_bezier (value, 1, 0.42, 0.0, 0.58, 1.0);
    default:
      g_assert_not_reached ();
    }
}

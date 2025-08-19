/* bz-global-progress.c
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

#include "config.h"

#include <adwaita.h>

#include "bz-global-progress.h"

struct _BzGlobalProgress
{
  GtkWidget parent_instance;

  GtkWidget *child;
  gboolean   active;
  double     fraction;
  double     actual_fraction;
  double     transition_progress;
  int        expand_size;

  AdwAnimation *transition_animation;
  AdwAnimation *fraction_animation;

  AdwSpringParams *transition_spring_up;
  AdwSpringParams *transition_spring_down;
  AdwSpringParams *fraction_spring;
};

G_DEFINE_FINAL_TYPE (BzGlobalProgress, bz_global_progress, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_ACTIVE,
  PROP_FRACTION,
  PROP_ACTUAL_FRACTION,
  PROP_TRANSITION_PROGRESS,
  PROP_EXPAND_SIZE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_global_progress_dispose (GObject *object)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_clear_object (&self->transition_animation);
  g_clear_object (&self->fraction_animation);

  g_clear_pointer (&self->transition_spring_up, adw_spring_params_unref);
  g_clear_pointer (&self->transition_spring_down, adw_spring_params_unref);
  g_clear_pointer (&self->fraction_spring, adw_spring_params_unref);

  G_OBJECT_CLASS (bz_global_progress_parent_class)->dispose (object);
}

static void
bz_global_progress_get_property (GObject *object,
                                 guint    prop_id,
                                 GValue  *value,

                                 GParamSpec *pspec)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, bz_global_progress_get_child (self));
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, bz_global_progress_get_active (self));
      break;
    case PROP_FRACTION:
      g_value_set_double (value, bz_global_progress_get_fraction (self));
      break;
    case PROP_ACTUAL_FRACTION:
      g_value_set_double (value, bz_global_progress_get_actual_fraction (self));
      break;
    case PROP_TRANSITION_PROGRESS:
      g_value_set_double (value, bz_global_progress_get_transition_progress (self));
      break;
    case PROP_EXPAND_SIZE:
      g_value_set_int (value, bz_global_progress_get_expand_size (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_global_progress_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      bz_global_progress_set_child (self, g_value_get_object (value));
      break;
    case PROP_ACTIVE:
      bz_global_progress_set_active (self, g_value_get_boolean (value));
      break;
    case PROP_FRACTION:
      bz_global_progress_set_fraction (self, g_value_get_double (value));
      break;
    case PROP_ACTUAL_FRACTION:
      bz_global_progress_set_actual_fraction (self, g_value_get_double (value));
      break;
    case PROP_TRANSITION_PROGRESS:
      bz_global_progress_set_transition_progress (self, g_value_get_double (value));
      break;
    case PROP_EXPAND_SIZE:
      bz_global_progress_set_expand_size (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_global_progress_measure (GtkWidget     *widget,
                            GtkOrientation orientation,
                            int            for_size,
                            int           *minimum,
                            int           *natural,
                            int           *minimum_baseline,
                            int           *natural_baseline)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (widget);

  if (self->child != NULL)
    gtk_widget_measure (
        self->child, orientation,
        for_size, minimum, natural,
        minimum_baseline, natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int add = 0;

      add = round (self->transition_progress * self->expand_size);

      (*minimum) += add;
      (*natural) += add;
    }
}

static void
bz_global_progress_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (widget);

  if (self->child != NULL)
    gtk_widget_allocate (self->child, width, height, baseline, NULL);
}

static void
bz_global_progress_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  BzGlobalProgress *self           = BZ_GLOBAL_PROGRESS (widget);
  double            width          = 0;
  double            height         = 0;
  double            corner_radius  = 0.0;
  GskRoundedRect    total_clip     = { 0 };
  GskRoundedRect    fraction_clip  = { 0 };
  AdwStyleManager  *style_manager  = NULL;
  g_autoptr (GdkRGBA) accent_color = NULL;

  if (self->child != NULL)
    {
      gtk_snapshot_push_opacity (snapshot, CLAMP (1.0 - self->transition_progress, 0.0, 1.0));
      gtk_widget_snapshot_child (widget, self->child, snapshot);
      gtk_snapshot_pop (snapshot);
    }

  width         = gtk_widget_get_width (widget);
  height        = gtk_widget_get_height (widget);
  corner_radius = height * 0.5 * (0.3 * self->transition_progress + 0.2);

  total_clip.bounds           = GRAPHENE_RECT_INIT (0.0, 0.0, width, height);
  total_clip.corner[0].width  = corner_radius;
  total_clip.corner[0].height = corner_radius;
  total_clip.corner[1].width  = corner_radius;
  total_clip.corner[1].height = corner_radius;
  total_clip.corner[2].width  = corner_radius;
  total_clip.corner[2].height = corner_radius;
  total_clip.corner[3].width  = corner_radius;
  total_clip.corner[3].height = corner_radius;

  fraction_clip.bounds           = GRAPHENE_RECT_INIT (0.0, 0.0, width * self->actual_fraction, height);
  fraction_clip.corner[0].width  = corner_radius;
  fraction_clip.corner[0].height = corner_radius;
  fraction_clip.corner[1].width  = corner_radius;
  fraction_clip.corner[1].height = corner_radius;
  fraction_clip.corner[2].width  = corner_radius;
  fraction_clip.corner[2].height = corner_radius;
  fraction_clip.corner[3].width  = corner_radius;
  fraction_clip.corner[3].height = corner_radius;

  gtk_snapshot_push_rounded_clip (snapshot, &total_clip);
  gtk_snapshot_push_opacity (snapshot, CLAMP (self->transition_progress, 0.0, 1.0));

  style_manager = adw_style_manager_get_default ();
  accent_color  = adw_style_manager_get_accent_color_rgba (style_manager);

  accent_color->alpha = 0.2;
  gtk_snapshot_append_color (snapshot, accent_color, &total_clip.bounds);

  gtk_snapshot_push_rounded_clip (snapshot, &fraction_clip);
  accent_color->alpha = 1.0;
  gtk_snapshot_append_color (snapshot, accent_color, &fraction_clip.bounds);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_pop (snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
bz_global_progress_class_init (BzGlobalProgressClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_global_progress_dispose;
  object_class->get_property = bz_global_progress_get_property;
  object_class->set_property = bz_global_progress_set_property;

  props[PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FRACTION] =
      g_param_spec_double (
          "fraction",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACTUAL_FRACTION] =
      g_param_spec_double (
          "actual-fraction",
          NULL, NULL,
          0.0, 2.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSITION_PROGRESS] =
      g_param_spec_double (
          "transition-progress",
          NULL, NULL,
          -10.0, 10.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_EXPAND_SIZE] =
      g_param_spec_int (
          "expand-size",
          NULL, NULL,
          0, G_MAXINT, 100,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure       = bz_global_progress_measure;
  widget_class->size_allocate = bz_global_progress_size_allocate;
  widget_class->snapshot      = bz_global_progress_snapshot;
}

static void
bz_global_progress_init (BzGlobalProgress *self)
{
  AdwAnimationTarget *transition_target = NULL;
  AdwSpringParams    *transition_spring = NULL;
  AdwAnimationTarget *fraction_target   = NULL;
  AdwSpringParams    *fraction_spring   = NULL;

  self->expand_size = 100;

  transition_target          = adw_property_animation_target_new (G_OBJECT (self), "transition-progress");
  transition_spring          = adw_spring_params_new (0.75, 0.8, 200.0);
  self->transition_animation = adw_spring_animation_new (
      GTK_WIDGET (self),
      0.0,
      0.0,
      transition_spring,
      transition_target);
  adw_spring_animation_set_epsilon (
      ADW_SPRING_ANIMATION (self->transition_animation), 0.00005);

  fraction_target          = adw_property_animation_target_new (G_OBJECT (self), "actual-fraction");
  fraction_spring          = adw_spring_params_new (1.0, 0.75, 200.0);
  self->fraction_animation = adw_spring_animation_new (
      GTK_WIDGET (self),
      0.0,
      0.0,
      fraction_spring,
      fraction_target);

  self->transition_spring_up   = adw_spring_params_ref (transition_spring);
  self->transition_spring_down = adw_spring_params_new (1.5, 0.1, 100.0);
  self->fraction_spring        = adw_spring_params_ref (fraction_spring);
}

GtkWidget *
bz_global_progress_new (void)
{
  return g_object_new (BZ_TYPE_GLOBAL_PROGRESS, NULL);
}

void
bz_global_progress_set_child (BzGlobalProgress *self,
                              GtkWidget        *child)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

GtkWidget *
bz_global_progress_get_child (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), NULL);
  return self->child;
}

void
bz_global_progress_set_active (BzGlobalProgress *self,
                               gboolean          active)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if ((active && self->active) ||
      (!active && !self->active))
    return;

  self->active = active;

  adw_spring_animation_set_value_from (
      ADW_SPRING_ANIMATION (self->transition_animation),
      self->transition_progress);
  adw_spring_animation_set_value_to (
      ADW_SPRING_ANIMATION (self->transition_animation),
      active ? 1.0 : 0.0);
  adw_spring_animation_set_initial_velocity (
      ADW_SPRING_ANIMATION (self->transition_animation),
      adw_spring_animation_get_velocity (ADW_SPRING_ANIMATION (self->transition_animation)));

  adw_spring_animation_set_spring_params (
      ADW_SPRING_ANIMATION (self->transition_animation),
      active ? self->transition_spring_up : self->transition_spring_down);

  adw_animation_play (self->transition_animation);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
}

gboolean
bz_global_progress_get_active (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->active;
}

void
bz_global_progress_set_fraction (BzGlobalProgress *self,
                                 double            fraction)
{
  double last = 0.0;

  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  last           = self->actual_fraction;
  self->fraction = CLAMP (fraction, 0.0, 1.0);

  if (self->fraction < last ||
      G_APPROX_VALUE (last, self->fraction, 0.001))
    {
      adw_animation_reset (self->fraction_animation);
      bz_global_progress_set_actual_fraction (self, self->fraction);
    }
  else
    {
      adw_spring_animation_set_value_from (
          ADW_SPRING_ANIMATION (self->fraction_animation),
          self->actual_fraction);
      adw_spring_animation_set_value_to (
          ADW_SPRING_ANIMATION (self->fraction_animation),
          self->fraction);
      adw_spring_animation_set_initial_velocity (
          ADW_SPRING_ANIMATION (self->fraction_animation),
          adw_spring_animation_get_velocity (
              ADW_SPRING_ANIMATION (self->fraction_animation)));

      adw_animation_play (self->fraction_animation);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FRACTION]);
}

double
bz_global_progress_get_fraction (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->fraction;
}

void
bz_global_progress_set_actual_fraction (BzGlobalProgress *self,
                                        double            fraction)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  self->actual_fraction = CLAMP (fraction, 0.0, 1.0);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FRACTION]);
}

double
bz_global_progress_get_actual_fraction (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->actual_fraction;
}

void
bz_global_progress_set_transition_progress (BzGlobalProgress *self,
                                            double            progress)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  self->transition_progress = MAX (progress, 0.0);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSITION_PROGRESS]);
}

double
bz_global_progress_get_transition_progress (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->transition_progress;
}

void
bz_global_progress_set_expand_size (BzGlobalProgress *self,
                                    int               expand_size)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  self->expand_size = MAX (expand_size, 0);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EXPAND_SIZE]);
}

int
bz_global_progress_get_expand_size (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->expand_size;
}

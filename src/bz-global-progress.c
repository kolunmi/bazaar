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
#include "progress-bar-designs/common.h"

struct _BzGlobalProgress
{
  GtkWidget parent_instance;

  GtkWidget *child;
  gboolean   active;
  gboolean   pending;
  double     fraction;
  double     actual_fraction;
  double     pending_progress;
  double     transition_progress;
  int        expand_size;
  GSettings *settings;

  AdwAnimation *transition_animation;
  AdwAnimation *pending_animation;
  AdwAnimation *fraction_animation;

  AdwSpringParams *transition_spring_up;
  AdwSpringParams *transition_spring_down;
  AdwSpringParams *pending_spring;
  AdwSpringParams *fraction_spring;

  guint  tick;
  double pending_time_mod;
};

G_DEFINE_FINAL_TYPE (BzGlobalProgress, bz_global_progress, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_ACTIVE,
  PROP_PENDING,
  PROP_FRACTION,
  PROP_ACTUAL_FRACTION,
  PROP_TRANSITION_PROGRESS,
  PROP_PENDING_PROGRESS,
  PROP_EXPAND_SIZE,
  PROP_SETTINGS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
global_progress_bar_theme_changed (BzGlobalProgress *self,
                                   const char       *key,
                                   GSettings        *settings);

static void
bz_global_progress_dispose (GObject *object)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->tick);
  self->tick = 0;

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_func (
        self->settings,
        global_progress_bar_theme_changed,
        self);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  g_clear_object (&self->settings);

  g_clear_object (&self->transition_animation);
  g_clear_object (&self->pending_animation);
  g_clear_object (&self->fraction_animation);

  g_clear_pointer (&self->transition_spring_up, adw_spring_params_unref);
  g_clear_pointer (&self->transition_spring_down, adw_spring_params_unref);
  g_clear_pointer (&self->pending_spring, adw_spring_params_unref);
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
    case PROP_PENDING:
      g_value_set_boolean (value, bz_global_progress_get_pending (self));
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
    case PROP_PENDING_PROGRESS:
      g_value_set_double (value, bz_global_progress_get_pending_progress (self));
      break;
    case PROP_EXPAND_SIZE:
      g_value_set_int (value, bz_global_progress_get_expand_size (self));
      break;
    case PROP_SETTINGS:
      g_value_set_object (value, bz_global_progress_get_settings (self));
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
    case PROP_PENDING:
      bz_global_progress_set_pending (self, g_value_get_boolean (value));
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
    case PROP_PENDING_PROGRESS:
      bz_global_progress_set_pending_progress (self, g_value_get_double (value));
      break;
    case PROP_EXPAND_SIZE:
      bz_global_progress_set_expand_size (self, g_value_get_int (value));
      break;
    case PROP_SETTINGS:
      bz_global_progress_set_settings (self, g_value_get_object (value));
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
  double            inner_radius   = 0.0;
  double            gap            = 0.0;
  GskRoundedRect    total_clip     = { 0 };
  graphene_rect_t   fraction_rect  = { 0 };
  graphene_rect_t   pending_rect   = { 0 };
  GskRoundedRect    fraction_clip  = { 0 };
  g_autoptr (GdkRGBA) accent_color = NULL;

  accent_color = g_new0 (GdkRGBA, 1);
  gtk_widget_get_color (widget, accent_color);

  if (self->child != NULL)
    {
      gtk_snapshot_push_opacity (snapshot, CLAMP (1.0 - self->transition_progress, 0.0, 1.0));
      gtk_widget_snapshot_child (widget, self->child, snapshot);
      gtk_snapshot_pop (snapshot);
    }

  width         = gtk_widget_get_width (widget);
  height        = gtk_widget_get_height (widget);
  corner_radius = height * 0.5 * (0.3 * self->transition_progress + 0.2);
  gap           = height * 0.1;
  inner_radius  = MAX (corner_radius - gap, 0.0);

  total_clip.bounds           = GRAPHENE_RECT_INIT (0.0, 0.0, width, height);
  total_clip.corner[0].width  = corner_radius;
  total_clip.corner[0].height = corner_radius;
  total_clip.corner[1].width  = corner_radius;
  total_clip.corner[1].height = corner_radius;
  total_clip.corner[2].width  = corner_radius;
  total_clip.corner[2].height = corner_radius;
  total_clip.corner[3].width  = corner_radius;
  total_clip.corner[3].height = corner_radius;

  fraction_rect = GRAPHENE_RECT_INIT (
      0.0,
      0.0,
      width * self->actual_fraction,
      height);
  pending_rect = GRAPHENE_RECT_INIT (
      (height * 0.2) + MAX ((width - height * 0.4) * 0.35, 0.0) * self->pending_time_mod,
      height * 0.2,
      MAX ((width - height * 0.4) * 0.65, 0.0),
      height * 0.6);

  graphene_rect_interpolate (
      &fraction_rect,
      &pending_rect,
      self->pending_progress,
      &fraction_clip.bounds);
  fraction_clip.corner[0].width  = inner_radius;
  fraction_clip.corner[0].height = inner_radius;
  fraction_clip.corner[1].width  = inner_radius;
  fraction_clip.corner[1].height = inner_radius;
  fraction_clip.corner[2].width  = inner_radius;
  fraction_clip.corner[2].height = inner_radius;
  fraction_clip.corner[3].width  = inner_radius;
  fraction_clip.corner[3].height = inner_radius;

  gtk_snapshot_push_rounded_clip (snapshot, &total_clip);
  gtk_snapshot_push_opacity (snapshot, CLAMP (self->transition_progress, 0.0, 1.0));

  accent_color->alpha = 0.2;
  gtk_snapshot_append_color (snapshot, accent_color, &total_clip.bounds);
  accent_color->alpha = 1.0;

  gtk_snapshot_push_rounded_clip (snapshot, &fraction_clip);
  if (self->settings != NULL)
    {
      const char *theme = NULL;

      theme = g_settings_get_string (self->settings, "global-progress-bar-theme");

      if (theme == NULL || g_strcmp0 (theme, "accent-color") == 0)
        gtk_snapshot_append_color (snapshot, accent_color, &fraction_clip.bounds);
      else if (g_strcmp0 (theme, "pride-rainbow-flag") == 0 ||
               g_strcmp0 (theme, "lesbian-pride-flag") == 0 ||
               g_strcmp0 (theme, "transgender-flag") == 0 ||
               g_strcmp0 (theme, "nonbinary-flag") == 0 ||
               g_strcmp0 (theme, "bisexual-flag") == 0 ||
               g_strcmp0 (theme, "asexual-flag") == 0 ||
               g_strcmp0 (theme, "pansexual-flag") == 0 ||
               g_strcmp0 (theme, "aromantic-flag") == 0 ||
               g_strcmp0 (theme, "genderfluid-flag") == 0 ||
               g_strcmp0 (theme, "polysexual-flag") == 0 ||
               g_strcmp0 (theme, "omnisexual-flag") == 0 ||
               g_strcmp0 (theme, "aroace-flag") == 0 ||
               g_strcmp0 (theme, "agender-flag") == 0 ||
               g_strcmp0 (theme, "genderqueer-flag") == 0 ||
               g_strcmp0 (theme, "intersex-flag") == 0 ||
               g_strcmp0 (theme, "demigender-flag") == 0 ||
               g_strcmp0 (theme, "biromantic-flag") == 0)
        append_pride_flag (snapshot, &fraction_clip.bounds, theme);
      else
        gtk_snapshot_append_color (snapshot, accent_color, &fraction_clip.bounds);
    }
  else
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

  props[PROP_PENDING] =
      g_param_spec_boolean (
          "pending",
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
          0.0, 2.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PENDING_PROGRESS] =
      g_param_spec_double (
          "pending-progress",
          NULL, NULL,
          0.0, 2.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_EXPAND_SIZE] =
      g_param_spec_int (
          "expand-size",
          NULL, NULL,
          0, G_MAXINT, 100,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SETTINGS] =
      g_param_spec_object (
          "settings",
          NULL, NULL,
          G_TYPE_SETTINGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure       = bz_global_progress_measure;
  widget_class->size_allocate = bz_global_progress_size_allocate;
  widget_class->snapshot      = bz_global_progress_snapshot;

  gtk_widget_class_set_css_name (widget_class, "global-progress");
}

static gboolean
tick_cb (BzGlobalProgress *self,
         GdkFrameClock    *frame_clock,
         gpointer          user_data)
{
  gint64 frame_time   = 0;
  double linear_value = 0.0;

  frame_time   = gdk_frame_clock_get_frame_time (frame_clock);
  linear_value = fmod ((double) (frame_time % (gint64) G_MAXDOUBLE) * 0.000001, 2.0);
  if (linear_value > 1.0)
    linear_value = 2.0 - linear_value;

  self->pending_time_mod = adw_easing_ease (ADW_EASE_IN_OUT_CUBIC, linear_value);

  if (self->pending_progress > 0.0 &&
      self->transition_progress > 0.0)
    gtk_widget_queue_draw (GTK_WIDGET (self));

  return G_SOURCE_CONTINUE;
}

static void
bz_global_progress_init (BzGlobalProgress *self)
{
  AdwAnimationTarget *transition_target = NULL;
  AdwSpringParams    *transition_spring = NULL;
  AdwAnimationTarget *pending_target    = NULL;
  AdwSpringParams    *pending_spring    = NULL;
  AdwAnimationTarget *fraction_target   = NULL;
  AdwSpringParams    *fraction_spring   = NULL;

  self->expand_size = 100;

  self->tick = gtk_widget_add_tick_callback (GTK_WIDGET (self), (GtkTickCallback) tick_cb, NULL, NULL);

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

  pending_target          = adw_property_animation_target_new (G_OBJECT (self), "pending-progress");
  pending_spring          = adw_spring_params_new (1.0, 0.75, 200.0);
  self->pending_animation = adw_spring_animation_new (
      GTK_WIDGET (self),
      0.0,
      0.0,
      pending_spring,
      pending_target);

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
  self->pending_spring         = adw_spring_params_ref (pending_spring);
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
bz_global_progress_set_pending (BzGlobalProgress *self,
                                gboolean          pending)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if ((pending && self->pending) ||
      (!pending && !self->pending))
    return;

  self->pending = pending;

  adw_spring_animation_set_value_from (
      ADW_SPRING_ANIMATION (self->pending_animation),
      self->pending_progress);
  adw_spring_animation_set_value_to (
      ADW_SPRING_ANIMATION (self->pending_animation),
      pending ? 1.0 : 0.0);
  adw_spring_animation_set_initial_velocity (
      ADW_SPRING_ANIMATION (self->pending_animation),
      adw_spring_animation_get_velocity (
          ADW_SPRING_ANIMATION (self->pending_animation)));

  adw_animation_play (self->pending_animation);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
}

gboolean
bz_global_progress_get_pending (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->pending;
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
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), 0.0);
  return self->transition_progress;
}

void
bz_global_progress_set_pending_progress (BzGlobalProgress *self,
                                         double            progress)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  self->pending_progress = MAX (progress, 0.0);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING_PROGRESS]);
}

double
bz_global_progress_get_pending_progress (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), 0.0);
  return self->pending_progress;
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

void
bz_global_progress_set_settings (BzGlobalProgress *self,
                                 GSettings        *settings)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_func (
        self->settings,
        global_progress_bar_theme_changed,
        self);
  g_clear_object (&self->settings);

  if (settings != NULL)
    {
      self->settings = g_object_ref (settings);
      g_signal_connect_swapped (
          self->settings,
          "changed::global-progress-bar-theme",
          G_CALLBACK (global_progress_bar_theme_changed),
          self);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SETTINGS]);
}

GSettings *
bz_global_progress_get_settings (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->settings;
}

static void
global_progress_bar_theme_changed (BzGlobalProgress *self,
                                   const char       *key,
                                   GSettings        *settings)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

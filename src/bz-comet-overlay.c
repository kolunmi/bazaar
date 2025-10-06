/* bz-comet-overlay.c
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

#include <adwaita.h>

#include "bz-comet-overlay.h"

typedef struct
{
  double x;
  double y;
  double progress;
} PulseState;

struct _BzCometOverlay
{
  GtkWidget parent_instance;

  GtkWidget *child;

  GHashTable *nodes;
  GArray     *pulses;
  GdkRGBA    *pulse_color;
};

G_DEFINE_FINAL_TYPE (BzCometOverlay, bz_comet_overlay, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_PULSE_COLOR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
progress_changed (BzComet        *comet,
                  GParamSpec     *pspec,
                  BzCometOverlay *self);

static void
animation_done (AdwAnimation   *animation,
                BzCometOverlay *self);

static void
update_params (BzCometOverlay *self,
               BzComet        *comet,
               int             width,
               int             height);

static void
pulse_cb (double     value,
          GtkWidget *widget);

static void
append_pulse (GtkSnapshot *snapshot,
              double       size,
              double       opacity,
              GdkRGBA     *color);

static void
bz_comet_overlay_dispose (GObject *object)
{
  BzCometOverlay *self = BZ_COMET_OVERLAY (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  g_clear_pointer (&self->nodes, g_hash_table_unref);
  g_clear_pointer (&self->pulses, g_array_unref);
  g_clear_pointer (&self->pulse_color, gdk_rgba_free);

  G_OBJECT_CLASS (bz_comet_overlay_parent_class)->dispose (object);
}

static void
bz_comet_overlay_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzCometOverlay *self = BZ_COMET_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, bz_comet_overlay_get_child (self));
      break;
    case PROP_PULSE_COLOR:
      g_value_set_boxed (value, bz_comet_overlay_get_pulse_color (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_comet_overlay_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzCometOverlay *self = BZ_COMET_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      bz_comet_overlay_set_child (self, g_value_get_object (value));
      break;
    case PROP_PULSE_COLOR:
      bz_comet_overlay_set_pulse_color (self, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_comet_overlay_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  BzCometOverlay *self = BZ_COMET_OVERLAY (widget);
  GHashTableIter  iter = { 0 };

  if (self->child != NULL && gtk_widget_should_layout (self->child))
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  g_hash_table_iter_init (&iter, self->nodes);
  for (;;)
    {
      BzComet       *comet = NULL;
      GskRenderNode *node  = NULL;

      if (!g_hash_table_iter_next (
              &iter, (gpointer *) &comet, (gpointer *) &node))
        break;

      update_params (self, comet, width, height);
    }
}

static void
bz_comet_overlay_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  BzCometOverlay *self  = BZ_COMET_OVERLAY (widget);
  GdkRGBA        *color = NULL;
  GHashTableIter  iter  = { 0 };

  if (self->child != NULL)
    gtk_widget_snapshot_child (widget, self->child, snapshot);

  color = bz_comet_overlay_get_pulse_color (self);

  g_hash_table_iter_init (&iter, self->nodes);
  for (;;)
    {
      BzComet       *comet                    = NULL;
      GskRenderNode *node                     = NULL;
      double         progress                 = 0.0;
      GskPath       *path                     = NULL;
      double         path_length              = 0.0;
      g_autoptr (GskPathMeasure) path_measure = NULL;
      GskPathPoint     path_point             = { 0 };
      graphene_point_t end_position           = { 0 };
      double           pulse_radius           = 0.0;
      GskRoundedRect   clip                   = { 0 };
      graphene_point_t paintable_position     = { 0 };
      GdkRGBA          clip_color             = { 0 };

      if (!g_hash_table_iter_next (
              &iter, (gpointer *) &comet, (gpointer *) &node))
        break;

      progress    = bz_comet_get_progress (comet);
      path        = bz_comet_get_path (comet);
      path_length = bz_comet_get_path_length (comet);

      gsk_path_get_end_point (path, &path_point);
      gsk_path_point_get_position (&path_point, path, &end_position);
      pulse_radius = progress / path_length * 150.0;

      clip_color       = *color;
      clip_color.alpha = color->alpha * (1.0 - (progress / path_length));

      clip.bounds = GRAPHENE_RECT_INIT (
          end_position.x - pulse_radius,
          end_position.y - pulse_radius,
          pulse_radius * 2.0,
          pulse_radius * 2.0);
      clip.corner[0].width  = pulse_radius;
      clip.corner[0].height = pulse_radius;
      clip.corner[1].width  = pulse_radius;
      clip.corner[1].height = pulse_radius;
      clip.corner[2].width  = pulse_radius;
      clip.corner[2].height = pulse_radius;
      clip.corner[3].width  = pulse_radius;
      clip.corner[3].height = pulse_radius;

      gtk_snapshot_push_rounded_clip (snapshot, &clip);
      gtk_snapshot_append_color (snapshot, &clip_color, &clip.bounds);
      gtk_snapshot_pop (snapshot);

      path_measure = gsk_path_measure_new (path);
      gsk_path_measure_get_point (path_measure, progress, &path_point);
      gsk_path_point_get_position (&path_point, path, &paintable_position);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &paintable_position);
      gtk_snapshot_append_node (snapshot, node);
      gtk_snapshot_restore (snapshot);
    }

  for (guint i = 0; i < self->pulses->len; i++)
    {
      PulseState *pulse = NULL;

      pulse = &g_array_index (self->pulses, PulseState, i);
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (pulse->x, pulse->y));
      append_pulse (snapshot, pulse->progress * 200.0, 1.0 - pulse->progress, color);
      gtk_snapshot_restore (snapshot);
    }
  g_array_set_size (self->pulses, 0);
}

static void
bz_comet_overlay_class_init (BzCometOverlayClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_comet_overlay_dispose;
  object_class->get_property = bz_comet_overlay_get_property;
  object_class->set_property = bz_comet_overlay_set_property;

  props[PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PULSE_COLOR] =
      g_param_spec_boxed (
          "pulse-color",
          NULL, NULL,
          GDK_TYPE_RGBA,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->size_allocate = bz_comet_overlay_size_allocate;
  widget_class->snapshot      = bz_comet_overlay_snapshot;
}

static void
bz_comet_overlay_init (BzCometOverlay *self)
{
  AdwStyleManager *style_manager = NULL;

  self->nodes = g_hash_table_new_full (
      g_direct_hash, g_direct_equal,
      g_object_unref,
      (GDestroyNotify) gsk_render_node_unref);
  self->pulses = g_array_new (FALSE, FALSE, sizeof (PulseState));

  style_manager     = adw_style_manager_get_default ();
  self->pulse_color = adw_style_manager_get_accent_color_rgba (style_manager);
}

GtkWidget *
bz_comet_overlay_new (void)
{
  return g_object_new (BZ_TYPE_COMET_OVERLAY, NULL);
}

void
bz_comet_overlay_set_child (BzCometOverlay *self,
                            GtkWidget      *child)
{
  g_return_if_fail (BZ_IS_COMET_OVERLAY (self));
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
bz_comet_overlay_get_child (BzCometOverlay *self)
{
  g_return_val_if_fail (BZ_IS_COMET_OVERLAY (self), NULL);
  return self->child;
}

void
bz_comet_overlay_set_pulse_color (BzCometOverlay *self,
                                  GdkRGBA        *color)
{
  g_return_if_fail (BZ_IS_COMET_OVERLAY (self));

  if (self->pulse_color != NULL && color != NULL &&
      gdk_rgba_equal (self->pulse_color, color))
    return;

  g_clear_pointer (&self->pulse_color, gdk_rgba_free);

  if (color != NULL)
    self->pulse_color = gdk_rgba_copy (color);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PULSE_COLOR]);
}

GdkRGBA *
bz_comet_overlay_get_pulse_color (BzCometOverlay *self)
{
  AdwStyleManager *style_manager = NULL;

  g_return_val_if_fail (BZ_IS_COMET_OVERLAY (self), NULL);

  if (self->pulse_color != NULL)
    return self->pulse_color;

  style_manager = adw_style_manager_get_default ();
  return adw_style_manager_get_accent_color_rgba (style_manager);
}

void
bz_comet_overlay_spawn (BzCometOverlay *self,
                        BzComet        *comet)
{
  AdwAnimationTarget *target         = NULL;
  AdwSpringParams    *spring         = NULL;
  GtkWidget          *from           = NULL;
  GtkWidget          *to             = NULL;
  GdkPaintable       *paintable      = NULL;
  g_autoptr (AdwAnimation) animation = NULL;

  g_return_if_fail (BZ_IS_COMET_OVERLAY (self));
  g_return_if_fail (BZ_IS_COMET (comet));

  from      = bz_comet_get_from (comet);
  to        = bz_comet_get_to (comet);
  paintable = bz_comet_get_paintable (comet);

  g_return_if_fail (from != NULL && gtk_widget_is_ancestor (from, GTK_WIDGET (self)));
  g_return_if_fail (to != NULL && gtk_widget_is_ancestor (to, GTK_WIDGET (self)));
  g_return_if_fail (paintable != NULL);

  target = adw_property_animation_target_new (G_OBJECT (comet), "progress");
  spring = adw_spring_params_new (1.0, 0.1, 3.0);

  animation = adw_spring_animation_new (
      GTK_WIDGET (self),
      0.0,
      0.0,
      spring,
      target);
  adw_spring_animation_set_epsilon (
      ADW_SPRING_ANIMATION (animation), 0.0001);

  g_object_bind_property (comet, "path-length", animation, "value-to", G_BINDING_DEFAULT);
  update_params (
      self, comet,
      gtk_widget_get_width (GTK_WIDGET (self)),
      gtk_widget_get_height (GTK_WIDGET (self)));

  g_signal_connect (comet, "notify::progress", G_CALLBACK (progress_changed), self);
  bz_comet_set_progress (comet, 0.0);

  g_signal_connect (animation, "done", G_CALLBACK (animation_done), self);

  adw_animation_play (animation);
}

void
bz_comet_overlay_pulse_child (BzCometOverlay *self,
                              GtkWidget      *child)
{
  AdwAnimationTarget *target         = NULL;
  AdwSpringParams    *spring         = NULL;
  g_autoptr (AdwAnimation) animation = NULL;

  g_return_if_fail (BZ_IS_COMET_OVERLAY (self));
  g_return_if_fail (GTK_IS_WIDGET (child) &&
                    gtk_widget_is_ancestor (child, GTK_WIDGET (self)));

  target = adw_callback_animation_target_new (
      (AdwAnimationTargetFunc) pulse_cb,
      g_object_ref (child), g_object_unref);
  spring = adw_spring_params_new (1.5, 0.1, 5.0);

  animation = adw_spring_animation_new (
      GTK_WIDGET (self),
      0.0,
      1.0,
      spring,
      target);
  adw_spring_animation_set_epsilon (
      ADW_SPRING_ANIMATION (animation), 0.0001);
  adw_animation_play (animation);
}

static void
progress_changed (BzComet        *comet,
                  GParamSpec     *pspec,
                  BzCometOverlay *self)
{
  double        path_length        = 0.0;
  double        progress           = 0.0;
  GdkPaintable *paintable          = NULL;
  double        intrinsic_width    = 0;
  double        t                  = 0.0;
  double        size_scale         = 0.0;
  double        icon_size          = 0.0;
  double        grad_size          = 0.0;
  GdkRGBA      *color              = NULL;
  g_autoptr (GtkSnapshot) snapshot = NULL;

  path_length = bz_comet_get_path_length (comet);
  progress    = bz_comet_get_progress (comet);
  paintable   = bz_comet_get_paintable (comet);

  intrinsic_width = gdk_paintable_get_intrinsic_width (paintable);
  t               = progress / path_length;
  size_scale      = 1.0 - 4.0 * (t - 0.5) * (t - 0.5);

  icon_size = size_scale * intrinsic_width;
  grad_size = MAX (1.0, (path_length - progress) / path_length * intrinsic_width * 2.0);

  color = bz_comet_overlay_get_pulse_color (self);

  snapshot = gtk_snapshot_new ();
  append_pulse (snapshot, grad_size, 1.0 - (path_length - progress), color);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-icon_size / 2.0, -icon_size / 2.0));
  gdk_paintable_snapshot (paintable, snapshot, icon_size, icon_size);
  gtk_snapshot_restore (snapshot);

  g_hash_table_replace (self->nodes, g_object_ref (comet), gtk_snapshot_to_node (snapshot));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
animation_done (AdwAnimation   *animation,
                BzCometOverlay *self)
{
  AdwAnimationTarget *target = NULL;
  GObject            *comet  = NULL;

  target = adw_animation_get_target (animation);
  comet  = adw_property_animation_target_get_object (
      ADW_PROPERTY_ANIMATION_TARGET (target));

  g_hash_table_remove (self->nodes, comet);
}

static void
update_params (BzCometOverlay *self,
               BzComet        *comet,
               int             width,
               int             height)
{
  GtkWidget       *from        = NULL;
  GtkWidget       *to          = NULL;
  graphene_rect_t  from_rect   = { 0 };
  graphene_rect_t  to_rect     = { 0 };
  graphene_point_t from_center = { 0 };
  graphene_point_t to_center   = { 0 };
  graphene_point_t low_interp  = { 0 };
  graphene_point_t high_interp = { 0 };
  // GskPath         *last_path              = NULL;
  g_autoptr (GskPathBuilder) path_builder = NULL;
  g_autoptr (GskPath) path                = NULL;
  g_autoptr (GskPathMeasure) path_measure = NULL;
  float distance                          = 0.0;

  from = bz_comet_get_from (comet);
  to   = bz_comet_get_to (comet);
  g_assert (from != NULL);
  g_assert (to != NULL);

  g_assert (gtk_widget_compute_bounds (from, GTK_WIDGET (self), &from_rect));
  g_assert (gtk_widget_compute_bounds (to, GTK_WIDGET (self), &to_rect));
  graphene_rect_get_center (&from_rect, &from_center);
  graphene_rect_get_center (&to_rect, &to_center);

  graphene_point_interpolate (&from_center, &to_center, 0.333, &low_interp);
  graphene_point_interpolate (&to_center, &from_center, 0.333, &high_interp);

  path_builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (path_builder, from_center.x, from_center.y);
  gsk_path_builder_cubic_to (
      path_builder,
      high_interp.x, from_center.y,
      to_center.x, low_interp.y,
      to_center.x, to_center.y);

  path         = gsk_path_builder_to_path (path_builder);
  path_measure = gsk_path_measure_new (path);
  distance     = gsk_path_measure_get_length (path_measure);

  bz_comet_set_path (comet, path);
  bz_comet_set_path_length (comet, distance);
}

static void
pulse_cb (double     value,
          GtkWidget *widget)
{
  BzCometOverlay *self   = NULL;
  graphene_rect_t rect   = { 0 };
  gboolean        result = FALSE;
  PulseState      pulse  = { 0 };

  self = (BzCometOverlay *) gtk_widget_get_ancestor (widget, BZ_TYPE_COMET_OVERLAY);
  if (self == NULL)
    {
      g_warning ("Couldn't find ancestor BzCometOverlay for pulse!");
      return;
    }

  result = gtk_widget_compute_bounds (widget, GTK_WIDGET (self), &rect);
  if (!result)
    {
      g_warning ("Couldn't compute bounds of widget for pulse!");
      return;
    }

  pulse.x        = rect.origin.x + rect.size.width / 2.0;
  pulse.y        = rect.origin.y + rect.size.height / 2.0;
  pulse.progress = value;

  g_array_append_val (self->pulses, pulse);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
append_pulse (GtkSnapshot *snapshot,
              double       size,
              double       opacity,
              GdkRGBA     *color)
{
  const GdkRGBA transparent = {
    .red   = 1.0,
    .green = 1.0,
    .blue  = 1.0,
    .alpha = 0.0
  };

  GdkRGBA      pulse_color   = { 0 };
  GskColorStop grad_stops[2] = { 0 };

  if (size < 1.0)
    return;

  pulse_color       = *color;
  pulse_color.alpha = color->alpha * 0.75 * opacity;

  grad_stops[0].color  = pulse_color;
  grad_stops[0].offset = 0.9;
  grad_stops[1].color  = transparent;
  grad_stops[1].offset = 0.9;

  gtk_snapshot_append_radial_gradient (
      snapshot,
      &GRAPHENE_RECT_INIT (-size / 2.0, -size / 2.0, size, size),
      &GRAPHENE_POINT_INIT (0.0, 0.0),
      size / 2.0,
      size / 2.0,
      0.0,
      1.0,
      grad_stops,
      G_N_ELEMENTS (grad_stops));
}

/* bz-data-graph.c
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

#include "bz-data-graph.h"
#include "bz-data-point.h"

#define LABEL_MARGIN 75.0

struct _BzDataGraph
{
  GtkWidget parent_instance;

  GListModel *model;
  char       *independent_axis_label;
  char       *dependent_axis_label;
  int         independent_decimals;
  int         dependent_decimals;
  gboolean    has_dependent_min;
  double      dependent_min;
  gboolean    has_dependent_max;
  double      dependent_max;
  double      transition_progress;

  GskPath        *path;
  GskPathMeasure *path_measure;
  GskRenderNode  *fg;

  gboolean wants_animate_open;
  double   data_dependent_min;
  double   data_dependent_max;
  double   actual_dependent_min;
  double   actual_dependent_max;

  AdwAnimation *lower_bound_anim;
  AdwAnimation *upper_bound_anim;

  GtkEventController *motion;
  double              motion_x;
  double              motion_y;
};

G_DEFINE_FINAL_TYPE (BzDataGraph, bz_data_graph, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_INDEPENDENT_AXIS_LABEL,
  PROP_DEPENDENT_AXIS_LABEL,
  PROP_INDEPENDENT_DECIMALS,
  PROP_DEPENDENT_DECIMALS,
  PROP_HAS_DEPENDENT_MIN,
  PROP_DEPENDENT_MIN,
  PROP_HAS_DEPENDENT_MAX,
  PROP_DEPENDENT_MAX,
  PROP_TRANSITION_PROGRESS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
items_changed (GListModel  *model,
               guint        position,
               guint        removed,
               guint        added,
               BzDataGraph *self);

static void
animate_lower_bound_cb (double       value,
                        BzDataGraph *self);

static void
animate_upper_bound_cb (double       value,
                        BzDataGraph *self);

static void
refresh_path (BzDataGraph *self,
              double       width,
              double       height);

static void
animate_lower_bound (BzDataGraph *self,
                     double       from,
                     double       to);

static void
animate_upper_bound (BzDataGraph *self,
                     double       from,
                     double       to);

static void
bz_data_graph_dispose (GObject *object)
{
  BzDataGraph *self = BZ_DATA_GRAPH (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (
        self->model, items_changed, self);

  g_clear_object (&self->model);
  g_clear_pointer (&self->independent_axis_label, g_free);
  g_clear_pointer (&self->dependent_axis_label, g_free);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->path_measure, gsk_path_measure_unref);
  g_clear_pointer (&self->fg, gsk_render_node_unref);
  g_clear_object (&self->lower_bound_anim);
  g_clear_object (&self->upper_bound_anim);

  G_OBJECT_CLASS (bz_data_graph_parent_class)->dispose (object);
}

static void
bz_data_graph_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BzDataGraph *self = BZ_DATA_GRAPH (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_data_graph_get_model (self));
      break;
    case PROP_INDEPENDENT_AXIS_LABEL:
      g_value_set_string (value, bz_data_graph_get_independent_axis_label (self));
      break;
    case PROP_DEPENDENT_AXIS_LABEL:
      g_value_set_string (value, bz_data_graph_get_dependent_axis_label (self));
      break;
    case PROP_INDEPENDENT_DECIMALS:
      g_value_set_int (value, bz_data_graph_get_independent_decimals (self));
      break;
    case PROP_DEPENDENT_DECIMALS:
      g_value_set_int (value, bz_data_graph_get_dependent_decimals (self));
      break;
    case PROP_HAS_DEPENDENT_MIN:
      g_value_set_boolean (value, bz_data_graph_get_has_dependent_min (self));
      break;
    case PROP_DEPENDENT_MIN:
      g_value_set_double (value, bz_data_graph_get_dependent_min (self));
      break;
    case PROP_HAS_DEPENDENT_MAX:
      g_value_set_boolean (value, bz_data_graph_get_has_dependent_max (self));
      break;
    case PROP_DEPENDENT_MAX:
      g_value_set_double (value, bz_data_graph_get_dependent_max (self));
      break;
    case PROP_TRANSITION_PROGRESS:
      g_value_set_double (value, bz_data_graph_get_transition_progress (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_data_graph_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BzDataGraph *self = BZ_DATA_GRAPH (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_data_graph_set_model (self, g_value_get_object (value));
      break;
    case PROP_INDEPENDENT_AXIS_LABEL:
      bz_data_graph_set_independent_axis_label (self, g_value_get_string (value));
      break;
    case PROP_DEPENDENT_AXIS_LABEL:
      bz_data_graph_set_dependent_axis_label (self, g_value_get_string (value));
      break;
    case PROP_INDEPENDENT_DECIMALS:
      bz_data_graph_set_independent_decimals (self, g_value_get_int (value));
      break;
    case PROP_DEPENDENT_DECIMALS:
      bz_data_graph_set_dependent_decimals (self, g_value_get_int (value));
      break;
    case PROP_HAS_DEPENDENT_MIN:
      bz_data_graph_set_has_dependent_min (self, g_value_get_boolean (value));
      break;
    case PROP_DEPENDENT_MIN:
      bz_data_graph_set_dependent_min (self, g_value_get_double (value));
      break;
    case PROP_HAS_DEPENDENT_MAX:
      bz_data_graph_set_has_dependent_max (self, g_value_get_boolean (value));
      break;
    case PROP_DEPENDENT_MAX:
      bz_data_graph_set_dependent_max (self, g_value_get_double (value));
      break;
    case PROP_TRANSITION_PROGRESS:
      bz_data_graph_set_transition_progress (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_data_graph_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  BzDataGraph *self = BZ_DATA_GRAPH (widget);

  refresh_path (self, (double) width - LABEL_MARGIN * 2.0, (double) height - LABEL_MARGIN);
  gtk_widget_queue_draw (widget);
}

static void
bz_data_graph_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  BzDataGraph     *self             = BZ_DATA_GRAPH (widget);
  double           widget_width     = 0.0;
  double           widget_height    = 0.0;
  AdwStyleManager *style_manager    = NULL;
  g_autoptr (GdkRGBA) accent_color  = NULL;
  GdkRGBA widget_color              = { 0 };
  g_autoptr (GskPath) transitioning = NULL;
  g_autoptr (GskStroke) stroke      = NULL;

  if (self->path == NULL)
    return;

  widget_width  = gtk_widget_get_width (widget);
  widget_height = gtk_widget_get_height (widget);

  style_manager = adw_style_manager_get_default ();
  accent_color  = adw_style_manager_get_accent_color_rgba (style_manager);
  gtk_widget_get_color (widget, &widget_color);

  if (self->transition_progress > 0.0 && self->transition_progress < 1.0)
    {
      GskPathPoint point0                = { 0 };
      double       path_distance         = 0.0;
      GskPathPoint point1                = { 0 };
      g_autoptr (GskPathBuilder) builder = NULL;

      gsk_path_get_start_point (self->path, &point0);
      path_distance = gsk_path_measure_get_length (self->path_measure) * self->transition_progress;
      gsk_path_measure_get_point (self->path_measure, path_distance, &point1);

      builder = gsk_path_builder_new ();
      gsk_path_builder_add_segment (builder, self->path, &point0, &point1);
      transitioning = gsk_path_builder_to_path (builder);
    }

  stroke = gsk_stroke_new (6.0);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (LABEL_MARGIN, 0.0));

  if (self->fg != NULL)
    {
      graphene_rect_t bounds = { 0 };

      gsk_render_node_get_bounds (self->fg, &bounds);

      gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);
      gtk_snapshot_append_node (snapshot, self->fg);
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_append_color (snapshot, &widget_color, &bounds);
      gtk_snapshot_pop (snapshot);
    }

  if (self->transition_progress > 0.0)
    gtk_snapshot_append_stroke (
        snapshot,
        transitioning != NULL
            ? transitioning
            : self->path,
        stroke,
        accent_color);
  gtk_snapshot_restore (snapshot);

  if (self->motion_x >= LABEL_MARGIN &&
      self->motion_y >= 0.0 &&
      self->motion_x < widget_width - LABEL_MARGIN &&
      self->motion_y < widget_height - LABEL_MARGIN)
    {
      guint n_items                          = 0;
      guint hovered_idx                      = 0;
      g_autoptr (BzDataPoint) point          = NULL;
      g_autoptr (GskStroke) crosshair_stroke = NULL;
      g_autoptr (PangoLayout) layout         = NULL;
      g_autofree char *layout_text           = NULL;

      n_items     = g_list_model_get_n_items (self->model);
      hovered_idx = floor ((double) n_items *
                           (self->motion_x - LABEL_MARGIN) /
                           (widget_width - LABEL_MARGIN * 2.0));
      hovered_idx = CLAMP (hovered_idx, 0, n_items - 1);

      point = g_list_model_get_item (self->model, hovered_idx);

      crosshair_stroke = gsk_stroke_new (2.0);
      /* Docs: If `n_dash` is 1, an alternating "on" and "off" pattern with the
         single dash length provided is assumed. */
      gsk_stroke_set_dash (crosshair_stroke, (const float[1]) { 5.0 }, 1);

#define APPEND_LINE(x0, y0, x1, y1)                                               \
  G_STMT_START                                                                    \
  {                                                                               \
    g_autoptr (GskPathBuilder) builder = NULL;                                    \
    g_autoptr (GskPath) path           = NULL;                                    \
                                                                                  \
    builder = gsk_path_builder_new ();                                            \
    gsk_path_builder_move_to (builder, (x0), (y0));                               \
    gsk_path_builder_line_to (builder, (x1), (y1));                               \
                                                                                  \
    path = gsk_path_builder_to_path (builder);                                    \
    gtk_snapshot_append_stroke (snapshot, path, crosshair_stroke, &widget_color); \
  }                                                                               \
  G_STMT_END

      APPEND_LINE (self->motion_x, 0.0, self->motion_x, widget_height - LABEL_MARGIN);
      APPEND_LINE (LABEL_MARGIN, self->motion_y, widget_width - LABEL_MARGIN, self->motion_y);

#undef APPEND_LINE

      layout      = pango_layout_new (gtk_widget_get_pango_context (widget));
      layout_text = g_strdup_printf (
          "( %s, %0.0f )",
          bz_data_point_get_label (point),
          bz_data_point_get_dependent (point));
      pango_layout_set_text (layout, layout_text, -1);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (
          snapshot,
          &GRAPHENE_POINT_INIT (
              self->motion_x + 5.0,
              self->motion_y + 5.0));
      gtk_snapshot_append_layout (snapshot, layout, &widget_color);
      gtk_snapshot_restore (snapshot);
    }

  if (self->wants_animate_open)
    {
      AdwAnimationTarget *transition_target = NULL;
      AdwSpringParams    *transition_spring = NULL;
      g_autoptr (AdwAnimation) transition   = NULL;

      self->wants_animate_open = FALSE;

      transition_target = adw_property_animation_target_new (G_OBJECT (self), "transition-progress");
      transition_spring = adw_spring_params_new (1.0, 1.0, 80.0);
      transition        = adw_spring_animation_new (GTK_WIDGET (self), 0.0, 1.0, transition_spring, transition_target);
      adw_spring_animation_set_epsilon (ADW_SPRING_ANIMATION (transition), 0.000001);
      adw_animation_play (transition);
    }
}

static void
bz_data_graph_class_init (BzDataGraphClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_data_graph_dispose;
  object_class->get_property = bz_data_graph_get_property;
  object_class->set_property = bz_data_graph_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_INDEPENDENT_AXIS_LABEL] =
      g_param_spec_string (
          "independent-axis-label",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEPENDENT_AXIS_LABEL] =
      g_param_spec_string (
          "dependent-axis-label",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_INDEPENDENT_DECIMALS] =
      g_param_spec_int (
          "independent-decimals",
          NULL, NULL,
          -1, 4, (int) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEPENDENT_DECIMALS] =
      g_param_spec_int (
          "dependent-decimals",
          NULL, NULL,
          -1, 4, (int) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_HAS_DEPENDENT_MIN] =
      g_param_spec_boolean (
          "has-dependent-min",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEPENDENT_MIN] =
      g_param_spec_double (
          "dependent-min",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_HAS_DEPENDENT_MAX] =
      g_param_spec_boolean (
          "has-dependent-max",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEPENDENT_MAX] =
      g_param_spec_double (
          "dependent-max",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSITION_PROGRESS] =
      g_param_spec_double (
          "transition-progress",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->size_allocate = bz_data_graph_size_allocate;
  widget_class->snapshot      = bz_data_graph_snapshot;
}

static void
motion_enter (BzDataGraph              *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller)
{
  self->motion_x = x;
  self->motion_y = y;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
motion_event (BzDataGraph              *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller)
{
  self->motion_x = x;
  self->motion_y = y;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
motion_leave (BzDataGraph              *self,
              GtkEventControllerMotion *controller)
{
  self->motion_x = -1.0;
  self->motion_y = -1.0;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
bz_data_graph_init (BzDataGraph *self)
{
  AdwAnimationTarget *lower_bound_transition_target = NULL;
  AdwSpringParams    *lower_bound_transition_spring = NULL;
  g_autoptr (AdwAnimation) lower_bound_transition   = NULL;
  AdwAnimationTarget *upper_bound_transition_target = NULL;
  AdwSpringParams    *upper_bound_transition_spring = NULL;
  g_autoptr (AdwAnimation) upper_bound_transition   = NULL;

  gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "crosshair");

  self->dependent_min = 0.0;
  self->dependent_max = 1.0;

  lower_bound_transition_target = adw_callback_animation_target_new (
      (AdwAnimationTargetFunc) animate_lower_bound_cb, self, NULL);
  lower_bound_transition_spring = adw_spring_params_new (1.0, 1.0, 300.0);
  lower_bound_transition        = adw_spring_animation_new (
      GTK_WIDGET (self), 0.0, 0.0, lower_bound_transition_spring, lower_bound_transition_target);
  adw_spring_animation_set_epsilon (ADW_SPRING_ANIMATION (lower_bound_transition), 0.000001);
  self->lower_bound_anim = g_steal_pointer (&lower_bound_transition);

  upper_bound_transition_target = adw_callback_animation_target_new (
      (AdwAnimationTargetFunc) animate_upper_bound_cb, self, NULL);
  upper_bound_transition_spring = adw_spring_params_new (1.0, 1.0, 300.0);
  upper_bound_transition        = adw_spring_animation_new (
      GTK_WIDGET (self), 0.0, 0.0, upper_bound_transition_spring, upper_bound_transition_target);
  adw_spring_animation_set_epsilon (ADW_SPRING_ANIMATION (upper_bound_transition), 0.000001);
  self->upper_bound_anim = g_steal_pointer (&upper_bound_transition);

  self->motion = gtk_event_controller_motion_new ();
  g_signal_connect_swapped (self->motion, "enter", G_CALLBACK (motion_enter), self);
  g_signal_connect_swapped (self->motion, "motion", G_CALLBACK (motion_event), self);
  g_signal_connect_swapped (self->motion, "leave", G_CALLBACK (motion_leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->motion);

  self->motion_x = -1.0;
  self->motion_y = -1.0;
}

GtkWidget *
bz_data_graph_new (void)
{
  return g_object_new (BZ_TYPE_DATA_GRAPH, NULL);
}

GListModel *
bz_data_graph_get_model (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), NULL);
  return self->model;
}

const char *
bz_data_graph_get_independent_axis_label (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), NULL);
  return self->independent_axis_label;
}

const char *
bz_data_graph_get_dependent_axis_label (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), NULL);
  return self->dependent_axis_label;
}

int
bz_data_graph_get_independent_decimals (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), 0);
  return self->independent_decimals;
}

int
bz_data_graph_get_dependent_decimals (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), 0);
  return self->dependent_decimals;
}

gboolean
bz_data_graph_get_has_dependent_min (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), FALSE);
  return self->has_dependent_min;
}

double
bz_data_graph_get_dependent_min (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), 0.0);
  return self->dependent_min;
}

gboolean
bz_data_graph_get_has_dependent_max (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), FALSE);
  return self->has_dependent_max;
}

double
bz_data_graph_get_dependent_max (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), 0.0);
  return self->dependent_max;
}

double
bz_data_graph_get_transition_progress (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), 0.0);
  return self->transition_progress;
}

void
bz_data_graph_set_model (BzDataGraph *self,
                         GListModel  *model)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (
        self->model, items_changed, self);
  g_clear_object (&self->model);

  if (model != NULL)
    self->model = g_object_ref (model);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

void
bz_data_graph_set_independent_axis_label (BzDataGraph *self,
                                          const char  *independent_axis_label)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  g_clear_pointer (&self->independent_axis_label, g_free);
  if (independent_axis_label != NULL)
    self->independent_axis_label = g_strdup (independent_axis_label);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INDEPENDENT_AXIS_LABEL]);
}

void
bz_data_graph_set_dependent_axis_label (BzDataGraph *self,
                                        const char  *dependent_axis_label)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  g_clear_pointer (&self->dependent_axis_label, g_free);
  if (dependent_axis_label != NULL)
    self->dependent_axis_label = g_strdup (dependent_axis_label);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEPENDENT_AXIS_LABEL]);
}

void
bz_data_graph_set_independent_decimals (BzDataGraph *self,
                                        int          independent_decimals)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->independent_decimals = CLAMP (independent_decimals, -1, 4);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INDEPENDENT_DECIMALS]);
}

void
bz_data_graph_set_dependent_decimals (BzDataGraph *self,
                                      int          dependent_decimals)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->dependent_decimals = CLAMP (dependent_decimals, -1, 4);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEPENDENT_DECIMALS]);
}

void
bz_data_graph_set_has_dependent_min (BzDataGraph *self,
                                     gboolean     has_dependent_min)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->has_dependent_min = has_dependent_min;
  animate_lower_bound (
      self,
      self->actual_dependent_min,
      has_dependent_min
          ? self->dependent_min
          : self->data_dependent_min);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_DEPENDENT_MIN]);
}

void
bz_data_graph_set_dependent_min (BzDataGraph *self,
                                 double       dependent_min)
{
  g_autoptr (AdwAnimation) transition = NULL;

  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->dependent_min = dependent_min;
  animate_lower_bound (self, self->dependent_min, dependent_min);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEPENDENT_MIN]);
}

void
bz_data_graph_set_has_dependent_max (BzDataGraph *self,
                                     gboolean     has_dependent_max)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->has_dependent_max = has_dependent_max;
  if (has_dependent_max)
    animate_upper_bound (self, self->actual_dependent_max, self->dependent_max);
  else
    animate_upper_bound (self, self->actual_dependent_max, self->data_dependent_max);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_DEPENDENT_MAX]);
}

void
bz_data_graph_set_dependent_max (BzDataGraph *self,
                                 double       dependent_max)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->dependent_max = dependent_max;
  animate_upper_bound (self, self->dependent_max, dependent_max);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEPENDENT_MAX]);
}

void
bz_data_graph_set_transition_progress (BzDataGraph *self,
                                       double       transition_progress)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->transition_progress = transition_progress;

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSITION_PROGRESS]);
}

void
bz_data_graph_animate_open (BzDataGraph *self)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  self->wants_animate_open = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
items_changed (GListModel  *model,
               guint        position,
               guint        removed,
               guint        added,
               BzDataGraph *self)
{
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
animate_lower_bound_cb (double       value,
                        BzDataGraph *self)
{
  self->actual_dependent_min = value;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
animate_upper_bound_cb (double       value,
                        BzDataGraph *self)
{
  self->actual_dependent_max = value;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
refresh_path (BzDataGraph *self,
              double       width,
              double       height)
{
  guint             n_items                = 0;
  double            min_independent        = 0.0;
  double            max_independent        = 0.0;
  double            min_dependent          = 0.0;
  double            max_dependent          = 0.0;
  PangoContext     *pango                  = NULL;
  PangoFontMetrics *metrics                = NULL;
  double            font_height            = 0.0;
  int               independent_label_step = 0;
  int               dependent_label_step   = 0;
  g_autoptr (GskPathBuilder) curve_builder = NULL;
  g_autoptr (GtkSnapshot) snapshot         = NULL;
  g_autoptr (GskPathBuilder) grid_builder  = NULL;
  g_autoptr (GskPath) grid                 = NULL;
  g_autoptr (GskStroke) grid_stroke        = NULL;

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->path_measure, gsk_path_measure_unref);
  g_clear_pointer (&self->fg, gsk_render_node_unref);

  if (self->model == NULL)
    return;
  if (width < LABEL_MARGIN || height < LABEL_MARGIN)
    return;

  n_items = g_list_model_get_n_items (self->model);
  if (n_items <= 1)
    return;

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzDataPoint) point = NULL;
      double independent            = 0.0;
      double dependent              = 0.0;

      point       = g_list_model_get_item (self->model, i);
      independent = bz_data_point_get_independent (point);
      dependent   = bz_data_point_get_dependent (point);

      if (i == 0)
        {
          min_independent = independent;
          max_independent = independent;
          min_dependent   = dependent;
          max_dependent   = dependent;
        }
      else
        {
          min_independent = MIN (independent, min_independent);
          max_independent = MAX (independent, max_independent);
          min_dependent   = MIN (dependent, min_dependent);
          max_dependent   = MAX (dependent, max_dependent);
        }
    }
  self->data_dependent_min = min_dependent;
  self->data_dependent_max = max_dependent;

  if (self->has_dependent_min ||
      adw_animation_get_state (self->lower_bound_anim) == ADW_ANIMATION_PLAYING)
    min_dependent = MIN (min_dependent, self->actual_dependent_min);
  else
    self->actual_dependent_min = min_dependent;

  if (self->has_dependent_max ||
      adw_animation_get_state (self->upper_bound_anim) == ADW_ANIMATION_PLAYING)
    max_dependent = MAX (max_dependent, self->actual_dependent_max);
  else
    self->actual_dependent_max = max_dependent;

  pango       = gtk_widget_get_pango_context (GTK_WIDGET (self));
  metrics     = pango_context_get_metrics (pango, NULL, NULL);
  font_height = (double) (int) PANGO_PIXELS_CEIL (pango_font_metrics_get_height (metrics));
  g_clear_pointer (&metrics, pango_font_metrics_unref);
  independent_label_step = MAX (1, n_items / MAX (1, floor (width / MAX (font_height + 10.0, LABEL_MARGIN))));
  dependent_label_step   = MAX (1, floor (height / (font_height + 10.0)));

  curve_builder = gsk_path_builder_new ();
  snapshot      = gtk_snapshot_new ();
  grid_builder  = gsk_path_builder_new ();

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzDataPoint) point = NULL;
      double independent            = 0.0;
      double dependent              = 0.0;
      double x                      = 0.0;
      double y                      = 0.0;

      point       = g_list_model_get_item (self->model, i);
      independent = bz_data_point_get_independent (point);
      dependent   = bz_data_point_get_dependent (point);

      x = (independent - min_independent) / (max_independent - min_independent) * width;
      y = (1.0 - (dependent - min_dependent) / (max_dependent - min_dependent)) * height;

      if (i == 0)
        gsk_path_builder_move_to (curve_builder, x, y);
      else
        gsk_path_builder_line_to (curve_builder, x, y);

      if (i % independent_label_step == 0)
        {
          const char *label              = NULL;
          char        buf[32]            = { 0 };
          g_autoptr (PangoLayout) layout = NULL;

          label = bz_data_point_get_label (point);
          if (label == NULL)
            {
              switch (self->independent_decimals)
                {
                case 0:
                  g_snprintf (buf, sizeof (buf), "%d", (int) round (independent));
                  break;
                case 1:
                  g_snprintf (buf, sizeof (buf), "%.1f", independent);
                  break;
                case 2:
                  g_snprintf (buf, sizeof (buf), "%.2f", independent);
                  break;
                case 3:
                  g_snprintf (buf, sizeof (buf), "%.3f", independent);
                  break;
                default:
                  g_snprintf (buf, sizeof (buf), "%f", independent);
                  break;
                }
              label = buf;
            }

          layout = pango_layout_new (pango);
          pango_layout_set_text (layout, label, -1);

          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, height + LABEL_MARGIN / 10.0));
          gtk_snapshot_rotate (snapshot, 25.0);
          /* white so we can modulate later without regenerating */
          gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 1.0, 1.0, 1.0, 1.0 });
          gtk_snapshot_restore (snapshot);

          gsk_path_builder_move_to (grid_builder, x, 0.0);
          gsk_path_builder_line_to (grid_builder, x, height);
        }
    }
  gsk_path_builder_move_to (grid_builder, width, 0);
  gsk_path_builder_line_to (grid_builder, width, height);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-LABEL_MARGIN * 0.75, -font_height / 2.0));
  for (int i = 0; i < dependent_label_step; i++)
    {
      double value                   = 0.0;
      char   buf[32]                 = { 0 };
      g_autoptr (PangoLayout) layout = NULL;

      value = min_dependent +
              (double) (dependent_label_step - i) *
                  ((max_dependent - min_dependent) /
                   (double) dependent_label_step);

      switch (self->dependent_decimals)
        {
        case 0:
          g_snprintf (buf, sizeof (buf), "%d", (int) round (value));
          break;
        case 1:
          g_snprintf (buf, sizeof (buf), "%.1f", value);
          break;
        case 2:
          g_snprintf (buf, sizeof (buf), "%.2f", value);
          break;
        case 3:
          g_snprintf (buf, sizeof (buf), "%.3f", value);
          break;
        default:
          g_snprintf (buf, sizeof (buf), "%f", value);
          break;
        }

      layout = pango_layout_new (pango);
      pango_layout_set_text (layout, buf, -1);

      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 1.0, 1.0, 1.0, 1.0 });
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, font_height + 10.0));

      gsk_path_builder_move_to (grid_builder, 0.0, (font_height + 10.0) * (double) i);
      gsk_path_builder_line_to (grid_builder, width, (font_height + 10.0) * (double) i);
    }
  gsk_path_builder_move_to (grid_builder, 0.0, height);
  gsk_path_builder_line_to (grid_builder, width, height);
  gtk_snapshot_restore (snapshot);

  grid        = gsk_path_builder_to_path (grid_builder);
  grid_stroke = gsk_stroke_new (1.0);
  gtk_snapshot_push_opacity (snapshot, 0.25);
  gtk_snapshot_append_stroke (snapshot, grid, grid_stroke, &(GdkRGBA) { 1.0, 1.0, 1.0, 1.0 });
  gtk_snapshot_pop (snapshot);

  self->path         = gsk_path_builder_to_path (curve_builder);
  self->path_measure = gsk_path_measure_new (self->path);
  self->fg           = gtk_snapshot_to_node (snapshot);
}

static void
animate_lower_bound (BzDataGraph *self,
                     double       from,
                     double       to)
{
  adw_spring_animation_set_initial_velocity (
      ADW_SPRING_ANIMATION (self->lower_bound_anim),
      adw_spring_animation_get_velocity (
          ADW_SPRING_ANIMATION (self->lower_bound_anim)));
  adw_spring_animation_set_value_from (
      ADW_SPRING_ANIMATION (self->lower_bound_anim),
      from);
  adw_spring_animation_set_value_to (
      ADW_SPRING_ANIMATION (self->lower_bound_anim),
      to);
  adw_animation_play (self->lower_bound_anim);
}

static void
animate_upper_bound (BzDataGraph *self,
                     double       from,
                     double       to)
{
  adw_spring_animation_set_initial_velocity (
      ADW_SPRING_ANIMATION (self->upper_bound_anim),
      adw_spring_animation_get_velocity (
          ADW_SPRING_ANIMATION (self->upper_bound_anim)));
  adw_spring_animation_set_value_from (
      ADW_SPRING_ANIMATION (self->upper_bound_anim),
      from);
  adw_spring_animation_set_value_to (
      ADW_SPRING_ANIMATION (self->upper_bound_anim),
      to);
  adw_animation_play (self->upper_bound_anim);
}

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
#include <math.h>

#define LABEL_MARGIN        75.0
#define CARD_EDGE_THRESHOLD 160

struct _BzDataGraph
{
  GtkWidget parent_instance;

  GListModel *model;
  char       *independent_axis_label;
  char       *dependent_axis_label;
  char       *tooltip_prefix;
  int         independent_decimals;
  int         dependent_decimals;
  double      transition_progress;
  double      rounded_axis_max;

  GskPath        *path;
  GskPathMeasure *path_measure;
  GskRenderNode  *fg;

  gboolean wants_animate_open;

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
  PROP_TOOLTIP_PREFIX,
  PROP_INDEPENDENT_DECIMALS,
  PROP_DEPENDENT_DECIMALS,
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
refresh_path (BzDataGraph *self,
              double       width,
              double       height);

static double
calculate_axis_tick_value (double value, gboolean round_up);

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
  g_clear_pointer (&self->tooltip_prefix, g_free);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->path_measure, gsk_path_measure_unref);
  g_clear_pointer (&self->fg, gsk_render_node_unref);

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
    case PROP_TOOLTIP_PREFIX:
      g_value_set_string (value, bz_data_graph_get_tooltip_prefix (self));
      break;
    case PROP_INDEPENDENT_DECIMALS:
      g_value_set_int (value, bz_data_graph_get_independent_decimals (self));
      break;
    case PROP_DEPENDENT_DECIMALS:
      g_value_set_int (value, bz_data_graph_get_dependent_decimals (self));
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
    case PROP_TOOLTIP_PREFIX:
      bz_data_graph_set_tooltip_prefix (self, g_value_get_string (value));
      break;
    case PROP_INDEPENDENT_DECIMALS:
      bz_data_graph_set_independent_decimals (self, g_value_get_int (value));
      break;
    case PROP_DEPENDENT_DECIMALS:
      bz_data_graph_set_dependent_decimals (self, g_value_get_int (value));
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

  stroke = gsk_stroke_new (3.0);
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
      g_autoptr (PangoLayout) layout1        = NULL;
      g_autoptr (PangoLayout) layout2        = NULL;
      g_autofree char *line1_text            = NULL;
      g_autofree char *line2_text            = NULL;
      double           graph_height          = 0.0;
      double           graph_width           = 0.0;
      double           fraction              = 0.0;
      double           point_x               = 0.0;
      double           point_y               = 0.0;
      GskRoundedRect   rounded_rect          = { { { 0 } } };
      GdkRGBA          line_color            = { 0 };
      PangoRectangle   text1_extents         = { 0 };
      PangoRectangle   text2_extents         = { 0 };
      GskRoundedRect   text_bg_rect          = { { { 0 } } };
      GdkRGBA          text_bg_color         = { 0 };
      GdkRGBA          shadow_color          = { 0 };
      double           card_width            = 0.0;
      double           card_height           = 0.0;
      double           card_x                = 0.0;
      double           card_y                = 0.0;
      double           rounded_axis_max      = 0.0;
      const char      *prefix                = NULL;

      n_items     = g_list_model_get_n_items (self->model);
      graph_width = widget_width - LABEL_MARGIN * 2.0;
      fraction    = (self->motion_x - LABEL_MARGIN) / graph_width;
      hovered_idx = floor ((double) n_items * fraction);
      if (hovered_idx >= n_items)
        hovered_idx = n_items - 1;

      point = g_list_model_get_item (self->model, hovered_idx);

      if (self->rounded_axis_max > 0.0)
        {
          rounded_axis_max = self->rounded_axis_max;
        }
      else
        {
          double max_dependent = 0.0;
          for (guint i = 0; i < n_items; i++)
            {
              g_autoptr (BzDataPoint) p = g_list_model_get_item (self->model, i);
              double dep                = bz_data_point_get_dependent (p);
              if (i == 0 || dep > max_dependent)
                max_dependent = dep;
            }
          rounded_axis_max = calculate_axis_tick_value (max_dependent, TRUE);
        }

      graph_height = widget_height - LABEL_MARGIN;

      point_x = ((double) hovered_idx / (double) (n_items - 1)) * graph_width + LABEL_MARGIN;
      point_y = (1.0 - bz_data_point_get_dependent (point) / rounded_axis_max) * graph_height;

      line_color       = widget_color;
      line_color.alpha = 0.5;

      crosshair_stroke = gsk_stroke_new (1.0);

#define APPEND_LINE(x0, y0, x1, y1, color)                                  \
  G_STMT_START                                                              \
  {                                                                         \
    g_autoptr (GskPathBuilder) builder = NULL;                              \
    g_autoptr (GskPath) path           = NULL;                              \
                                                                            \
    builder = gsk_path_builder_new ();                                      \
    gsk_path_builder_move_to (builder, (x0), (y0));                         \
    gsk_path_builder_line_to (builder, (x1), (y1));                         \
                                                                            \
    path = gsk_path_builder_to_path (builder);                              \
    gtk_snapshot_append_stroke (snapshot, path, crosshair_stroke, (color)); \
  }                                                                         \
  G_STMT_END

      APPEND_LINE (self->motion_x, 0.0, self->motion_x, widget_height - LABEL_MARGIN, &line_color);

#undef APPEND_LINE

      gsk_rounded_rect_init_from_rect (
          &rounded_rect,
          &GRAPHENE_RECT_INIT (point_x - 4.0, point_y - 4.0, 8.0, 8.0),
          4.0);
      gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);
      gtk_snapshot_append_color (snapshot, accent_color, &rounded_rect.bounds);
      gtk_snapshot_pop (snapshot);

      layout1    = pango_layout_new (gtk_widget_get_pango_context (widget));
      line1_text = g_strdup (bz_data_point_get_label (point));
      pango_layout_set_text (layout1, line1_text, -1);
      pango_layout_get_pixel_extents (layout1, NULL, &text1_extents);

      prefix     = self->tooltip_prefix != NULL ? self->tooltip_prefix : ("");
      layout2    = pango_layout_new (gtk_widget_get_pango_context (widget));
      line2_text = g_strdup_printf ("%s %.0f", prefix, bz_data_point_get_dependent (point));
      pango_layout_set_text (layout2, line2_text, -1);
      pango_layout_get_pixel_extents (layout2, NULL, &text2_extents);

      card_width  = MAX (text1_extents.width, text2_extents.width) + 16.0;
      card_height = text1_extents.height + text2_extents.height + 20.0;

      if (widget_width - self->motion_x < CARD_EDGE_THRESHOLD)
        card_x = self->motion_x - card_width - 10.0;
      else
        card_x = self->motion_x + 10.0;
      card_y = self->motion_y + 10.0;

      /* The proper way is to make each actual element it's own widget or Gizmo
       but that's a lot of work */
      if (adw_style_manager_get_dark (style_manager))
        {
          text_bg_color = (GdkRGBA) { 0.18, 0.18, 0.2, 1.0 };
          shadow_color  = (GdkRGBA) { 0.0, 0.0, 0.06, 0.20 };
        }
      else
        {
          text_bg_color = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
          shadow_color  = (GdkRGBA) { 0.0, 0.0, 0.0, 0.20 };
        }

      gsk_rounded_rect_init_from_rect (
          &text_bg_rect,
          &GRAPHENE_RECT_INIT (card_x, card_y, card_width, card_height),
          6.0);

      gtk_snapshot_append_outset_shadow (
          snapshot,
          &text_bg_rect,
          &shadow_color,
          0.0,
          0.0,
          1.0,
          3.0);

      gtk_snapshot_push_rounded_clip (snapshot, &text_bg_rect);
      gtk_snapshot_append_color (snapshot, &text_bg_color, &text_bg_rect.bounds);
      gtk_snapshot_pop (snapshot);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (
          snapshot,
          &GRAPHENE_POINT_INIT (card_x + 8.0, card_y + 8.0));
      gtk_snapshot_append_layout (snapshot, layout1, &widget_color);
      gtk_snapshot_restore (snapshot);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (
          snapshot,
          &GRAPHENE_POINT_INIT (card_x + 8.0, card_y + 8.0 + text1_extents.height + 4.0));
      gtk_snapshot_append_layout (snapshot, layout2, &widget_color);
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

  props[PROP_TOOLTIP_PREFIX] =
      g_param_spec_string (
          "tooltip-prefix",
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
update_cursor (BzDataGraph *self,
               gdouble      x,
               gdouble      y)
{
  double widget_width  = gtk_widget_get_width (GTK_WIDGET (self));
  double widget_height = gtk_widget_get_height (GTK_WIDGET (self));

  if (x >= LABEL_MARGIN &&
      y >= 0.0 &&
      x < widget_width - LABEL_MARGIN &&
      y < widget_height - LABEL_MARGIN)
    gtk_widget_set_cursor_from_name (GTK_WIDGET (self), "crosshair");
  else
    gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
}

static void
motion_enter (BzDataGraph              *self,
              gdouble                   x,
              gdouble                   y,
              GtkEventControllerMotion *controller)
{
  self->motion_x = x;
  self->motion_y = y;
  update_cursor (self, x, y);
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
  update_cursor (self, x, y);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
motion_leave (BzDataGraph              *self,
              GtkEventControllerMotion *controller)
{
  self->motion_x = -1.0;
  self->motion_y = -1.0;
  gtk_widget_set_cursor (GTK_WIDGET (self), NULL);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
bz_data_graph_init (BzDataGraph *self)
{
  self->motion = gtk_event_controller_motion_new ();
  g_signal_connect_swapped (self->motion, "enter", G_CALLBACK (motion_enter), self);
  g_signal_connect_swapped (self->motion, "motion", G_CALLBACK (motion_event), self);
  g_signal_connect_swapped (self->motion, "leave", G_CALLBACK (motion_leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->motion);

  self->motion_x         = -1.0;
  self->motion_y         = -1.0;
  self->rounded_axis_max = 0.0;
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

const char *
bz_data_graph_get_tooltip_prefix (BzDataGraph *self)
{
  g_return_val_if_fail (BZ_IS_DATA_GRAPH (self), NULL);
  return self->tooltip_prefix;
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
bz_data_graph_set_tooltip_prefix (BzDataGraph *self,
                                  const char  *tooltip_prefix)
{
  g_return_if_fail (BZ_IS_DATA_GRAPH (self));

  g_clear_pointer (&self->tooltip_prefix, g_free);
  if (tooltip_prefix != NULL)
    self->tooltip_prefix = g_strdup (tooltip_prefix);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TOOLTIP_PREFIX]);
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

static double
calculate_axis_tick_value (double value, gboolean round_up)
{
  double exponent              = 0.0;
  double fraction              = 0.0;
  double rounded_axis_fraction = 0.0;

  exponent = floor (log10 (value));
  fraction = value / pow (10, exponent);

  if (round_up)
    {
      if (fraction <= 1.0)
        rounded_axis_fraction = 1.0;
      else if (fraction <= 2.0)
        rounded_axis_fraction = 2.0;
      else if (fraction <= 5.0)
        rounded_axis_fraction = 5.0;
      else
        rounded_axis_fraction = 10.0;
    }
  else if (fraction < 1.5)
    rounded_axis_fraction = 1.0;
  else if (fraction < 3.0)
    rounded_axis_fraction = 2.0;
  else if (fraction < 7.0)
    rounded_axis_fraction = 5.0;
  else
    rounded_axis_fraction = 10.0;

  return rounded_axis_fraction * pow (10, exponent);
}

static void
refresh_path (BzDataGraph *self,
              double       width,
              double       height)
{
  guint             n_items                = 0;
  double            min_independent        = 0.0;
  double            max_independent        = 0.0;
  double            max_dependent          = 0.0;
  PangoContext     *pango                  = NULL;
  PangoFontMetrics *metrics                = NULL;
  double            font_height            = 0.0;
  int               independent_label_step = 0;
  g_autoptr (GskPathBuilder) curve_builder = NULL;
  g_autoptr (GtkSnapshot) snapshot         = NULL;
  g_autoptr (GskPathBuilder) grid_builder  = NULL;
  g_autoptr (GskPath) grid                 = NULL;
  g_autoptr (GskStroke) grid_stroke        = NULL;
  double rounded_axis_max                  = 0.0;
  double tick_spacing                      = 0.0;
  int    num_ticks                         = 0;

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
          max_dependent   = dependent;
        }
      else
        {
          min_independent = MIN (independent, min_independent);
          max_independent = MAX (independent, max_independent);
          max_dependent   = MAX (dependent, max_dependent);
        }
    }

  rounded_axis_max = calculate_axis_tick_value (max_dependent, TRUE);

  pango       = gtk_widget_get_pango_context (GTK_WIDGET (self));
  metrics     = pango_context_get_metrics (pango, NULL, NULL);
  font_height = (double) (int) PANGO_PIXELS_CEIL (pango_font_metrics_get_height (metrics));
  g_clear_pointer (&metrics, pango_font_metrics_unref);

  num_ticks = floor (height / (font_height + 10.0));
  if (num_ticks < 2)
    num_ticks = 2;

  tick_spacing = calculate_axis_tick_value (rounded_axis_max / (double) num_ticks, FALSE);
  if (tick_spacing == 0.0)
    tick_spacing = 1.0;

  rounded_axis_max       = ceil (max_dependent / tick_spacing) * tick_spacing;
  self->rounded_axis_max = rounded_axis_max;

  independent_label_step = MAX (1, n_items / MAX (1, floor (width / MAX (font_height + 10.0, LABEL_MARGIN))));

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
      y = (1.0 - dependent / rounded_axis_max) * height;

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

  for (double value = 0.0; value <= rounded_axis_max; value += tick_spacing)
    {
      char buf[32]                   = { 0 };
      g_autoptr (PangoLayout) layout = NULL;
      double y_pos                   = (1.0 - value / rounded_axis_max) * height;

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

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0, y_pos));
      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 1.0, 1.0, 1.0, 1.0 });
      gtk_snapshot_restore (snapshot);

      gsk_path_builder_move_to (grid_builder, 0.0, y_pos);
      gsk_path_builder_line_to (grid_builder, width, y_pos);
    }

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

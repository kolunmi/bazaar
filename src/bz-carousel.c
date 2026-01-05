/* bz-carousel.c
 *
 * Copyright 2025 Alexander Vanhee
 * Copyright (C) 2019 Alice Mikhaylenko <alicem@gnome.org>
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

#include "bz-carousel.h"

#include <adwaita.h>
#include <math.h>

typedef struct
{
  GtkWidget *widget;
  int        position;
  gboolean   visible;
  double     snap_point;
  int        actual_size;
} ChildInfo;

struct _BzCarousel
{
  GtkWidget parent_instance;

  GList         *children;
  double         distance;
  double         position;
  guint          spacing;
  GtkOrientation orientation;

  double        animation_source_position;
  AdwAnimation *animation;
  ChildInfo    *animation_target_child;

  AdwSwipeTracker *tracker;

  guint    scroll_timeout_id;
  gboolean is_being_allocated;
};

static void bz_carousel_buildable_init (GtkBuildableIface *iface);
static void bz_carousel_swipeable_init (AdwSwipeableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzCarousel,
    bz_carousel,
    GTK_TYPE_WIDGET,
    G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL);
    G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, bz_carousel_buildable_init);
    G_IMPLEMENT_INTERFACE (ADW_TYPE_SWIPEABLE, bz_carousel_swipeable_init);)

static GtkBuildableIface *parent_buildable_iface;

enum
{
  PROP_0,
  PROP_N_PAGES,
  PROP_POSITION,
  PROP_ORIENTATION,
  LAST_PROP = PROP_POSITION + 1,
};

static GParamSpec *props[LAST_PROP];

enum
{
  SIGNAL_PAGE_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

static ChildInfo *
find_child_info (BzCarousel *self,
                 GtkWidget  *widget)
{
  GList *l;

  for (l = self->children; l; l = l->next)
    {
      ChildInfo *info = l->data;

      if (widget == info->widget)
        return info;
    }

  return NULL;
}

static int
find_child_index (BzCarousel *self,
                  GtkWidget  *widget)
{
  GList *l;
  int    i;

  i = 0;
  for (l = self->children; l; l = l->next)
    {
      ChildInfo *info = l->data;

      if (widget == info->widget)
        return i;

      i++;
    }

  return -1;
}

static GList *
get_nth_link (BzCarousel *self,
              int         n)
{
  GList *l;
  int    i;

  i = n;
  for (l = self->children; l; l = l->next)
    {
      if (i-- == 0)
        return l;
    }

  return NULL;
}

static ChildInfo *
get_closest_child_at (BzCarousel *self,
                      double      position)
{
  GList     *l;
  ChildInfo *closest_child = NULL;

  for (l = self->children; l; l = l->next)
    {
      ChildInfo *child = l->data;

      if (!closest_child ||
          ABS (closest_child->snap_point - position) >
              ABS (child->snap_point - position))
        closest_child = child;
    }

  return closest_child;
}

static inline void
get_range (BzCarousel *self,
           double     *lower,
           double     *upper)
{
  GList     *l     = g_list_last (self->children);
  ChildInfo *child = l ? l->data : NULL;

  if (lower)
    *lower = 0;

  if (upper)
    *upper = MAX (0, child ? child->snap_point : 0);
}

static GtkWidget *
get_page_at_position (BzCarousel *self,
                      double      position)
{
  double     lower = 0, upper = 0;
  ChildInfo *child;

  get_range (self, &lower, &upper);

  position = CLAMP (position, lower, upper);

  child = get_closest_child_at (self, position);

  if (!child)
    return NULL;

  return child->widget;
}

static void
set_position (BzCarousel *self,
              double      position)
{
  double lower = 0, upper = 0;

  get_range (self, &lower, &upper);

  position = CLAMP (position, lower, upper);

  self->position = position;
  gtk_widget_queue_allocate (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_POSITION]);
}

static void
scroll_animation_value_cb (double      value,
                           BzCarousel *self)
{
  set_position (self, value);
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
scroll_animation_done_cb (BzCarousel *self)
{
  GtkWidget *child;
  int        index;

  self->animation_source_position = 0;
  self->animation_target_child    = NULL;

  child = get_page_at_position (self, self->position);
  index = find_child_index (self, child);

  g_signal_emit (self, signals[SIGNAL_PAGE_CHANGED], 0, index);
}

static void
scroll_to (BzCarousel *self,
           GtkWidget  *widget,
           double      velocity)
{
  self->animation_target_child = find_child_info (self, widget);

  if (self->animation_target_child == NULL)
    return;

  self->animation_source_position = self->position;

  adw_spring_animation_set_value_from (ADW_SPRING_ANIMATION (self->animation),
                                       self->animation_source_position);
  adw_spring_animation_set_value_to (ADW_SPRING_ANIMATION (self->animation),
                                     self->animation_target_child->snap_point);
  adw_spring_animation_set_initial_velocity (ADW_SPRING_ANIMATION (self->animation),
                                             velocity);
  adw_animation_play (self->animation);
}

static inline double
get_closest_snap_point (BzCarousel *self)
{
  ChildInfo *closest_child = get_closest_child_at (self, self->position);

  if (!closest_child)
    return 0;

  return closest_child->snap_point;
}

static void
begin_swipe_cb (AdwSwipeTracker *tracker,
                BzCarousel      *self)
{
  adw_animation_pause (self->animation);
}

static void
update_swipe_cb (AdwSwipeTracker *tracker,
                 double           progress,
                 BzCarousel      *self)
{
  set_position (self, progress);
}

static void
end_swipe_cb (AdwSwipeTracker *tracker,
              double           velocity,
              double           to,
              BzCarousel      *self)
{
  GtkWidget *child = get_page_at_position (self, to);
  scroll_to (self, child, velocity);
}

static void
set_orientable_style_classes (GtkOrientable *orientable)
{
  GtkOrientation orientation = gtk_orientable_get_orientation (orientable);
  GtkWidget     *widget      = GTK_WIDGET (orientable);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_widget_add_css_class (widget, "horizontal");
      gtk_widget_remove_css_class (widget, "vertical");
    }
  else
    {
      gtk_widget_add_css_class (widget, "vertical");
      gtk_widget_remove_css_class (widget, "horizontal");
    }
}

static void
update_orientation (BzCarousel *self)
{
  gboolean reversed =
      self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->tracker),
                                  self->orientation);
  adw_swipe_tracker_set_reversed (self->tracker, reversed);

  set_orientable_style_classes (GTK_ORIENTABLE (self));
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
  GList      *children;

  if (minimum)
    *minimum = 0;
  if (natural)
    *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  for (children = self->children; children; children = children->next)
    {
      ChildInfo *child_info = children->data;
      GtkWidget *child      = child_info->widget;
      int        child_min, child_nat;

      if (!gtk_widget_get_visible (child))
        continue;

      gtk_widget_measure (child, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);

      if (minimum)
        *minimum = MAX (*minimum, child_min);
      if (natural)
        *natural = MAX (*natural, child_nat);
    }
}

static void
bz_carousel_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  BzCarousel *self;
  GList      *children;
  double      x, y;
  gboolean    is_rtl;
  double      snap_point;
  double      total_size;
  double      current_offset;
  guint       n_pages;
  double      remaining_position;
  double      center_offset_size;
  ChildInfo  *current_child;
  ChildInfo  *next_child;

  self = BZ_CAROUSEL (widget);

  total_size = 0;
  for (children = self->children; children; children = children->next)
    {
      ChildInfo *child_info;
      GtkWidget *child;
      int        min, nat;
      int        child_size;

      child_info = children->data;
      child      = child_info->widget;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_measure (child, self->orientation,
                              height, &min, &nat, NULL, NULL);
          if (gtk_widget_get_hexpand (child))
            child_size = width;
          else
            child_size = CLAMP (nat, min, width);
        }
      else
        {
          gtk_widget_measure (child, self->orientation,
                              width, &min, &nat, NULL, NULL);
          if (gtk_widget_get_vexpand (child))
            child_size = height;
          else
            child_size = CLAMP (nat, min, height);
        }

      child_info->actual_size = child_size;
      total_size += child_size;
    }

  n_pages = bz_carousel_get_n_pages (self);
  if (n_pages > 0 && total_size > 0)
    self->distance = total_size / n_pages;
  else
    self->distance = 1;

  snap_point = 0;
  for (children = self->children; children; children = children->next)
    {
      ChildInfo *child_info;

      child_info = children->data;

      child_info->snap_point = snap_point;
      snap_point += 1.0;

      if (child_info == self->animation_target_child)
        adw_spring_animation_set_value_to (ADW_SPRING_ANIMATION (self->animation),
                                           child_info->snap_point);
    }

  x = 0;
  y = 0;

  is_rtl = (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL);

  current_offset     = 0;
  remaining_position = self->position;
  center_offset_size = 0;
  current_child      = NULL;
  next_child         = NULL;

  for (children = self->children; children; children = children->next)
    {
      ChildInfo *child_info;

      child_info = children->data;

      if (remaining_position < 1.0)
        {
          GList *next;
          double fraction;

          current_child = child_info;
          next          = children->next;
          if (next)
            {
              next_child = next->data;
            }

          current_offset += child_info->actual_size * remaining_position;
          fraction = remaining_position;
          if (next_child)
            {
              center_offset_size = child_info->actual_size * (1.0 - fraction) +
                                   next_child->actual_size * fraction;
            }
          else
            {
              center_offset_size = child_info->actual_size;
            }

          break;
        }

      current_offset += child_info->actual_size;
      remaining_position -= 1.0;
    }

  if (!current_child && self->children)
    {
      ChildInfo *child_info = self->children->data;
      center_offset_size    = child_info->actual_size;
    }

  if (self->orientation == GTK_ORIENTATION_VERTICAL)
    {
      y = -current_offset + (height - center_offset_size) / 2.0;
    }
  else if (is_rtl)
    {
      x = current_offset + (width - center_offset_size) / 2.0;
    }
  else
    {
      x = -current_offset + (width - center_offset_size) / 2.0;
    }

  for (children = self->children; children; children = children->next)
    {
      ChildInfo    *child_info;
      GskTransform *transform;
      int           child_width, child_height;

      child_info = children->data;
      transform  = gsk_transform_new ();

      if (!gtk_widget_get_visible (child_info->widget))
        continue;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_width  = child_info->actual_size;
          child_height = height;
        }
      else
        {
          child_width  = width;
          child_height = child_info->actual_size;
        }

      if (self->orientation == GTK_ORIENTATION_VERTICAL)
        {
          child_info->position = y;
          child_info->visible  = child_info->position < height &&
                                child_info->position + child_height > 0;

          transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, child_info->position));

          y += child_info->actual_size;
        }
      else
        {
          child_info->position = x;
          child_info->visible  = child_info->position < width &&
                                child_info->position + child_width > 0;

          transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (child_info->position, 0));

          if (is_rtl)
            x -= child_info->actual_size;
          else
            x += child_info->actual_size;
        }

      gtk_widget_allocate (child_info->widget, child_width, child_height, baseline, transform);
    }

  self->is_being_allocated = FALSE;
}

static void
bz_carousel_direction_changed (GtkWidget       *widget,
                               GtkTextDirection previous_direction)
{
  BzCarousel *self = BZ_CAROUSEL (widget);
  update_orientation (self);
}

static void
bz_carousel_constructed (GObject *object)
{
  BzCarousel *self = (BzCarousel *) object;
  update_orientation (self);
  G_OBJECT_CLASS (bz_carousel_parent_class)->constructed (object);
}

static void
bz_carousel_dispose (GObject *object)
{
  BzCarousel *self = BZ_CAROUSEL (object);

  while (self->children)
    {
      ChildInfo *info = self->children->data;
      gtk_widget_unparent (info->widget);
      self->children = g_list_remove (self->children, info);
      g_free (info);
    }

  g_clear_object (&self->tracker);
  g_clear_object (&self->animation);
  g_clear_handle_id (&self->scroll_timeout_id, g_source_remove);

  G_OBJECT_CLASS (bz_carousel_parent_class)->dispose (object);
}

static void
bz_carousel_finalize (GObject *object)
{
  BzCarousel *self = BZ_CAROUSEL (object);
  g_list_free_full (self->children, (GDestroyNotify) g_free);
  G_OBJECT_CLASS (bz_carousel_parent_class)->finalize (object);
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
    case PROP_N_PAGES:
      g_value_set_uint (value, bz_carousel_get_n_pages (self));
      break;
    case PROP_POSITION:
      g_value_set_double (value, bz_carousel_get_position (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
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
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (orientation != self->orientation)
          {
            self->orientation = orientation;
            update_orientation (self);
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify (G_OBJECT (self), "orientation");
          }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_carousel_class_init (BzCarouselClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed  = bz_carousel_constructed;
  object_class->dispose      = bz_carousel_dispose;
  object_class->finalize     = bz_carousel_finalize;
  object_class->get_property = bz_carousel_get_property;
  object_class->set_property = bz_carousel_set_property;

  widget_class->measure           = bz_carousel_measure;
  widget_class->size_allocate     = bz_carousel_size_allocate;
  widget_class->direction_changed = bz_carousel_direction_changed;

  props[PROP_N_PAGES] =
      g_param_spec_uint ("n-pages", NULL, NULL,
                         0, G_MAXUINT, 0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_POSITION] =
      g_param_spec_double ("position", NULL, NULL,
                           0, G_MAXDOUBLE, 0,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_PAGE_CHANGED] =
      g_signal_new ("page-changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__UINT,
                    G_TYPE_NONE, 1, G_TYPE_UINT);

  gtk_widget_class_set_css_name (widget_class, "carousel");
}

static void
bz_carousel_init (BzCarousel *self)
{
  GtkEventController *controller;
  AdwAnimationTarget *target;

  self->spacing     = 0;
  self->orientation = GTK_ORIENTATION_HORIZONTAL;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);

  self->tracker = adw_swipe_tracker_new (ADW_SWIPEABLE (self));
  adw_swipe_tracker_set_enabled (self->tracker, TRUE);
  adw_swipe_tracker_set_allow_mouse_drag (self->tracker, TRUE);
  adw_swipe_tracker_set_allow_long_swipes (self->tracker, TRUE);

  g_signal_connect_object (self->tracker, "begin-swipe", G_CALLBACK (begin_swipe_cb), self, 0);
  g_signal_connect_object (self->tracker, "update-swipe", G_CALLBACK (update_swipe_cb), self, 0);
  g_signal_connect_object (self->tracker, "end-swipe", G_CALLBACK (end_swipe_cb), self, 0);

  controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  target = adw_callback_animation_target_new ((AdwAnimationTargetFunc)
                                                  scroll_animation_value_cb,
                                              self, NULL);
  self->animation =
      adw_spring_animation_new (GTK_WIDGET (self), 0, 0,
                                adw_spring_params_new (1, 0.5, 500),
                                target);
  adw_spring_animation_set_clamp (ADW_SPRING_ANIMATION (self->animation), TRUE);

  g_signal_connect_swapped (self->animation, "done",
                            G_CALLBACK (scroll_animation_done_cb), self);
}

static void
bz_carousel_buildable_add_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 GObject      *child,
                                 const char   *type)
{
  parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
bz_carousel_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child       = bz_carousel_buildable_add_child;
}

static double
bz_carousel_get_distance (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);
  return self->distance;
}

static double *
bz_carousel_get_snap_points (AdwSwipeable *swipeable,
                             int          *n_snap_points)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);
  guint       i, n_pages;
  double     *points;
  GList      *l;

  n_pages = MAX (g_list_length (self->children), 1);
  points  = g_new0 (double, n_pages);

  i = 0;
  for (l = self->children; l; l = l->next)
    {
      ChildInfo *info = l->data;
      points[i++]     = info->snap_point;
    }

  if (n_snap_points)
    *n_snap_points = n_pages;

  return points;
}

static double
bz_carousel_get_progress (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);
  return bz_carousel_get_position (self);
}

static double
bz_carousel_get_cancel_progress (AdwSwipeable *swipeable)
{
  BzCarousel *self = BZ_CAROUSEL (swipeable);
  return get_closest_snap_point (self);
}

static void
bz_carousel_swipeable_init (AdwSwipeableInterface *iface)
{
  iface->get_distance        = bz_carousel_get_distance;
  iface->get_snap_points     = bz_carousel_get_snap_points;
  iface->get_progress        = bz_carousel_get_progress;
  iface->get_cancel_progress = bz_carousel_get_cancel_progress;
}

GtkWidget *
bz_carousel_new (void)
{
  return g_object_new (BZ_TYPE_CAROUSEL, NULL);
}

void
bz_carousel_set_widgets (BzCarousel *self,
                         GList      *widgets)
{
  GList *l;

  g_return_if_fail (BZ_IS_CAROUSEL (self));

  while (self->children)
    {
      ChildInfo *info = self->children->data;
      gtk_widget_unparent (info->widget);
      self->children = g_list_remove (self->children, info);
      g_free (info);
    }

  for (l = widgets; l; l = l->next)
    {
      GtkWidget *widget = GTK_WIDGET (l->data);
      ChildInfo *info;

      g_return_if_fail (GTK_IS_WIDGET (widget));
      g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

      info         = g_new0 (ChildInfo, 1);
      info->widget = widget;

      self->children = g_list_append (self->children, info);
      gtk_widget_set_parent (widget, GTK_WIDGET (self));
    }

  self->position = 0;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_N_PAGES]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_POSITION]);
}

static void
do_scroll_to (BzCarousel *self,
              GtkWidget  *widget,
              gboolean    animate)
{
  scroll_to (self, widget, 0);

  if (!animate)
    adw_animation_skip (self->animation);
}

typedef struct
{
  BzCarousel *carousel;
  GtkWidget  *widget;
  gboolean    animate;
} ScrollData;

static void
scroll_to_idle_cb (ScrollData *data)
{
  do_scroll_to (data->carousel, data->widget, data->animate);

  g_object_unref (data->carousel);
  g_object_unref (data->widget);
  g_free (data);
}

void
bz_carousel_scroll_to (BzCarousel *self,
                       GtkWidget  *widget,
                       gboolean    animate)
{
  ScrollData *data;
  g_return_if_fail (BZ_IS_CAROUSEL (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_parent (widget) == GTK_WIDGET (self));

  data           = g_new (ScrollData, 1);
  data->carousel = g_object_ref (self);
  data->widget   = g_object_ref (widget);
  data->animate  = animate;

  g_idle_add_once ((GSourceOnceFunc) scroll_to_idle_cb, data);
}

GtkWidget *
bz_carousel_get_nth_page (BzCarousel *self,
                          guint       n)
{
  ChildInfo *info;
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), NULL);
  g_return_val_if_fail (n < bz_carousel_get_n_pages (self), NULL);
  info = get_nth_link (self, n)->data;
  return info->widget;
}

guint
bz_carousel_get_n_pages (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0);
  return g_list_length (self->children);
}

double
bz_carousel_get_position (BzCarousel *self)
{
  g_return_val_if_fail (BZ_IS_CAROUSEL (self), 0.0);
  return self->position;
}

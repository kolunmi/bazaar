/* bz-popup-overlay.c
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

#define G_LOG_DOMAIN "BAZAAR::POPUP-OVERLAY"

#include "bz-popup-overlay.h"
#include "bz-animation.h"
#include "bz-util.h"

struct _BzPopupOverlay
{
  GtkWidget parent_instance;

  GtkWidget *child;

  GPtrArray   *stack;
  BzAnimation *animation;

  int width;
  int height;
};

G_DEFINE_FINAL_TYPE (BzPopupOverlay, bz_popup_overlay, GTK_TYPE_WIDGET);

BZ_DEFINE_DATA (
    popup,
    Popup,
    {
      GtkWidget      *child;
      GtkWidget      *source;
      graphene_rect_t allocation;
      gboolean        initialized;

      graphene_point_t offset;
      graphene_size_t  scale;
      double           opacity;
    },
    BZ_RELEASE_DATA (child, gtk_widget_unparent);
    BZ_RELEASE_DATA (source, g_object_unref))

static void
animate (BzPopupOverlay *self,
         const char     *key,
         double          value,
         PopupData      *data);

static gboolean
close_request_cb (BzPopupOverlay *self,
                  GtkRoot        *root);

enum
{
  PROP_0,

  PROP_CHILD,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_popup_overlay_dispose (GObject *object)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  g_clear_pointer (&self->stack, g_ptr_array_unref);
  g_clear_object (&self->animation);

  G_OBJECT_CLASS (bz_popup_overlay_parent_class)->dispose (object);
}

static void
bz_popup_overlay_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, bz_popup_overlay_get_child (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_popup_overlay_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      bz_popup_overlay_set_child (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_popup_overlay_measure (GtkWidget     *widget,
                          GtkOrientation orientation,
                          int            for_size,
                          int           *minimum,
                          int           *natural,
                          int           *minimum_baseline,
                          int           *natural_baseline)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (widget);

  if (self->child != NULL)
    gtk_widget_measure (
        self->child, orientation,
        for_size, minimum, natural,
        minimum_baseline, natural_baseline);
}

static void
bz_popup_overlay_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (widget);

  if (self->child != NULL && gtk_widget_should_layout (self->child))
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  for (guint i = 0; i < self->stack->len;)
    {
      gboolean         result            = FALSE;
      PopupData       *data              = NULL;
      graphene_rect_t  source_bounds     = { 0 };
      graphene_point_t source_center     = { 0 };
      int              minimum_width     = 0;
      int              natural_width     = 0;
      int              minimum_height    = 0;
      int              natural_height    = 0;
      graphene_size_t  popup_size        = { 0 };
      graphene_rect_t  popup_bounds      = { 0 };
      int              unused            = 0;
      g_autoptr (GskTransform) transform = NULL;

      data = g_ptr_array_index (self->stack, i);

      result = gtk_widget_compute_bounds (data->source, GTK_WIDGET (self), &source_bounds);
      if (!result)
        {
          g_critical ("popup lost track of its source widget!");
          g_ptr_array_remove_index (self->stack, i);
          continue;
        }
      graphene_rect_get_center (&source_bounds, &source_center);

      gtk_widget_measure (
          data->child, GTK_ORIENTATION_HORIZONTAL,
          width, &minimum_width, &natural_width,
          &unused, &unused);
      gtk_widget_measure (
          data->child, GTK_ORIENTATION_VERTICAL,
          height, &minimum_height, &natural_height,
          &unused, &unused);

      popup_size = GRAPHENE_SIZE_INIT (
          CLAMP ((double) natural_width, (double) minimum_width, (double) width),
          CLAMP ((double) natural_height, (double) minimum_height, (double) height));

      popup_bounds = GRAPHENE_RECT_INIT (
          CLAMP (source_center.x / 2.0 < (double) width / 2.0
                     ? source_bounds.origin.x + source_bounds.size.width
                     : source_bounds.origin.x - popup_size.width,
                 0.0, (double) width - popup_size.width),
          CLAMP (source_center.y / 2.0 < (double) height / 2.0
                     ? source_bounds.origin.y + source_bounds.size.height
                     : source_bounds.origin.y - popup_size.height,
                 0.0, (double) height - popup_size.height),
          popup_size.width, popup_size.height);

      if (!data->initialized)
        {
          char buf[64] = { 0 };

          g_snprintf (buf, sizeof (buf), "x-%p", data);
          bz_animation_add_spring (
              self->animation, buf,
              popup_bounds.origin.x - source_bounds.origin.x, 0.0,
              0.9, 1.0, 0.1,
              (BzAnimationCallback) animate,
              popup_data_ref (data), popup_data_unref);

          buf[0] = 'y';
          bz_animation_add_spring (
              self->animation, buf,
              popup_bounds.origin.y - source_bounds.origin.y, 0.0,
              0.9, 1.0, 0.1,
              (BzAnimationCallback) animate,
              popup_data_ref (data), popup_data_unref);

          buf[0] = 'w';
          bz_animation_add_spring (
              self->animation, buf,
              0.0, 1.0,
              0.9, 1.0, 0.1,
              (BzAnimationCallback) animate,
              popup_data_ref (data), popup_data_unref);

          buf[0] = 'h';
          bz_animation_add_spring (
              self->animation, buf,
              0.0, 1.0,
              0.9, 1.0, 0.1,
              (BzAnimationCallback) animate,
              popup_data_ref (data), popup_data_unref);

          buf[0] = 'o';
          bz_animation_add_spring (
              self->animation, buf,
              0.0, 1.0,
              1.0, 1.0, 0.2,
              (BzAnimationCallback) animate,
              popup_data_ref (data), popup_data_unref);

          data->initialized = TRUE;
        }

      popup_bounds.origin.x -= data->offset.x;
      popup_bounds.origin.y -= data->offset.y;
      transform = gsk_transform_translate (transform, &popup_bounds.origin);
      transform = gsk_transform_scale (transform, data->scale.width, data->scale.height);

      gtk_widget_allocate (
          data->child,
          popup_bounds.size.width,
          popup_bounds.size.height,
          baseline,
          g_steal_pointer (&transform));

      i++;
    }
}

static void
bz_popup_overlay_snapshot (GtkWidget   *widget,
                           GtkSnapshot *snapshot)
{
  BzPopupOverlay *self   = BZ_POPUP_OVERLAY (widget);
  double          width  = 0.0;
  double          height = 0.0;

  if (self->child != NULL)
    gtk_widget_snapshot_child (widget, self->child, snapshot);

  width  = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  for (guint i = 0; i < self->stack->len; i++)
    {
      PopupData *data = NULL;

      data = g_ptr_array_index (self->stack, i);

      gtk_snapshot_push_opacity (snapshot, data->opacity);
      gtk_snapshot_append_color (
          snapshot,
          &(const GdkRGBA) {
              .red   = 0.0,
              .green = 0.0,
              .blue  = 0.0,
              .alpha = 0.25,
          },
          &GRAPHENE_RECT_INIT (0.0, 0.0, width, height));
      gtk_widget_snapshot_child (widget, data->child, snapshot);
      gtk_snapshot_pop (snapshot);
    }
}

static void
bz_popup_overlay_root (GtkWidget *widget)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (widget);
  GtkRoot        *root = NULL;

  GTK_WIDGET_CLASS (bz_popup_overlay_parent_class)->root (widget);

  root = gtk_widget_get_root (GTK_WIDGET (self));
  g_signal_connect_swapped (
      root,
      "close-request",
      G_CALLBACK (close_request_cb),
      self);
}

static void
bz_popup_overlay_unroot (GtkWidget *widget)
{
  BzPopupOverlay *self = BZ_POPUP_OVERLAY (widget);
  GtkRoot        *root = NULL;

  root = gtk_widget_get_root (GTK_WIDGET (self));
  g_signal_handlers_disconnect_by_func (root, close_request_cb, self);

  GTK_WIDGET_CLASS (bz_popup_overlay_parent_class)->unroot (widget);
}

static void
bz_popup_overlay_class_init (BzPopupOverlayClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_popup_overlay_set_property;
  object_class->get_property = bz_popup_overlay_get_property;
  object_class->dispose      = bz_popup_overlay_dispose;

  props[PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure       = bz_popup_overlay_measure;
  widget_class->size_allocate = bz_popup_overlay_size_allocate;
  widget_class->snapshot      = bz_popup_overlay_snapshot;
  widget_class->root          = bz_popup_overlay_root;
  widget_class->unroot        = bz_popup_overlay_unroot;
}

static void
bz_popup_overlay_init (BzPopupOverlay *self)
{
  self->stack     = g_ptr_array_new_with_free_func (popup_data_unref);
  self->animation = bz_animation_new (GTK_WIDGET (self));

  self->width  = -1;
  self->height = -1;
}

BzPopupOverlay *
bz_popup_overlay_new (void)
{
  return g_object_new (BZ_TYPE_POPUP_OVERLAY, NULL);
}

GtkWidget *
bz_popup_overlay_get_child (BzPopupOverlay *self)
{
  g_return_val_if_fail (BZ_IS_POPUP_OVERLAY (self), NULL);
  return self->child;
}

void
bz_popup_overlay_set_child (BzPopupOverlay *self,
                            GtkWidget      *child)
{
  g_return_if_fail (BZ_IS_POPUP_OVERLAY (self));

  if (child == self->child)
    return;

  if (child != NULL)
    g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  self->child = child;

  if (child != NULL)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

void
bz_popup_overlay_push (BzPopupOverlay *self,
                       GtkWidget      *widget,
                       GtkWidget      *source)
{
  g_autoptr (PopupData) data = NULL;

  g_return_if_fail (BZ_IS_POPUP_OVERLAY (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (source));
  g_return_if_fail (gtk_widget_is_ancestor (source, GTK_WIDGET (self)));

  data              = popup_data_new ();
  data->child       = widget;
  data->source      = g_object_ref (source);
  data->allocation  = (graphene_rect_t) { 0 };
  data->initialized = FALSE;
  data->offset      = (graphene_point_t) { 0 };
  data->scale       = (graphene_size_t) { 0 };
  data->opacity     = 0.0;
  g_ptr_array_add (self->stack, popup_data_ref (data));

  gtk_widget_set_parent (widget, GTK_WIDGET (self));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

void
bz_popup_present (GtkWidget *popup,
                  GtkWidget *source)
{
  GtkWidget *overlay = NULL;

  g_return_if_fail (GTK_IS_WIDGET (popup));
  g_return_if_fail (GTK_IS_WIDGET (source));

  overlay = gtk_widget_get_ancestor (source, BZ_TYPE_POPUP_OVERLAY);
  if (overlay == NULL)
    {
      g_critical ("source widget does not have an ancestor of type %s!",
                  g_type_name (BZ_TYPE_POPUP_OVERLAY));
      return;
    }

  bz_popup_overlay_push (BZ_POPUP_OVERLAY (overlay), popup, source);
}

static void
animate (BzPopupOverlay *self,
         const char     *key,
         double          value,
         PopupData      *data)
{
  switch (*key)
    {
    case 'x':
      data->offset.x = value;
      break;
    case 'y':
      data->offset.y = value;
      break;
    case 'w':
      data->scale.width = value;
      break;
    case 'h':
      data->scale.height = value;
      break;
    case 'o':
      data->opacity = value;
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
close_request_cb (BzPopupOverlay *self,
                  GtkRoot        *root)
{
  if (self->stack->len > 0)
    {
      g_ptr_array_remove_index (self->stack, self->stack->len - 1);
      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

/* End of bz-popup-overlay.c */

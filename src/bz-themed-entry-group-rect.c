/* bz-themed-entry-group-rect.c
 *
 * Copyright 2025 Eva M
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

#include "bz-themed-entry-group-rect.h"

struct _BzThemedEntryGroupRect
{
  GtkWidget parent_instance;

  GtkWidget    *child;
  BzEntryGroup *group;
};

G_DEFINE_FINAL_TYPE (BzThemedEntryGroupRect, bz_themed_entry_group_rect, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_CHILD,
  PROP_GROUP,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_themed_entry_group_rect_dispose (GObject *object)
{
  BzThemedEntryGroupRect *self = BZ_THEMED_ENTRY_GROUP_RECT (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  g_clear_pointer (&self->group, g_object_unref);

  G_OBJECT_CLASS (bz_themed_entry_group_rect_parent_class)->dispose (object);
}

static void
bz_themed_entry_group_rect_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  BzThemedEntryGroupRect *self = BZ_THEMED_ENTRY_GROUP_RECT (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, bz_themed_entry_group_rect_get_child (self));
      break;
    case PROP_GROUP:
      g_value_set_object (value, bz_themed_entry_group_rect_get_group (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_themed_entry_group_rect_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BzThemedEntryGroupRect *self = BZ_THEMED_ENTRY_GROUP_RECT (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      bz_themed_entry_group_rect_set_child (self, g_value_get_object (value));
      break;
    case PROP_GROUP:
      bz_themed_entry_group_rect_set_group (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_themed_entry_group_rect_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  BzThemedEntryGroupRect *self = BZ_THEMED_ENTRY_GROUP_RECT (widget);

  if (self->child != NULL)
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  gtk_widget_queue_draw (widget);
}

static void
bz_themed_entry_group_rect_snapshot (GtkWidget   *widget,
                                     GtkSnapshot *snapshot)
{
  BzThemedEntryGroupRect *self          = BZ_THEMED_ENTRY_GROUP_RECT (widget);
  double                  widget_width  = 0.0;
  double                  widget_height = 0.0;
  const char             *light_color  = NULL;
  const char             *dark_color   = NULL;
  GdkRGBA                 light_rgba   = { 0 };
  GdkRGBA                 dark_rgba    = { 0 };
  gboolean                is_dark      = FALSE;

  widget_width  = (double) gtk_widget_get_width (widget);
  widget_height = (double) gtk_widget_get_height (widget);

  if (self->group != NULL)
    {
      light_color = bz_entry_group_get_light_accent_color (self->group);
      dark_color  = bz_entry_group_get_dark_accent_color (self->group);
    }

  if (light_color != NULL)
    gdk_rgba_parse (&light_rgba, light_color);
  else
    gdk_rgba_parse (&light_rgba, "#ebebed");

  if (dark_color != NULL)
    gdk_rgba_parse (&dark_rgba, dark_color);
  else
    gdk_rgba_parse (&dark_rgba, "#2e2e32");

  is_dark = adw_style_manager_get_dark (adw_style_manager_get_default ());

  gtk_snapshot_push_opacity (snapshot, 0.70);
  gtk_snapshot_append_color (
      snapshot,
      is_dark ? &dark_rgba : &light_rgba,
      &GRAPHENE_RECT_INIT (0.0, 0.0, widget_width, widget_height));
  gtk_snapshot_pop (snapshot);

  if (self->child != NULL)
    gtk_widget_snapshot_child (widget, self->child, snapshot);
}

static void
bz_themed_entry_group_rect_class_init (BzThemedEntryGroupRectClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_themed_entry_group_rect_set_property;
  object_class->get_property = bz_themed_entry_group_rect_get_property;
  object_class->dispose      = bz_themed_entry_group_rect_dispose;

  widget_class->size_allocate = bz_themed_entry_group_rect_allocate;
  widget_class->snapshot      = bz_themed_entry_group_rect_snapshot;

  props[PROP_CHILD] =
      g_param_spec_object (
          "child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
dark_changed (BzThemedEntryGroupRect *self,
              GParamSpec             *pspec,
              AdwStyleManager        *mgr)
{
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
bz_themed_entry_group_rect_init (BzThemedEntryGroupRect *self)
{
  g_signal_connect_object (
      adw_style_manager_get_default (),
      "notify::dark",
      G_CALLBACK (dark_changed),
      self,
      G_CONNECT_SWAPPED);
}

GtkWidget *
bz_themed_entry_group_rect_new (void)
{
  return g_object_new (BZ_TYPE_THEMED_ENTRY_GROUP_RECT, NULL);
}

BzEntryGroup *
bz_themed_entry_group_rect_get_group (BzThemedEntryGroupRect *self)
{
  g_return_val_if_fail (BZ_IS_THEMED_ENTRY_GROUP_RECT (self), NULL);
  return self->group;
}

void
bz_themed_entry_group_rect_set_group (BzThemedEntryGroupRect *self,
                                      BzEntryGroup           *group)
{
  g_return_if_fail (BZ_IS_THEMED_ENTRY_GROUP_RECT (self));

  g_clear_pointer (&self->group, g_object_unref);
  if (group != NULL)
    self->group = g_object_ref (group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

GtkWidget *
bz_themed_entry_group_rect_get_child (BzThemedEntryGroupRect *self)
{
  g_return_val_if_fail (BZ_IS_THEMED_ENTRY_GROUP_RECT (self), NULL);
  return self->child;
}

void
bz_themed_entry_group_rect_set_child (BzThemedEntryGroupRect *self,
                                      GtkWidget              *child)
{
  g_return_if_fail (BZ_THEMED_ENTRY_GROUP_RECT (self));
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

/* End of bz-themed-entry-group-rect.c */

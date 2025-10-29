/* bz-context-tile.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-context-tile.h"
#include <glib/gi18n.h>

struct _BzContextTile
{
  GtkButton parent_instance;

  char *lozenge_style;

  /* Template widgets */
  GtkBox   *lozenge;
  GtkLabel *label;
};

G_DEFINE_FINAL_TYPE (BzContextTile, bz_context_tile, GTK_TYPE_BUTTON)

enum
{
  PROP_0,

  PROP_LOZENGE_CHILD,
  PROP_LABEL,
  PROP_LOZENGE_STYLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_context_tile_dispose (GObject *object)
{
  BzContextTile *self = BZ_CONTEXT_TILE (object);

  g_clear_pointer (&self->lozenge_style, g_free);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_CONTEXT_TILE);

  G_OBJECT_CLASS (bz_context_tile_parent_class)->dispose (object);
}

static void
bz_context_tile_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzContextTile *self = BZ_CONTEXT_TILE (object);

  switch (prop_id)
    {
    case PROP_LOZENGE_CHILD:
      g_value_set_object (value, bz_context_tile_get_lozenge_child (self));
      break;
    case PROP_LABEL:
      g_value_set_string (value, bz_context_tile_get_label (self));
      break;
    case PROP_LOZENGE_STYLE:
      g_value_set_string (value, bz_context_tile_get_lozenge_style (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_context_tile_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzContextTile *self = BZ_CONTEXT_TILE (object);

  switch (prop_id)
    {
    case PROP_LOZENGE_CHILD:
      bz_context_tile_set_lozenge_child (self, g_value_get_object (value));
      break;
    case PROP_LABEL:
      bz_context_tile_set_label (self, g_value_get_string (value));
      break;
    case PROP_LOZENGE_STYLE:
      bz_context_tile_set_lozenge_style (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_context_tile_class_init (BzContextTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_context_tile_set_property;
  object_class->get_property = bz_context_tile_get_property;
  object_class->dispose      = bz_context_tile_dispose;

  props[PROP_LOZENGE_CHILD] =
      g_param_spec_object (
          "lozenge-child",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_LABEL] =
      g_param_spec_string (
          "label",
          NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_LOZENGE_STYLE] =
      g_param_spec_string (
          "lozenge-style",
          NULL, NULL,
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-context-tile.ui");
  gtk_widget_class_bind_template_child (widget_class, BzContextTile, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzContextTile, label);
}

static void
on_enter_notify (GtkEventController *controller, gpointer user_data)
{
  g_autoptr (GdkCursor) cursor = gdk_cursor_new_from_name ("pointer", NULL);
  gtk_widget_set_cursor (GTK_WIDGET (user_data), cursor);
}

static void
on_leave_notify (GtkEventController *controller, gpointer user_data)
{
  gtk_widget_set_cursor (GTK_WIDGET (user_data), NULL);
}

static void
bz_context_tile_init (BzContextTile *self)
{
  GtkEventController *enter_leave = gtk_event_controller_motion_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (enter_leave, "enter", G_CALLBACK (on_enter_notify), GTK_WIDGET (self));
  g_signal_connect (enter_leave, "leave", G_CALLBACK (on_leave_notify), GTK_WIDGET (self));

  gtk_widget_add_controller (GTK_WIDGET (self), enter_leave);
}

BzContextTile *
bz_context_tile_new (void)
{
  return g_object_new (BZ_TYPE_CONTEXT_TILE, NULL);
}

GtkWidget *
bz_context_tile_get_lozenge_child (BzContextTile *self)
{
  g_return_val_if_fail (BZ_IS_CONTEXT_TILE (self), NULL);
  return gtk_widget_get_first_child (GTK_WIDGET (self->lozenge));
}

void
bz_context_tile_set_lozenge_child (BzContextTile *self,
                                   GtkWidget     *child)
{
  GtkWidget *old_child;

  g_return_if_fail (BZ_IS_CONTEXT_TILE (self));

  old_child = gtk_widget_get_first_child (GTK_WIDGET (self->lozenge));

  if (old_child == child)
    return;

  if (old_child != NULL)
    gtk_box_remove (self->lozenge, old_child);

  if (child != NULL)
    gtk_box_append (self->lozenge, child);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOZENGE_CHILD]);
}

const char *
bz_context_tile_get_label (BzContextTile *self)
{
  g_return_val_if_fail (BZ_IS_CONTEXT_TILE (self), NULL);
  return gtk_label_get_label (self->label);
}

void
bz_context_tile_set_label (BzContextTile *self,
                           const char    *label)
{
  g_return_if_fail (BZ_IS_CONTEXT_TILE (self));

  if (g_strcmp0 (gtk_label_get_label (self->label), label) == 0)
    return;

  gtk_label_set_label (self->label, label);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LABEL]);
}

const char *
bz_context_tile_get_lozenge_style (BzContextTile *self)
{
  g_return_val_if_fail (BZ_IS_CONTEXT_TILE (self), NULL);
  return self->lozenge_style;
}

void
bz_context_tile_set_lozenge_style (BzContextTile *self,
                                   const char    *style)
{
  g_return_if_fail (BZ_IS_CONTEXT_TILE (self));

  if (style != NULL && *style == '\0')
    style = NULL;

  if (g_strcmp0 (self->lozenge_style, style) == 0)
    return;

  if (self->lozenge_style != NULL)
    gtk_widget_remove_css_class (GTK_WIDGET (self->lozenge), self->lozenge_style);

  g_clear_pointer (&self->lozenge_style, g_free);

  if (style != NULL)
    {
      self->lozenge_style = g_strdup (style);
      gtk_widget_add_css_class (GTK_WIDGET (self->lozenge), self->lozenge_style);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOZENGE_STYLE]);
}

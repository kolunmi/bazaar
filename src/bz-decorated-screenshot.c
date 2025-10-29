/* bz-decorated-screenshot.c
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

#include "bz-decorated-screenshot.h"
#include "bz-screenshot.h"
#include <glib/gi18n.h>

struct _BzDecoratedScreenshot
{
  GtkButton parent_instance;

  BzAsyncTexture *async_texture;
  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzDecoratedScreenshot, bz_decorated_screenshot, GTK_TYPE_BUTTON)

enum
{
  PROP_0,

  PROP_ASYNC_TEXTURE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_decorated_screenshot_dispose (GObject *object)
{
  BzDecoratedScreenshot *self = BZ_DECORATED_SCREENSHOT (object);

  g_clear_pointer (&self->async_texture, g_object_unref);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_DECORATED_SCREENSHOT);

  G_OBJECT_CLASS (bz_decorated_screenshot_parent_class)->dispose (object);
}

static void
bz_decorated_screenshot_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzDecoratedScreenshot *self = BZ_DECORATED_SCREENSHOT (object);

  switch (prop_id)
    {
    case PROP_ASYNC_TEXTURE:
      g_value_set_object (value, bz_decorated_screenshot_get_async_texture (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_decorated_screenshot_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzDecoratedScreenshot *self = BZ_DECORATED_SCREENSHOT (object);

  switch (prop_id)
    {
    case PROP_ASYNC_TEXTURE:
      bz_decorated_screenshot_set_async_texture (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_decorated_screenshot_class_init (BzDecoratedScreenshotClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_decorated_screenshot_set_property;
  object_class->get_property = bz_decorated_screenshot_get_property;
  object_class->dispose      = bz_decorated_screenshot_dispose;

  props[PROP_ASYNC_TEXTURE] =
      g_param_spec_object (
          "async-texture",
          NULL, NULL,
          BZ_TYPE_ASYNC_TEXTURE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-decorated-screenshot.ui");
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
bz_decorated_screenshot_init (BzDecoratedScreenshot *self)
{
  GtkEventController *enter_leave = gtk_event_controller_motion_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (enter_leave, "enter", G_CALLBACK (on_enter_notify), GTK_WIDGET (self));
  g_signal_connect (enter_leave, "leave", G_CALLBACK (on_leave_notify), GTK_WIDGET (self));

  gtk_widget_add_controller (GTK_WIDGET (self), enter_leave);
}

BzDecoratedScreenshot *
bz_decorated_screenshot_new (void)
{
  return g_object_new (BZ_TYPE_DECORATED_SCREENSHOT, NULL);
}

BzAsyncTexture *
bz_decorated_screenshot_get_async_texture (BzDecoratedScreenshot *self)
{
  g_return_val_if_fail (BZ_IS_DECORATED_SCREENSHOT (self), NULL);
  return self->async_texture;
}

void
bz_decorated_screenshot_set_async_texture (BzDecoratedScreenshot *self,
                                           BzAsyncTexture        *async_texture)
{
  g_return_if_fail (BZ_IS_DECORATED_SCREENSHOT (self));

  g_clear_pointer (&self->async_texture, g_object_unref);
  if (async_texture != NULL)
    self->async_texture = g_object_ref (async_texture);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ASYNC_TEXTURE]);
}

/* End of bz-decorated-screenshot.c */

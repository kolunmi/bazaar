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
  GtkWidget parent_instance;

  BzAsyncTexture *async_texture;
  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzDecoratedScreenshot, bz_decorated_screenshot, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_ASYNC_TEXTURE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  CLICKED,

  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
bz_decorated_screenshot_dispose (GObject *object)
{
  BzDecoratedScreenshot *self = BZ_DECORATED_SCREENSHOT (object);

  g_clear_pointer (&self->async_texture, g_object_unref);

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
screenshot_clicked (BzDecoratedScreenshot *self,
                    GtkButton             *button)
{
  g_signal_emit (self, signals[CLICKED], 0);
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

  signals[CLICKED] =
      g_signal_new ("clicked",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    0);

  g_type_ensure (BZ_TYPE_SCREENSHOT);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-decorated-screenshot.ui");
  gtk_widget_class_bind_template_callback (widget_class, screenshot_clicked);
}

static void
bz_decorated_screenshot_init (BzDecoratedScreenshot *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
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

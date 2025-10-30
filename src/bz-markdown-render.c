/* bz-markdown-render.c
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

#include <md4c.h>

#include "bz-markdown-render.h"

struct _BzMarkdownRender
{
  AdwBin parent_instance;

  char    *markdown;
  gboolean selectable;

  /* Template widgets */
  GtkBox *box;
};

G_DEFINE_FINAL_TYPE (BzMarkdownRender, bz_markdown_render, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_MARKDOWN,
  PROP_SELECTABLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_markdown_render_dispose (GObject *object)
{
  BzMarkdownRender *self = BZ_MARKDOWN_RENDER (object);

  g_clear_pointer (&self->markdown, g_free);

  G_OBJECT_CLASS (bz_markdown_render_parent_class)->dispose (object);
}

static void
bz_markdown_render_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzMarkdownRender *self = BZ_MARKDOWN_RENDER (object);

  switch (prop_id)
    {
    case PROP_MARKDOWN:
      g_value_set_string (value, bz_markdown_render_get_markdown (self));
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, bz_markdown_render_get_selectable (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_markdown_render_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzMarkdownRender *self = BZ_MARKDOWN_RENDER (object);

  switch (prop_id)
    {
    case PROP_MARKDOWN:
      bz_markdown_render_set_markdown (self, g_value_get_string (value));
      break;
    case PROP_SELECTABLE:
      bz_markdown_render_set_selectable (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_markdown_render_class_init (BzMarkdownRenderClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_markdown_render_set_property;
  object_class->get_property = bz_markdown_render_get_property;
  object_class->dispose      = bz_markdown_render_dispose;

  props[PROP_MARKDOWN] =
      g_param_spec_string (
          "markdown",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SELECTABLE] =
      g_param_spec_boolean (
          "selectable",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-markdown-render.ui");
  gtk_widget_class_bind_template_child (widget_class, BzMarkdownRender, box);
}

static void
bz_markdown_render_init (BzMarkdownRender *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzMarkdownRender *
bz_markdown_render_new (void)
{
  return g_object_new (BZ_TYPE_MARKDOWN_RENDER, NULL);
}

const char *
bz_markdown_render_get_markdown (BzMarkdownRender *self)
{
  g_return_val_if_fail (BZ_IS_MARKDOWN_RENDER (self), NULL);
  return self->markdown;
}

gboolean
bz_markdown_render_get_selectable (BzMarkdownRender *self)
{
  g_return_val_if_fail (BZ_IS_MARKDOWN_RENDER (self), FALSE);
  return self->selectable;
}

void
bz_markdown_render_set_markdown (BzMarkdownRender *self,
                                 const char       *markdown)
{
  g_return_if_fail (BZ_IS_MARKDOWN_RENDER (self));

  g_clear_pointer (&self->markdown, g_free);
  if (markdown != NULL)
    self->markdown = g_strdup (markdown);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MARKDOWN]);
}

void
bz_markdown_render_set_selectable (BzMarkdownRender *self,
                                   gboolean          selectable)
{
  g_return_if_fail (BZ_IS_MARKDOWN_RENDER (self));

  self->selectable = selectable;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTABLE]);
}

/* End of bz-markdown-render.c */

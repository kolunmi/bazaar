/* bz-popup.c
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

#include "bz-popup.h"

G_DEFINE_ENUM_TYPE (
    BzPopupKind,
    bz_popup_kind,
    G_DEFINE_ENUM_VALUE (BZ_POPUP_KIND_CENTERED, "centered"),
    G_DEFINE_ENUM_VALUE (BZ_POPUP_KIND_ANCHORED, "anchored"));

typedef struct
{
  AdwBin parent_instance;

  BzPopupKind kind;
  int         content_width;
  int         content_height;
} BzPopupPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzPopup, bz_popup, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_KIND,
  PROP_CONTENT_WIDTH,
  PROP_CONTENT_HEIGHT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_popup_dispose (GObject *object)
{
  BzPopup *self = BZ_POPUP (object);

  (void) self;

  G_OBJECT_CLASS (bz_popup_parent_class)->dispose (object);
}

static void
bz_popup_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  BzPopup *self = BZ_POPUP (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, bz_popup_get_kind (self));
      break;
    case PROP_CONTENT_WIDTH:
      g_value_set_int (value, bz_popup_get_content_width (self));
      break;
    case PROP_CONTENT_HEIGHT:
      g_value_set_int (value, bz_popup_get_content_height (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_popup_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  BzPopup *self = BZ_POPUP (object);

  switch (prop_id)
    {
    case PROP_KIND:
      bz_popup_set_kind (self, g_value_get_enum (value));
      break;
    case PROP_CONTENT_WIDTH:
      bz_popup_set_content_width (self, g_value_get_int (value));
      break;
    case PROP_CONTENT_HEIGHT:
      bz_popup_set_content_height (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_popup_class_init (BzPopupClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_popup_set_property;
  object_class->get_property = bz_popup_get_property;
  object_class->dispose      = bz_popup_dispose;

  props[PROP_KIND] =
      g_param_spec_enum (
          "kind",
          NULL, NULL,
          BZ_TYPE_POPUP_KIND, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CONTENT_WIDTH] =
      g_param_spec_int (
          "content-width",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CONTENT_HEIGHT] =
      g_param_spec_int (
          "content-height",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "bzpopup");
}

static void
bz_popup_init (BzPopup *self)
{
}

BzPopup *
bz_popup_new (void)
{
  return g_object_new (BZ_TYPE_POPUP, NULL);
}

BzPopupKind
bz_popup_get_kind (BzPopup *self)
{
  BzPopupPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_POPUP (self), 0);

  priv = bz_popup_get_instance_private (self);
  return priv->kind;
}

int
bz_popup_get_content_width (BzPopup *self)
{
  BzPopupPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_POPUP (self), 0);

  priv = bz_popup_get_instance_private (self);
  return priv->content_width;
}

int
bz_popup_get_content_height (BzPopup *self)
{
  BzPopupPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_POPUP (self), 0);

  priv = bz_popup_get_instance_private (self);
  return priv->content_height;
}

void
bz_popup_set_kind (BzPopup    *self,
                   BzPopupKind kind)
{
  BzPopupPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_POPUP (self));

  priv = bz_popup_get_instance_private (self);

  if (kind == priv->kind)
    return;

  priv->kind = kind;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_KIND]);
}

void
bz_popup_set_content_width (BzPopup *self,
                            int      content_width)
{
  BzPopupPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_POPUP (self));

  priv = bz_popup_get_instance_private (self);

  if (content_width == priv->content_width)
    return;

  priv->content_width = content_width;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_WIDTH]);
}

void
bz_popup_set_content_height (BzPopup *self,
                             int      content_height)
{
  BzPopupPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_POPUP (self));

  priv = bz_popup_get_instance_private (self);

  if (content_height == priv->content_height)
    return;

  priv->content_height = content_height;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_HEIGHT]);
}

/* End of bz-popup.c */

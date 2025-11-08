/* bz-content-section.c
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

#include "bz-content-section.h"
#include "bz-entry-group.h"

G_DEFINE_ENUM_TYPE (
    BzContentSectionType,
    bz_content_section_type,
    G_DEFINE_ENUM_VALUE (BZ_CONTENT_SECTION_TYPE_CATEGORY, "category"),
    G_DEFINE_ENUM_VALUE (BZ_CONTENT_SECTION_TYPE_ARTICLE, "article"));

typedef struct
{
  char       *error;
  GListModel *classes;
  GListModel *light_classes;
  GListModel *dark_classes;

  BzContentSectionType section_type;

  char         *title;
  char         *subtitle;
  char         *description;
  GtkAlign      banner_text_halign;
  GtkAlign      banner_text_valign;
  double        banner_text_label_xalign;
  char         *light_banner;
  char         *dark_banner;
  int           banner_height;
  GtkContentFit banner_fit;
  GListModel   *groups;
  int           rows;
  char         *markdown;
} BzContentSectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BzContentSection, bz_content_section, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_ERROR,
  PROP_CLASSES,
  PROP_LIGHT_CLASSES,
  PROP_DARK_CLASSES,

  PROP_SECTION_TYPE,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_DESCRIPTION,
  PROP_APPIDS,

  PROP_BANNER_TEXT_HALIGN,
  PROP_BANNER_TEXT_VALIGN,
  PROP_BANNER_TEXT_LABEL_XALIGN,
  /* -> These are all weird like this for backwards compat */
  PROP_BANNER,
  PROP_LIGHT_BANNER,
  PROP_DARK_BANNER,
  PROP_BANNER_HEIGHT,
  PROP_BANNER_FIT,
  PROP_ROWS,

  PROP_MARKDOWN,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_content_section_dispose (GObject *object)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  g_clear_pointer (&priv->error, g_free);
  g_clear_object (&priv->classes);
  g_clear_object (&priv->light_classes);
  g_clear_object (&priv->dark_classes);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->subtitle, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->light_banner, g_free);
  g_clear_pointer (&priv->dark_banner, g_free);
  g_clear_object (&priv->groups);
  g_clear_pointer (&priv->markdown, g_free);

  G_OBJECT_CLASS (bz_content_section_parent_class)->dispose (object);
}

static void
bz_content_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ERROR:
      g_value_set_string (value, priv->error);
      break;
    case PROP_CLASSES:
      g_value_set_object (value, priv->classes);
      break;
    case PROP_LIGHT_CLASSES:
      g_value_set_object (value, priv->light_classes);
      break;
    case PROP_DARK_CLASSES:
      g_value_set_object (value, priv->dark_classes);
      break;
    case PROP_SECTION_TYPE:
      g_value_set_enum (value, priv->section_type);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, priv->subtitle);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_BANNER_TEXT_HALIGN:
      g_value_set_enum (value, priv->banner_text_halign);
      break;
    case PROP_BANNER_TEXT_VALIGN:
      g_value_set_enum (value, priv->banner_text_valign);
      break;
    case PROP_BANNER_TEXT_LABEL_XALIGN:
      g_value_set_double (value, priv->banner_text_label_xalign);
      break;
    case PROP_BANNER:
      g_value_set_string (
          value,
          priv->light_banner != NULL
              ? priv->light_banner
              : priv->dark_banner);
      break;
    case PROP_LIGHT_BANNER:
      g_value_set_string (
          value,
          priv->light_banner != NULL
              ? priv->light_banner
              : priv->dark_banner);
      break;
    case PROP_DARK_BANNER:
      g_value_set_string (
          value,
          priv->dark_banner != NULL
              ? priv->dark_banner
              : priv->light_banner);
      break;
    case PROP_BANNER_HEIGHT:
      g_value_set_int (value, priv->banner_height);
      break;
    case PROP_BANNER_FIT:
      g_value_set_enum (value, priv->banner_fit);
      break;
    case PROP_APPIDS:
      g_value_set_object (value, priv->groups);
      break;
    case PROP_ROWS:
      g_value_set_int (value, priv->rows);
      break;
    case PROP_MARKDOWN:
      g_value_set_string (value, priv->markdown);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_section_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzContentSection        *self = BZ_CONTENT_SECTION (object);
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ERROR:
      g_clear_pointer (&priv->error, g_free);
      priv->error = g_value_dup_string (value);
      break;
    case PROP_CLASSES:
      g_clear_object (&priv->classes);
      priv->classes = g_value_dup_object (value);
      break;
    case PROP_LIGHT_CLASSES:
      g_clear_object (&priv->light_classes);
      priv->light_classes = g_value_dup_object (value);
      break;
    case PROP_DARK_CLASSES:
      g_clear_object (&priv->dark_classes);
      priv->dark_classes = g_value_dup_object (value);
      break;
    case PROP_SECTION_TYPE:
      priv->section_type = g_value_get_enum (value);
      break;
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_SUBTITLE:
      g_clear_pointer (&priv->subtitle, g_free);
      priv->subtitle = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_BANNER_TEXT_HALIGN:
      priv->banner_text_halign = g_value_get_enum (value);
      break;
    case PROP_BANNER_TEXT_VALIGN:
      priv->banner_text_valign = g_value_get_enum (value);
      break;
    case PROP_BANNER_TEXT_LABEL_XALIGN:
      priv->banner_text_label_xalign = g_value_get_double (value);
      break;
    case PROP_BANNER:
      g_clear_pointer (&priv->light_banner, g_free);
      priv->light_banner = g_value_dup_string (value);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LIGHT_BANNER]);
      break;
    case PROP_LIGHT_BANNER:
      g_clear_pointer (&priv->light_banner, g_free);
      priv->light_banner = g_value_dup_string (value);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BANNER]);
      break;
    case PROP_DARK_BANNER:
      g_clear_pointer (&priv->dark_banner, g_free);
      priv->dark_banner = g_value_dup_string (value);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BANNER]);
      break;
    case PROP_BANNER_HEIGHT:
      priv->banner_height = g_value_get_int (value);
      break;
    case PROP_BANNER_FIT:
      priv->banner_fit = g_value_get_enum (value);
      break;
    case PROP_APPIDS:
      g_clear_object (&priv->groups);
      priv->groups = g_value_dup_object (value);
      break;
    case PROP_ROWS:
      priv->rows = g_value_get_int (value);
      break;
    case PROP_MARKDOWN:
      g_clear_pointer (&priv->markdown, g_free);
      priv->markdown = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_content_section_class_init (BzContentSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_content_section_set_property;
  object_class->get_property = bz_content_section_get_property;
  object_class->dispose      = bz_content_section_dispose;

  props[PROP_ERROR] =
      g_param_spec_string (
          "error",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_CLASSES] =
      g_param_spec_object (
          "classes",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_LIGHT_CLASSES] =
      g_param_spec_object (
          "light-classes",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_DARK_CLASSES] =
      g_param_spec_object (
          "dark-classes",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_SECTION_TYPE] =
      g_param_spec_enum (
          "section-type",
          NULL, NULL,
          BZ_TYPE_CONTENT_SECTION_TYPE,
          BZ_CONTENT_SECTION_TYPE_CATEGORY,
          G_PARAM_READWRITE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SUBTITLE] =
      g_param_spec_string (
          "subtitle",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_BANNER_TEXT_HALIGN] =
      g_param_spec_enum (
          "banner-text-halign",
          NULL, NULL,
          GTK_TYPE_ALIGN,
          GTK_ALIGN_START,
          G_PARAM_READWRITE);

  props[PROP_BANNER_TEXT_VALIGN] =
      g_param_spec_enum (
          "banner-text-valign",
          NULL, NULL,
          GTK_TYPE_ALIGN,
          GTK_ALIGN_START,
          G_PARAM_READWRITE);

  props[PROP_BANNER_TEXT_LABEL_XALIGN] =
      g_param_spec_double (
          "banner-text-label-xalign",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_BANNER] =
      g_param_spec_string (
          "banner",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_LIGHT_BANNER] =
      g_param_spec_string (
          "light-banner",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DARK_BANNER] =
      g_param_spec_string (
          "dark-banner",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_BANNER_HEIGHT] =
      g_param_spec_int (
          "banner-height",
          NULL, NULL,
          100, 1000, 300,
          G_PARAM_READWRITE);

  props[PROP_BANNER_FIT] =
      g_param_spec_enum (
          "banner-fit",
          NULL, NULL,
          GTK_TYPE_CONTENT_FIT,
          GTK_CONTENT_FIT_COVER,
          G_PARAM_READWRITE);

  props[PROP_APPIDS] =
      g_param_spec_object (
          "appids",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  /* Deprecated */
  props[PROP_ROWS] =
      g_param_spec_int (
          "rows",
          NULL, NULL,
          1, 16, 3,
          G_PARAM_READWRITE);

  props[PROP_MARKDOWN] =
      g_param_spec_string (
          "markdown",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_content_section_init (BzContentSection *self)
{
  BzContentSectionPrivate *priv = bz_content_section_get_instance_private (self);

  priv->section_type             = BZ_CONTENT_SECTION_TYPE_CATEGORY;
  priv->rows                     = 3;
  priv->banner_text_halign       = GTK_ALIGN_START;
  priv->banner_text_valign       = GTK_ALIGN_START;
  priv->banner_text_label_xalign = 0.0;
  priv->banner_height            = 300;
}

void
bz_content_section_notify_dark_light (BzContentSection *self)
{
  g_return_if_fail (BZ_IS_CONTENT_SECTION (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BANNER]);
}

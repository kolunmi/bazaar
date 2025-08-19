/* bz-release.c
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

#include "bz-release.h"

struct _BzRelease
{
  GObject parent_instance;

  GListModel *issues;
  guint64     timestamp;
  char       *url;
  char       *version;
};

G_DEFINE_FINAL_TYPE (BzRelease, bz_release, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_ISSUES,
  PROP_TIMESTAMP,
  PROP_URL,
  PROP_VERSION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_release_dispose (GObject *object)
{
  BzRelease *self = BZ_RELEASE (object);

  g_clear_pointer (&self->issues, g_object_unref);
  g_clear_pointer (&self->url, g_free);
  g_clear_pointer (&self->version, g_free);

  G_OBJECT_CLASS (bz_release_parent_class)->dispose (object);
}

static void
bz_release_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BzRelease *self = BZ_RELEASE (object);

  switch (prop_id)
    {
    case PROP_ISSUES:
      g_value_set_object (value, bz_release_get_issues (self));
      break;
    case PROP_TIMESTAMP:
      g_value_set_uint64 (value, bz_release_get_timestamp (self));
      break;
    case PROP_URL:
      g_value_set_string (value, bz_release_get_url (self));
      break;
    case PROP_VERSION:
      g_value_set_string (value, bz_release_get_version (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_release_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  BzRelease *self = BZ_RELEASE (object);

  switch (prop_id)
    {
    case PROP_ISSUES:
      bz_release_set_issues (self, g_value_get_object (value));
      break;
    case PROP_TIMESTAMP:
      bz_release_set_timestamp (self, g_value_get_uint64 (value));
      break;
    case PROP_URL:
      bz_release_set_url (self, g_value_get_string (value));
      break;
    case PROP_VERSION:
      bz_release_set_version (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_release_class_init (BzReleaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_release_set_property;
  object_class->get_property = bz_release_get_property;
  object_class->dispose      = bz_release_dispose;

  props[PROP_ISSUES] =
      g_param_spec_object (
          "issues",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TIMESTAMP] =
      g_param_spec_uint64 (
          "timestamp",
          NULL, NULL,
          0, G_MAXUINT64, (guint64) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_URL] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_VERSION] =
      g_param_spec_string (
          "version",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_release_init (BzRelease *self)
{
}

BzRelease *
bz_release_new (void)
{
  return g_object_new (BZ_TYPE_RELEASE, NULL);
}

GListModel *
bz_release_get_issues (BzRelease *self)
{
  g_return_val_if_fail (BZ_IS_RELEASE (self), NULL);
  return self->issues;
}

guint64
bz_release_get_timestamp (BzRelease *self)
{
  g_return_val_if_fail (BZ_IS_RELEASE (self), 0);
  return self->timestamp;
}

const char *
bz_release_get_url (BzRelease *self)
{
  g_return_val_if_fail (BZ_IS_RELEASE (self), NULL);
  return self->url;
}

const char *
bz_release_get_version (BzRelease *self)
{
  g_return_val_if_fail (BZ_IS_RELEASE (self), NULL);
  return self->version;
}

void
bz_release_set_issues (BzRelease  *self,
                       GListModel *issues)
{
  g_return_if_fail (BZ_IS_RELEASE (self));

  g_clear_pointer (&self->issues, g_object_unref);
  if (issues != NULL)
    self->issues = g_object_ref (issues);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ISSUES]);
}

void
bz_release_set_timestamp (BzRelease *self,
                          guint64    timestamp)
{
  g_return_if_fail (BZ_IS_RELEASE (self));

  self->timestamp = timestamp;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TIMESTAMP]);
}

void
bz_release_set_url (BzRelease  *self,
                    const char *url)
{
  g_return_if_fail (BZ_IS_RELEASE (self));

  g_clear_pointer (&self->url, g_free);
  if (url != NULL)
    self->url = g_strdup (url);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_URL]);
}

void
bz_release_set_version (BzRelease  *self,
                        const char *version)
{
  g_return_if_fail (BZ_IS_RELEASE (self));

  g_clear_pointer (&self->version, g_free);
  if (version != NULL)
    self->version = g_strdup (version);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_VERSION]);
}

/* End of bz-release.c */

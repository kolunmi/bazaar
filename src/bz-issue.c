/* bz-issue.c
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

#include "bz-issue.h"

struct _BzIssue
{
  GObject parent_instance;

  char *id;
  char *url;
};

G_DEFINE_FINAL_TYPE (BzIssue, bz_issue, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_ID,
  PROP_URL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_issue_dispose (GObject *object)
{
  BzIssue *self = BZ_ISSUE (object);

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (bz_issue_parent_class)->dispose (object);
}

static void
bz_issue_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  BzIssue *self = BZ_ISSUE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, bz_issue_get_id (self));
      break;
    case PROP_URL:
      g_value_set_string (value, bz_issue_get_url (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_issue_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  BzIssue *self = BZ_ISSUE (object);

  switch (prop_id)
    {
    case PROP_ID:
      bz_issue_set_id (self, g_value_get_string (value));
      break;
    case PROP_URL:
      bz_issue_set_url (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_issue_class_init (BzIssueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_issue_set_property;
  object_class->get_property = bz_issue_get_property;
  object_class->dispose      = bz_issue_dispose;

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_URL] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_issue_init (BzIssue *self)
{
}

BzIssue *
bz_issue_new (void)
{
  return g_object_new (BZ_TYPE_ISSUE, NULL);
}

const char *
bz_issue_get_id (BzIssue *self)
{
  g_return_val_if_fail (BZ_IS_ISSUE (self), NULL);
  return self->id;
}

const char *
bz_issue_get_url (BzIssue *self)
{
  g_return_val_if_fail (BZ_IS_ISSUE (self), NULL);
  return self->url;
}

void
bz_issue_set_id (BzIssue    *self,
                 const char *id)
{
  g_return_if_fail (BZ_IS_ISSUE (self));

  g_clear_pointer (&self->id, g_free);
  if (id != NULL)
    self->id = g_strdup (id);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ID]);
}

void
bz_issue_set_url (BzIssue    *self,
                  const char *url)
{
  g_return_if_fail (BZ_IS_ISSUE (self));

  g_clear_pointer (&self->url, g_free);
  if (url != NULL)
    self->url = g_strdup (url);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_URL]);
}

/* End of bz-issue.c */

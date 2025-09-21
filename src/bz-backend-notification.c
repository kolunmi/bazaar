/* bz-backend-notification.c
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

#include "bz-backend-notification.h"

G_DEFINE_ENUM_TYPE (
    BzBackendNotificationKind,
    bz_backend_notification_kind,
    G_DEFINE_ENUM_VALUE (BZ_BACKEND_NOTIFICATION_KIND_ANY, "any"),
    G_DEFINE_ENUM_VALUE (BZ_BACKEND_NOTIFICATION_KIND_INSTALLATION, "installation"),
    G_DEFINE_ENUM_VALUE (BZ_BACKEND_NOTIFICATION_KIND_UPDATE, "update"),
    G_DEFINE_ENUM_VALUE (BZ_BACKEND_NOTIFICATION_KIND_REMOVAL, "removal"));

struct _BzBackendNotification
{
  GObject parent_instance;

  BzBackendNotificationKind kind;
  BzEntry                  *entry;
  char                     *description;
};

G_DEFINE_FINAL_TYPE (BzBackendNotification, bz_backend_notification, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_KIND,
  PROP_ENTRY,
  PROP_DESCRIPTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_backend_notification_dispose (GObject *object)
{
  BzBackendNotification *self = BZ_BACKEND_NOTIFICATION (object);

  g_clear_pointer (&self->entry, g_object_unref);
  g_clear_pointer (&self->description, g_free);

  G_OBJECT_CLASS (bz_backend_notification_parent_class)->dispose (object);
}

static void
bz_backend_notification_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  BzBackendNotification *self = BZ_BACKEND_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, bz_backend_notification_get_kind (self));
      break;
    case PROP_ENTRY:
      g_value_set_object (value, bz_backend_notification_get_entry (self));
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, bz_backend_notification_get_description (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_backend_notification_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  BzBackendNotification *self = BZ_BACKEND_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_KIND:
      bz_backend_notification_set_kind (self, g_value_get_enum (value));
      break;
    case PROP_ENTRY:
      bz_backend_notification_set_entry (self, g_value_get_object (value));
      break;
    case PROP_DESCRIPTION:
      bz_backend_notification_set_description (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_backend_notification_class_init (BzBackendNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_backend_notification_set_property;
  object_class->get_property = bz_backend_notification_get_property;
  object_class->dispose      = bz_backend_notification_dispose;

  props[PROP_KIND] =
      g_param_spec_enum (
          "kind",
          NULL, NULL,
          BZ_TYPE_BACKEND_NOTIFICATION_KIND,
          BZ_BACKEND_NOTIFICATION_KIND_ANY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_backend_notification_init (BzBackendNotification *self)
{
}

BzBackendNotification *
bz_backend_notification_new (void)
{
  return g_object_new (BZ_TYPE_BACKEND_NOTIFICATION, NULL);
}

BzBackendNotificationKind
bz_backend_notification_get_kind (BzBackendNotification *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_NOTIFICATION (self), 0);
  return self->kind;
}

BzEntry *
bz_backend_notification_get_entry (BzBackendNotification *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_NOTIFICATION (self), NULL);
  return self->entry;
}

const char *
bz_backend_notification_get_description (BzBackendNotification *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_NOTIFICATION (self), NULL);
  return self->description;
}

void
bz_backend_notification_set_kind (BzBackendNotification    *self,
                                  BzBackendNotificationKind kind)
{
  g_return_if_fail (BZ_IS_BACKEND_NOTIFICATION (self));

  self->kind = kind;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_KIND]);
}

void
bz_backend_notification_set_entry (BzBackendNotification *self,
                                   BzEntry               *entry)
{
  g_return_if_fail (BZ_IS_BACKEND_NOTIFICATION (self));

  g_clear_pointer (&self->entry, g_object_unref);
  if (entry != NULL)
    self->entry = g_object_ref (entry);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY]);
}

void
bz_backend_notification_set_description (BzBackendNotification *self,
                                         const char            *description)
{
  g_return_if_fail (BZ_IS_BACKEND_NOTIFICATION (self));

  g_clear_pointer (&self->description, g_free);
  if (description != NULL)
    self->description = g_strdup (description);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DESCRIPTION]);
}

/* End of bz-backend-notification.c */

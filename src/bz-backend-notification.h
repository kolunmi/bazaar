/* bz-backend-notification.h
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

#pragma once

#include "bz-entry.h"

G_BEGIN_DECLS

typedef enum
{
  BZ_BACKEND_NOTIFICATION_KIND_ANY,
  BZ_BACKEND_NOTIFICATION_KIND_INSTALLATION,
  BZ_BACKEND_NOTIFICATION_KIND_UPDATE,
  BZ_BACKEND_NOTIFICATION_KIND_REMOVAL,
} BzBackendNotificationKind;

GType bz_backend_notification_kind_get_type (void);
#define BZ_TYPE_BACKEND_NOTIFICATION_KIND (bz_backend_notification_kind_get_type ())

#define BZ_TYPE_BACKEND_NOTIFICATION (bz_backend_notification_get_type ())
G_DECLARE_FINAL_TYPE (BzBackendNotification, bz_backend_notification, BZ, BACKEND_NOTIFICATION, GObject)

BzBackendNotification *
bz_backend_notification_new (void);

BzBackendNotificationKind
bz_backend_notification_get_kind (BzBackendNotification *self);

BzEntry *
bz_backend_notification_get_entry (BzBackendNotification *self);

const char *
bz_backend_notification_get_description (BzBackendNotification *self);

void
bz_backend_notification_set_kind (BzBackendNotification    *self,
                                  BzBackendNotificationKind kind);

void
bz_backend_notification_set_entry (BzBackendNotification *self,
                                   BzEntry               *entry);

void
bz_backend_notification_set_description (BzBackendNotification *self,
                                         const char            *description);

G_END_DECLS

/* End of bz-backend-notification.h */

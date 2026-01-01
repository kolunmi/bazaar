/* bz-app-permissions.h
 *
 * Copyright 2026 Alexander Vanhee
 * Copyright (C) 2022 Red Hat <www.redhat.com>
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

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BZ_TYPE_APP_PERMISSIONS (bz_app_permissions_get_type ())

G_DECLARE_FINAL_TYPE (BzAppPermissions, bz_app_permissions, BZ, APP_PERMISSIONS, GObject)

typedef enum
{
  BZ_APP_PERMISSIONS_FLAGS_NONE             = 0,
  BZ_APP_PERMISSIONS_FLAGS_NETWORK          = 1 << 0,
  BZ_APP_PERMISSIONS_FLAGS_SYSTEM_BUS       = 1 << 1,
  BZ_APP_PERMISSIONS_FLAGS_SESSION_BUS      = 1 << 2,
  BZ_APP_PERMISSIONS_FLAGS_DEVICES          = 1 << 3,
  BZ_APP_PERMISSIONS_FLAGS_HOME_FULL        = 1 << 4,
  BZ_APP_PERMISSIONS_FLAGS_HOME_READ        = 1 << 5,
  BZ_APP_PERMISSIONS_FLAGS_FILESYSTEM_FULL  = 1 << 6,
  BZ_APP_PERMISSIONS_FLAGS_FILESYSTEM_READ  = 1 << 7,
  BZ_APP_PERMISSIONS_FLAGS_DOWNLOADS_FULL   = 1 << 8,
  BZ_APP_PERMISSIONS_FLAGS_DOWNLOADS_READ   = 1 << 9,
  BZ_APP_PERMISSIONS_FLAGS_SETTINGS         = 1 << 10,
  BZ_APP_PERMISSIONS_FLAGS_X11              = 1 << 11,
  BZ_APP_PERMISSIONS_FLAGS_ESCAPE_SANDBOX   = 1 << 12,
  BZ_APP_PERMISSIONS_FLAGS_FILESYSTEM_OTHER = 1 << 13,
  BZ_APP_PERMISSIONS_FLAGS_BUS_POLICY_OTHER = 1 << 14,
  BZ_APP_PERMISSIONS_FLAGS_AUDIO_DEVICES    = 1 << 15,
  BZ_APP_PERMISSIONS_FLAGS_INPUT_DEVICES    = 1 << 16,
  BZ_APP_PERMISSIONS_FLAGS_SYSTEM_DEVICES   = 1 << 17,
  BZ_APP_PERMISSIONS_FLAGS_SCREEN           = 1 << 18,
} BzAppPermissionsFlags;

#define BZ_TYPE_APP_PERMISSIONS_FLAGS (bz_app_permissions_flags_get_type ())

typedef enum
{
  BZ_SAFETY_RATING_SAFE,
  BZ_SAFETY_RATING_NEUTRAL,
  BZ_SAFETY_RATING_PROBABLY_SAFE,
  BZ_SAFETY_RATING_POTENTIALLY_UNSAFE,
  BZ_SAFETY_RATING_UNSAFE,
} BzSafetyRating;

#define BZ_TYPE_SAFETY_RATING (bz_safety_rating_get_type ())

typedef enum
{
  BZ_FILESYSTEM_PATH_SYSTEM_ROOT,
  BZ_FILESYSTEM_PATH_HOME_SUBDIR,
  BZ_FILESYSTEM_PATH_HOST_OS,
  BZ_FILESYSTEM_PATH_HOST_ETC,
  BZ_FILESYSTEM_PATH_XDG_DESKTOP,
  BZ_FILESYSTEM_PATH_XDG_DOCUMENTS,
  BZ_FILESYSTEM_PATH_XDG_MUSIC,
  BZ_FILESYSTEM_PATH_XDG_PICTURES,
  BZ_FILESYSTEM_PATH_XDG_PUBLIC_SHARE,
  BZ_FILESYSTEM_PATH_XDG_VIDEOS,
  BZ_FILESYSTEM_PATH_XDG_TEMPLATES,
  BZ_FILESYSTEM_PATH_XDG_CACHE,
  BZ_FILESYSTEM_PATH_XDG_CONFIG,
  BZ_FILESYSTEM_PATH_XDG_DATA,
  BZ_FILESYSTEM_PATH_XDG_RUN,
  BZ_FILESYSTEM_PATH_CUSTOM,
} BzFilesystemPathType;

#define BZ_TYPE_FILESYSTEM_PATH_TYPE (bz_filesystem_path_type_get_type ())

typedef enum
{
  BZ_BUS_POLICY_PERMISSION_UNKNOWN = 0,
  BZ_BUS_POLICY_PERMISSION_NONE,
  BZ_BUS_POLICY_PERMISSION_SEE,
  BZ_BUS_POLICY_PERMISSION_TALK,
  BZ_BUS_POLICY_PERMISSION_OWN,
} BzBusPolicyPermission;

#define BZ_TYPE_BUS_POLICY_PERMISSION (bz_bus_policy_permission_get_type ())

typedef struct
{
  BzFilesystemPathType type;
  char                *subpath;
} BzFilesystemPath;

typedef struct
{
  GBusType              bus_type;
  char                 *bus_name;
  BzBusPolicyPermission permission;
} BzBusPolicy;

GType bz_app_permissions_flags_get_type (void) G_GNUC_CONST;
GType bz_safety_rating_get_type (void) G_GNUC_CONST;
GType bz_filesystem_path_type_get_type (void) G_GNUC_CONST;
GType bz_bus_policy_permission_get_type (void) G_GNUC_CONST;

BzFilesystemPath *bz_filesystem_path_new (BzFilesystemPathType type,
                                          const char          *subpath);
void              bz_filesystem_path_free (BzFilesystemPath *self);
char             *bz_filesystem_path_to_display_string (const BzFilesystemPath *path);

BzBusPolicy *bz_bus_policy_new (GBusType              bus_type,
                                const char           *bus_name,
                                BzBusPolicyPermission permission);
void         bz_bus_policy_free (BzBusPolicy *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BzFilesystemPath, bz_filesystem_path_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (BzBusPolicy, bz_bus_policy_free)

BzAppPermissions *bz_app_permissions_new (void);

BzAppPermissions *bz_app_permissions_new_from_metadata (GKeyFile *keyfile,
                                                        GError  **error);

void bz_app_permissions_seal (BzAppPermissions *self);

gboolean bz_app_permissions_is_sealed (BzAppPermissions *self);

gboolean bz_app_permissions_is_empty (BzAppPermissions *self);

void bz_app_permissions_set_flags (BzAppPermissions     *self,
                                   BzAppPermissionsFlags flags);

BzAppPermissionsFlags bz_app_permissions_get_flags (BzAppPermissions *self);

void bz_app_permissions_add_flag (BzAppPermissions     *self,
                                  BzAppPermissionsFlags flags);

void bz_app_permissions_remove_flag (BzAppPermissions     *self,
                                     BzAppPermissionsFlags flags);

void bz_app_permissions_add_filesystem_read (BzAppPermissions    *self,
                                             BzFilesystemPathType type,
                                             const char          *subpath);

const GPtrArray *bz_app_permissions_get_filesystem_read (BzAppPermissions *self);

void bz_app_permissions_add_filesystem_full (BzAppPermissions    *self,
                                             BzFilesystemPathType type,
                                             const char          *subpath);

const GPtrArray *bz_app_permissions_get_filesystem_full (BzAppPermissions *self);

void bz_app_permissions_add_bus_policy (BzAppPermissions     *self,
                                        GBusType              bus_type,
                                        const char           *bus_name,
                                        BzBusPolicyPermission permission);

const BzBusPolicy *const *bz_app_permissions_get_bus_policies (BzAppPermissions *self,
                                                               size_t           *out_n_bus_policies);

void bz_app_permissions_serialize (BzAppPermissions *self,
                                   GVariantBuilder  *builder);

gboolean bz_app_permissions_deserialize (BzAppPermissions *self,
                                         GVariant         *import,
                                         GError          **error);

G_END_DECLS

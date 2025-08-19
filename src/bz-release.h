/* bz-release.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_RELEASE (bz_release_get_type ())
G_DECLARE_FINAL_TYPE (BzRelease, bz_release, BZ, RELEASE, GObject)

BzRelease *
bz_release_new (void);

GListModel *
bz_release_get_issues (BzRelease *self);

guint64
bz_release_get_timestamp (BzRelease *self);

const char *
bz_release_get_url (BzRelease *self);

const char *
bz_release_get_version (BzRelease *self);

void
bz_release_set_issues (BzRelease  *self,
                       GListModel *issues);

void
bz_release_set_timestamp (BzRelease *self,
                          guint64    timestamp);

void
bz_release_set_url (BzRelease  *self,
                    const char *url);

void
bz_release_set_version (BzRelease  *self,
                        const char *version);

G_END_DECLS

/* End of bz-release.h */

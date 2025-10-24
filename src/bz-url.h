/* bz-url.h
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

#define BZ_TYPE_URL (bz_url_get_type ())
G_DECLARE_FINAL_TYPE (BzUrl, bz_url, BZ, URL, GObject)

BzUrl *
bz_url_new (void);

const char *
bz_url_get_name (BzUrl *self);

const char *
bz_url_get_url (BzUrl *self);

const char *
bz_url_get_icon_name (BzUrl *self);

void
bz_url_set_name (BzUrl      *self,
                 const char *name);

void
bz_url_set_url (BzUrl      *self,
                const char *url);

void
bz_url_set_icon_name (BzUrl      *self,
                      const char *icon_name);

G_END_DECLS

/* End of bz-url.h */

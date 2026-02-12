/* bz-proxy-resolver.h
 *
 * Copyright 2025 Adam Masciola
 * Copyright 2026 libffi <contact@ffi.lol>
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

// Include guard.
#pragma once

// Import external headers.
#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

// Declare our resolver type.
#define BZ_TYPE_PROXY_RESOLVER (bz_proxy_resolver_get_type ())
G_DECLARE_FINAL_TYPE (BzProxyResolver, bz_proxy_resolver, BZ, PROXY_RESOLVER, GSimpleProxyResolver)

BzProxyResolver *
bz_proxy_resolver_new (void);

G_END_DECLS
/* End of bz-proxy-resolver.h */

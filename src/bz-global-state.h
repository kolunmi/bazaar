/* bz-global-state.h
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

#include <libdex.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

DexFuture *
bz_send_with_global_http_session (SoupMessage *message);

DexFuture *
bz_send_with_global_http_session_then_splice_into (SoupMessage   *message,
                                                   GOutputStream *output);

DexFuture *
bz_query_flathub_v2_json (const char *request);

DexFuture *
bz_query_flathub_v2_json_take (char *request);

G_END_DECLS

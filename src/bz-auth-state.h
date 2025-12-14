/* bz-auth-state.h
 *
 * Copyright 2025 Alexander Vanhee
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

#include <gdk/gdk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BZ_TYPE_AUTH_STATE (bz_auth_state_get_type ())

G_DECLARE_FINAL_TYPE (BzAuthState, bz_auth_state, BZ, AUTH_STATE, GObject)

BzAuthState *bz_auth_state_new (void);

const char *
bz_auth_state_get_name (BzAuthState *self);

const char *
bz_auth_state_get_token (BzAuthState *self);

const char *
bz_auth_state_get_profile_icon_url (BzAuthState *self);

gboolean
bz_auth_state_is_authenticated (BzAuthState *self);

GdkPaintable *
bz_auth_state_get_paintable (BzAuthState *self);

void
bz_auth_state_set_authenticated (BzAuthState *self,
                                 const char  *name,
                                 const char  *token,
                                 const char  *profile_icon_url);

void
bz_auth_state_clear (BzAuthState *self);

G_END_DECLS

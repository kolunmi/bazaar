/* bge-version-macros.h
 *
 * Copyright 2026 Eva M
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

#include "bge-version.h"

#ifndef _BGE_EXTERN
#define _BGE_EXTERN extern
#endif

#define BGE_VERSION_CUR_STABLE (G_ENCODE_VERSION (BGE_MAJOR_VERSION, 0))

#ifdef BGE_DISABLE_DEPRECATION_WARNINGS
#define BGE_DEPRECATED            _BGE_EXTERN
#define BGE_DEPRECATED_FOR(f)     _BGE_EXTERN
#define BGE_UNAVAILABLE(maj, min) _BGE_EXTERN
#else
#define BGE_DEPRECATED            G_DEPRECATED _BGE_EXTERN
#define BGE_DEPRECATED_FOR(f)     G_DEPRECATED_FOR (f) _BGE_EXTERN
#define BGE_UNAVAILABLE(maj, min) G_UNAVAILABLE (maj, min) _BGE_EXTERN
#endif

#define BGE_VERSION_1_0 (G_ENCODE_VERSION (1, 0))

#if BGE_MAJOR_VERSION == BGE_VERSION_1_0
#define BGE_VERSION_PREV_STABLE (BGE_VERSION_1_0)
#else
#define BGE_VERSION_PREV_STABLE (G_ENCODE_VERSION (BGE_MAJOR_VERSION - 1, 0))
#endif

#ifndef BGE_VERSION_MIN_REQUIRED
#define BGE_VERSION_MIN_REQUIRED (BGE_VERSION_CUR_STABLE)
#endif

#ifndef BGE_VERSION_MAX_ALLOWED
#if BGE_VERSION_MIN_REQUIRED > BGE_VERSION_PREV_STABLE
#define BGE_VERSION_MAX_ALLOWED (BGE_VERSION_MIN_REQUIRED)
#else
#define BGE_VERSION_MAX_ALLOWED (BGE_VERSION_CUR_STABLE)
#endif
#endif

#define BGE_AVAILABLE_IN_ALL _BGE_EXTERN

#if BGE_VERSION_MIN_REQUIRED >= BGE_VERSION_1_0
#define BGE_DEPRECATED_IN_1_0        BGE_DEPRECATED
#define BGE_DEPRECATED_IN_1_0_FOR(f) BGE_DEPRECATED_FOR (f)
#else
#define BGE_DEPRECATED_IN_1_0        _BGE_EXTERN
#define BGE_DEPRECATED_IN_1_0_FOR(f) _BGE_EXTERN
#endif
#if BGE_VERSION_MAX_ALLOWED < BGE_VERSION_1_0
#define BGE_AVAILABLE_IN_1_0 BGE_UNAVAILABLE (1, 0)
#else
#define BGE_AVAILABLE_IN_1_0 _BGE_EXTERN
#endif

/* bz-decorated-screenshot.h
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

#include <adwaita.h>

#include "bz-async-texture.h"

G_BEGIN_DECLS

#define BZ_TYPE_DECORATED_SCREENSHOT (bz_decorated_screenshot_get_type ())
G_DECLARE_FINAL_TYPE (BzDecoratedScreenshot, bz_decorated_screenshot, BZ, DECORATED_SCREENSHOT, AdwBin)

BzDecoratedScreenshot *
bz_decorated_screenshot_new (void);

BzAsyncTexture *
bz_decorated_screenshot_get_async_texture (BzDecoratedScreenshot *self);

void
bz_decorated_screenshot_set_async_texture (BzDecoratedScreenshot *self,
                                           BzAsyncTexture        *async_texture);

G_END_DECLS

/* End of bz-decorated-screenshot.h */

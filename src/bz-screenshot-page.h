/* bz-screenshot-page.h
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

#include "bz-async-texture.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define BZ_TYPE_SCREENSHOT_PAGE (bz_screenshot_page_get_type ())

G_DECLARE_FINAL_TYPE (BzScreenshotPage, bz_screenshot_page, BZ, SCREENSHOT_PAGE, AdwNavigationPage)

AdwNavigationPage *bz_screenshot_page_new (GListModel *screenshots,
                                           GListModel *captions,
                                           guint       initial_index);

void
bz_screenshot_page_set_captions (BzScreenshotPage *self,
                                 GListModel       *captions);

const char *
bz_screenshot_page_get_current_caption (BzScreenshotPage *self);

G_END_DECLS

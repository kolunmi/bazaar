/* bz-screenshot.h
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

#define BZ_TYPE_SCREENSHOT (bz_screenshot_get_type ())
G_DECLARE_FINAL_TYPE (BzScreenshot, bz_screenshot, BZ, SCREENSHOT, GtkWidget)

GtkWidget *
bz_screenshot_new (void);

void
bz_screenshot_set_paintable (BzScreenshot *self,
                             GdkPaintable *paintable);

GdkPaintable *
bz_screenshot_get_paintable (BzScreenshot *self);

void
bz_screenshot_set_focus_x (BzScreenshot *self,
                           double        focus_x);

double
bz_screenshot_get_focus_x (BzScreenshot *self);

void
bz_screenshot_set_focus_y (BzScreenshot *self,
                           double        focus_y);

double
bz_screenshot_get_focus_y (BzScreenshot *self);

void
bz_screenshot_set_rounded_corners (BzScreenshot *self,
                                   gboolean      rounded_corners);
gboolean
bz_screenshot_get_rounded_corners (BzScreenshot *self);

void
bz_screenshot_set_top_half (BzScreenshot *self,
                            gboolean      top_half);

gboolean
bz_screenshot_get_top_half (BzScreenshot *self);

G_END_DECLS

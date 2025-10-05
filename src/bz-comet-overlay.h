/* bz-comet-overlay.h
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

#include "bz-comet.h"

G_BEGIN_DECLS

#define BZ_TYPE_COMET_OVERLAY (bz_comet_overlay_get_type ())
G_DECLARE_FINAL_TYPE (BzCometOverlay, bz_comet_overlay, BZ, COMET_OVERLAY, GtkWidget)

GtkWidget *
bz_comet_overlay_new (void);

void
bz_comet_overlay_set_child (BzCometOverlay *self,
                            GtkWidget      *child);

GtkWidget *
bz_comet_overlay_get_child (BzCometOverlay *self);

void
bz_comet_overlay_set_pulse_color (BzCometOverlay *self,
                                  GdkRGBA        *color);
GdkRGBA *
bz_comet_overlay_get_pulse_color (BzCometOverlay *self);

void
bz_comet_overlay_spawn (BzCometOverlay *self,
                        BzComet        *comet);

void
bz_comet_overlay_pulse_child (BzCometOverlay *self,
                              GtkWidget      *child);

G_END_DECLS

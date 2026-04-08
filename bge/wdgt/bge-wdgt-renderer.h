/* bge-wdgt-renderer.h
 *
 * Copyright 2026 Eva M
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

#ifndef BGE_INSIDE
#error "Only <bge.h> can be included directly."
#endif

#include "bge-wdgt-spec.h"

G_BEGIN_DECLS

#define BGE_TYPE_WDGT_RENDERER (bge_wdgt_renderer_get_type ())
G_DECLARE_FINAL_TYPE (BgeWdgtRenderer, bge_wdgt_renderer, BGE, WDGT_RENDERER, GtkWidget)

BgeWdgtRenderer *
bge_wdgt_renderer_new (void);

BgeWdgtSpec *
bge_wdgt_renderer_get_spec (BgeWdgtRenderer *self);

const char *
bge_wdgt_renderer_get_state (BgeWdgtRenderer *self);

GObject *
bge_wdgt_renderer_get_reference (BgeWdgtRenderer *self);

GtkWidget *
bge_wdgt_renderer_get_child (BgeWdgtRenderer *self);

void
bge_wdgt_renderer_set_spec (BgeWdgtRenderer *self,
                            BgeWdgtSpec     *spec);

void
bge_wdgt_renderer_set_state (BgeWdgtRenderer *self,
                             const char      *state);

void
bge_wdgt_renderer_set_reference (BgeWdgtRenderer *self,
                                 GObject         *reference);

void
bge_wdgt_renderer_set_child (BgeWdgtRenderer *self,
                             GtkWidget       *child);

void
bge_wdgt_renderer_set_state_take (BgeWdgtRenderer *self,
                                  char            *state);

#define bge_wdgt_renderer_set_state_take_printf(self, ...) bge_wdgt_renderer_set_state_take (self, g_strdup_printf (__VA_ARGS__))

G_END_DECLS

/* End of bge-wdgt-renderer.h */

/* bge-animation.h
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

#include "bge-version-macros.h"

G_BEGIN_DECLS

#define BGE_TYPE_ANIMATION (bge_animation_get_type ())
G_DECLARE_FINAL_TYPE (BgeAnimation, bge_animation, BGE, ANIMATION, GObject)

typedef void (*BgeAnimationCallback) (GtkWidget  *widget,
                                      const char *key,
                                      double      value,
                                      gpointer    user_data);

BGE_AVAILABLE_IN_ALL
BgeAnimation *
bge_animation_new (GtkWidget *widget);

BGE_AVAILABLE_IN_ALL
GtkWidget *
bge_animation_dup_widget (BgeAnimation *self);

BGE_AVAILABLE_IN_ALL
void
bge_animation_add_spring (BgeAnimation        *self,
                          const char          *key,
                          double               from,
                          double               to,
                          double               damping_ratio,
                          double               mass,
                          double               stiffness,
                          BgeAnimationCallback cb,
                          gpointer             user_data,
                          GDestroyNotify       destroy_data);

BGE_AVAILABLE_IN_ALL
void
bge_animation_cancel (BgeAnimation *self,
                      const char   *key);

BGE_AVAILABLE_IN_ALL
void
bge_animation_cancel_all (BgeAnimation *self);

G_END_DECLS

/* bz-animation.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BZ_TYPE_ANIMATION (bz_animation_get_type ())
G_DECLARE_FINAL_TYPE (BzAnimation, bz_animation, BZ, ANIMATION, GObject)

typedef void (*BzAnimationCallback) (GtkWidget  *widget,
                                     const char *key,
                                     double      value,
                                     gpointer    user_data);

BzAnimation *
bz_animation_new (GtkWidget *widget);

GtkWidget *
bz_animation_dup_widget (BzAnimation *self);

void
bz_animation_add_spring (BzAnimation        *self,
                         const char         *key,
                         double              from,
                         double              to,
                         double              damping_ratio,
                         double              mass,
                         double              stiffness,
                         BzAnimationCallback cb,
                         gpointer            user_data,
                         GDestroyNotify      destroy_data);

void
bz_animation_cancel (BzAnimation *self,
                     const char  *key);

void
bz_animation_cancel_all (BzAnimation *self);

G_END_DECLS

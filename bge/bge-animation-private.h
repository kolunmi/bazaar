/* bge-animation-private.h
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

/* Copied with modifications from libadwaita */
double
spring_oscillate (double  damping,
                  double  mass,
                  double  stiffness,
                  double  from,
                  double  to,
                  double  time,
                  double *velocity);

/* Copied with modifications from libadwaita */
double
spring_get_first_zero (double damping,
                       double mass,
                       double stiffness,
                       double from,
                       double to);

/* Copied with modifications from libadwaita */
double
spring_calculate_duration (double   damping,
                           double   mass,
                           double   stiffness,
                           double   from,
                           double   to,
                           gboolean clamp);

gboolean
bge_should_animate (GtkWidget *widget);

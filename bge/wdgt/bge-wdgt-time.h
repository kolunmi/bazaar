/* bge-wdgt-time.h
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

G_BEGIN_DECLS

#define BGE_TYPE_WDGT_TIME (bge_wdgt_time_get_type ())
G_DECLARE_FINAL_TYPE (BgeWdgtTime, bge_wdgt_time, BGE, WDGT_TIME, GObject)

BGE_AVAILABLE_IN_ALL
BgeWdgtTime *
bge_wdgt_time_new (void);

BGE_AVAILABLE_IN_ALL
double
bge_wdgt_time_get_time (BgeWdgtTime *self);

BGE_AVAILABLE_IN_ALL
guint
bge_wdgt_time_get_notify_msec (BgeWdgtTime *self);

BGE_AVAILABLE_IN_ALL
void
bge_wdgt_time_set_notify_msec (BgeWdgtTime *self,
                               guint        notify_msec);

G_END_DECLS

/* End of bge-wdgt-time.h */

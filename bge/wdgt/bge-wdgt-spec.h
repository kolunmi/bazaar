/* bge-wdgt-spec.h
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

typedef enum
{
  BGE_WDGT_SPECIAL_VALUE_MOTION_X,
  BGE_WDGT_SPECIAL_VALUE_MOTION_Y,
} BgeWdgtSpecialValue;

#define BGE_TYPE_WDGT_SPEC (bge_wdgt_spec_get_type ())
G_DECLARE_FINAL_TYPE (BgeWdgtSpec, bge_wdgt_spec, BGE, WDGT_SPEC, GObject)

BgeWdgtSpec *
bge_wdgt_spec_new (void);

const char *
bge_wdgt_spec_get_name (BgeWdgtSpec *self);

void
bge_wdgt_spec_set_name (BgeWdgtSpec *self,
                        const char  *name);

void
bge_wdgt_spec_set_name_take (BgeWdgtSpec *self,
                             char        *name);

#define bge_wdgt_spec_set_name_take_printf(self, ...) bge_wdgt_spec_set_name_take (self, g_strdup_printf (__VA_ARGS__))

void
bge_wdgt_spec_add_constant_source_value (BgeWdgtSpec  *self,
                                         const char   *name,
                                         const GValue *constant);

void
bge_wdgt_spec_add_special_source_value (BgeWdgtSpec        *self,
                                        const char         *name,
                                        BgeWdgtSpecialValue kind);

void
bge_wdgt_spec_add_property_value (BgeWdgtSpec *self,
                                  const char  *name,
                                  const char  *child,
                                  const char  *property);

void
bge_wdgt_spec_add_child (BgeWdgtSpec *self,
                         GType        type,
                         const char  *name);

void
bge_wdgt_spec_add_state (BgeWdgtSpec *self,
                         const char  *name);

void
bge_wdgt_spec_set_value (BgeWdgtSpec *self,
                         const char  *state,
                         const char  *dest_value,
                         const char  *src_value);

G_END_DECLS

/* End of bge-wdgt-spec.h */

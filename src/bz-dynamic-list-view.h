/* bz-dynamic-list-view.h
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

G_BEGIN_DECLS

typedef enum
{
  BZ_DYNAMIC_LIST_VIEW_KIND_HBOX,
  BZ_DYNAMIC_LIST_VIEW_KIND_VBOX,
  BZ_DYNAMIC_LIST_VIEW_KIND_LIST_BOX,
  BZ_DYNAMIC_LIST_VIEW_KIND_FLOW_BOX,
  BZ_DYNAMIC_LIST_VIEW_KIND_CAROUSEL,

  /*< private >*/
  BZ_DYNAMIC_LIST_VIEW_N_KINDS,
} BzDynamicListViewKind;

GType bz_dynamic_list_view_kind_get_type (void);
#define BZ_TYPE_DYNAMIC_LIST_VIEW_KIND (bz_dynamic_list_view_kind_get_type ())

#define BZ_TYPE_DYNAMIC_LIST_VIEW (bz_dynamic_list_view_get_type ())
G_DECLARE_FINAL_TYPE (BzDynamicListView, bz_dynamic_list_view, BZ, DYNAMIC_LIST_VIEW, AdwBin)

BzDynamicListView *
bz_dynamic_list_view_new (void);

GListModel *
bz_dynamic_list_view_get_model (BzDynamicListView *self);

gboolean
bz_dynamic_list_view_get_scroll (BzDynamicListView *self);

BzDynamicListViewKind
bz_dynamic_list_view_get_noscroll_kind (BzDynamicListView *self);

const char *
bz_dynamic_list_view_get_child_type (BzDynamicListView *self);

const char *
bz_dynamic_list_view_get_child_prop (BzDynamicListView *self);

const char *
bz_dynamic_list_view_get_object_prop (BzDynamicListView *self);

void
bz_dynamic_list_view_set_model (BzDynamicListView *self,
                                GListModel        *model);

void
bz_dynamic_list_view_set_scroll (BzDynamicListView *self,
                                 gboolean           scroll);

void
bz_dynamic_list_view_set_noscroll_kind (BzDynamicListView    *self,
                                        BzDynamicListViewKind noscroll_kind);

void
bz_dynamic_list_view_set_child_type (BzDynamicListView *self,
                                     const char        *child_type);

void
bz_dynamic_list_view_set_child_prop (BzDynamicListView *self,
                                     const char        *child_prop);

void
bz_dynamic_list_view_set_object_prop (BzDynamicListView *self,
                                      const char        *object_prop);

guint
bz_dynamic_list_view_get_max_children_per_line (BzDynamicListView *self);

void
bz_dynamic_list_view_set_max_children_per_line (BzDynamicListView *self,
                                                guint              max_children);

GtkAdjustment *bz_dynamic_list_view_get_vadjustment (BzDynamicListView *self);

G_END_DECLS

/* End of bz-dynamic-list-view.h */

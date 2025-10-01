/* bz-data-graph.h
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

#define BZ_TYPE_DATA_GRAPH (bz_data_graph_get_type ())
G_DECLARE_FINAL_TYPE (BzDataGraph, bz_data_graph, BZ, DATA_GRAPH, GtkWidget)

GtkWidget *
bz_data_graph_new (void);

GListModel *
bz_data_graph_get_model (BzDataGraph *self);

const char *
bz_data_graph_get_independent_axis_label (BzDataGraph *self);

const char *
bz_data_graph_get_dependent_axis_label (BzDataGraph *self);

int
bz_data_graph_get_independent_decimals (BzDataGraph *self);

int
bz_data_graph_get_dependent_decimals (BzDataGraph *self);

double
bz_data_graph_get_transition_progress (BzDataGraph *self);

void
bz_data_graph_set_model (BzDataGraph *self,
                         GListModel  *model);

void
bz_data_graph_set_independent_axis_label (BzDataGraph *self,
                                          const char  *independent_axis_label);

void
bz_data_graph_set_dependent_axis_label (BzDataGraph *self,
                                        const char  *dependent_axis_label);

void
bz_data_graph_set_independent_decimals (BzDataGraph *self,
                                        int          independent_decimals);

void
bz_data_graph_set_dependent_decimals (BzDataGraph *self,
                                      int          dependent_decimals);

const char *bz_data_graph_get_tooltip_prefix (BzDataGraph *self);

void bz_data_graph_set_tooltip_prefix (BzDataGraph *self,
                                       const char *tooltip_prefix);

void
bz_data_graph_set_transition_progress (BzDataGraph *self,
                                       double       transition_progress);

void
bz_data_graph_animate_open (BzDataGraph *self);

G_END_DECLS


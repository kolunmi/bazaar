/* bz-content-provider.h
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

#include "bz-parser.h"

G_BEGIN_DECLS

#define BZ_TYPE_CONTENT_PROVIDER (bz_content_provider_get_type ())
G_DECLARE_FINAL_TYPE (BzContentProvider, bz_content_provider, BZ, CONTENT_PROVIDER, GObject)

BzContentProvider *
bz_content_provider_new (void);

void
bz_content_provider_set_input_files (BzContentProvider *self,
                                     GListModel        *input_files);

GListModel *
bz_content_provider_get_input_files (BzContentProvider *self);

void
bz_content_provider_set_parser (BzContentProvider *self,
                                BzParser          *parser);

BzParser *
bz_content_provider_get_parser (BzContentProvider *self);

gboolean
bz_content_provider_get_has_inputs (BzContentProvider *self);

G_END_DECLS

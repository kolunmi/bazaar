/* bge.h - Bazaar GTK Extensions
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
#include <libdex.h>

G_BEGIN_DECLS

#define BGE_INSIDE
#include "bge-version-macros.h"

#include "bge-animation.h"
#include "bge-carousel.h"
#include "bge-markdown-render.h"
#include "wdgt/bge-wdgt-spec-renderer.h"
#include "wdgt/bge-wdgt-spec.h"
#undef BGE_INSIDE

BGE_AVAILABLE_IN_ALL
void
bge_init (void);

G_END_DECLS

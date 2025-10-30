/* bz-markdown-render.h
 *
 * Copyright 2025 Eva M
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

#define BZ_TYPE_MARKDOWN_RENDER (bz_markdown_render_get_type ())
G_DECLARE_FINAL_TYPE (BzMarkdownRender, bz_markdown_render, BZ, MARKDOWN_RENDER, AdwBin)

BzMarkdownRender *
bz_markdown_render_new (void);

const char *
bz_markdown_render_get_markdown (BzMarkdownRender *self);

gboolean
bz_markdown_render_get_selectable (BzMarkdownRender *self);

void
bz_markdown_render_set_markdown (BzMarkdownRender *self,
                                 const char       *markdown);

void
bz_markdown_render_set_selectable (BzMarkdownRender *self,
                                   gboolean          selectable);

G_END_DECLS

/* End of bz-markdown-render.h */

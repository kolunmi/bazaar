/* bz-appstream-description-render.h
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

#define BZ_TYPE_APPSTREAM_DESCRIPTION_RENDER (bz_appstream_description_render_get_type ())
G_DECLARE_FINAL_TYPE (BzAppstreamDescriptionRender, bz_appstream_description_render, BZ, APPSTREAM_DESCRIPTION_RENDER, AdwBin)

BzAppstreamDescriptionRender *
bz_appstream_description_render_new (void);

const char *
bz_appstream_description_render_get_appstream_description (BzAppstreamDescriptionRender *self);

gboolean
bz_appstream_description_render_get_selectable (BzAppstreamDescriptionRender *self);

void
bz_appstream_description_render_set_appstream_description (BzAppstreamDescriptionRender *self,
                                                           const char                   *appstream_description);

void
bz_appstream_description_render_set_selectable (BzAppstreamDescriptionRender *self,
                                                gboolean                      selectable);

G_END_DECLS

/* End of bz-appstream-description-render.h */

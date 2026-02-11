/* bz-popup.h
 *
 * Copyright 2026 AUTOGEN
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
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  BZ_POPUP_KIND_CENTERED,
  BZ_POPUP_KIND_ANCHORED,
} BzPopupKind;
GType bz_popup_kind_get_type (void);
#define BZ_TYPE_POPUP_KIND (bz_popup_kind_get_type ())

struct _BzPopupClass
{
  AdwBinClass parent_class;
};

#define BZ_TYPE_POPUP (bz_popup_get_type ())
G_DECLARE_DERIVABLE_TYPE (BzPopup, bz_popup, BZ, POPUP, AdwBin)

BzPopup *
bz_popup_new (void);

BzPopupKind
bz_popup_get_kind (BzPopup *self);

int
bz_popup_get_content_width (BzPopup *self);

int
bz_popup_get_content_height (BzPopup *self);

void
bz_popup_set_kind (BzPopup    *self,
                   BzPopupKind kind);

void
bz_popup_set_content_width (BzPopup *self,
                            int      content_width);

void
bz_popup_set_content_height (BzPopup *self,
                             int      content_height);

G_END_DECLS

/* End of bz-popup.h */

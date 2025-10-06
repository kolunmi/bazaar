/* bz-country-data-point.h
 *
 * Copyright 2025 Alexander Vanhee
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

#define BZ_TYPE_COUNTRY_DATA_POINT (bz_country_data_point_get_type ())

G_DECLARE_FINAL_TYPE (BzCountryDataPoint, bz_country_data_point, BZ, COUNTRY_DATA_POINT, GObject)

BzCountryDataPoint *
bz_country_data_point_new (void);

const char *
bz_country_data_point_get_country_code (BzCountryDataPoint *self);

guint
bz_country_data_point_get_downloads (BzCountryDataPoint *self);

void
bz_country_data_point_set_country_code (BzCountryDataPoint *self,
                                        const char         *country_code);

void
bz_country_data_point_set_downloads (BzCountryDataPoint *self,
                                     guint               downloads);

G_END_DECLS

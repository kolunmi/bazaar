/* bz-country.h
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

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define BZ_TYPE_COUNTRY (bz_country_get_type ())

G_DECLARE_FINAL_TYPE (BzCountry, bz_country, BZ, COUNTRY, GObject)

BzCountry *
bz_country_new (void);

const char *
bz_country_get_name (BzCountry *self);

const char *
bz_country_get_iso_code (BzCountry *self);

JsonArray *
bz_country_get_coordinates (BzCountry *self);

double
bz_country_get_value (BzCountry *self);

void
bz_country_set_name (BzCountry  *self,
                     const char *name);
void
bz_country_set_iso_code (BzCountry  *self,
                         const char *iso_code);

void
bz_country_set_coordinates (BzCountry *self,
                            JsonArray *coordinates);

void
bz_country_set_value (BzCountry *self,
                      double     value);

G_END_DECLS

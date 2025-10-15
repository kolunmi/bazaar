/* bz-country-data-point.c
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

#include "bz-country-data-point.h"

struct _BzCountryDataPoint
{
  GObject parent_instance;

  char *country_code;
  guint downloads;
};

G_DEFINE_FINAL_TYPE (BzCountryDataPoint, bz_country_data_point, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_COUNTRY_CODE,
  PROP_DOWNLOADS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_country_data_point_dispose (GObject *object)
{
  BzCountryDataPoint *self = BZ_COUNTRY_DATA_POINT (object);

  g_clear_pointer (&self->country_code, g_free);

  G_OBJECT_CLASS (bz_country_data_point_parent_class)->dispose (object);
}

static void
bz_country_data_point_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BzCountryDataPoint *self = BZ_COUNTRY_DATA_POINT (object);

  switch (prop_id)
    {
    case PROP_COUNTRY_CODE:
      g_value_set_string (value, bz_country_data_point_get_country_code (self));
      break;
    case PROP_DOWNLOADS:
      g_value_set_uint (value, bz_country_data_point_get_downloads (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_country_data_point_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  BzCountryDataPoint *self = BZ_COUNTRY_DATA_POINT (object);

  switch (prop_id)
    {
    case PROP_COUNTRY_CODE:
      bz_country_data_point_set_country_code (self, g_value_get_string (value));
      break;
    case PROP_DOWNLOADS:
      bz_country_data_point_set_downloads (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_country_data_point_class_init (BzCountryDataPointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_country_data_point_set_property;
  object_class->get_property = bz_country_data_point_get_property;
  object_class->dispose      = bz_country_data_point_dispose;

  props[PROP_COUNTRY_CODE] =
      g_param_spec_string (
          "country-code",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DOWNLOADS] =
      g_param_spec_uint (
          "downloads",
          NULL, NULL,
          0, G_MAXUINT, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_country_data_point_init (BzCountryDataPoint *self)
{
}

BzCountryDataPoint *
bz_country_data_point_new (void)
{
  return g_object_new (BZ_TYPE_COUNTRY_DATA_POINT, NULL);
}

const char *
bz_country_data_point_get_country_code (BzCountryDataPoint *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY_DATA_POINT (self), NULL);
  return self->country_code;
}

guint
bz_country_data_point_get_downloads (BzCountryDataPoint *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY_DATA_POINT (self), 0);
  return self->downloads;
}

void
bz_country_data_point_set_country_code (BzCountryDataPoint *self,
                                        const char         *country_code)
{
  g_return_if_fail (BZ_IS_COUNTRY_DATA_POINT (self));

  g_clear_pointer (&self->country_code, g_free);
  if (country_code != NULL)
    self->country_code = g_strdup (country_code);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COUNTRY_CODE]);
}

void
bz_country_data_point_set_downloads (BzCountryDataPoint *self,
                                     guint               downloads)
{
  g_return_if_fail (BZ_IS_COUNTRY_DATA_POINT (self));

  self->downloads = downloads;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DOWNLOADS]);
}

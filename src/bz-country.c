/* bz-country.c
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

#include "bz-country.h"

struct _BzCountry
{
  GObject parent_instance;

  char      *name;
  char      *iso_code;
  JsonArray *coordinates;
  double     value;
};

G_DEFINE_FINAL_TYPE (BzCountry, bz_country, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_NAME,
  PROP_ISO_CODE,
  PROP_COORDINATES,
  PROP_VALUE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_country_dispose (GObject *object)
{
  BzCountry *self = BZ_COUNTRY (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->iso_code, g_free);
  g_clear_pointer (&self->coordinates, json_array_unref);

  G_OBJECT_CLASS (bz_country_parent_class)->dispose (object);
}

static void
bz_country_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  BzCountry *self = BZ_COUNTRY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, bz_country_get_name (self));
      break;
    case PROP_ISO_CODE:
      g_value_set_string (value, bz_country_get_iso_code (self));
      break;
    case PROP_COORDINATES:
      g_value_set_boxed (value, bz_country_get_coordinates (self));
      break;
    case PROP_VALUE:
      g_value_set_double (value, bz_country_get_value (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_country_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  BzCountry *self = BZ_COUNTRY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      bz_country_set_name (self, g_value_get_string (value));
      break;
    case PROP_ISO_CODE:
      bz_country_set_iso_code (self, g_value_get_string (value));
      break;
    case PROP_COORDINATES:
      bz_country_set_coordinates (self, g_value_get_boxed (value));
      break;
    case PROP_VALUE:
      bz_country_set_value (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_country_class_init (BzCountryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_country_set_property;
  object_class->get_property = bz_country_get_property;
  object_class->dispose      = bz_country_dispose;

  props[PROP_NAME] =
      g_param_spec_string (
          "name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ISO_CODE] =
      g_param_spec_string (
          "iso-code",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_COORDINATES] =
      g_param_spec_boxed (
          "coordinates",
          NULL, NULL,
          JSON_TYPE_ARRAY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_VALUE] =
      g_param_spec_double (
          "value",
          NULL, NULL,
          0.0, G_MAXDOUBLE, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_country_init (BzCountry *self)
{
}

BzCountry *
bz_country_new (void)
{
  return g_object_new (BZ_TYPE_COUNTRY, NULL);
}

const char *
bz_country_get_name (BzCountry *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY (self), NULL);
  return self->name;
}

const char *
bz_country_get_iso_code (BzCountry *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY (self), NULL);
  return self->iso_code;
}

JsonArray *
bz_country_get_coordinates (BzCountry *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY (self), NULL);
  return self->coordinates;
}

double
bz_country_get_value (BzCountry *self)
{
  g_return_val_if_fail (BZ_IS_COUNTRY (self), 0.0);
  return self->value;
}

void
bz_country_set_name (BzCountry  *self,
                     const char *name)
{
  g_return_if_fail (BZ_IS_COUNTRY (self));

  g_clear_pointer (&self->name, g_free);
  if (name != NULL)
    self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}

void
bz_country_set_iso_code (BzCountry  *self,
                         const char *iso_code)
{
  g_return_if_fail (BZ_IS_COUNTRY (self));

  g_clear_pointer (&self->iso_code, g_free);
  if (iso_code != NULL)
    self->iso_code = g_strdup (iso_code);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ISO_CODE]);
}

void
bz_country_set_coordinates (BzCountry *self,
                            JsonArray *coordinates)
{
  g_return_if_fail (BZ_IS_COUNTRY (self));

  g_clear_pointer (&self->coordinates, json_array_unref);
  if (coordinates != NULL)
    self->coordinates = json_array_ref (coordinates);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COORDINATES]);
}

void
bz_country_set_value (BzCountry *self,
                      double     value)
{
  g_return_if_fail (BZ_IS_COUNTRY (self));

  self->value = value;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_VALUE]);
}

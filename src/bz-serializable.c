/* bz-serializable.c
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

#include "bz-serializable.h"

G_DEFINE_INTERFACE (BzSerializable, bz_serializable, G_TYPE_OBJECT)

static void
bz_serializable_real_serialize (BzSerializable  *self,
                                GVariantBuilder *builder)
{
  return;
}

static gboolean
bz_serializable_real_deserialize (BzSerializable *self,
                                  GVariant       *import,
                                  GError        **error)
{
  return TRUE;
}

static void
bz_serializable_default_init (BzSerializableInterface *iface)
{
  iface->serialize   = bz_serializable_real_serialize;
  iface->deserialize = bz_serializable_real_deserialize;
}

void
bz_serializable_serialize (BzSerializable  *self,
                           GVariantBuilder *builder)
{
  g_return_if_fail (BZ_IS_SERIALIZABLE (self));
  g_return_if_fail (builder != NULL);

  BZ_SERIALIZABLE_GET_IFACE (self)->serialize (
      self,
      builder);
}

gboolean
bz_serializable_deserialize (BzSerializable *self,
                             GVariant       *import,
                             GError        **error)
{
  g_return_val_if_fail (BZ_IS_SERIALIZABLE (self), FALSE);
  g_return_val_if_fail (import != NULL, FALSE);

  return BZ_SERIALIZABLE_GET_IFACE (self)->deserialize (
      self,
      import,
      error);
}

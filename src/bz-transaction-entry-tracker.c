/* bz-transaction-entry-tracker.c
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

#include "bz-transaction-entry-tracker.h"

struct _BzTransactionEntryTracker
{
  GObject parent_instance;

  BzEntry    *entry;
  GListModel *current_ops;
  GListModel *finished_ops;
};

G_DEFINE_FINAL_TYPE (BzTransactionEntryTracker, bz_transaction_entry_tracker, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_ENTRY,
  PROP_CURRENT_OPS,
  PROP_FINISHED_OPS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_transaction_entry_tracker_dispose (GObject *object)
{
  BzTransactionEntryTracker *self = BZ_TRANSACTION_ENTRY_TRACKER (object);

  g_clear_pointer (&self->entry, g_object_unref);
  g_clear_pointer (&self->current_ops, g_object_unref);
  g_clear_pointer (&self->finished_ops, g_object_unref);

  G_OBJECT_CLASS (bz_transaction_entry_tracker_parent_class)->dispose (object);
}

static void
bz_transaction_entry_tracker_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  BzTransactionEntryTracker *self = BZ_TRANSACTION_ENTRY_TRACKER (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      g_value_set_object (value, bz_transaction_entry_tracker_get_entry (self));
      break;
    case PROP_CURRENT_OPS:
      g_value_set_object (value, bz_transaction_entry_tracker_get_current_ops (self));
      break;
    case PROP_FINISHED_OPS:
      g_value_set_object (value, bz_transaction_entry_tracker_get_finished_ops (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_entry_tracker_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  BzTransactionEntryTracker *self = BZ_TRANSACTION_ENTRY_TRACKER (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      bz_transaction_entry_tracker_set_entry (self, g_value_get_object (value));
      break;
    case PROP_CURRENT_OPS:
      bz_transaction_entry_tracker_set_current_ops (self, g_value_get_object (value));
      break;
    case PROP_FINISHED_OPS:
      bz_transaction_entry_tracker_set_finished_ops (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_entry_tracker_class_init (BzTransactionEntryTrackerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_transaction_entry_tracker_set_property;
  object_class->get_property = bz_transaction_entry_tracker_get_property;
  object_class->dispose      = bz_transaction_entry_tracker_dispose;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CURRENT_OPS] =
      g_param_spec_object (
          "current-ops",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FINISHED_OPS] =
      g_param_spec_object (
          "finished-ops",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_transaction_entry_tracker_init (BzTransactionEntryTracker *self)
{
}

BzTransactionEntryTracker *
bz_transaction_entry_tracker_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_ENTRY_TRACKER, NULL);
}

BzEntry *
bz_transaction_entry_tracker_get_entry (BzTransactionEntryTracker *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self), NULL);
  return self->entry;
}

GListModel *
bz_transaction_entry_tracker_get_current_ops (BzTransactionEntryTracker *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self), NULL);
  return self->current_ops;
}

GListModel *
bz_transaction_entry_tracker_get_finished_ops (BzTransactionEntryTracker *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self), NULL);
  return self->finished_ops;
}

void
bz_transaction_entry_tracker_set_entry (BzTransactionEntryTracker *self,
                                        BzEntry                   *entry)
{
  g_return_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self));

  g_clear_pointer (&self->entry, g_object_unref);
  if (entry != NULL)
    self->entry = g_object_ref (entry);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY]);
}

void
bz_transaction_entry_tracker_set_current_ops (BzTransactionEntryTracker *self,
                                              GListModel                *current_ops)
{
  g_return_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self));

  g_clear_pointer (&self->current_ops, g_object_unref);
  if (current_ops != NULL)
    self->current_ops = g_object_ref (current_ops);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_OPS]);
}

void
bz_transaction_entry_tracker_set_finished_ops (BzTransactionEntryTracker *self,
                                               GListModel                *finished_ops)
{
  g_return_if_fail (BZ_IS_TRANSACTION_ENTRY_TRACKER (self));

  g_clear_pointer (&self->finished_ops, g_object_unref);
  if (finished_ops != NULL)
    self->finished_ops = g_object_ref (finished_ops);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FINISHED_OPS]);
}

/* End of bz-transaction-entry-tracker.c */

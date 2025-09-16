/* bz-transaction-task.c
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

#include "bz-transaction-task.h"

struct _BzTransactionTask
{
  GObject parent_instance;

  BzBackendTransactionOpPayload         *op;
  BzBackendTransactionOpProgressPayload *last_progress;
  char                                  *error;
};

G_DEFINE_FINAL_TYPE (BzTransactionTask, bz_transaction_task, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_OP,
  PROP_LAST_PROGRESS,
  PROP_ERROR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_transaction_task_dispose (GObject *object)
{
  BzTransactionTask *self = BZ_TRANSACTION_TASK (object);

  g_clear_pointer (&self->op, g_object_unref);
  g_clear_pointer (&self->last_progress, g_object_unref);
  g_clear_pointer (&self->error, g_free);

  G_OBJECT_CLASS (bz_transaction_task_parent_class)->dispose (object);
}

static void
bz_transaction_task_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzTransactionTask *self = BZ_TRANSACTION_TASK (object);

  switch (prop_id)
    {
    case PROP_OP:
      g_value_set_object (value, bz_transaction_task_get_op (self));
      break;
    case PROP_LAST_PROGRESS:
      g_value_set_object (value, bz_transaction_task_get_last_progress (self));
      break;
    case PROP_ERROR:
      g_value_set_string (value, bz_transaction_task_get_error (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_task_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzTransactionTask *self = BZ_TRANSACTION_TASK (object);

  switch (prop_id)
    {
    case PROP_OP:
      bz_transaction_task_set_op (self, g_value_get_object (value));
      break;
    case PROP_LAST_PROGRESS:
      bz_transaction_task_set_last_progress (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_task_class_init (BzTransactionTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_transaction_task_set_property;
  object_class->get_property = bz_transaction_task_get_property;
  object_class->dispose      = bz_transaction_task_dispose;

  props[PROP_OP] =
      g_param_spec_object (
          "op",
          NULL, NULL,
          BZ_TYPE_BACKEND_TRANSACTION_OP_PAYLOAD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_LAST_PROGRESS] =
      g_param_spec_object (
          "last-progress",
          NULL, NULL,
          BZ_TYPE_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ERROR] =
      g_param_spec_string (
          "error",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_transaction_task_init (BzTransactionTask *self)
{
}

BzTransactionTask *
bz_transaction_task_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_TASK, NULL);
}

BzBackendTransactionOpPayload *
bz_transaction_task_get_op (BzTransactionTask *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_TASK (self), NULL);
  return self->op;
}

BzBackendTransactionOpProgressPayload *
bz_transaction_task_get_last_progress (BzTransactionTask *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_TASK (self), NULL);
  return self->last_progress;
}

const char *
bz_transaction_task_get_error (BzTransactionTask *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_TASK (self), NULL);
  return self->error;
}

void
bz_transaction_task_set_op (BzTransactionTask             *self,
                            BzBackendTransactionOpPayload *op)
{
  g_return_if_fail (BZ_IS_TRANSACTION_TASK (self));

  g_clear_pointer (&self->op, g_object_unref);
  if (op != NULL)
    self->op = g_object_ref (op);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OP]);
}

void
bz_transaction_task_set_last_progress (BzTransactionTask                     *self,
                                       BzBackendTransactionOpProgressPayload *last_progress)
{
  g_return_if_fail (BZ_IS_TRANSACTION_TASK (self));

  g_clear_pointer (&self->last_progress, g_object_unref);
  if (last_progress != NULL)
    self->last_progress = g_object_ref (last_progress);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LAST_PROGRESS]);
}

void
bz_transaction_task_set_error (BzTransactionTask *self,
                               const char        *error)
{
  g_return_if_fail (BZ_IS_TRANSACTION_TASK (self));

  g_clear_pointer (&self->error, g_object_unref);
  if (error != NULL)
    self->error = g_strdup (error);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ERROR]);
}

/* End of bz-transaction-task.c */

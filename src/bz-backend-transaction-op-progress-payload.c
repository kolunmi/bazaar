/* bz-backend-transaction-op-progress-payload.c
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

#include "bz-backend-transaction-op-progress-payload.h"

struct _BzBackendTransactionOpProgressPayload
{
  GObject parent_instance;

  BzBackendTransactionOpPayload *op;
  char                          *status;
  gboolean                       is_estimating;
  double                         progress;
  double                         total_progress;
  guint64                        bytes_transferred;
  guint64                        start_time;
};

G_DEFINE_FINAL_TYPE (BzBackendTransactionOpProgressPayload, bz_backend_transaction_op_progress_payload, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_OP,
  PROP_STATUS,
  PROP_IS_ESTIMATING,
  PROP_PROGRESS,
  PROP_TOTAL_PROGRESS,
  PROP_BYTES_TRANSFERRED,
  PROP_START_TIME,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_backend_transaction_op_progress_payload_dispose (GObject *object)
{
  BzBackendTransactionOpProgressPayload *self = BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object);

  g_clear_pointer (&self->op, g_object_unref);
  g_clear_pointer (&self->status, g_free);

  G_OBJECT_CLASS (bz_backend_transaction_op_progress_payload_parent_class)->dispose (object);
}

static void
bz_backend_transaction_op_progress_payload_get_property (GObject    *object,
                                                         guint       prop_id,
                                                         GValue     *value,
                                                         GParamSpec *pspec)
{
  BzBackendTransactionOpProgressPayload *self = BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object);

  switch (prop_id)
    {
    case PROP_OP:
      g_value_set_object (value, bz_backend_transaction_op_progress_payload_get_op (self));
      break;
    case PROP_STATUS:
      g_value_set_string (value, bz_backend_transaction_op_progress_payload_get_status (self));
      break;
    case PROP_IS_ESTIMATING:
      g_value_set_boolean (value, bz_backend_transaction_op_progress_payload_get_is_estimating (self));
      break;
    case PROP_PROGRESS:
      g_value_set_double (value, bz_backend_transaction_op_progress_payload_get_progress (self));
      break;
    case PROP_TOTAL_PROGRESS:
      g_value_set_double (value, bz_backend_transaction_op_progress_payload_get_total_progress (self));
      break;
    case PROP_BYTES_TRANSFERRED:
      g_value_set_uint64 (value, bz_backend_transaction_op_progress_payload_get_bytes_transferred (self));
      break;
    case PROP_START_TIME:
      g_value_set_uint64 (value, bz_backend_transaction_op_progress_payload_get_start_time (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_backend_transaction_op_progress_payload_set_property (GObject      *object,
                                                         guint         prop_id,
                                                         const GValue *value,
                                                         GParamSpec   *pspec)
{
  BzBackendTransactionOpProgressPayload *self = BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object);

  switch (prop_id)
    {
    case PROP_OP:
      bz_backend_transaction_op_progress_payload_set_op (self, g_value_get_object (value));
      break;
    case PROP_STATUS:
      bz_backend_transaction_op_progress_payload_set_status (self, g_value_get_string (value));
      break;
    case PROP_IS_ESTIMATING:
      bz_backend_transaction_op_progress_payload_set_is_estimating (self, g_value_get_boolean (value));
      break;
    case PROP_PROGRESS:
      bz_backend_transaction_op_progress_payload_set_progress (self, g_value_get_double (value));
      break;
    case PROP_TOTAL_PROGRESS:
      bz_backend_transaction_op_progress_payload_set_total_progress (self, g_value_get_double (value));
      break;
    case PROP_BYTES_TRANSFERRED:
      bz_backend_transaction_op_progress_payload_set_bytes_transferred (self, g_value_get_uint64 (value));
      break;
    case PROP_START_TIME:
      bz_backend_transaction_op_progress_payload_set_start_time (self, g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_backend_transaction_op_progress_payload_class_init (BzBackendTransactionOpProgressPayloadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_backend_transaction_op_progress_payload_set_property;
  object_class->get_property = bz_backend_transaction_op_progress_payload_get_property;
  object_class->dispose      = bz_backend_transaction_op_progress_payload_dispose;

  props[PROP_OP] =
      g_param_spec_object (
          "op",
          NULL, NULL,
          BZ_TYPE_BACKEND_TRANSACTION_OP_PAYLOAD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STATUS] =
      g_param_spec_string (
          "status",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_IS_ESTIMATING] =
      g_param_spec_boolean (
          "is-estimating",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PROGRESS] =
      g_param_spec_double (
          "progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TOTAL_PROGRESS] =
      g_param_spec_double (
          "total-progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_BYTES_TRANSFERRED] =
      g_param_spec_uint64 (
          "bytes-transferred",
          NULL, NULL,
          0, G_MAXUINT64, (guint64) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_START_TIME] =
      g_param_spec_uint64 (
          "start-time",
          NULL, NULL,
          0, G_MAXUINT64, (guint64) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_backend_transaction_op_progress_payload_init (BzBackendTransactionOpProgressPayload *self)
{
}

BzBackendTransactionOpProgressPayload *
bz_backend_transaction_op_progress_payload_new (void)
{
  return g_object_new (BZ_TYPE_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD, NULL);
}

BzBackendTransactionOpPayload *
bz_backend_transaction_op_progress_payload_get_op (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), NULL);
  return self->op;
}

const char *
bz_backend_transaction_op_progress_payload_get_status (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), NULL);
  return self->status;
}

gboolean
bz_backend_transaction_op_progress_payload_get_is_estimating (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), FALSE);
  return self->is_estimating;
}

double
bz_backend_transaction_op_progress_payload_get_progress (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), 0.0);
  return self->progress;
}

double
bz_backend_transaction_op_progress_payload_get_total_progress (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), 0.0);
  return self->total_progress;
}

guint64
bz_backend_transaction_op_progress_payload_get_bytes_transferred (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), 0);
  return self->bytes_transferred;
}

guint64
bz_backend_transaction_op_progress_payload_get_start_time (BzBackendTransactionOpProgressPayload *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self), 0);
  return self->start_time;
}

void
bz_backend_transaction_op_progress_payload_set_op (BzBackendTransactionOpProgressPayload *self,
                                                   BzBackendTransactionOpPayload         *op)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  g_clear_pointer (&self->op, g_object_unref);
  if (op != NULL)
    self->op = g_object_ref (op);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OP]);
}

void
bz_backend_transaction_op_progress_payload_set_status (BzBackendTransactionOpProgressPayload *self,
                                                       const char                            *status)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  g_clear_pointer (&self->status, g_free);
  if (status != NULL)
    self->status = g_strdup (status);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATUS]);
}

void
bz_backend_transaction_op_progress_payload_set_is_estimating (BzBackendTransactionOpProgressPayload *self,
                                                              gboolean                               is_estimating)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  self->is_estimating = is_estimating;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_ESTIMATING]);
}

void
bz_backend_transaction_op_progress_payload_set_progress (BzBackendTransactionOpProgressPayload *self,
                                                         double                                 progress)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  self->progress = CLAMP (progress, 0.0, 0.999999);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROGRESS]);
}

void
bz_backend_transaction_op_progress_payload_set_total_progress (BzBackendTransactionOpProgressPayload *self,
                                                               double                                 total_progress)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  self->total_progress = total_progress;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TOTAL_PROGRESS]);
}

void
bz_backend_transaction_op_progress_payload_set_bytes_transferred (BzBackendTransactionOpProgressPayload *self,
                                                                  guint64                                bytes_transferred)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  self->bytes_transferred = bytes_transferred;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BYTES_TRANSFERRED]);
}

void
bz_backend_transaction_op_progress_payload_set_start_time (BzBackendTransactionOpProgressPayload *self,
                                                           guint64                                start_time)
{
  g_return_if_fail (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (self));

  self->start_time = start_time;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_START_TIME]);
}

/* End of bz-backend-transaction-op-progress-payload.c */

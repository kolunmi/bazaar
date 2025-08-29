/* bz-result.c
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

#include "bz-result.h"

struct _BzResult
{
  GObject parent_instance;

  DexFuture *finally;
  GObject   *object;
  GError    *error;
  GTimer    *timer;
  char      *non_error_msg;
};

G_DEFINE_FINAL_TYPE (BzResult, bz_result, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_PENDING,
  PROP_RESOLVED,
  PROP_REJECTED,
  PROP_OBJECT,
  PROP_MESSAGE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static DexFuture *
future_finally (DexFuture *future,
                BzResult  *self);

static void
bz_result_dispose (GObject *object)
{
  BzResult *self = BZ_RESULT (object);

  dex_clear (&self->finally);
  g_clear_object (&self->object);
  g_clear_pointer (&self->error, g_error_free);
  g_clear_pointer (&self->timer, g_timer_destroy);
  g_clear_pointer (&self->non_error_msg, g_free);

  G_OBJECT_CLASS (bz_result_parent_class)->dispose (object);
}

static void
bz_result_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BzResult *self = BZ_RESULT (object);

  switch (prop_id)
    {
    case PROP_PENDING:
      g_value_set_boolean (value, bz_result_get_pending (self));
      break;
    case PROP_RESOLVED:
      g_value_set_boolean (value, bz_result_get_resolved (self));
      break;
    case PROP_REJECTED:
      g_value_set_boolean (value, bz_result_get_rejected (self));
      break;
    case PROP_OBJECT:
      g_value_set_object (value, bz_result_get_object (self));
      break;
    case PROP_MESSAGE:
      g_value_set_string (value, bz_result_get_message (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_result_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  // BzResult *self = BZ_RESULT (object);

  switch (prop_id)
    {
    case PROP_PENDING:
    case PROP_RESOLVED:
    case PROP_REJECTED:
    case PROP_OBJECT:
    case PROP_MESSAGE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_result_class_init (BzResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_result_set_property;
  object_class->get_property = bz_result_get_property;
  object_class->dispose      = bz_result_dispose;

  props[PROP_PENDING] =
      g_param_spec_boolean (
          "pending",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RESOLVED] =
      g_param_spec_boolean (
          "resolved",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_REJECTED] =
      g_param_spec_boolean (
          "rejected",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_OBJECT] =
      g_param_spec_object (
          "object",
          NULL, NULL,
          G_TYPE_OBJECT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MESSAGE] =
      g_param_spec_string (
          "message",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_result_init (BzResult *self)
{
}

BzResult *
bz_result_new (DexFuture *future)
{
  BzResult       *self   = NULL;
  DexFutureStatus status = DEX_FUTURE_STATUS_PENDING;

  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  self = g_object_new (BZ_TYPE_RESULT, NULL);

  status = dex_future_get_status (future);
  switch (status)
    {
    case DEX_FUTURE_STATUS_PENDING:
      self->timer   = g_timer_new ();
      self->finally = dex_future_finally (
          dex_ref (future),
          (DexFutureCallback) future_finally,
          g_object_ref (self), g_object_unref);
      break;
    case DEX_FUTURE_STATUS_RESOLVED:
      self->object        = g_value_dup_object (dex_future_get_value (future, NULL));
      self->non_error_msg = g_strdup ("Object was already successfully resolved");
      break;
    case DEX_FUTURE_STATUS_REJECTED:
      {
        g_autoptr (GError) local_error = NULL;

        dex_future_get_value (future, &local_error);
        self->error = g_error_copy (local_error);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  return self;
}

gboolean
bz_result_get_pending (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), FALSE);
  return self->finally != NULL;
}

gboolean
bz_result_get_resolved (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), FALSE);
  return self->object != NULL;
}

gboolean
bz_result_get_rejected (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), FALSE);
  return self->error != NULL;
}

gpointer
bz_result_get_object (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), NULL);
  return self->object;
}

const char *
bz_result_get_message (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), NULL);

  if (self->error != NULL)
    return self->error->message;
  else
    return self->non_error_msg;
}

DexFuture *
bz_result_dup_future (BzResult *self)
{
  g_return_val_if_fail (BZ_IS_RESULT (self), NULL);

  if (self->finally != NULL)
    return dex_ref (self->finally);
  else if (self->object != NULL)
    return dex_future_new_for_object (self->object);
  else if (self->error != NULL)
    return dex_future_new_for_error (g_error_copy (self->error));
  else
    return NULL;
}

static DexFuture *
future_finally (DexFuture *future,
                BzResult  *self)
{
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  dex_clear (&self->finally);
  g_timer_stop (self->timer);

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      self->object        = g_value_dup_object (value);
      self->non_error_msg = g_strdup_printf (
          "Successfully resolved object in %f seconds",
          g_timer_elapsed (self->timer, NULL));
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJECT]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MESSAGE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RESOLVED]);
    }
  else
    {
      self->error = g_error_copy (local_error);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MESSAGE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REJECTED]);
    }

  return dex_ref (future);
}

/* End of bz-result.c */

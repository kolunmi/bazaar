/* bge-wdgt-time.c
 *
 * Copyright 2026 Eva M
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

#include "bge.h"

struct _BgeWdgtTime
{
  GObject parent_instance;

  guint notify_msec;

  GTimer *timer;
  guint   source;
};

G_DEFINE_FINAL_TYPE (BgeWdgtTime, bge_wdgt_time, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_TIME,
  PROP_NOTIFY_MSEC,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
make_timeout_source (BgeWdgtTime *self);

static gboolean
timeout_cb (BgeWdgtTime *self);

static void
bge_wdgt_time_dispose (GObject *object)
{
  BgeWdgtTime *self = BGE_WDGT_TIME (object);

  g_clear_pointer (&self->timer, g_timer_destroy);
  g_clear_handle_id (&self->source, g_source_remove);

  G_OBJECT_CLASS (bge_wdgt_time_parent_class)->dispose (object);
}

static void
bge_wdgt_time_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  BgeWdgtTime *self = BGE_WDGT_TIME (object);

  switch (prop_id)
    {
    case PROP_TIME:
      g_value_set_double (value, bge_wdgt_time_get_time (self));
      break;
    case PROP_NOTIFY_MSEC:
      g_value_set_uint (value, bge_wdgt_time_get_notify_msec (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_time_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  BgeWdgtTime *self = BGE_WDGT_TIME (object);

  switch (prop_id)
    {
    case PROP_NOTIFY_MSEC:
      bge_wdgt_time_set_notify_msec (self, g_value_get_uint (value));
      break;
    case PROP_TIME:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_wdgt_time_class_init (BgeWdgtTimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bge_wdgt_time_set_property;
  object_class->get_property = bge_wdgt_time_get_property;
  object_class->dispose      = bge_wdgt_time_dispose;

  props[PROP_TIME] =
      g_param_spec_double (
          "time",
          NULL, NULL,
          G_MININT, G_MAXINT, 0.0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_NOTIFY_MSEC] =
      g_param_spec_uint (
          "notify-msec",
          NULL, NULL,
          0, G_MAXUINT, 30,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bge_wdgt_time_init (BgeWdgtTime *self)
{
  self->notify_msec = 30;
  self->timer       = g_timer_new ();

  make_timeout_source (self);
}

BgeWdgtTime *
bge_wdgt_time_new (void)
{
  return g_object_new (BGE_TYPE_WDGT_TIME, NULL);
}

double
bge_wdgt_time_get_time (BgeWdgtTime *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_TIME (self), 0.0);
  return g_timer_elapsed (self->timer, NULL);
}

guint
bge_wdgt_time_get_notify_msec (BgeWdgtTime *self)
{
  g_return_val_if_fail (BGE_IS_WDGT_TIME (self), 0);
  return self->notify_msec;
}

void
bge_wdgt_time_set_notify_msec (BgeWdgtTime *self,
                               guint        notify_msec)
{
  g_return_if_fail (BGE_IS_WDGT_TIME (self));

  if (notify_msec == self->notify_msec)
    return;

  self->notify_msec = notify_msec;
  make_timeout_source (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NOTIFY_MSEC]);
}

static void
make_timeout_source (BgeWdgtTime *self)
{
  g_clear_handle_id (&self->source, g_source_remove);
  if (self->notify_msec > 0)
    self->source = g_timeout_add (self->notify_msec,
                                  (GSourceFunc) timeout_cb,
                                  self);
}

static gboolean
timeout_cb (BgeWdgtTime *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TIME]);
  return G_SOURCE_CONTINUE;
}

/* End of bge-wdgt-time.c */

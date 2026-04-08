/* bz-global-progress.c
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

#include "config.h"

#include <bge.h>

#include "bz-global-progress.h"
#include "progress-bar-designs/common.h"

struct _BzGlobalProgress
{
  AdwBin parent_instance;

  GtkWidget   *wdgt;
  BgeWdgtSpec *wdgt_spec;

  char *draw_widget_class;

  BzStateInfo *state;
  gboolean     active;
  gboolean     pending;
  double       fraction;
  int          expand_size;
  GSettings   *settings;
};

G_DEFINE_FINAL_TYPE (BzGlobalProgress, bz_global_progress, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_ACTIVE,
  PROP_PENDING,
  PROP_EXPAND_SIZE,
  PROP_STATE,
  PROP_SETTINGS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
global_progress_bar_flag_changed (BzGlobalProgress *self,
                                  const char       *key,
                                  GSettings        *settings);

static void
set_wdgt_state (BzGlobalProgress *self);

static void
ensure_draw_css (BzGlobalProgress *self);

static void
bz_global_progress_dispose (GObject *object)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_func (
        self->settings,
        global_progress_bar_flag_changed,
        self);

  g_clear_object (&self->wdgt_spec);
  g_clear_object (&self->state);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (bz_global_progress_parent_class)->dispose (object);
}

static void
bz_global_progress_get_property (GObject *object,
                                 guint    prop_id,
                                 GValue  *value,

                                 GParamSpec *pspec)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, bz_global_progress_get_active (self));
      break;
    case PROP_PENDING:
      g_value_set_boolean (value, bz_global_progress_get_pending (self));
      break;
    case PROP_EXPAND_SIZE:
      g_value_set_int (value, bz_global_progress_get_expand_size (self));
      break;
    case PROP_STATE:
      g_value_set_object (value, bz_global_progress_get_state (self));
      break;
    case PROP_SETTINGS:
      g_value_set_object (value, bz_global_progress_get_settings (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_global_progress_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BzGlobalProgress *self = BZ_GLOBAL_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      bz_global_progress_set_active (self, g_value_get_boolean (value));
      break;
    case PROP_PENDING:
      bz_global_progress_set_pending (self, g_value_get_boolean (value));
      break;
    case PROP_EXPAND_SIZE:
      bz_global_progress_set_expand_size (self, g_value_get_int (value));
      break;
    case PROP_STATE:
      bz_global_progress_set_state (self, g_value_get_object (value));
      break;
    case PROP_SETTINGS:
      bz_global_progress_set_settings (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_global_progress_class_init (BzGlobalProgressClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_global_progress_dispose;
  object_class->get_property = bz_global_progress_get_property;
  object_class->set_property = bz_global_progress_set_property;

  props[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PENDING] =
      g_param_spec_boolean (
          "pending",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_EXPAND_SIZE] =
      g_param_spec_int (
          "expand-size",
          NULL, NULL,
          0, G_MAXINT, 100,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SETTINGS] =
      g_param_spec_object (
          "settings",
          NULL, NULL,
          G_TYPE_SETTINGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "global-progress");
}

static void
bz_global_progress_init (BzGlobalProgress *self)
{
  self->wdgt_spec = bge_wdgt_spec_new_for_resource ("/io/github/kolunmi/Bazaar/bz-global-progress.wdgt", NULL);
  g_assert (self->wdgt_spec != NULL);

  self->wdgt = (GtkWidget *) bge_wdgt_renderer_new ();
  adw_bin_set_child (ADW_BIN (self), self->wdgt);

  bge_wdgt_renderer_set_state (BGE_WDGT_RENDERER (self->wdgt), "inactive");
  bge_wdgt_renderer_set_spec (BGE_WDGT_RENDERER (self->wdgt), self->wdgt_spec);
}

GtkWidget *
bz_global_progress_new (void)
{
  return g_object_new (BZ_TYPE_GLOBAL_PROGRESS, NULL);
}

void
bz_global_progress_set_active (BzGlobalProgress *self,
                               gboolean          active)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if ((active && self->active) ||
      (!active && !self->active))
    return;

  self->active = active;
  set_wdgt_state (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
}

gboolean
bz_global_progress_get_active (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->active;
}

void
bz_global_progress_set_pending (BzGlobalProgress *self,
                                gboolean          pending)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if ((pending && self->pending) ||
      (!pending && !self->pending))
    return;

  self->pending = pending;
  set_wdgt_state (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
}

gboolean
bz_global_progress_get_pending (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->pending;
}

void
bz_global_progress_set_expand_size (BzGlobalProgress *self,
                                    int               expand_size)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  self->expand_size = MAX (expand_size, 0);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EXPAND_SIZE]);
}

int
bz_global_progress_get_expand_size (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->expand_size;
}

void
bz_global_progress_set_state (BzGlobalProgress *self,
                              BzStateInfo      *state)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));
  g_return_if_fail (state == NULL || BZ_IS_STATE_INFO (state));

  g_clear_object (&self->state);
  if (state != NULL)
    self->state = g_object_ref (state);

  bge_wdgt_renderer_set_reference (BGE_WDGT_RENDERER (self->wdgt),
                                   (GObject *) state);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

BzStateInfo *
bz_global_progress_get_state (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), NULL);
  return self->state;
}

void
bz_global_progress_set_settings (BzGlobalProgress *self,
                                 GSettings        *settings)
{
  g_return_if_fail (BZ_IS_GLOBAL_PROGRESS (self));

  if (self->settings != NULL)
    g_signal_handlers_disconnect_by_func (
        self->settings,
        global_progress_bar_flag_changed,
        self);
  g_clear_object (&self->settings);

  if (settings != NULL)
    {
      self->settings = g_object_ref (settings);
      g_signal_connect_swapped (
          self->settings,
          "changed::global-progress-bar-theme",
          G_CALLBACK (global_progress_bar_flag_changed),
          self);
      g_signal_connect_swapped (
          self->settings,
          "changed::rotate-flag",
          G_CALLBACK (global_progress_bar_flag_changed),
          self);
    }
  ensure_draw_css (self);

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SETTINGS]);
}

GSettings *
bz_global_progress_get_settings (BzGlobalProgress *self)
{
  g_return_val_if_fail (BZ_IS_GLOBAL_PROGRESS (self), FALSE);
  return self->settings;
}

static void
global_progress_bar_flag_changed (BzGlobalProgress *self,
                                  const char       *key,
                                  GSettings        *settings)
{
  ensure_draw_css (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
set_wdgt_state (BzGlobalProgress *self)
{
  const char *state = NULL;

  if (self->active)
    {
      if (self->pending)
        state = "pending";
      else
        state = "fraction";
    }
  else
    state = "inactive";

  bge_wdgt_renderer_set_state (BGE_WDGT_RENDERER (self->wdgt), state);
}

static void
ensure_draw_css (BzGlobalProgress *self)
{
  g_autoptr (GtkWidget) draw_widget = NULL;

  draw_widget = bge_wdgt_renderer_lookup_object (
      BGE_WDGT_RENDERER (self->wdgt), "fg");
  if (draw_widget == NULL)
    return;

  if (self->settings != NULL)
    {
      g_autofree char *id       = NULL;
      g_autofree char *final_id = NULL;
      g_autofree char *class    = NULL;
      gboolean         rotate   = FALSE;

      id     = g_settings_get_string (self->settings, "global-progress-bar-theme");
      rotate = g_settings_get_boolean (self->settings, "rotate-flag");

      if (rotate && g_strcmp0 (id, "accent-color") != 0)
        final_id = g_strdup_printf ("%s-horizontal", id);
      else
        final_id = g_strdup (id);

      class = bz_dup_css_class_for_pride_id (final_id);

      if (self->draw_widget_class != NULL &&
          g_strcmp0 (self->draw_widget_class, class) == 0)
        return;

      if (self->draw_widget_class != NULL)
        gtk_widget_remove_css_class (draw_widget, self->draw_widget_class);
      g_clear_pointer (&self->draw_widget_class, g_free);
      gtk_widget_add_css_class (draw_widget, class);
      self->draw_widget_class = g_steal_pointer (&class);
    }
  else
    {
      if (self->draw_widget_class != NULL)
        gtk_widget_remove_css_class (draw_widget, self->draw_widget_class);
      g_clear_pointer (&self->draw_widget_class, g_free);
    }
}

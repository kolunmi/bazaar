/* bz-curated-view.c
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

#include "bz-curated-view.h"
#include "bz-curated-row.h"
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-inhibited-scrollable.h"
#include "bz-root-curated-config.h"
#include "bz-row-view.h"

struct _BzCuratedView
{
  AdwBin parent_instance;

  BzContentProvider *provider;
  gboolean           online;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzCuratedView, bz_curated_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_CONTENT_PROVIDER,
  PROP_ONLINE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_SELECTED,
  SIGNAL_BROWSE_FLATHUB,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
items_changed (GListModel     *model,
               guint           position,
               guint           removed,
               guint           added,
               BzCuratedView *self);

static void
set_page (BzCuratedView *self);

static void
bz_curated_view_dispose (GObject *object)
{
  BzCuratedView *self = BZ_CURATED_VIEW (object);

  if (self->provider != NULL)
    g_signal_handlers_disconnect_by_func (
        self->provider, items_changed, self);
  g_clear_object (&self->provider);

  G_OBJECT_CLASS (bz_curated_view_parent_class)->dispose (object);
}

static void
bz_curated_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzCuratedView *self = BZ_CURATED_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONTENT_PROVIDER:
      g_value_set_object (value, bz_curated_view_get_content_provider (self));
      break;
    case PROP_ONLINE:
      g_value_set_boolean (value, self->online);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_curated_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzCuratedView *self = BZ_CURATED_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONTENT_PROVIDER:
      bz_curated_view_set_content_provider (self, g_value_get_object (value));
      break;
    case PROP_ONLINE:
      self->online = g_value_get_boolean (value);
      if (self->online)
        set_page (self);
      else
        adw_view_stack_set_visible_child_name (self->stack, "offline");

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
browse_flathub_cb (BzCuratedView *self,
                   GtkButton      *button)
{
  g_signal_emit (self, signals[SIGNAL_BROWSE_FLATHUB], 0);
}

static void
bz_curated_view_class_init (BzCuratedViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_curated_view_dispose;
  object_class->get_property = bz_curated_view_get_property;
  object_class->set_property = bz_curated_view_set_property;

  props[PROP_CONTENT_PROVIDER] =
      g_param_spec_object (
          "content-provider",
          NULL, NULL,
          BZ_TYPE_CONTENT_PROVIDER,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ONLINE] =
      g_param_spec_boolean (
          "online",
          NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_SELECTED] =
      g_signal_new (
          "group-selected",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_GROUP_SELECTED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_BROWSE_FLATHUB] =
      g_signal_new (
          "browse-flathub",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

  g_type_ensure (BZ_TYPE_ROW_VIEW);
  g_type_ensure (BZ_TYPE_ROOT_CURATED_CONFIG);
  g_type_ensure (BZ_TYPE_CURATED_ROW);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_INHIBITED_SCROLLABLE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-curated-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzCuratedView, stack);
  gtk_widget_class_bind_template_callback (widget_class, browse_flathub_cb);
}

static void
bz_curated_view_init (BzCuratedView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_curated_view_new (void)
{
  return g_object_new (BZ_TYPE_CURATED_VIEW, NULL);
}

void
bz_curated_view_set_content_provider (BzCuratedView    *self,
                                       BzContentProvider *provider)
{
  g_return_if_fail (BZ_IS_CURATED_VIEW (self));
  g_return_if_fail (provider == NULL || BZ_IS_CONTENT_PROVIDER (provider));

  if (self->provider != NULL)
    g_signal_handlers_disconnect_by_func (
        self->provider, items_changed, self);
  g_clear_object (&self->provider);

  if (provider != NULL)
    {
      self->provider = g_object_ref (provider);
      g_signal_connect (
          self->provider, "items-changed",
          G_CALLBACK (items_changed), self);
    }

  set_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_PROVIDER]);
}

BzContentProvider *
bz_curated_view_get_content_provider (BzCuratedView *self)
{
  g_return_val_if_fail (BZ_IS_CURATED_VIEW (self), NULL);
  return self->provider;
}

static void
items_changed (GListModel     *model,
               guint           position,
               guint           removed,
               guint           added,
               BzCuratedView *self)
{
  set_page (self);
}

static void
set_page (BzCuratedView *self)
{
  adw_view_stack_set_visible_child_name (
      self->stack,
      self->provider != NULL &&
              g_list_model_get_n_items (G_LIST_MODEL (self->provider)) > 0
          ? "content"
          : "empty");
}

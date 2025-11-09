/* bz-installed-page.c
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

#include <glib/gi18n.h>

#include "bz-installed-page.h"
#include "bz-installed-tile.h"
#include "bz-section-view.h"
#include "bz-state-info.h"

struct _BzInstalledPage
{
  AdwBin parent_instance;

  GListModel  *model;
  BzStateInfo *state;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzInstalledPage, bz_installed_page, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_REMOVE,
  SIGNAL_INSTALL,
  SIGNAL_SHOW,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
items_changed (BzInstalledPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model);

static void
set_page (BzInstalledPage *self);

static void
bz_installed_page_dispose (GObject *object)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_installed_page_parent_class)->dispose (object);
}

static void
bz_installed_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_installed_page_get_model (self));
      break;
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_installed_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_installed_page_set_model (self, g_value_get_object (value));
      break;
    case PROP_STATE:
      g_clear_object (&self->state);
      self->state = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static void
bz_installed_page_class_init (BzInstalledPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_installed_page_dispose;
  object_class->get_property = bz_installed_page_get_property;
  object_class->set_property = bz_installed_page_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_REMOVE] =
      g_signal_new (
          "remove",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_REMOVE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_SHOW] =
      g_signal_new (
          "show-entry",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SHOW],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);
  g_type_ensure (BZ_TYPE_INSTALLED_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-installed-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, stack);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
}

static void
bz_installed_page_init (BzInstalledPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_installed_page_new (void)
{
  return g_object_new (BZ_TYPE_INSTALLED_PAGE, NULL);
}

void
bz_installed_page_set_model (BzInstalledPage *self,
                             GListModel      *model)
{
  g_return_if_fail (BZ_IS_INSTALLED_PAGE (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  if (model != NULL)
    {
      self->model = g_object_ref (model);
      g_signal_connect_swapped (model, "items-changed", G_CALLBACK (items_changed), self);
    }
  set_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
bz_installed_page_get_model (BzInstalledPage *self)
{
  g_return_val_if_fail (BZ_IS_INSTALLED_PAGE (self), NULL);
  return self->model;
}

static void
items_changed (BzInstalledPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model)
{
  set_page (self);
}

static void
set_page (BzInstalledPage *self)
{
  if (self->model != NULL &&
      g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0)
    adw_view_stack_set_visible_child_name (self->stack, "content");
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");
}

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

#include "bz-entry-group-util.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-installed-page.h"
#include "bz-installed-tile.h"
#include "bz-section-view.h"
#include "bz-state-info.h"
#include "bz-util.h"

struct _BzInstalledPage
{
  AdwBin parent_instance;

  GListModel  *model;
  BzStateInfo *state;

  /* Template widgets */
  AdwViewStack    *stack;
  GtkText         *search_bar;
  GtkCustomFilter *filter;
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
  SIGNAL_REMOVE_ADDON,
  SIGNAL_INSTALL_ADDON,
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

static gboolean
filter (BzEntryGroup    *group,
        BzInstalledPage *self);

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

static DexFuture *
row_activated_fiber (GWeakRef *wr)
{
  g_autoptr (GError) local_error   = NULL;
  g_autoptr (BzInstalledTile) tile = NULL;
  BzInstalledPage *self            = NULL;
  GtkWidget       *window          = NULL;
  BzEntryGroup    *group           = NULL;
  g_autoptr (BzEntry) entry        = NULL;

  bz_weak_get_or_return_reject (tile, wr);

  self = (BzInstalledPage *) gtk_widget_get_ancestor (GTK_WIDGET (tile), BZ_TYPE_INSTALLED_PAGE);
  if (self == NULL)
    return NULL;
  if (self->model == NULL)
    goto err;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  if (window == NULL)
    goto err;

  group = bz_installed_tile_get_group (tile);
  if (group == NULL)
    goto err;

  entry = bz_entry_group_find_entry (group, NULL, window, &local_error);
  if (entry == NULL)
    goto err;

  g_signal_emit (self, signals[SIGNAL_SHOW], 0, entry);
  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
tile_activated_cb (BzInstalledTile *tile)
{
  g_assert (BZ_IS_INSTALLED_TILE (tile));

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) row_activated_fiber,
      bz_track_weak (tile), bz_weak_release));
}

static gboolean
is_valid_string (gpointer    object,
                 const char *value)
{
  return value != NULL && *value != '\0';
}

static void
search_text_changed (BzInstalledPage *self,
                     GParamSpec      *pspec,
                     GtkEntry        *entry)
{
  gtk_filter_changed (GTK_FILTER (self->filter),
                      GTK_FILTER_CHANGE_DIFFERENT);
}

static void
reset_search_cb (BzInstalledPage *self,
                 GtkButton      *button)
{
  gtk_text_set_buffer (self->search_bar, NULL);
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

  signals[SIGNAL_INSTALL_ADDON] =
      g_signal_new (
          "install-addon",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL_ADDON],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_REMOVE_ADDON] =
      g_signal_new (
          "remove-addon",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_REMOVE_ADDON],
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
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, search_bar);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, filter);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_valid_string);
  gtk_widget_class_bind_template_callback (widget_class, tile_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_text_changed);
}

static void
bz_installed_page_init (BzInstalledPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_custom_filter_set_filter_func (
      self->filter, (GtkCustomFilterFunc) filter,
      self, NULL);
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

static gboolean
filter (BzEntryGroup    *group,
        BzInstalledPage *self)
{
  const char *id    = NULL;
  const char *title = NULL;
  const char *text  = NULL;

  id    = bz_entry_group_get_id (group);
  title = bz_entry_group_get_id (group);

  text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));

  if (text != NULL && *text != '\0')
    return strcasestr (id, text) != NULL ||
           strcasestr (title, text) != NULL;
  else
    return TRUE;
}

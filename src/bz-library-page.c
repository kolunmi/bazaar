/* bz-library-page.c
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
#include "bz-installed-tile.h"
#include "bz-library-page.h"
#include "bz-section-view.h"
#include "bz-template-callbacks.h"
#include "bz-transaction-tile.h"
#include "bz-updates-card.h"
#include "bz-util.h"

struct _BzLibraryPage
{
  AdwBin parent_instance;

  GListModel  *model;
  BzStateInfo *state;

  /* Template widgets */
  AdwViewStack    *stack;
  GtkText         *search_bar;
  GtkCustomFilter *filter;
  GtkListView     *list_view;
};

G_DEFINE_FINAL_TYPE (BzLibraryPage, bz_library_page, ADW_TYPE_BIN)

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
  SIGNAL_UPDATE,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
items_changed (BzLibraryPage *self,
               guint          position,
               guint          removed,
               guint          added,
               GListModel    *model);

static void
set_page (BzLibraryPage *self);

static gboolean
set_page_idle_cb (BzLibraryPage *self);

static gboolean
filter (BzEntryGroup  *group,
        BzLibraryPage *self);

static void
bz_library_page_dispose (GObject *object)
{
  BzLibraryPage *self = BZ_LIBRARY_PAGE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_library_page_parent_class)->dispose (object);
}

static void
bz_library_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzLibraryPage *self = BZ_LIBRARY_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_library_page_get_model (self));
      break;
    case PROP_STATE:
      g_value_set_object (value, bz_library_page_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_library_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzLibraryPage *self = BZ_LIBRARY_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_library_page_set_model (self, g_value_get_object (value));
      break;
    case PROP_STATE:
      bz_library_page_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
no_results_found_subtitle (gpointer    object,
                           const char *search_text)
{
  if (search_text == NULL || *search_text == '\0')
    return g_strdup ("");

  return g_strdup_printf (_ ("No matches found for \"%s\" in the list of installed apps"), search_text);
}

static char *
format_update_count (gpointer    object,
                     GListModel *updates)
{
  guint n_updates = 0;

  if (updates == NULL)
    return g_strdup ("");

  n_updates = g_list_model_get_n_items (updates);
  return g_strdup_printf (ngettext ("%u Available Update",
                                    "%u Available Updates",
                                    n_updates),
                          n_updates);
}

static DexFuture *
row_activated_fiber (GWeakRef *wr)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (BzListTile) tile    = NULL;
  BzLibraryPage *self            = NULL;
  GtkWidget     *window          = NULL;
  BzEntryGroup  *group           = NULL;
  g_autoptr (BzEntry) entry      = NULL;

  bz_weak_get_or_return_reject (tile, wr);

  self = (BzLibraryPage *) gtk_widget_get_ancestor (GTK_WIDGET (tile), BZ_TYPE_LIBRARY_PAGE);
  if (self == NULL)
    return NULL;
  if (self->model == NULL)
    goto err;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  if (window == NULL)
    goto err;

  if (BZ_IS_INSTALLED_TILE (tile))
    {
      group = bz_installed_tile_get_group (BZ_INSTALLED_TILE (tile));
      if (group == NULL)
        goto err;

      entry = bz_entry_group_find_entry (group, NULL, window, &local_error);
      if (entry == NULL)
        goto err;
    }
  else if (BZ_IS_TRANSACTION_TILE (tile))
    {
      BzTransactionEntryTracker *tracker = NULL;

      tracker = bz_transaction_tile_get_tracker (BZ_TRANSACTION_TILE (tile));
      if (tracker == NULL)
        goto err;

      entry = bz_transaction_entry_tracker_get_entry (tracker);
      if (entry == NULL)
        goto err;

      entry = g_object_ref (entry);
    }
  else
    goto err;

  g_signal_emit (self, signals[SIGNAL_SHOW], 0, entry);
  return dex_future_new_true ();

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return dex_future_new_for_error (g_steal_pointer (&local_error));
}

static void
tile_activated_cb (BzListTile *tile)
{
  g_assert (BZ_IS_LIST_TILE (tile));

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) row_activated_fiber,
      bz_track_weak (tile), bz_weak_release));
}

static void
search_text_changed (BzLibraryPage *self,
                     GParamSpec    *pspec,
                     GtkEntry      *entry)
{
  gtk_filter_changed (GTK_FILTER (self->filter),
                      GTK_FILTER_CHANGE_DIFFERENT);
  set_page (self);
}

static void
reset_search_cb (BzLibraryPage *self,
                 GtkButton     *button)
{
  gtk_text_set_buffer (self->search_bar, NULL);
}

static void
clear_tasks_cb (BzLibraryPage *self)
{
  BzTransactionManager *manager = NULL;
  manager                       = bz_state_info_get_transaction_manager (self->state);
  bz_transaction_manager_clear_finished (manager);
}

static void
updates_card_update_cb (BzLibraryPage *self,
                        GListModel    *entries,
                        BzUpdatesCard *card)
{
  g_signal_emit (self, signals[SIGNAL_UPDATE], 0, entries);
}

static void
bz_library_page_class_init (BzLibraryPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_library_page_dispose;
  object_class->get_property = bz_library_page_get_property;
  object_class->set_property = bz_library_page_set_property;

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

  signals[SIGNAL_UPDATE] =
      g_signal_new (
          "update",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          G_TYPE_LIST_MODEL);
  g_signal_set_va_marshaller (
      signals[SIGNAL_UPDATE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);
  g_type_ensure (BZ_TYPE_INSTALLED_TILE);
  g_type_ensure (BZ_TYPE_TRANSACTION_TILE);
  g_type_ensure (BZ_TYPE_UPDATES_CARD);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-library-page.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzLibraryPage, stack);
  gtk_widget_class_bind_template_child (widget_class, BzLibraryPage, search_bar);
  gtk_widget_class_bind_template_child (widget_class, BzLibraryPage, filter);
  gtk_widget_class_bind_template_child (widget_class, BzLibraryPage, list_view);
  gtk_widget_class_bind_template_callback (widget_class, no_results_found_subtitle);
  gtk_widget_class_bind_template_callback (widget_class, format_update_count);
  gtk_widget_class_bind_template_callback (widget_class, tile_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_search_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, clear_tasks_cb);
  gtk_widget_class_bind_template_callback (widget_class, updates_card_update_cb);
}

static void
bz_library_page_init (BzLibraryPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_custom_filter_set_filter_func (
      self->filter, (GtkCustomFilterFunc) filter,
      self, NULL);
}

GtkWidget *
bz_library_page_new (void)
{
  return g_object_new (BZ_TYPE_LIBRARY_PAGE, NULL);
}

void
bz_library_page_set_model (BzLibraryPage *self,
                           GListModel    *model)
{
  g_return_if_fail (BZ_IS_LIBRARY_PAGE (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  if (model != NULL)
    {
      self->model = g_object_ref (model);
      g_signal_connect_swapped (model, "items-changed", G_CALLBACK (items_changed), self);
    }
  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) set_page_idle_cb,
      g_object_ref (self),
      g_object_unref);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
bz_library_page_get_model (BzLibraryPage *self)
{
  g_return_val_if_fail (BZ_IS_LIBRARY_PAGE (self), NULL);
  return self->model;
}

void
bz_library_page_set_state (BzLibraryPage *self,
                           BzStateInfo   *state)
{
  g_return_if_fail (BZ_IS_LIBRARY_PAGE (self));
  g_return_if_fail (state == NULL || BZ_IS_STATE_INFO (state));

  g_clear_object (&self->state);
  if (state != NULL)
    self->state = g_object_ref (state);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

BzStateInfo *
bz_library_page_get_state (BzLibraryPage *self)
{
  g_return_val_if_fail (BZ_IS_LIBRARY_PAGE (self), NULL);
  return self->state;
}

gboolean
bz_library_page_ensure_active (BzLibraryPage *self,
                               const char    *initial)
{
  const char *text = NULL;

  g_return_val_if_fail (BZ_IS_LIBRARY_PAGE (self), FALSE);

  text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
  if (text != NULL && *text != '\0' &&
      gtk_widget_has_focus (GTK_WIDGET (self->search_bar)))
    return FALSE;

  gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
  gtk_editable_set_text (GTK_EDITABLE (self->search_bar), initial);
  if (initial != NULL)
    gtk_editable_set_position (GTK_EDITABLE (self->search_bar), g_utf8_strlen (initial, -1));

  return TRUE;
}

void
bz_library_page_reset_search (BzLibraryPage *self)
{
  g_return_if_fail (BZ_IS_LIBRARY_PAGE (self));

  gtk_text_set_buffer (self->search_bar, NULL);
}

static void
items_changed (BzLibraryPage *self,
               guint          position,
               guint          removed,
               guint          added,
               GListModel    *model)
{
  set_page (self);
}

static void
set_page (BzLibraryPage *self)
{
  GtkSelectionModel *selection_model;
  GListModel        *filter_model;

  if (self->model == NULL || g_list_model_get_n_items (self->model) == 0)
    {
      adw_view_stack_set_visible_child_name (self->stack, "empty");
      return;
    }

  selection_model = gtk_list_view_get_model (self->list_view);
  filter_model    = gtk_no_selection_get_model (GTK_NO_SELECTION (selection_model));

  if (g_list_model_get_n_items (filter_model) == 0)
    adw_view_stack_set_visible_child_name (self->stack, "no-results");
  else
    adw_view_stack_set_visible_child_name (self->stack, "content");
}

static gboolean
set_page_idle_cb (BzLibraryPage *self)
{
  set_page (self);
  return G_SOURCE_REMOVE;
}

static gboolean
filter (BzEntryGroup  *group,
        BzLibraryPage *self)
{
  const char *id    = NULL;
  const char *title = NULL;
  const char *text  = NULL;

  id    = bz_entry_group_get_id (group);
  title = bz_entry_group_get_title (group);

  text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));

  if (text != NULL && *text != '\0')
    return strcasestr (id, text) != NULL ||
           strcasestr (title, text) != NULL;
  else
    return TRUE;
}

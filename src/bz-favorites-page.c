/* bz-favorites-page.c
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
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

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "bz-application-map-factory.h"
#include "bz-entry-group-util.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-favorites-page.h"
#include "bz-favorites-tile.h"
#include "bz-global-net.h"
#include "bz-io.h"
#include "bz-util.h"

struct _BzFavoritesPage
{
  AdwNavigationPage parent_instance;

  BzStateInfo *state;
  GListModel  *model;
  gboolean     show_sidebar;

  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzFavoritesPage, bz_favorites_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,

  PROP_STATE,
  PROP_MODEL,
  PROP_SHOW_SIDEBAR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,
  SIGNAL_REMOVE,
  SIGNAL_SHOW,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static DexFuture *
fetch_favorites_fiber (GWeakRef *wr);

static void
items_changed (BzFavoritesPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model);

static void
set_page (BzFavoritesPage *self);

static void
bz_favorites_page_dispose (GObject *object)
{
  BzFavoritesPage *self = BZ_FAVORITES_PAGE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_favorites_page_parent_class)->dispose (object);
}

static void
bz_favorites_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BzFavoritesPage *self = BZ_FAVORITES_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;
    case PROP_SHOW_SIDEBAR:
      g_value_set_boolean (value, self->show_sidebar);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_favorites_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BzFavoritesPage *self = BZ_FAVORITES_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      self->state = g_value_dup_object (value);
      break;
    case PROP_SHOW_SIDEBAR:
      self->show_sidebar = g_value_get_boolean (value);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SHOW_SIDEBAR]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_favorites_page_constructed (GObject *object)
{
  BzFavoritesPage *self        = BZ_FAVORITES_PAGE (object);
  g_autoptr (DexFuture) future = NULL;

  G_OBJECT_CLASS (bz_favorites_page_parent_class)->constructed (object);

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) fetch_favorites_fiber,
      bz_track_weak (self),
      bz_weak_release);
  dex_future_disown (g_steal_pointer (&future));
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static DexFuture *
tile_activated_fiber (GWeakRef *wr)
{
  g_autoptr (BzFavoritesTile) tile   = NULL;
  g_autoptr (GError) local_error     = NULL;
  g_autoptr (BzEntry) entry          = NULL;
  g_autoptr (GListModel) all_entries = NULL;
  BzFavoritesPage *self              = NULL;
  GtkWidget       *window            = NULL;
  BzEntryGroup    *group             = NULL;

  bz_weak_get_or_return_reject (tile, wr);

  self = (BzFavoritesPage *) gtk_widget_get_ancestor (GTK_WIDGET (tile), BZ_TYPE_FAVORITES_PAGE);
  if (self == NULL)
    return NULL;
  if (self->model == NULL)
    goto err;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  if (window == NULL)
    goto err;

  group = bz_favorites_tile_get_group (tile);
  if (group == NULL)
    goto err;

  entry = bz_entry_group_find_entry (group, NULL, window, &local_error);
  if (entry == NULL)
    {
      if (local_error != NULL)
        g_clear_error (&local_error);

      all_entries = dex_await_object (
          bz_entry_group_dup_all_into_store (group),
          &local_error);
      if (all_entries == NULL || g_list_model_get_n_items (all_entries) == 0)
        goto err;

      entry = g_list_model_get_item (all_entries, 0);
    }

  g_signal_emit (self, signals[SIGNAL_SHOW], 0, entry);
  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
tile_activated_cb (BzFavoritesTile *tile)
{
  g_assert (BZ_IS_FAVORITES_TILE (tile));

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) tile_activated_fiber,
      bz_track_weak (tile), bz_weak_release));
}

static void
bz_favorites_page_class_init (BzFavoritesPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_favorites_page_dispose;
  object_class->constructed  = bz_favorites_page_constructed;
  object_class->get_property = bz_favorites_page_get_property;
  object_class->set_property = bz_favorites_page_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_SHOW_SIDEBAR] =
      g_param_spec_boolean (
          "show-sidebar",
          NULL, NULL,
          FALSE,
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

  g_type_ensure (BZ_TYPE_FAVORITES_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-favorites-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzFavoritesPage, stack);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, tile_activated_cb);
}

static void
bz_favorites_page_init (BzFavoritesPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_favorites_page_new (BzStateInfo *state)
{
  return g_object_new (BZ_TYPE_FAVORITES_PAGE,
                       "state", state,
                       NULL);
}

static void
items_changed (BzFavoritesPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model)
{
  set_page (self);
}

static void
set_page (BzFavoritesPage *self)
{
  if (self->model != NULL &&
      g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0)
    adw_view_stack_set_visible_child_name (self->stack, "content");
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");
}

static int
compare_entry_groups_by_title (BzEntryGroup *group_a,
                               BzEntryGroup *group_b)
{
  const char *title_a = bz_entry_group_get_title (group_a);
  const char *title_b = bz_entry_group_get_title (group_b);
  return g_utf8_collate (title_a, title_b);
}

static DexFuture *
fetch_favorites_fiber (GWeakRef *wr)
{
  g_autoptr (BzFavoritesPage) self    = NULL;
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (GtkStringList) id_list   = NULL;
  g_autoptr (GListModel) model        = NULL;
  g_autoptr (GListStore) sorted_store = NULL;
  g_autoptr (JsonNode) node           = NULL;
  BzApplicationMapFactory *factory    = NULL;
  BzAuthState *auth_state             = NULL;
  const char  *token                  = NULL;
  JsonArray   *array                  = NULL;
  guint        n_favorites            = 0;
  guint        n_items                = 0;

  self = g_weak_ref_get (wr);
  if (self == NULL)
    return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Page destroyed");

  id_list = gtk_string_list_new (NULL);

  auth_state = bz_state_info_get_auth_state (self->state);
  token      = bz_auth_state_get_token (auth_state);

  if (token != NULL && bz_auth_state_is_authenticated (auth_state))
    {
      node = dex_await_boxed (
          bz_query_flathub_v2_json_authenticated ("/favorites", token),
          &local_error);

      if (node == NULL)
        {
          g_warning ("Failed to fetch favorites from Flathub: %s", local_error->message);
          goto done;
        }

      if (!JSON_NODE_HOLDS_ARRAY (node))
        {
          g_warning ("Unexpected response format from Flathub favorites API");
          goto done;
        }

      array       = json_node_get_array (node);
      n_favorites = json_array_get_length (array);

      for (guint i = 0; i < n_favorites; i++)
        {
          JsonObject *favorite_obj = NULL;
          const char *app_id       = NULL;

          favorite_obj = json_array_get_object_element (array, i);
          app_id       = json_object_get_string_member (favorite_obj, "app_id");

          if (app_id != NULL)
            gtk_string_list_append (id_list, app_id);
        }
    }

done:
  factory = bz_state_info_get_application_factory (self->state);
  model   = bz_application_map_factory_generate (factory, G_LIST_MODEL (id_list));

  sorted_store = g_list_store_new (BZ_TYPE_ENTRY_GROUP);
  n_items      = g_list_model_get_n_items (model);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntryGroup) group = g_list_model_get_item (model, i);
      g_list_store_append (sorted_store, group);
    }
  g_list_store_sort (sorted_store, (GCompareDataFunc) compare_entry_groups_by_title, NULL);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);

  g_clear_object (&self->model);
  self->model = G_LIST_MODEL (g_steal_pointer (&sorted_store));

  g_signal_connect_swapped (self->model, "items-changed", G_CALLBACK (items_changed), self);
  set_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);

  return dex_future_new_true ();
}

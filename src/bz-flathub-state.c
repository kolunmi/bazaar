/* bz-flathub-state.c
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

#define G_LOG_DOMAIN                 "BAZAAR::FLATHUB"
#define COLLECTION_FETCH_SIZE        192
#define CATEGORY_FETCH_SIZE          96
#define QUALITY_MODERATION_PAGE_SIZE 300

#include <json-glib/json-glib.h>
#include <libdex.h>

#include "bz-env.h"
#include "bz-flathub-category.h"
#include "bz-flathub-state.h"
#include "bz-global-state.h"
#include "bz-io.h"
#include "bz-util.h"

struct _BzFlathubState
{
  GObject parent_instance;

  char                    *for_day;
  BzApplicationMapFactory *map_factory;
  char                    *app_of_the_day;
  GtkStringList           *apps_of_the_week;
  GListStore              *categories;
  gboolean                 has_connection_error;

  DexFuture *initializing;
};

G_DEFINE_FINAL_TYPE (BzFlathubState, bz_flathub_state, G_TYPE_OBJECT);

static GListModel *bz_flathub_state_dup_apps_of_the_day_week (BzFlathubState *self);

enum
{
  PROP_0,

  PROP_FOR_DAY,
  PROP_MAP_FACTORY,
  PROP_APP_OF_THE_DAY,
  PROP_APP_OF_THE_DAY_GROUP,
  PROP_APPS_OF_THE_WEEK,
  PROP_APPS_OF_THE_DAY_WEEK,
  PROP_CATEGORIES,
  PROP_HAS_CONNECTION_ERROR,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static DexFuture *
initialize_fiber (GWeakRef *wr);
static DexFuture *
initialize_finally (DexFuture *future,
                    GWeakRef  *wr);
static gboolean
bz_flathub_state_get_has_connection_error (BzFlathubState *self);

static void
bz_flathub_state_dispose (GObject *object)
{
  BzFlathubState *self = BZ_FLATHUB_STATE (object);

  dex_clear (&self->initializing);

  g_clear_pointer (&self->for_day, g_free);
  g_clear_pointer (&self->map_factory, g_object_unref);
  g_clear_pointer (&self->app_of_the_day, g_free);
  g_clear_pointer (&self->apps_of_the_week, g_object_unref);
  g_clear_pointer (&self->categories, g_object_unref);

  G_OBJECT_CLASS (bz_flathub_state_parent_class)->dispose (object);
}

static void
bz_flathub_state_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzFlathubState *self = BZ_FLATHUB_STATE (object);

  switch (prop_id)
    {
    case PROP_FOR_DAY:
      g_value_set_string (value, bz_flathub_state_get_for_day (self));
      break;
    case PROP_MAP_FACTORY:
      g_value_set_object (value, bz_flathub_state_get_map_factory (self));
      break;
    case PROP_APP_OF_THE_DAY:
      g_value_set_string (value, bz_flathub_state_get_app_of_the_day (self));
      break;
    case PROP_APP_OF_THE_DAY_GROUP:
      g_value_take_object (value, bz_flathub_state_dup_app_of_the_day_group (self));
      break;
    case PROP_APPS_OF_THE_WEEK:
      g_value_take_object (value, bz_flathub_state_dup_apps_of_the_week (self));
      break;
    case PROP_APPS_OF_THE_DAY_WEEK:
      g_value_take_object (value, bz_flathub_state_dup_apps_of_the_day_week (self));
      break;
    case PROP_CATEGORIES:
      g_value_set_object (value, bz_flathub_state_get_categories (self));
      break;
    case PROP_HAS_CONNECTION_ERROR:
      g_value_set_boolean (value, bz_flathub_state_get_has_connection_error (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_state_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzFlathubState *self = BZ_FLATHUB_STATE (object);

  switch (prop_id)
    {
    case PROP_FOR_DAY:
      bz_flathub_state_set_for_day (self, g_value_get_string (value));
      break;
    case PROP_MAP_FACTORY:
      bz_flathub_state_set_map_factory (self, g_value_get_object (value));
      break;
    case PROP_APP_OF_THE_DAY:
    case PROP_APP_OF_THE_DAY_GROUP:
    case PROP_APPS_OF_THE_WEEK:
    case PROP_APPS_OF_THE_DAY_WEEK:
    case PROP_CATEGORIES:
    case PROP_HAS_CONNECTION_ERROR:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_state_class_init (BzFlathubStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_flathub_state_set_property;
  object_class->get_property = bz_flathub_state_get_property;
  object_class->dispose      = bz_flathub_state_dispose;

  props[PROP_FOR_DAY] =
      g_param_spec_string (
          "for-day",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MAP_FACTORY] =
      g_param_spec_object (
          "map-factory",
          NULL, NULL,
          BZ_TYPE_APPLICATION_MAP_FACTORY,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_APP_OF_THE_DAY] =
      g_param_spec_string (
          "app-of-the-day",
          NULL, NULL, NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_APP_OF_THE_DAY_GROUP] =
      g_param_spec_object (
          "app-of-the-day-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_APPS_OF_THE_WEEK] =
      g_param_spec_object (
          "apps-of-the-week",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_APPS_OF_THE_DAY_WEEK] =
      g_param_spec_object (
          "apps-of-the-day-week",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CATEGORIES] =
      g_param_spec_object (
          "categories",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_HAS_CONNECTION_ERROR] =
      g_param_spec_boolean (
          "has-connection-error",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_flathub_state_init (BzFlathubState *self)
{
}

BzFlathubState *
bz_flathub_state_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_STATE, NULL);
}

const char *
bz_flathub_state_get_for_day (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  return self->for_day;
}

BzApplicationMapFactory *
bz_flathub_state_get_map_factory (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  return self->map_factory;
}

const char *
bz_flathub_state_get_app_of_the_day (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  if (self->initializing != NULL)
    return NULL;
  return self->app_of_the_day;
}

BzEntryGroup *
bz_flathub_state_dup_app_of_the_day_group (BzFlathubState *self)
{
  g_autoptr (GtkStringObject) string = NULL;

  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  if (self->initializing != NULL)
    return NULL;
  g_return_val_if_fail (self->map_factory != NULL, NULL);

  string = gtk_string_object_new (self->app_of_the_day);
  return bz_application_map_factory_convert_one (self->map_factory, string);
}

GListModel *
bz_flathub_state_dup_apps_of_the_week (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  if (self->initializing != NULL)
    return NULL;

  if (self->apps_of_the_week != NULL)
    {
      if (self->map_factory != NULL)
        return bz_application_map_factory_generate (
            self->map_factory, G_LIST_MODEL (self->apps_of_the_week));
      else
        return G_LIST_MODEL (g_object_ref (self->apps_of_the_week));
    }
  else
    return NULL;
}

GListModel *
bz_flathub_state_dup_apps_of_the_day_week (BzFlathubState *self)
{
  g_autoptr (GtkStringList) combined_list = NULL;

  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  if (self->initializing != NULL)
    return NULL;

  combined_list = gtk_string_list_new (NULL);

  if (self->app_of_the_day != NULL)
    gtk_string_list_append (combined_list, self->app_of_the_day);

  if (self->apps_of_the_week != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->apps_of_the_week));
      for (guint i = 0; i < n_items; i++)
        {
          const char *app_id = gtk_string_list_get_string (self->apps_of_the_week, i);
          gtk_string_list_append (combined_list, app_id);
        }
    }

  if (self->map_factory != NULL)
    return bz_application_map_factory_generate (self->map_factory, G_LIST_MODEL (combined_list));
  else
    return G_LIST_MODEL (g_object_ref (combined_list));
}

GListModel *
bz_flathub_state_get_categories (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), NULL);
  if (self->initializing != NULL)
    return NULL;
  return G_LIST_MODEL (self->categories);
}

static gboolean
bz_flathub_state_get_has_connection_error (BzFlathubState *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_STATE (self), FALSE);
  return self->has_connection_error;
}

void
bz_flathub_state_set_for_day (BzFlathubState *self,
                              const char     *for_day)
{
  g_return_if_fail (BZ_IS_FLATHUB_STATE (self));

  dex_clear (&self->initializing);

  g_clear_pointer (&self->for_day, g_free);
  g_clear_pointer (&self->app_of_the_day, g_free);
  g_clear_pointer (&self->apps_of_the_week, g_object_unref);
  g_clear_pointer (&self->categories, g_object_unref);
  self->has_connection_error = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APP_OF_THE_DAY]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APP_OF_THE_DAY_GROUP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPS_OF_THE_WEEK]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPS_OF_THE_DAY_WEEK]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CATEGORIES]);

  if (for_day != NULL)
    {
      g_autoptr (DexFuture) future = NULL;

      self->for_day          = g_strdup (for_day);
      self->apps_of_the_week = gtk_string_list_new (NULL);
      self->categories       = g_list_store_new (BZ_TYPE_FLATHUB_CATEGORY);

      future = dex_scheduler_spawn (
          bz_get_io_scheduler (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) initialize_fiber,
          bz_track_weak (self), bz_weak_release);
      future = dex_future_finally (
          future,
          (DexFutureCallback) initialize_finally,
          bz_track_weak (self), bz_weak_release);
      self->initializing = g_steal_pointer (&future);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FOR_DAY]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_CONNECTION_ERROR]);
}

void
bz_flathub_state_update_to_today (BzFlathubState *self)
{
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree gchar *for_day      = NULL;

  g_return_if_fail (BZ_IS_FLATHUB_STATE (self));

  datetime = g_date_time_new_now_utc ();
  for_day  = g_date_time_format (datetime, "%F");

  g_debug ("Syncing with flathub for day: %s", for_day);
  bz_flathub_state_set_for_day (self, for_day);
}

void
bz_flathub_state_set_map_factory (BzFlathubState          *self,
                                  BzApplicationMapFactory *map_factory)
{
  g_return_if_fail (BZ_IS_FLATHUB_STATE (self));
  g_return_if_fail (map_factory == NULL || BZ_IS_APPLICATION_MAP_FACTORY (map_factory));

  g_clear_object (&self->map_factory);
  if (map_factory != NULL)
    self->map_factory = g_object_ref (map_factory);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAP_FACTORY]);
}

static void
add_collection_category (BzFlathubState *self,
                         const char     *name,
                         JsonNode       *node,
                         GHashTable     *quality_set)
{
  g_autoptr (BzFlathubCategory) category  = NULL;
  g_autoptr (GtkStringList) store         = NULL;
  g_autoptr (GtkStringList) quality_store = NULL;
  JsonObject *response_object             = NULL;
  JsonArray  *hits_array                  = NULL;
  guint       hits_length                 = 0;
  int         total_hits                  = 0;

  category      = bz_flathub_category_new ();
  store         = gtk_string_list_new (NULL);
  quality_store = gtk_string_list_new (NULL);
  bz_flathub_category_set_name (category, name);
  bz_flathub_category_set_is_spotlight (category, TRUE);
  bz_flathub_category_set_applications (category, G_LIST_MODEL (store));

  response_object = json_node_get_object (node);
  hits_array      = json_object_get_array_member (response_object, "hits");
  hits_length     = json_array_get_length (hits_array);
  total_hits      = json_object_get_int_member (response_object, "totalHits");
  bz_flathub_category_set_total_entries (category, total_hits);

  for (guint i = 0; i < hits_length; i++)
    {
      JsonObject *element = NULL;
      const char *app_id  = NULL;

      element = json_array_get_object_element (hits_array, i);
      app_id  = json_object_get_string_member (element, "app_id");
      gtk_string_list_append (store, app_id);

      if (g_hash_table_contains (quality_set, app_id))
        {
          gtk_string_list_append (quality_store, app_id);
        }
    }

  bz_flathub_category_set_quality_applications (category, G_LIST_MODEL (quality_store));
  g_list_store_append (self->categories, category);
}

static DexFuture *
initialize_fiber (GWeakRef *wr)
{
  g_autoptr (BzFlathubState) self    = NULL;
  const char *for_day                = NULL;
  g_autoptr (GError) local_error     = NULL;
  g_autoptr (GHashTable) futures     = NULL;
  g_autoptr (GHashTable) nodes       = NULL;
  g_autoptr (GHashTable) quality_set = NULL;
  guint total_requests               = 0;
  guint successful_requests          = 0;

  bz_weak_get_or_return_reject (self, wr);
  for_day = self->for_day;

  futures     = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, dex_unref);
  nodes       = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) json_node_unref);
  quality_set = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

#define ADD_REQUEST(key, ...)                  \
  G_STMT_START                                 \
  {                                            \
    g_autofree char *_request     = NULL;      \
    g_autoptr (DexFuture) _future = NULL;      \
                                               \
    _request = g_strdup_printf (__VA_ARGS__);  \
    _future  = bz_query_flathub_v2_json_take ( \
        g_steal_pointer (&_request));         \
    g_hash_table_replace (                     \
        futures,                               \
        g_strdup (key),                        \
        g_steal_pointer (&_future));           \
  }                                            \
  G_STMT_END

  ADD_REQUEST ("/app-picks/app-of-the-day", "/app-picks/app-of-the-day/%s", for_day);
  ADD_REQUEST ("/app-picks/apps-of-the-week", "/app-picks/apps-of-the-week/%s", for_day);
  ADD_REQUEST ("/collection/category", "/collection/category");
  ADD_REQUEST ("/collection/recently-updated", "/collection/recently-updated?page=0&per_page=%d", COLLECTION_FETCH_SIZE);
  ADD_REQUEST ("/collection/recently-added", "/collection/recently-added?page=0&per_page=%d", COLLECTION_FETCH_SIZE);
  ADD_REQUEST ("/collection/popular", "/collection/popular?page=0&per_page=%d", COLLECTION_FETCH_SIZE);
  ADD_REQUEST ("/collection/trending", "/collection/trending?page=0&per_page=%d", COLLECTION_FETCH_SIZE);
  ADD_REQUEST ("/collection/mobile", "/collection/mobile?page=0&per_page=%d", COLLECTION_FETCH_SIZE);
  ADD_REQUEST ("/quality-moderation/passing-apps", "/quality-moderation/passing-apps?page=1&page_size=%d", QUALITY_MODERATION_PAGE_SIZE);

  total_requests = g_hash_table_size (futures);

  while (g_hash_table_size (futures) > 0)
    {
      GHashTableIter   iter        = { 0 };
      g_autofree char *request     = NULL;
      g_autoptr (DexFuture) future = NULL;
      g_autoptr (JsonNode) node    = NULL;

      g_hash_table_iter_init (&iter, futures);
      g_hash_table_iter_next (&iter, (gpointer *) &request, (gpointer *) &future);
      g_hash_table_iter_steal (&iter);

      node = dex_await_boxed (g_steal_pointer (&future), &local_error);
      if (node == NULL)
        {
          g_warning ("Failed to complete request '%s' from flathub: %s", request, local_error->message);
          g_clear_error (&local_error);
          continue;
        }
      g_hash_table_replace (nodes, g_steal_pointer (&request), g_steal_pointer (&node));
      successful_requests++;
    }

  if (g_hash_table_contains (nodes, "/quality-moderation/passing-apps"))
    {
      JsonObject *object = NULL;
      JsonArray  *array  = NULL;
      guint       length = 0;

      object = json_node_get_object (g_hash_table_lookup (nodes, "/quality-moderation/passing-apps"));
      array  = json_object_get_array_member (object, "apps");
      length = json_array_get_length (array);

      for (guint i = 0; i < length; i++)
        {
          const char *app_id = NULL;

          app_id = json_array_get_string_element (array, i);
          g_hash_table_add (quality_set, g_strdup (app_id));
        }
    }

  if (g_hash_table_contains (nodes, "/app-picks/app-of-the-day"))
    {
      JsonObject *object = NULL;

      object               = json_node_get_object (g_hash_table_lookup (nodes, "/app-picks/app-of-the-day"));
      self->app_of_the_day = g_strdup (json_object_get_string_member (object, "app_id"));
    }
  if (g_hash_table_contains (nodes, "/app-picks/apps-of-the-week"))
    {
      JsonObject *object = NULL;
      JsonArray  *array  = NULL;
      guint       length = 0;

      object = json_node_get_object (g_hash_table_lookup (nodes, "/app-picks/apps-of-the-week"));
      array  = json_object_get_array_member (object, "apps");
      length = json_array_get_length (array);

      for (guint i = 0; i < length; i++)
        {
          JsonObject *element = NULL;

          element = json_array_get_object_element (array, i);
          gtk_string_list_append (
              self->apps_of_the_week,
              json_object_get_string_member (element, "app_id"));
        }
    }

  if (g_hash_table_contains (nodes, "/collection/trending"))
    add_collection_category (self, "trending",
                             g_hash_table_lookup (nodes, "/collection/trending"),
                             quality_set);

  if (g_hash_table_contains (nodes, "/collection/popular"))
    add_collection_category (self, "popular",
                             g_hash_table_lookup (nodes, "/collection/popular"),
                             quality_set);

  if (g_hash_table_contains (nodes, "/collection/recently-added"))
    add_collection_category (self, "recently-added",
                             g_hash_table_lookup (nodes, "/collection/recently-added"),
                             quality_set);

  if (g_hash_table_contains (nodes, "/collection/recently-updated"))
    add_collection_category (self, "recently-updated",
                             g_hash_table_lookup (nodes, "/collection/recently-updated"),
                             quality_set);

  if (g_hash_table_contains (nodes, "/collection/mobile"))
    add_collection_category (self, "mobile",
                             g_hash_table_lookup (nodes, "/collection/mobile"),
                             quality_set);

  /* Add regular categories */
  if (g_hash_table_contains (nodes, "/collection/category"))
    {
      JsonArray *array  = NULL;
      guint      length = 0;

      array  = json_node_get_array (g_hash_table_lookup (nodes, "/collection/category"));
      length = json_array_get_length (array);

      for (guint i = 0; i < length; i++)
        {
          const char *category = NULL;

          category = json_array_get_string_element (array, i);
          ADD_REQUEST (category, "/collection/category/%s?page=0&per_page=%d", category, CATEGORY_FETCH_SIZE);
        }

      total_requests += g_hash_table_size (futures);

      while (g_hash_table_size (futures) > 0)
        {
          GHashTableIter   iter                   = { 0 };
          g_autofree char *name                   = NULL;
          g_autoptr (DexFuture) future            = NULL;
          g_autoptr (JsonNode) node               = NULL;
          g_autoptr (BzFlathubCategory) category  = NULL;
          g_autoptr (GtkStringList) store         = NULL;
          g_autoptr (GtkStringList) quality_store = NULL;
          JsonObject *response_object             = NULL;
          JsonArray  *category_array              = NULL;
          guint       category_length             = 0;
          int         total_hits                  = 0;

          g_hash_table_iter_init (&iter, futures);
          g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &future);
          g_hash_table_iter_steal (&iter);

          node = dex_await_boxed (g_steal_pointer (&future), &local_error);
          if (node == NULL)
            {
              g_warning ("Failed to retrieve category '%s' from flathub: %s", name, local_error->message);
              g_clear_error (&local_error);
              continue;
            }

          successful_requests++;

          category      = bz_flathub_category_new ();
          store         = gtk_string_list_new (NULL);
          quality_store = gtk_string_list_new (NULL);
          bz_flathub_category_set_name (category, name);
          bz_flathub_category_set_applications (category, G_LIST_MODEL (store));
          response_object = json_node_get_object (node);
          category_array  = json_object_get_array_member (response_object, "hits");
          category_length = json_array_get_length (category_array);
          total_hits      = json_object_get_int_member (response_object, "totalHits");
          bz_flathub_category_set_total_entries (category, total_hits);

          for (guint i = 0; i < category_length; i++)
            {
              JsonObject *element = NULL;
              const char *app_id  = NULL;

              element = json_array_get_object_element (category_array, i);
              app_id  = json_object_get_string_member (element, "app_id");
              gtk_string_list_append (store, app_id);

              if (g_hash_table_contains (quality_set, app_id))
                {
                  gtk_string_list_append (quality_store, app_id);
                }
            }

          bz_flathub_category_set_quality_applications (category, G_LIST_MODEL (quality_store));

          g_list_store_append (self->categories, category);
        }
    }

  if (successful_requests == 0)
    {
      self->has_connection_error = TRUE;
      return dex_future_new_for_error (
          g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                       "All Flathub API requests failed"));
    }

  return dex_future_new_true ();
}

static DexFuture *
initialize_finally (DexFuture *future,
                    GWeakRef  *wr)
{
  g_autoptr (BzFlathubState) self = NULL;
  guint n_categories              = 0;

  bz_weak_get_or_return_reject (self, wr);

  n_categories = g_list_model_get_n_items (G_LIST_MODEL (self->categories));
  for (guint i = 0; i < n_categories; i++)
    {
      g_autoptr (BzFlathubCategory) category = NULL;

      category = g_list_model_get_item (G_LIST_MODEL (self->categories), i);
      g_object_bind_property (self, "map-factory", category, "map-factory", G_BINDING_SYNC_CREATE);
    }

  self->initializing = NULL;
  g_debug ("Done syncing flathub state; notifying property listeners...");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APP_OF_THE_DAY]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APP_OF_THE_DAY_GROUP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPS_OF_THE_WEEK]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPS_OF_THE_DAY_WEEK]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CATEGORIES]);

  return NULL;
}

/* End of bz-flathub-state.c */

/* bz-entry.c
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

#include <json-glib/json-glib.h>
#include <libdex.h>

#include "bz-data-point.h"
#include "bz-entry.h"
#include "bz-env.h"
#include "bz-global-state.h"
#include "bz-util.h"

G_DEFINE_FLAGS_TYPE (
    BzEntryKind,
    bz_entry_kind,
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_APPLICATION, "application"),
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_RUNTIME, "runtime"),
    G_DEFINE_ENUM_VALUE (BZ_ENTRY_KIND_ADDON, "addon"))

typedef struct
{
  gint        hold;
  gboolean    installed;
  guint       kinds;
  BzEntry    *runtime;
  GListStore *addons;

  char         *id;
  char         *unique_id;
  char         *title;
  char         *eol;
  char         *description;
  char         *long_description;
  char         *remote_repo_name;
  char         *url;
  guint64       size;
  GdkPaintable *icon_paintable;
  GIcon        *mini_icon;
  GdkPaintable *remote_repo_icon;
  GPtrArray    *search_tokens;
  char         *metadata_license;
  char         *project_license;
  gboolean      is_floss;
  char         *project_group;
  char         *developer;
  char         *developer_id;
  GListModel   *screenshot_paintables;
  GListModel   *share_urls;
  char         *donation_url;
  GListModel   *reviews;
  double        average_rating;
  char         *ratings_summary;
  GListModel   *version_history;

  gboolean    is_flathub;
  gboolean    verified;
  GListModel *download_stats;
  int         recent_downloads;

  GHashTable *flathub_prop_queries;
} BzEntryPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BzEntry, bz_entry, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_HOLDING,
  PROP_INSTALLED,
  PROP_KINDS,
  PROP_RUNTIME,
  PROP_ADDONS,

  PROP_ID,
  PROP_UNIQUE_ID,
  PROP_TITLE,
  PROP_EOL,
  PROP_DESCRIPTION,
  PROP_LONG_DESCRIPTION,
  PROP_REMOTE_REPO_NAME,
  PROP_URL,
  PROP_SIZE,
  PROP_ICON_PAINTABLE,
  PROP_MINI_ICON,
  PROP_SEARCH_TOKENS,
  PROP_REMOTE_REPO_ICON,
  PROP_METADATA_LICENSE,
  PROP_PROJECT_LICENSE,
  PROP_IS_FLOSS,
  PROP_PROJECT_GROUP,
  PROP_DEVELOPER,
  PROP_DEVELOPER_ID,
  PROP_SCREENSHOT_PAINTABLES,
  PROP_SHARE_URLS,
  PROP_DONATION_URL,
  PROP_REVIEWS,
  PROP_AVERAGE_RATING,
  PROP_RATINGS_SUMMARY,
  PROP_VERSION_HISTORY,

  PROP_IS_FLATHUB,
  PROP_VERIFIED,
  PROP_DOWNLOAD_STATS,
  PROP_RECENT_DOWNLOADS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    query_flathub,
    QueryFlathub,
    {
      BzEntry *self;
      int      prop;
      char    *id;
    },
    BZ_RELEASE_DATA (id, g_free));
static DexFuture *
query_flathub_fiber (QueryFlathubData *data);
static DexFuture *
query_flathub_then (DexFuture        *future,
                    QueryFlathubData *data);

static void
query_flathub (BzEntry *self,
               int      prop);

static void
download_stats_per_day_foreach (JsonObject  *object,
                                const gchar *member_name,
                                JsonNode    *member_node,
                                GListStore  *store);

static void
bz_entry_dispose (GObject *object)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  g_clear_pointer (&priv->flathub_prop_queries, g_hash_table_unref);

  g_clear_object (&priv->runtime);
  g_clear_object (&priv->addons);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->unique_id, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->eol, g_free);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->long_description, g_free);
  g_clear_pointer (&priv->remote_repo_name, g_free);
  g_clear_pointer (&priv->url, g_free);
  g_clear_object (&priv->icon_paintable);
  g_clear_object (&priv->mini_icon);
  g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
  g_clear_object (&priv->remote_repo_icon);
  g_clear_pointer (&priv->metadata_license, g_free);
  g_clear_pointer (&priv->project_license, g_free);
  g_clear_pointer (&priv->project_group, g_free);
  g_clear_pointer (&priv->developer, g_free);
  g_clear_pointer (&priv->developer_id, g_free);
  g_clear_object (&priv->screenshot_paintables);
  g_clear_object (&priv->share_urls);
  g_clear_pointer (&priv->donation_url, g_free);
  g_clear_object (&priv->reviews);
  g_clear_pointer (&priv->ratings_summary, g_free);
  g_clear_object (&priv->version_history);
  g_clear_object (&priv->download_stats);

  G_OBJECT_CLASS (bz_entry_parent_class)->dispose (object);
}

static void
bz_entry_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HOLDING:
      g_value_set_boolean (value, bz_entry_is_holding (self));
      break;
    case PROP_INSTALLED:
      g_value_set_boolean (value, priv->installed);
      break;
    case PROP_RUNTIME:
      g_value_set_object (value, priv->runtime);
      break;
    case PROP_ADDONS:
      g_value_set_object (value, priv->addons);
      break;
    case PROP_KINDS:
      g_value_set_flags (value, priv->kinds);
      break;
    case PROP_ID:
      g_value_set_string (value, priv->id);
      break;
    case PROP_UNIQUE_ID:
      g_value_set_string (value, priv->unique_id);
      break;
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_EOL:
      g_value_set_string (value, priv->eol);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    case PROP_LONG_DESCRIPTION:
      g_value_set_string (value, priv->long_description);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_value_set_string (value, priv->remote_repo_name);
      break;
    case PROP_URL:
      g_value_set_string (value, priv->url);
      break;
    case PROP_SIZE:
      g_value_set_uint64 (value, priv->size);
      break;
    case PROP_ICON_PAINTABLE:
      g_value_set_object (value, priv->icon_paintable);
      break;
    case PROP_MINI_ICON:
      g_value_set_object (value, priv->mini_icon);
      break;
    case PROP_SEARCH_TOKENS:
      g_value_set_boxed (value, priv->search_tokens);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_value_set_object (value, priv->remote_repo_icon);
      break;
    case PROP_METADATA_LICENSE:
      g_value_set_string (value, priv->metadata_license);
      break;
    case PROP_PROJECT_LICENSE:
      g_value_set_string (value, priv->project_license);
      break;
    case PROP_IS_FLOSS:
      g_value_set_boolean (value, priv->is_floss);
      break;
    case PROP_PROJECT_GROUP:
      g_value_set_string (value, priv->project_group);
      break;
    case PROP_DEVELOPER:
      g_value_set_string (value, priv->developer);
      break;
    case PROP_DEVELOPER_ID:
      g_value_set_string (value, priv->developer_id);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_value_set_object (value, priv->screenshot_paintables);
      break;
    case PROP_SHARE_URLS:
      g_value_set_object (value, priv->share_urls);
      break;
    case PROP_DONATION_URL:
      g_value_set_string (value, priv->donation_url);
      break;
    case PROP_REVIEWS:
      g_value_set_object (value, priv->reviews);
      break;
    case PROP_AVERAGE_RATING:
      g_value_set_double (value, priv->average_rating);
      break;
    case PROP_RATINGS_SUMMARY:
      g_value_set_string (value, priv->ratings_summary);
      break;
    case PROP_VERSION_HISTORY:
      g_value_set_object (value, priv->version_history);
      break;
    case PROP_IS_FLATHUB:
      g_value_set_boolean (value, priv->is_flathub);
      break;
    case PROP_VERIFIED:
      query_flathub (self, PROP_VERIFIED);
      g_value_set_boolean (value, priv->verified);
      break;
    case PROP_DOWNLOAD_STATS:
      query_flathub (self, PROP_DOWNLOAD_STATS);
      g_value_set_object (value, priv->download_stats);
      break;
    case PROP_RECENT_DOWNLOADS:
      query_flathub (self, PROP_DOWNLOAD_STATS);
      g_value_set_int (value, priv->recent_downloads);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  BzEntry        *self = BZ_ENTRY (object);
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_INSTALLED:
      priv->installed = g_value_get_boolean (value);
      break;
    case PROP_RUNTIME:
      g_clear_object (&priv->runtime);
      priv->runtime = g_value_dup_object (value);
      break;
    case PROP_ADDONS:
      g_clear_object (&priv->addons);
      priv->addons = g_value_dup_object (value);
      break;
    case PROP_KINDS:
      priv->kinds = g_value_get_flags (value);
      break;
    case PROP_ID:
      g_clear_pointer (&priv->id, g_free);
      priv->id = g_value_dup_string (value);
      break;
    case PROP_UNIQUE_ID:
      g_clear_pointer (&priv->unique_id, g_free);
      priv->unique_id = g_value_dup_string (value);
      break;
    case PROP_TITLE:
      g_clear_pointer (&priv->title, g_free);
      priv->title = g_value_dup_string (value);
      break;
    case PROP_EOL:
      g_clear_pointer (&priv->eol, g_free);
      priv->eol = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      g_clear_pointer (&priv->description, g_free);
      priv->description = g_value_dup_string (value);
      break;
    case PROP_LONG_DESCRIPTION:
      g_clear_pointer (&priv->long_description, g_free);
      priv->long_description = g_value_dup_string (value);
      break;
    case PROP_REMOTE_REPO_NAME:
      g_clear_pointer (&priv->remote_repo_name, g_free);
      priv->remote_repo_name = g_value_dup_string (value);
      priv->is_flathub       = g_strcmp0 (priv->remote_repo_name, "flathub") == 0;
      g_object_notify_by_pspec (object, props[PROP_IS_FLATHUB]);
      break;
    case PROP_URL:
      g_clear_pointer (&priv->url, g_free);
      priv->url = g_value_dup_string (value);
      break;
    case PROP_SIZE:
      priv->size = g_value_get_uint64 (value);
      break;
    case PROP_ICON_PAINTABLE:
      g_clear_object (&priv->icon_paintable);
      priv->icon_paintable = g_value_dup_object (value);
      break;
    case PROP_MINI_ICON:
      g_clear_object (&priv->mini_icon);
      priv->mini_icon = g_value_dup_object (value);
      break;
    case PROP_SEARCH_TOKENS:
      g_clear_pointer (&priv->search_tokens, g_ptr_array_unref);
      priv->search_tokens = g_value_dup_boxed (value);
      break;
    case PROP_REMOTE_REPO_ICON:
      g_clear_object (&priv->remote_repo_icon);
      priv->remote_repo_icon = g_value_dup_object (value);
      break;
    case PROP_METADATA_LICENSE:
      g_clear_pointer (&priv->metadata_license, g_free);
      priv->metadata_license = g_value_dup_string (value);
      break;
    case PROP_PROJECT_LICENSE:
      g_clear_pointer (&priv->project_license, g_free);
      priv->project_license = g_value_dup_string (value);
      break;
    case PROP_IS_FLOSS:
      priv->is_floss = g_value_get_boolean (value);
      break;
    case PROP_PROJECT_GROUP:
      g_clear_pointer (&priv->project_group, g_free);
      priv->project_group = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER:
      g_clear_pointer (&priv->developer, g_free);
      priv->developer = g_value_dup_string (value);
      break;
    case PROP_DEVELOPER_ID:
      g_clear_pointer (&priv->developer_id, g_free);
      priv->developer_id = g_value_dup_string (value);
      break;
    case PROP_SCREENSHOT_PAINTABLES:
      g_clear_object (&priv->screenshot_paintables);
      priv->screenshot_paintables = g_value_dup_object (value);
      break;
    case PROP_SHARE_URLS:
      g_clear_object (&priv->share_urls);
      priv->share_urls = g_value_dup_object (value);
      break;
    case PROP_DONATION_URL:
      g_clear_pointer (&priv->donation_url, g_free);
      priv->donation_url = g_value_dup_string (value);
      break;
    case PROP_REVIEWS:
      g_clear_object (&priv->reviews);
      priv->reviews = g_value_dup_object (value);
      break;
    case PROP_AVERAGE_RATING:
      priv->average_rating = g_value_get_double (value);
      break;
    case PROP_RATINGS_SUMMARY:
      g_clear_pointer (&priv->ratings_summary, g_free);
      priv->ratings_summary = g_value_dup_string (value);
      break;
    case PROP_VERSION_HISTORY:
      g_clear_object (&priv->version_history);
      priv->version_history = g_value_dup_object (value);
      break;
    case PROP_IS_FLATHUB:
      priv->is_flathub = g_value_get_boolean (value);
      break;
    case PROP_VERIFIED:
      priv->verified = g_value_get_boolean (value);
      break;
    case PROP_DOWNLOAD_STATS:
      {
        g_clear_object (&priv->download_stats);
        priv->download_stats = g_value_dup_object (value);

        if (priv->download_stats != NULL)
          {
            guint n_items          = 0;
            guint start            = 0;
            guint recent_downloads = 0;

            n_items = g_list_model_get_n_items (priv->download_stats);
            start   = n_items - MIN (n_items, 30);

            for (guint i = start; i < n_items; i++)
              {
                g_autoptr (BzDataPoint) point = NULL;

                point = g_list_model_get_item (priv->download_stats, i);
                recent_downloads += bz_data_point_get_dependent (point);
              }
            priv->recent_downloads = recent_downloads;
          }
        else
          priv->recent_downloads = 0;
        g_object_notify_by_pspec (object, props[PROP_RECENT_DOWNLOADS]);
      }
      break;
    case PROP_RECENT_DOWNLOADS:
      priv->recent_downloads = g_value_get_int (value);
      break;
    case PROP_HOLDING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_class_init (BzEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_set_property;
  object_class->get_property = bz_entry_get_property;
  object_class->dispose      = bz_entry_dispose;

  props[PROP_HOLDING] =
      g_param_spec_boolean (
          "holding",
          NULL, NULL, FALSE,
          G_PARAM_READABLE);

  props[PROP_INSTALLED] =
      g_param_spec_boolean (
          "installed",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_RUNTIME] =
      g_param_spec_object (
          "runtime",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_READWRITE);

  props[PROP_ADDONS] =
      g_param_spec_object (
          "addons",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_KINDS] =
      g_param_spec_flags (
          "kinds",
          NULL, NULL,
          BZ_TYPE_ENTRY_KIND, 0,
          G_PARAM_READWRITE);

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_UNIQUE_ID] =
      g_param_spec_string (
          "unique-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_EOL] =
      g_param_spec_string (
          "eol",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_LONG_DESCRIPTION] =
      g_param_spec_string (
          "long-description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_URL] =
      g_param_spec_string (
          "url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_NAME] =
      g_param_spec_string (
          "remote-repo-name",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SIZE] =
      g_param_spec_uint64 (
          "size",
          NULL, NULL,
          0, G_MAXUINT64, 0,
          G_PARAM_READWRITE);

  props[PROP_ICON_PAINTABLE] =
      g_param_spec_object (
          "icon-paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_MINI_ICON] =
      g_param_spec_object (
          "mini-icon",
          NULL, NULL,
          G_TYPE_ICON,
          G_PARAM_READWRITE);

  props[PROP_SEARCH_TOKENS] =
      g_param_spec_boxed (
          "search-tokens",
          NULL, NULL,
          G_TYPE_PTR_ARRAY,
          G_PARAM_READWRITE);

  props[PROP_REMOTE_REPO_ICON] =
      g_param_spec_object (
          "remote-repo-icon",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READWRITE);

  props[PROP_METADATA_LICENSE] =
      g_param_spec_string (
          "metadata-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_LICENSE] =
      g_param_spec_string (
          "project-license",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_IS_FLOSS] =
      g_param_spec_boolean (
          "is-floss",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_PROJECT_GROUP] =
      g_param_spec_string (
          "project-group",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER] =
      g_param_spec_string (
          "developer",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_DEVELOPER_ID] =
      g_param_spec_string (
          "developer-id",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_SCREENSHOT_PAINTABLES] =
      g_param_spec_object (
          "screenshot-paintables",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_SHARE_URLS] =
      g_param_spec_object (
          "share-urls",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_DONATION_URL] =
      g_param_spec_string (
          "donation-url",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_REVIEWS] =
      g_param_spec_object (
          "reviews",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_AVERAGE_RATING] =
      g_param_spec_double (
          "average-rating",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READWRITE);

  props[PROP_RATINGS_SUMMARY] =
      g_param_spec_string (
          "ratings-summary",
          NULL, NULL, NULL,
          G_PARAM_READWRITE);

  props[PROP_VERSION_HISTORY] =
      g_param_spec_object (
          "version-history",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_IS_FLATHUB] =
      g_param_spec_boolean (
          "is-flathub",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_VERIFIED] =
      g_param_spec_boolean (
          "verified",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE);

  props[PROP_DOWNLOAD_STATS] =
      g_param_spec_object (
          "download-stats",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE);

  props[PROP_RECENT_DOWNLOADS] =
      g_param_spec_int (
          "recent-downloads",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_init (BzEntry *self)
{
  BzEntryPrivate *priv = bz_entry_get_instance_private (self);

  priv->hold = 0;
}

void
bz_entry_hold (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));

  priv = bz_entry_get_instance_private (self);

  if (++priv->hold == 1)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HOLDING]);
}

void
bz_entry_release (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));

  priv = bz_entry_get_instance_private (self);

  if (--priv->hold == 0)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HOLDING]);
}

gboolean
bz_entry_is_holding (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);

  priv = bz_entry_get_instance_private (self);
  return priv->hold > 0;
}

gboolean
bz_entry_is_of_kinds (BzEntry *self,
                      guint    kinds)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), FALSE);

  priv = bz_entry_get_instance_private (self);
  return (priv->kinds & kinds) == kinds;
}

void
bz_entry_add_addon (BzEntry *self,
                    BzEntry *addon)
{
  BzEntryPrivate *priv     = NULL;
  guint           n_addons = 0;
  const char     *addon_id = NULL;

  g_return_if_fail (BZ_IS_ENTRY (self));
  g_return_if_fail (BZ_IS_ENTRY (addon));

  priv = bz_entry_get_instance_private (self);
  if (priv->addons != NULL)
    n_addons = g_list_model_get_n_items (G_LIST_MODEL (priv->addons));
  else
    priv->addons = g_list_store_new (BZ_TYPE_ENTRY);

  addon_id = bz_entry_get_id (addon);
  for (guint i = 0; i < n_addons; i++)
    {
      g_autoptr (BzEntry) existing = NULL;
      const char *existing_id      = NULL;

      existing    = g_list_model_get_item (G_LIST_MODEL (priv->addons), i);
      existing_id = bz_entry_get_id (existing);

      if (existing == addon ||
          g_strcmp0 (existing_id, addon_id) == 0)
        return;
    }

  g_list_store_append (priv->addons, addon);
}

const char *
bz_entry_get_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->id;
}

const char *
bz_entry_get_unique_id (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->unique_id;
}

const char *
bz_entry_get_title (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->title;
}

const char *
bz_entry_get_eol (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->eol;
}

const char *
bz_entry_get_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->description;
}

const char *
bz_entry_get_long_description (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->long_description;
}

const char *
bz_entry_get_remote_repo_name (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->remote_repo_name;
}

guint64
bz_entry_get_size (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->size;
}

GdkPaintable *
bz_entry_get_icon_paintable (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->icon_paintable;
}

GListModel *
bz_entry_get_screenshot_paintables (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->screenshot_paintables;
}

GIcon *
bz_entry_get_mini_icon (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->mini_icon;
}

GPtrArray *
bz_entry_get_search_tokens (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), NULL);

  priv = bz_entry_get_instance_private (self);
  return priv->search_tokens;
}

const char *
bz_entry_get_donation_url (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->donation_url;
}

gboolean
bz_entry_get_is_foss (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->is_floss;
}

gboolean
bz_entry_get_is_flathub (BzEntry *self)
{
  BzEntryPrivate *priv = NULL;

  g_return_val_if_fail (BZ_IS_ENTRY (self), 0);

  priv = bz_entry_get_instance_private (self);
  return priv->is_flathub;
}

gint
bz_entry_cmp_usefulness (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
  BzEntryPrivate *priv_a  = bz_entry_get_instance_private (BZ_ENTRY ((gpointer) a));
  BzEntryPrivate *priv_b  = bz_entry_get_instance_private (BZ_ENTRY ((gpointer) b));
  int             a_score = 0;
  int             b_score = 0;

  if (priv_a->is_flathub && !priv_b->is_flathub)
    return -1;
  if (!priv_a->is_flathub && priv_b->is_flathub)
    return 1;

  a_score += priv_a->title != NULL ? 5 : 0;
  a_score += priv_a->description != NULL ? 1 : 0;
  a_score += priv_a->long_description != NULL ? 5 : 0;
  a_score += priv_a->url != NULL ? 1 : 0;
  a_score += priv_a->size > 0 ? 1 : 0;
  a_score += priv_a->icon_paintable != NULL ? 15 : 0;
  a_score += priv_a->remote_repo_icon != NULL ? 1 : 0;
  a_score += priv_a->metadata_license != NULL ? 1 : 0;
  a_score += priv_a->project_license != NULL ? 1 : 0;
  a_score += priv_a->project_group != NULL ? 1 : 0;
  a_score += priv_a->developer != NULL ? 1 : 0;
  a_score += priv_a->developer_id != NULL ? 1 : 0;
  a_score += priv_a->screenshot_paintables != NULL ? 5 : 0;
  a_score += priv_a->share_urls != NULL ? 5 : 0;
  a_score += priv_a->reviews != NULL ? 1 : 0;

  b_score += priv_b->title != NULL ? 5 : 0;
  b_score += priv_b->description != NULL ? 1 : 0;
  b_score += priv_b->long_description != NULL ? 5 : 0;
  b_score += priv_b->url != NULL ? 1 : 0;
  b_score += priv_b->size > 0 ? 1 : 0;
  b_score += priv_b->icon_paintable != NULL ? 15 : 0;
  b_score += priv_b->remote_repo_icon != NULL ? 1 : 0;
  b_score += priv_b->metadata_license != NULL ? 1 : 0;
  b_score += priv_b->project_license != NULL ? 1 : 0;
  b_score += priv_b->project_group != NULL ? 1 : 0;
  b_score += priv_b->developer != NULL ? 1 : 0;
  b_score += priv_b->developer_id != NULL ? 1 : 0;
  b_score += priv_b->screenshot_paintables != NULL ? 5 : 0;
  b_score += priv_b->share_urls != NULL ? 5 : 0;
  b_score += priv_b->reviews != NULL ? 1 : 0;

  return b_score - a_score;
}

static void
query_flathub (BzEntry *self,
               int      prop)
{
  BzEntryPrivate *priv              = NULL;
  g_autoptr (QueryFlathubData) data = NULL;
  g_autoptr (DexFuture) future      = NULL;

  priv = bz_entry_get_instance_private (self);

  if (!priv->is_flathub)
    return;
  if (priv->id == NULL)
    return;

  if (priv->flathub_prop_queries == NULL)
    priv->flathub_prop_queries = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, dex_unref);
  else if (g_hash_table_contains (priv->flathub_prop_queries, GINT_TO_POINTER (prop)))
    return;

  data       = query_flathub_data_new ();
  data->self = self;
  data->prop = prop;
  data->id   = g_strdup (priv->id);

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) query_flathub_fiber,
      query_flathub_data_ref (data), query_flathub_data_unref);
  future = dex_future_then (
      future, (DexFutureCallback) query_flathub_then,
      query_flathub_data_ref (data), query_flathub_data_unref);
  g_hash_table_replace (
      priv->flathub_prop_queries,
      GINT_TO_POINTER (prop),
      g_steal_pointer (&future));
}

static DexFuture *
query_flathub_fiber (QueryFlathubData *data)
{
  int   prop                     = data->prop;
  char *id                       = data->id;
  g_autoptr (GError) local_error = NULL;
  g_autofree char *request       = NULL;
  g_autoptr (JsonNode) node      = NULL;

  switch (prop)
    {
    case PROP_VERIFIED:
      request = g_strdup_printf ("/verification/%s/status", id);
      break;
    case PROP_DOWNLOAD_STATS:
      request = g_strdup_printf ("/stats/%s?all=false&days=175", id);
      break;
    default:
      g_assert_not_reached ();
      return NULL;
    }

  node = dex_await_boxed (bz_query_flathub_v2_json (request), &local_error);
  if (node == NULL)
    {
      g_critical ("Could not retrieve property %s for %s from flathub: %s",
                  props[prop]->name, id, local_error->message);
      return dex_future_new_for_error (g_steal_pointer (&local_error));
    }

  switch (prop)
    {
    case PROP_VERIFIED:
      return dex_future_new_for_boolean (
          json_object_get_boolean_member (
              json_node_get_object (node),
              "verified"));
      break;
    case PROP_DOWNLOAD_STATS:
      {
        JsonObject *per_day          = NULL;
        g_autoptr (GListStore) store = NULL;

        per_day = json_object_get_object_member (
            json_node_get_object (node),
            "installs_per_day");
        store = g_list_store_new (BZ_TYPE_DATA_POINT);

        json_object_foreach_member (
            per_day,
            (JsonObjectForeach) download_stats_per_day_foreach,
            store);

        return dex_future_new_for_object (store);
      }
      break;
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

static DexFuture *
query_flathub_then (DexFuture        *future,
                    QueryFlathubData *data)
{
  BzEntry      *self  = data->self;
  int           prop  = data->prop;
  const GValue *value = NULL;

  value = dex_future_get_value (future, NULL);
  g_object_set_property (G_OBJECT (self), props[prop]->name, value);
  return NULL;
}

static void
download_stats_per_day_foreach (JsonObject  *object,
                                const gchar *member_name,
                                JsonNode    *member_node,
                                GListStore  *store)
{
  double independent            = 0;
  double dependent              = 0;
  g_autoptr (BzDataPoint) point = NULL;

  independent = g_list_model_get_n_items (G_LIST_MODEL (store));
  dependent   = json_node_get_int (member_node);

  point = g_object_new (
      BZ_TYPE_DATA_POINT,
      "independent", independent,
      "dependent", dependent,
      "label", member_name,
      NULL);
  g_list_store_append (store, point);
}

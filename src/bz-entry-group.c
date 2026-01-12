/* bz-entry-group.c
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

#define G_LOG_DOMAIN  "BAZAAR::ENTRY-GROUP"
#define BAZAAR_MODULE "entry-group"

#include "bz-entry-group.h"
#include "bz-async-texture.h"
#include "bz-env.h"
#include "bz-io.h"
#include "bz-util.h"

struct _BzEntryGroup
{
  GObject parent_instance;

  BzApplicationMapFactory *factory;

  GtkStringList *unique_ids;
  char          *id;
  char          *title;
  char          *developer;
  char          *description;
  GdkPaintable  *icon_paintable;
  GIcon         *mini_icon;
  gboolean       is_floss;
  char          *light_accent_color;
  char          *dark_accent_color;
  gboolean       is_flathub;
  gboolean       is_verified;
  char          *search_tokens;
  char          *remote_repos_string;
  char          *eol;
  guint64        installed_size;
  int            n_addons;
  char          *donation_url;
  GListModel    *categories;

  int max_usefulness;

  int installable;
  int updatable;
  int removable;
  int installable_available;
  int updatable_available;
  int removable_available;

  guint64 user_data_size;

  DexFuture *user_data_size_future;
  DexFuture *reap_user_data_future;

  GWeakRef ui_entry;
  GMutex   mutex;
};

G_DEFINE_FINAL_TYPE (BzEntryGroup, bz_entry_group, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_ID,
  PROP_TITLE,
  PROP_DEVELOPER,
  PROP_DESCRIPTION,
  PROP_ICON_PAINTABLE,
  PROP_MINI_ICON,
  PROP_IS_FLOSS,
  PROP_LIGHT_ACCENT_COLOR,
  PROP_DARK_ACCENT_COLOR,
  PROP_IS_FLATHUB,
  PROP_IS_VERIFIED,
  PROP_SEARCH_TOKENS,
  PROP_UI_ENTRY,
  PROP_REMOTE_REPOS_STRING,
  PROP_EOL,
  PROP_INSTALLED_SIZE,
  PROP_N_ADDONS,
  PROP_DONATION_URL,
  PROP_CATEGORIES,
  PROP_INSTALLABLE,
  PROP_UPDATABLE,
  PROP_REMOVABLE,
  PROP_INSTALLABLE_AND_AVAILABLE,
  PROP_UPDATABLE_AND_AVAILABLE,
  PROP_REMOVABLE_AND_AVAILABLE,
  PROP_USER_DATA_SIZE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
installed_changed (BzEntryGroup *self,
                   GParamSpec   *pspec,
                   BzEntry      *entry);

static void
holding_changed (BzEntryGroup *self,
                 GParamSpec   *pspec,
                 BzEntry      *entry);

static DexFuture *
dup_all_into_store_fiber (BzEntryGroup *self);

static DexFuture *
user_data_size_then (DexFuture *future,
                     GWeakRef  *wr);

static void
check_user_data_size (BzEntryGroup *self);

static void
bz_entry_group_dispose (GObject *object)
{
  BzEntryGroup *self = BZ_ENTRY_GROUP (object);

  dex_clear (&self->user_data_size_future);
  dex_clear (&self->reap_user_data_future);
  g_clear_object (&self->factory);
  g_clear_object (&self->unique_ids);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->developer, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->light_accent_color, g_free);
  g_clear_pointer (&self->dark_accent_color, g_free);
  g_clear_object (&self->icon_paintable);
  g_clear_object (&self->mini_icon);
  g_clear_pointer (&self->search_tokens, g_free);
  g_clear_pointer (&self->remote_repos_string, g_free);
  g_clear_pointer (&self->eol, g_free);
  g_clear_pointer (&self->donation_url, g_free);
  g_clear_object (&self->categories);

  g_weak_ref_clear (&self->ui_entry);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (bz_entry_group_parent_class)->dispose (object);
}

static void
bz_entry_group_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  BzEntryGroup *self = BZ_ENTRY_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_entry_group_get_model (self));
      break;
    case PROP_ID:
      g_value_set_string (value, bz_entry_group_get_id (self));
      break;
    case PROP_TITLE:
      g_value_set_string (value, bz_entry_group_get_title (self));
      break;
    case PROP_DEVELOPER:
      g_value_set_string (value, bz_entry_group_get_developer (self));
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, bz_entry_group_get_description (self));
      break;
    case PROP_ICON_PAINTABLE:
      g_value_set_object (value, bz_entry_group_get_icon_paintable (self));
      break;
    case PROP_MINI_ICON:
      g_value_set_object (value, bz_entry_group_get_mini_icon (self));
      break;
    case PROP_IS_FLOSS:
      g_value_set_boolean (value, bz_entry_group_get_is_floss (self));
      break;
    case PROP_LIGHT_ACCENT_COLOR:
      g_value_set_string (value, bz_entry_group_get_light_accent_color (self));
      break;
    case PROP_DARK_ACCENT_COLOR:
      g_value_set_string (value, bz_entry_group_get_dark_accent_color (self));
      break;
    case PROP_IS_FLATHUB:
      g_value_set_boolean (value, bz_entry_group_get_is_flathub (self));
      break;
    case PROP_IS_VERIFIED:
      g_value_set_boolean (value, bz_entry_group_get_is_verified (self));
      break;
    case PROP_SEARCH_TOKENS:
      g_value_set_boxed (value, bz_entry_group_get_search_tokens (self));
      break;
    case PROP_EOL:
      g_value_set_string (value, bz_entry_group_get_eol (self));
      break;
    case PROP_INSTALLED_SIZE:
      g_value_set_uint64 (value, bz_entry_group_get_installed_size (self));
      break;
    case PROP_N_ADDONS:
      g_value_set_int (value, bz_entry_group_get_n_addons (self));
      break;
    case PROP_DONATION_URL:
      g_value_set_string (value, bz_entry_group_get_donation_url (self));
      break;
    case PROP_CATEGORIES:
      g_value_set_object (value, bz_entry_group_get_categories (self));
      break;
    case PROP_UI_ENTRY:
      g_value_take_object (value, bz_entry_group_dup_ui_entry (self));
      break;
    case PROP_REMOTE_REPOS_STRING:
      g_value_set_string (value, self->remote_repos_string);
      break;
    case PROP_INSTALLABLE:
      g_value_set_int (value, bz_entry_group_get_installable (self));
      break;
    case PROP_UPDATABLE:
      g_value_set_int (value, bz_entry_group_get_updatable (self));
      break;
    case PROP_REMOVABLE:
      g_value_set_int (value, bz_entry_group_get_removable (self));
      break;
    case PROP_INSTALLABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_installable_and_available (self));
      break;
    case PROP_UPDATABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_updatable_and_available (self));
      break;
    case PROP_REMOVABLE_AND_AVAILABLE:
      g_value_set_int (value, bz_entry_group_get_removable_and_available (self));
      break;
    case PROP_USER_DATA_SIZE:
      g_value_set_uint64 (value, bz_entry_group_get_user_data_size (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_group_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  // BzEntryGroup *self = BZ_ENTRY_GROUP (object);

  switch (prop_id)
    {
    case PROP_MODEL:
    case PROP_ID:
    case PROP_TITLE:
    case PROP_DEVELOPER:
    case PROP_DESCRIPTION:
    case PROP_ICON_PAINTABLE:
    case PROP_MINI_ICON:
    case PROP_IS_FLOSS:
    case PROP_LIGHT_ACCENT_COLOR:
    case PROP_DARK_ACCENT_COLOR:
    case PROP_IS_FLATHUB:
    case PROP_IS_VERIFIED:
    case PROP_SEARCH_TOKENS:
    case PROP_EOL:
    case PROP_UI_ENTRY:
    case PROP_REMOTE_REPOS_STRING:
    case PROP_INSTALLABLE:
    case PROP_UPDATABLE:
    case PROP_REMOVABLE:
    case PROP_INSTALLABLE_AND_AVAILABLE:
    case PROP_UPDATABLE_AND_AVAILABLE:
    case PROP_REMOVABLE_AND_AVAILABLE:
    case PROP_USER_DATA_SIZE:

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_entry_group_class_init (BzEntryGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_entry_group_set_property;
  object_class->get_property = bz_entry_group_get_property;
  object_class->dispose      = bz_entry_group_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE);

  props[PROP_ID] =
      g_param_spec_string (
          "id",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_TITLE] =
      g_param_spec_string (
          "title",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_DEVELOPER] =
      g_param_spec_string (
          "developer",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_DESCRIPTION] =
      g_param_spec_string (
          "description",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_ICON_PAINTABLE] =
      g_param_spec_object (
          "icon-paintable",
          NULL, NULL,
          GDK_TYPE_PAINTABLE,
          G_PARAM_READABLE);

  props[PROP_MINI_ICON] =
      g_param_spec_object (
          "mini-icon",
          NULL, NULL,
          G_TYPE_ICON,
          G_PARAM_READABLE);

  props[PROP_IS_FLOSS] =
      g_param_spec_boolean (
          "is-floss",
          NULL, NULL, FALSE,
          G_PARAM_READABLE);

  props[PROP_LIGHT_ACCENT_COLOR] =
      g_param_spec_string (
          "light-accent-color",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_DARK_ACCENT_COLOR] =
      g_param_spec_string (
          "dark-accent-color",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_IS_FLATHUB] =
      g_param_spec_boolean (
          "is-flathub",
          NULL, NULL, FALSE,
          G_PARAM_READABLE);

  props[PROP_IS_VERIFIED] =
      g_param_spec_boolean (
          "is-verified",
          NULL, NULL, FALSE,
          G_PARAM_READABLE);

  props[PROP_SEARCH_TOKENS] =
      g_param_spec_string (
          "search-tokens",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_EOL] =
      g_param_spec_string (
          "eol",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_INSTALLED_SIZE] =
      g_param_spec_uint64 (
          "installed-size",
          NULL, NULL,
          0, G_MAXUINT64, 0,
          G_PARAM_READABLE);

  props[PROP_N_ADDONS] =
      g_param_spec_int (
          "n-addons",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_DONATION_URL] =
      g_param_spec_string (
          "donation-url",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_CATEGORIES] =
      g_param_spec_object (
          "categories",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE);

  props[PROP_UI_ENTRY] =
      g_param_spec_object (
          "ui-entry",
          NULL, NULL,
          BZ_TYPE_RESULT,
          G_PARAM_READABLE);

  props[PROP_REMOTE_REPOS_STRING] =
      g_param_spec_string (
          "remote-repos-string",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_INSTALLABLE] =
      g_param_spec_int (
          "installable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_UPDATABLE] =
      g_param_spec_int (
          "updatable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_REMOVABLE] =
      g_param_spec_int (
          "removable",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_INSTALLABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "installable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_UPDATABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "updatable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_REMOVABLE_AND_AVAILABLE] =
      g_param_spec_int (
          "removable-and-available",
          NULL, NULL,
          0, G_MAXINT, 0,
          G_PARAM_READABLE);

  props[PROP_USER_DATA_SIZE] =
      g_param_spec_uint64 (
          "user-data-size",
          NULL, NULL,
          0, G_MAXUINT64, 0,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_entry_group_init (BzEntryGroup *self)
{
  self->unique_ids     = gtk_string_list_new (NULL);
  self->max_usefulness = -1;
  g_weak_ref_init (&self->ui_entry, NULL);
  g_mutex_init (&self->mutex);
}

BzEntryGroup *
bz_entry_group_new (BzApplicationMapFactory *factory)
{
  BzEntryGroup *group = NULL;

  g_return_val_if_fail (BZ_IS_APPLICATION_MAP_FACTORY (factory), NULL);

  group          = g_object_new (BZ_TYPE_ENTRY_GROUP, NULL);
  group->factory = g_object_ref (factory);

  return group;
}

GMutexLocker *
bz_entry_group_lock (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return g_mutex_locker_new (&self->mutex);
}

GListModel *
bz_entry_group_get_model (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return G_LIST_MODEL (self->unique_ids);
}

const char *
bz_entry_group_get_id (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->id;
}

const char *
bz_entry_group_get_title (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->title;
}

const char *
bz_entry_group_get_developer (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->developer;
}

const char *
bz_entry_group_get_description (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->description;
}

GdkPaintable *
bz_entry_group_get_icon_paintable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->icon_paintable;
}

GIcon *
bz_entry_group_get_mini_icon (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->mini_icon;
}

gboolean
bz_entry_group_get_is_floss (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  return self->is_floss;
}

const char *
bz_entry_group_get_light_accent_color (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->light_accent_color;
}

const char *
bz_entry_group_get_dark_accent_color (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->dark_accent_color;
}

gboolean
bz_entry_group_get_is_flathub (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  return self->is_flathub;
}

gboolean
bz_entry_group_get_is_verified (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), FALSE);
  return self->is_verified;
}

const char *
bz_entry_group_get_search_tokens (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->search_tokens;
}

const char *
bz_entry_group_get_eol (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->eol;
}

guint64
bz_entry_group_get_installed_size (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->installed_size;
}

int
bz_entry_group_get_n_addons (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->n_addons;
}

const char *
bz_entry_group_get_donation_url (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->donation_url;
}

GListModel *
bz_entry_group_get_categories (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);
  return self->categories;
}

guint64
bz_entry_group_get_user_data_size (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  check_user_data_size (self);
  return self->user_data_size;
}

BzResult *
bz_entry_group_dup_ui_entry (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->unique_ids)) > 0)
    {
      g_autoptr (BzResult) result = NULL;

      result = g_weak_ref_get (&self->ui_entry);
      if (result == NULL)
        {
          g_autoptr (GtkStringObject) id = NULL;

          id     = g_list_model_get_item (G_LIST_MODEL (self->unique_ids), 0);
          result = bz_application_map_factory_convert_one (self->factory, g_steal_pointer (&id));
          if (result == NULL)
            return NULL;

          g_weak_ref_set (&self->ui_entry, result);
        }
      return g_steal_pointer (&result);
    }
  else
    return NULL;
}

char *
bz_entry_group_dup_ui_entry_id (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->unique_ids)) > 0)
    {
      g_autoptr (GtkStringObject) id = NULL;

      id = g_list_model_get_item (G_LIST_MODEL (self->unique_ids), 0);
      return g_strdup (gtk_string_object_get_string (id));
    }
  else
    return NULL;
}

int
bz_entry_group_get_installable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->installable;
}

int
bz_entry_group_get_updatable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->updatable;
}

int
bz_entry_group_get_removable (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->removable;
}

int
bz_entry_group_get_installable_and_available (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->installable_available;
}

int
bz_entry_group_get_updatable_and_available (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->updatable_available;
}

int
bz_entry_group_get_removable_and_available (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), 0);
  return self->removable_available;
}

void
bz_entry_group_add (BzEntryGroup *self,
                    BzEntry      *entry,
                    BzEntry      *runtime)
{
  g_autoptr (GMutexLocker) locker  = NULL;
  const char   *unique_id          = NULL;
  gint          usefulness         = 0;
  const char   *eol                = NULL;
  const char   *title              = NULL;
  const char   *developer          = NULL;
  const char   *description        = NULL;
  GdkPaintable *icon_paintable     = NULL;
  GIcon        *mini_icon          = NULL;
  const char   *search_tokens      = NULL;
  gboolean      is_floss           = FALSE;
  const char   *light_accent_color = NULL;
  const char   *dark_accent_color  = NULL;
  gboolean      is_flathub         = FALSE;
  gboolean      is_verified        = FALSE;
  guint64       installed_size     = 0;
  GListModel   *addons             = NULL;
  int           n_addons           = 0;
  const char   *donation_url       = NULL;
  GListModel   *entry_categories   = NULL;
  guint         existing           = 0;

  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (BZ_IS_ENTRY (entry));
  g_return_if_fail (runtime == NULL || BZ_IS_ENTRY (runtime));

  locker = g_mutex_locker_new (&self->mutex);

  if (self->id == NULL)
    {
      self->id = g_strdup (bz_entry_get_id (entry));
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ID]);
    }
  unique_id = bz_entry_get_unique_id (entry);

  eol = bz_entry_get_eol (entry);
  if (eol == NULL && runtime != NULL)
    eol = bz_entry_get_eol (runtime);
  if (eol != NULL)
    {
      g_clear_pointer (&self->eol, g_free);
      self->eol = g_strdup (eol);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EOL]);
    }

  title              = bz_entry_get_title (entry);
  developer          = bz_entry_get_developer (entry);
  description        = bz_entry_get_description (entry);
  icon_paintable     = bz_entry_get_icon_paintable (entry);
  mini_icon          = bz_entry_get_mini_icon (entry);
  search_tokens      = bz_entry_get_search_tokens (entry);
  is_floss           = bz_entry_get_is_foss (entry);
  light_accent_color = bz_entry_get_light_accent_color (entry);
  dark_accent_color  = bz_entry_get_dark_accent_color (entry);
  is_flathub         = bz_entry_get_is_flathub (entry);
  is_verified        = bz_entry_is_verified (entry);
  installed_size     = bz_entry_get_installed_size (entry);
  donation_url       = bz_entry_get_donation_url (entry);
  entry_categories   = bz_entry_get_categories (entry);

  addons = bz_entry_get_addons (entry);
  if (addons != NULL)
    n_addons = g_list_model_get_n_items (addons);

  usefulness = bz_entry_calc_usefulness (entry);
  existing   = gtk_string_list_find (self->unique_ids, unique_id);

  if (usefulness >= self->max_usefulness)
    {
      if (existing != G_MAXUINT)
        gtk_string_list_remove (self->unique_ids, existing);
      gtk_string_list_splice (self->unique_ids, 0, 0, (const char *const[]) { unique_id, NULL });

      if (title != NULL)
        {
          g_clear_pointer (&self->title, g_free);
          self->title = g_strdup (title);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE]);
        }
      if (developer != NULL)
        {
          g_clear_pointer (&self->developer, g_free);
          self->developer = g_strdup (developer);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEVELOPER]);
        }
      if (description != NULL)
        {
          g_clear_pointer (&self->description, g_free);
          self->description = g_strdup (description);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DESCRIPTION]);
        }
      /* only grab icon paintable if we don't have it already to reduce
         flickering in UI */
      if (icon_paintable != NULL &&
          (self->icon_paintable == NULL ||
           (BZ_IS_ASYNC_TEXTURE (self->icon_paintable) &&
            !bz_async_texture_get_loaded (BZ_ASYNC_TEXTURE (self->icon_paintable)) &&
            !bz_async_texture_is_loading (BZ_ASYNC_TEXTURE (self->icon_paintable)))))
        {
          g_clear_object (&self->icon_paintable);
          self->icon_paintable = g_object_ref (icon_paintable);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ICON_PAINTABLE]);
        }
      if (mini_icon != NULL)
        {
          g_clear_object (&self->mini_icon);
          self->mini_icon = g_object_ref (mini_icon);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MINI_ICON]);
        }
      if (search_tokens != NULL)
        {
          g_clear_pointer (&self->search_tokens, g_free);
          self->search_tokens = g_strdup (search_tokens);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SEARCH_TOKENS]);
        }
      if (!!is_floss != !!self->is_floss)
        {
          self->is_floss = is_floss;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_FLOSS]);
        }
      if (light_accent_color != NULL)
        {
          g_clear_pointer (&self->light_accent_color, g_free);
          self->light_accent_color = g_strdup (light_accent_color);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LIGHT_ACCENT_COLOR]);
        }
      if (dark_accent_color != NULL)
        {
          g_clear_pointer (&self->dark_accent_color, g_free);
          self->dark_accent_color = g_strdup (dark_accent_color);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DARK_ACCENT_COLOR]);
        }
      if (!!is_flathub != !!self->is_flathub)
        {
          self->is_flathub = is_flathub;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_FLATHUB]);
        }
      if (!!is_verified != !!self->is_verified)
        {
          self->is_verified = is_verified;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_VERIFIED]);
        }
      if (installed_size != self->installed_size)
        {
          self->installed_size = installed_size;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLED_SIZE]);
        }
      if (n_addons != self->n_addons)
        {
          self->n_addons = n_addons;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_N_ADDONS]);
        }
      if (donation_url != NULL)
        {
          g_clear_pointer (&self->donation_url, g_free);
          self->donation_url = g_strdup (donation_url);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DONATION_URL]);
        }

      if (entry_categories != NULL && g_list_model_get_n_items (entry_categories) > 0)
        {
          g_clear_object (&self->categories);
          self->categories = g_object_ref (entry_categories);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CATEGORIES]);
        }

      self->max_usefulness = usefulness;
    }
  else
    {
      if (existing == G_MAXUINT)
        gtk_string_list_append (self->unique_ids, unique_id);

      if (title != NULL && self->title == NULL)
        {
          self->title = g_strdup (title);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE]);
        }
      if (developer != NULL && self->developer == NULL)
        {
          self->developer = g_strdup (developer);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEVELOPER]);
        }
      if (description != NULL && self->description == NULL)
        {
          self->description = g_strdup (description);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DESCRIPTION]);
        }
      if (icon_paintable != NULL && self->icon_paintable == NULL)
        {
          self->icon_paintable = g_object_ref (icon_paintable);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ICON_PAINTABLE]);
        }
      if (mini_icon != NULL && self->mini_icon == NULL)
        {
          self->mini_icon = g_object_ref (mini_icon);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MINI_ICON]);
        }
      if (search_tokens != NULL && self->search_tokens == NULL)
        {
          self->search_tokens = g_strdup (search_tokens);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SEARCH_TOKENS]);
        }
      if (light_accent_color != NULL && self->light_accent_color == NULL)
        {
          self->light_accent_color = g_strdup (light_accent_color);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LIGHT_ACCENT_COLOR]);
        }
      if (dark_accent_color != NULL && self->dark_accent_color == NULL)
        {
          self->dark_accent_color = g_strdup (dark_accent_color);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DARK_ACCENT_COLOR]);
        }
      if (installed_size > 0 && self->installed_size == 0)
        {
          self->installed_size = installed_size;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLED_SIZE]);
        }
      if (donation_url != NULL && self->donation_url == NULL)
        {
          self->donation_url = g_strdup (donation_url);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DONATION_URL]);
        }
    }

  if (existing == G_MAXUINT)
    {
      const char *remote_repo = NULL;

      remote_repo = bz_entry_get_remote_repo_name (entry);
      if (remote_repo != NULL)
        {
          g_autofree char *capitalized_repo = NULL;

          if (remote_repo[0] != '\0')
            {
              capitalized_repo    = g_strdup (remote_repo);
              capitalized_repo[0] = g_ascii_toupper (capitalized_repo[0]);
            }
          else
            {
              capitalized_repo = g_strdup (remote_repo);
            }

          if (self->remote_repos_string != NULL)
            {
              g_autofree char *old_string = NULL;
              if (strstr (self->remote_repos_string, capitalized_repo) == NULL)
                {
                  old_string                = g_steal_pointer (&self->remote_repos_string);
                  self->remote_repos_string = g_strdup_printf ("%s • %s", old_string, capitalized_repo);
                }
            }
          else
            {
              self->remote_repos_string = g_strdup (capitalized_repo);
            }
        }

      if (bz_entry_is_installed (entry))
        {
          self->removable++;
          if (!bz_entry_is_holding (entry))
            {
              self->removable_available++;
              g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
            }
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
        }
      else
        {
          self->installable++;
          if (!bz_entry_is_holding (entry))
            {
              self->installable_available++;
              g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
            }
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
        }
    }
}

void
bz_entry_group_connect_living (BzEntryGroup *self,
                               BzEntry      *entry)
{
  g_autoptr (GMutexLocker) locker = NULL;

  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (BZ_IS_ENTRY (entry));

  locker = g_mutex_locker_new (&self->mutex);

  g_signal_handlers_disconnect_by_func (entry, installed_changed, self);
  g_signal_handlers_disconnect_by_func (entry, holding_changed, self);
  g_signal_connect_object (entry, "notify::installed", G_CALLBACK (installed_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (entry, "notify::holding", G_CALLBACK (holding_changed), self, G_CONNECT_SWAPPED);
}

DexFuture *
bz_entry_group_dup_all_into_store (BzEntryGroup *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY_GROUP (self), NULL);

  /* _must_ be the main scheduler since invokations
   * of BzApplicationMapFactory functions expect this
   */
  return dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) dup_all_into_store_fiber,
      g_object_ref (self),
      g_object_unref);
}

static void
installed_changed (BzEntryGroup *self,
                   GParamSpec   *pspec,
                   BzEntry      *entry)
{
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->mutex);

  if (bz_entry_is_installed (entry))
    {
      self->installable--;
      self->removable++;
      if (!bz_entry_is_holding (entry))
        {
          self->installable_available--;
          self->removable_available++;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
        }
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
    }
  else
    {
      self->removable--;
      self->installable++;
      if (!bz_entry_is_holding (entry))
        {
          self->removable_available--;
          self->installable_available++;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
        }
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE]);
    }

  dex_clear (&self->user_data_size_future);
  self->user_data_size = 0;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USER_DATA_SIZE]);
}

static void
holding_changed (BzEntryGroup *self,
                 GParamSpec   *pspec,
                 BzEntry      *entry)
{
  g_autoptr (GMutexLocker) locker = NULL;

  locker = g_mutex_locker_new (&self->mutex);

  if (bz_entry_is_holding (entry))
    {
      if (bz_entry_is_installed (entry))
        self->removable_available--;
      else
        self->installable_available--;
    }
  else
    {
      if (bz_entry_is_installed (entry))
        self->removable_available++;
      else
        self->installable_available++;
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REMOVABLE_AND_AVAILABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_INSTALLABLE_AND_AVAILABLE]);
}

static DexFuture *
dup_all_into_store_fiber (BzEntryGroup *self)
{
  g_autoptr (GPtrArray) futures = NULL;
  guint n_items                 = 0;
  g_autoptr (GListStore) store  = NULL;
  guint n_resolved              = 0;

  futures = g_ptr_array_new_with_free_func (dex_unref);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->unique_ids));
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (GtkStringObject) string = NULL;
      g_autoptr (BzResult) result        = NULL;

      string = g_list_model_get_item (G_LIST_MODEL (self->unique_ids), i);
      result = bz_application_map_factory_convert_one (self->factory, g_steal_pointer (&string));

      g_ptr_array_add (futures, bz_result_dup_future (result));
    }

  dex_await (dex_future_allv (
                 (DexFuture *const *) futures->pdata, futures->len),
             NULL);

  store = g_list_store_new (BZ_TYPE_ENTRY);
  for (guint i = 0; i < futures->len; i++)
    {
      DexFuture *future = NULL;

      future = g_ptr_array_index (futures, i);
      if (dex_future_is_resolved (future))
        {
          BzEntry *entry = NULL;

          entry = g_value_get_object (dex_future_get_value (future, NULL));
          bz_entry_group_connect_living (self, entry);
          g_list_store_append (store, entry);
        }
    }

  n_resolved = g_list_model_get_n_items (G_LIST_MODEL (store));
  if (n_resolved == 0)
    {
      g_warning ("No entries for %s were able to be resolved", self->id);
      return dex_future_new_reject (
          G_IO_ERROR,
          G_IO_ERROR_UNKNOWN,
          "No entries for %s were able to be resolved",
          self->id);
    }
  if (n_resolved != n_items)
    g_warning ("Some entries for %s failed to resolve", self->id);

  return dex_future_new_for_object (store);
}

static DexFuture *
reap_user_data_then (DexFuture *future,
                     GWeakRef  *wr)
{
  g_autoptr (BzEntryGroup) self = NULL;
  guint64 old_size              = 0;

  bz_weak_get_or_return_reject (self, wr);
  dex_clear (&self->reap_user_data_future);

  old_size             = self->user_data_size;
  self->user_data_size = 0;

  if (old_size != 0)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USER_DATA_SIZE]);

  return dex_future_new_true ();
}

void
bz_entry_group_reap_user_data (BzEntryGroup *self)
{
  g_return_if_fail (BZ_IS_ENTRY_GROUP (self));
  g_return_if_fail (self->id != NULL);

  if (self->reap_user_data_future != NULL)
    return;

  self->reap_user_data_future = dex_future_then (
      bz_reap_user_data_dex (self->id),
      (DexFutureCallback) reap_user_data_then,
      bz_track_weak (self),
      bz_weak_release);
}

static DexFuture *
user_data_size_then (DexFuture *future,
                     GWeakRef  *wr)
{
  g_autoptr (BzEntryGroup) self = NULL;
  g_autoptr (GError) error      = NULL;
  guint64 size                  = 0;
  guint64 old_size              = 0;

  bz_weak_get_or_return_reject (self, wr);
  dex_clear (&self->user_data_size_future);

  size = dex_await_uint64 (dex_ref (future), &error);
  if (error != NULL)
    size = 0;

  old_size             = self->user_data_size;
  self->user_data_size = size;

  if (old_size != size)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USER_DATA_SIZE]);

  return dex_future_new_true ();
}

static void
check_user_data_size (BzEntryGroup *self)
{
  g_autoptr (DexFuture) future = NULL;

  if (self->user_data_size_future != NULL ||
      self->id == NULL)
    return;

  if (self->reap_user_data_future != NULL)
    return;

  future = bz_get_user_data_size_dex (self->id);
  future = dex_future_then (
      future,
      (DexFutureCallback) user_data_size_then,
      bz_track_weak (self), bz_weak_release);
  self->user_data_size_future = g_steal_pointer (&future);
}

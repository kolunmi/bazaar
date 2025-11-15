/* bz-flatpak-entry.c
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

#define G_LOG_DOMAIN  "BAZAAR::FLATPAK-ENTRY"
#define BAZAAR_MODULE "entry"

#include "config.h"

#include <glib/gi18n.h>
#include <xmlb.h>

#include "bz-async-texture.h"
#include "bz-flatpak-private.h"
#include "bz-io.h"
#include "bz-issue.h"
#include "bz-release.h"
#include "bz-serializable.h"
#include "bz-url.h"

enum
{
  NO_ELEMENT,
  PARAGRAPH,
  ORDERED_LIST,
  UNORDERED_LIST,
  LIST_ITEM,
  CODE,
  EMPHASIS,
};

struct _BzFlatpakEntry
{
  BzEntry parent_instance;

  gboolean user;
  char    *flatpak_name;
  char    *flatpak_id;
  char    *flatpak_version;
  char    *application_name;
  char    *application_runtime;
  char    *application_command;
  char    *runtime_name;
  char    *addon_extension_of_ref;

  FlatpakRef *ref;
};

static void
serializable_iface_init (BzSerializableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzFlatpakEntry,
    bz_flatpak_entry,
    BZ_TYPE_ENTRY,
    G_IMPLEMENT_INTERFACE (BZ_TYPE_SERIALIZABLE, serializable_iface_init))

enum
{
  PROP_0,

  PROP_USER,
  PROP_FLATPAK_NAME,
  PROP_FLATPAK_ID,
  PROP_FLATPAK_VERSION,
  PROP_APPLICATION_NAME,
  PROP_APPLICATION_RUNTIME,
  PROP_APPLICATION_COMMAND,
  PROP_RUNTIME_NAME,
  PROP_ADDON_OF_REF,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
clear_entry (BzFlatpakEntry *self);

static void
bz_flatpak_entry_dispose (GObject *object)
{
  BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  clear_entry (self);
  g_clear_object (&self->ref);

  G_OBJECT_CLASS (bz_flatpak_entry_parent_class)->dispose (object);
}

static void
bz_flatpak_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_USER:
      g_value_set_boolean (value, self->user);
      break;
    case PROP_FLATPAK_ID:
      g_value_set_string (value, self->flatpak_id);
      break;
    case PROP_FLATPAK_VERSION:
      g_value_set_string (value, self->flatpak_version);
      break;
    case PROP_APPLICATION_NAME:
      g_value_set_string (value, self->application_name);
      break;
    case PROP_APPLICATION_RUNTIME:
      g_value_set_string (value, self->application_runtime);
      break;
    case PROP_APPLICATION_COMMAND:
      g_value_set_string (value, self->application_command);
      break;
    case PROP_RUNTIME_NAME:
      g_value_set_string (value, self->runtime_name);
      break;
    case PROP_ADDON_OF_REF:
      g_value_set_string (value, self->addon_extension_of_ref);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flatpak_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  // BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_USER:
    case PROP_FLATPAK_NAME:
    case PROP_FLATPAK_ID:
    case PROP_FLATPAK_VERSION:
    case PROP_APPLICATION_NAME:
    case PROP_APPLICATION_RUNTIME:
    case PROP_APPLICATION_COMMAND:
    case PROP_RUNTIME_NAME:
    case PROP_ADDON_OF_REF:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flatpak_entry_class_init (BzFlatpakEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_flatpak_entry_set_property;
  object_class->get_property = bz_flatpak_entry_get_property;
  object_class->dispose      = bz_flatpak_entry_dispose;

  props[PROP_USER] =
      g_param_spec_boolean (
          "user",
          NULL, NULL,
          FALSE,
          G_PARAM_READABLE);

  props[PROP_FLATPAK_NAME] =
      g_param_spec_string (
          "flatpak-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_FLATPAK_ID] =
      g_param_spec_string (
          "flatpak-id",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_FLATPAK_VERSION] =
      g_param_spec_string (
          "flatpak-version",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_NAME] =
      g_param_spec_string (
          "application-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_RUNTIME] =
      g_param_spec_string (
          "application-runtime",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_APPLICATION_COMMAND] =
      g_param_spec_string (
          "application-command",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_RUNTIME_NAME] =
      g_param_spec_string (
          "runtime-name",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  props[PROP_ADDON_OF_REF] =
      g_param_spec_string (
          "addon-extension-of-ref",
          NULL, NULL, NULL,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_flatpak_entry_init (BzFlatpakEntry *self)
{
}

static void
bz_flatpak_entry_real_serialize (BzSerializable  *serializable,
                                 GVariantBuilder *builder)
{
  BzFlatpakEntry *self = BZ_FLATPAK_ENTRY (serializable);

  g_variant_builder_add (builder, "{sv}", "user", g_variant_new_boolean (self->user));
  if (self->flatpak_name != NULL)
    g_variant_builder_add (builder, "{sv}", "flatpak-name", g_variant_new_string (self->flatpak_name));
  if (self->flatpak_id != NULL)
    g_variant_builder_add (builder, "{sv}", "flatpak-id", g_variant_new_string (self->flatpak_id));
  if (self->flatpak_version != NULL)
    g_variant_builder_add (builder, "{sv}", "flatpak-version", g_variant_new_string (self->flatpak_version));
  if (self->application_name != NULL)
    g_variant_builder_add (builder, "{sv}", "application-name", g_variant_new_string (self->application_name));
  if (self->application_runtime != NULL)
    g_variant_builder_add (builder, "{sv}", "application-runtime", g_variant_new_string (self->application_runtime));
  if (self->application_command != NULL)
    g_variant_builder_add (builder, "{sv}", "application-command", g_variant_new_string (self->application_command));
  if (self->runtime_name != NULL)
    g_variant_builder_add (builder, "{sv}", "runtime-name", g_variant_new_string (self->runtime_name));
  if (self->addon_extension_of_ref != NULL)
    g_variant_builder_add (builder, "{sv}", "addon-extension-of-ref", g_variant_new_string (self->addon_extension_of_ref));

  bz_entry_serialize (BZ_ENTRY (self), builder);
}

static gboolean
bz_flatpak_entry_real_deserialize (BzSerializable *serializable,
                                   GVariant       *import,
                                   GError        **error)
{
  BzFlatpakEntry *self          = BZ_FLATPAK_ENTRY (serializable);
  g_autoptr (GVariantIter) iter = NULL;

  clear_entry (self);

  iter = g_variant_iter_new (import);
  for (;;)
    {
      g_autofree char *key       = NULL;
      g_autoptr (GVariant) value = NULL;

      if (!g_variant_iter_next (iter, "{sv}", &key, &value))
        break;

      if (g_strcmp0 (key, "user") == 0)
        self->user = g_variant_get_boolean (value);
      else if (g_strcmp0 (key, "flatpak-name") == 0)
        self->flatpak_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "flatpak-id") == 0)
        self->flatpak_id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "flatpak-version") == 0)
        self->flatpak_version = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "application-name") == 0)
        self->application_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "application-runtime") == 0)
        self->application_runtime = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "application-command") == 0)
        self->application_command = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "runtime-name") == 0)
        self->runtime_name = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "addon-extension-of-ref") == 0)
        self->addon_extension_of_ref = g_variant_dup_string (value, NULL);
    }

  return bz_entry_deserialize (BZ_ENTRY (self), import, error);
}

static void
serializable_iface_init (BzSerializableInterface *iface)
{
  iface->serialize   = bz_flatpak_entry_real_serialize;
  iface->deserialize = bz_flatpak_entry_real_deserialize;
}

static guint
parse_control_value (const char *value)
{
  if (g_strcmp0 (value, "pointing") == 0)
    return BZ_CONTROL_POINTING;
  else if (g_strcmp0 (value, "keyboard") == 0)
    return BZ_CONTROL_KEYBOARD;
  else if (g_strcmp0 (value, "console") == 0)
    return BZ_CONTROL_CONSOLE;
  else if (g_strcmp0 (value, "tablet") == 0)
    return BZ_CONTROL_TABLET;
  else if (g_strcmp0 (value, "touch") == 0)
    return BZ_CONTROL_TOUCH;
  else if (g_strcmp0 (value, "gamepad") == 0)
    return BZ_CONTROL_GAMEPAD;
  else if (g_strcmp0 (value, "tv-remote") == 0)
    return BZ_CONTROL_TV_REMOTE;
  else if (g_strcmp0 (value, "voice") == 0)
    return BZ_CONTROL_VOICE;
  else if (g_strcmp0 (value, "vision") == 0)
    return BZ_CONTROL_VISION;
  else
    return 0;
}

static gboolean
calculate_is_mobile_friendly (guint required_controls,
                              guint supported_controls,
                              gint  min_display_length,
                              gint  max_display_length)
{
  return (supported_controls & BZ_CONTROL_TOUCH) != 0;
}

BzFlatpakEntry *
bz_flatpak_entry_new_for_ref (FlatpakRef    *ref,
                              FlatpakRemote *remote,
                              gboolean       user,
                              AsComponent   *component,
                              const char    *appstream_dir,
                              GError       **error)
{
  g_autoptr (BzFlatpakEntry) self              = NULL;
  GBytes *bytes                                = NULL;
  g_autoptr (GKeyFile) key_file                = NULL;
  gboolean         result                      = FALSE;
  guint            kinds                       = 0;
  g_autofree char *module_dir                  = NULL;
  const char      *id                          = NULL;
  g_autofree char *unique_id                   = NULL;
  g_autofree char *unique_id_checksum          = NULL;
  guint64          download_size               = 0;
  const char      *title                       = NULL;
  const char      *eol                         = NULL;
  const char      *description                 = NULL;
  const char      *metadata_license            = NULL;
  const char      *project_license             = NULL;
  gboolean         is_floss                    = FALSE;
  const char      *project_group               = NULL;
  const char      *developer                   = NULL;
  const char      *developer_id                = NULL;
  const char      *long_description            = NULL;
  const char      *remote_name                 = NULL;
  const char      *project_url                 = NULL;
  g_autoptr (GPtrArray) as_search_tokens       = NULL;
  g_autofree char *search_tokens               = NULL;
  g_autoptr (GdkPaintable) icon_paintable      = NULL;
  g_autoptr (GIcon) mini_icon                  = NULL;
  g_autoptr (GListStore) screenshot_paintables = NULL;
  g_autoptr (GListStore) share_urls            = NULL;
  g_autofree char *donation_url                = NULL;
  g_autofree char *forge_url                   = NULL;
  g_autoptr (GListStore) native_reviews        = NULL;
  double           average_rating              = 0.0;
  g_autofree char *ratings_summary             = NULL;
  g_autoptr (GListStore) version_history       = NULL;
  const char *accent_color_light               = NULL;
  const char *accent_color_dark                = NULL;
  guint       required_controls                = 0;
  guint       recommended_controls             = 0;
  guint       supported_controls               = 0;
  gint        min_display_length               = 0;
  gint        max_display_length               = 0;
  gboolean    is_mobile_friendly               = FALSE;
  g_autoptr (AsContentRating) content_rating   = NULL;

  g_return_val_if_fail (FLATPAK_IS_REF (ref), NULL);
  g_return_val_if_fail (FLATPAK_IS_REMOTE_REF (ref) || FLATPAK_IS_BUNDLE_REF (ref), NULL);
  g_return_val_if_fail (component == NULL || appstream_dir != NULL, NULL);

  self       = g_object_new (BZ_TYPE_FLATPAK_ENTRY, NULL);
  self->user = user;
  self->ref  = g_object_ref (ref);

  key_file = g_key_file_new ();
  if (FLATPAK_IS_REMOTE_REF (ref))
    bytes = flatpak_remote_ref_get_metadata (FLATPAK_REMOTE_REF (ref));
  else if (FLATPAK_IS_BUNDLE_REF (ref))
    bytes = flatpak_bundle_ref_get_metadata (FLATPAK_BUNDLE_REF (ref));

  result = g_key_file_load_from_bytes (
      key_file, bytes, G_KEY_FILE_NONE, error);
  if (!result)
    return NULL;

#define GET_STRING(member, group_name, key) \
  G_STMT_START                              \
  {                                         \
    self->member = g_key_file_get_string (  \
        key_file, group_name, key, error);  \
    if (self->member == NULL)               \
      return NULL;                          \
  }                                         \
  G_STMT_END

  if (g_key_file_has_group (key_file, "Application"))
    {
      kinds |= BZ_ENTRY_KIND_APPLICATION;

      GET_STRING (application_name, "Application", "name");
      GET_STRING (application_runtime, "Application", "runtime");
      if (g_key_file_has_key (key_file, "Application", "command", NULL))
        GET_STRING (application_command, "Application", "command");
    }

  if (g_key_file_has_group (key_file, "Runtime"))
    {
      if (!g_key_file_has_group (key_file, "Build"))
        kinds |= BZ_ENTRY_KIND_RUNTIME;

      GET_STRING (runtime_name, "Runtime", "name");
    }

  if (g_key_file_has_group (key_file, "ExtensionOf"))
    {
      if (!(kinds & BZ_ENTRY_KIND_RUNTIME))
        kinds |= BZ_ENTRY_KIND_ADDON;

      GET_STRING (addon_extension_of_ref, "ExtensionOf", "ref");
    }

#undef GET_STRING

  // if (kinds == 0)
  //   {
  //     g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
  //                  "Key file presented no useful information");
  //     return NULL;
  //   }
  module_dir = bz_dup_module_dir ();

  self->flatpak_name    = g_strdup (flatpak_ref_get_name (ref));
  self->flatpak_id      = flatpak_ref_format_ref (ref);
  self->flatpak_version = g_strdup (flatpak_ref_get_branch (ref));

  id                 = flatpak_ref_get_name (ref);
  unique_id          = bz_flatpak_ref_format_unique (ref, user);
  unique_id_checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, unique_id, -1);

  if (remote != NULL)
    remote_name = flatpak_remote_get_name (remote);
  else if (FLATPAK_IS_BUNDLE_REF (ref))
    remote_name = flatpak_bundle_ref_get_origin (FLATPAK_BUNDLE_REF (ref));

  if (FLATPAK_IS_REMOTE_REF (ref))
    download_size = flatpak_remote_ref_get_download_size (FLATPAK_REMOTE_REF (ref));
  else if (FLATPAK_IS_BUNDLE_REF (ref))
    download_size = flatpak_bundle_ref_get_installed_size (FLATPAK_BUNDLE_REF (ref));

  if (component != NULL)
    {
      AsDeveloper   *developer_obj        = NULL;
      GPtrArray     *screenshots          = NULL;
      AsReleaseList *releases             = NULL;
      GPtrArray     *releases_arr         = NULL;
      GPtrArray     *icons                = NULL;
      AsBranding    *branding             = NULL;
      GPtrArray     *requires_relations   = NULL;
      GPtrArray     *recommends_relations = NULL;
      GPtrArray     *supports_relations   = NULL;

      title = as_component_get_name (component);
      if (title == NULL)
        title = as_component_get_id (component);

      description      = as_component_get_summary (component);
      metadata_license = as_component_get_metadata_license (component);
      project_license  = as_component_get_project_license (component);
      is_floss         = as_component_is_floss (component);
      project_group    = as_component_get_project_group (component);
      project_url      = as_component_get_url (component, AS_URL_KIND_HOMEPAGE);
      as_search_tokens = as_component_get_search_tokens (component);

      developer_obj = as_component_get_developer (component);
      if (developer_obj != NULL)
        {
          developer    = as_developer_get_name (developer_obj);
          developer_id = as_developer_get_id (developer_obj);
        }

      long_description = as_component_get_description (component);

      screenshots = as_component_get_screenshots_all (component);
      if (screenshots != NULL)
        {
          screenshot_paintables = g_list_store_new (BZ_TYPE_ASYNC_TEXTURE);

          for (guint i = 0; i < screenshots->len; i++)
            {
              AsScreenshot *screenshot = NULL;
              GPtrArray    *images     = NULL;

              screenshot = g_ptr_array_index (screenshots, i);
              images     = as_screenshot_get_images_all (screenshot);

              for (guint j = 0; j < images->len; j++)
                {
                  AsImage    *image_obj = NULL;
                  const char *url       = NULL;

                  image_obj = g_ptr_array_index (images, j);
                  url       = as_image_get_url (image_obj);

                  if (url != NULL)
                    {
                      g_autoptr (GFile) screenshot_file  = NULL;
                      g_autofree char *cache_basename    = NULL;
                      g_autoptr (GFile) cache_file       = NULL;
                      g_autoptr (BzAsyncTexture) texture = NULL;

                      screenshot_file = g_file_new_for_uri (url);
                      cache_basename  = g_strdup_printf ("screenshot_%d.png", i);
                      cache_file      = g_file_new_build_filename (
                          module_dir, unique_id_checksum, cache_basename, NULL);

                      texture = bz_async_texture_new_lazy (screenshot_file, cache_file);
                      g_list_store_append (screenshot_paintables, texture);
                      break;
                    }
                }
            }
        }

      share_urls = g_list_store_new (BZ_TYPE_URL);
      if (kinds & BZ_ENTRY_KIND_APPLICATION &&
          g_strcmp0 (remote_name, "flathub") == 0)
        {
          g_autofree char *flathub_url = NULL;
          g_autoptr (BzUrl) url        = NULL;

          flathub_url = g_strdup_printf ("https://flathub.org/apps/%s", id);

          url = bz_url_new ();
          bz_url_set_name (url, C_ ("Project URL Type", "Flathub Page"));
          bz_url_set_url (url, flathub_url);
          bz_url_set_icon_name (url, "flathub-symbolic");

          g_list_store_append (share_urls, url);
        }

      for (int e = AS_URL_KIND_UNKNOWN + 1; e < AS_URL_KIND_LAST; e++)
        {
          const char *url = NULL;

          url = as_component_get_url (component, e);
          if (url != NULL)
            {
              const char *enum_string     = NULL;
              const char *icon_name       = NULL;
              g_autoptr (BzUrl) share_url = NULL;

              switch (e)
                {
                case AS_URL_KIND_HOMEPAGE:
                  enum_string = C_ ("Project URL Type", "Project Website");
                  icon_name   = "globe-symbolic";
                  break;
                case AS_URL_KIND_BUGTRACKER:
                  enum_string = C_ ("Project URL Type", "Issue Tracker");
                  icon_name   = "computer-fail-symbolic";
                  break;
                case AS_URL_KIND_FAQ:
                  enum_string = C_ ("Project URL Type", "FAQ");
                  icon_name   = "help-faq-symbolic";
                  break;
                case AS_URL_KIND_HELP:
                  enum_string = C_ ("Project URL Type", "Help");
                  icon_name   = "help-browser-symbolic";
                  break;
                case AS_URL_KIND_DONATION:
                  enum_string = C_ ("Project URL Type", "Donate");
                  icon_name   = "heart-filled-symbolic";
                  g_clear_pointer (&donation_url, g_free);
                  donation_url = g_strdup (url);
                  break;
                case AS_URL_KIND_TRANSLATE:
                  enum_string = C_ ("Project URL Type", "Translate");
                  icon_name   = "translations-symbolic";
                  break;
                case AS_URL_KIND_CONTACT:
                  enum_string = C_ ("Project URL Type", "Contact");
                  icon_name   = "mail-send-symbolic";
                  break;
                case AS_URL_KIND_VCS_BROWSER:
                  enum_string = C_ ("Project URL Type", "Source Code");
                  icon_name   = "code-symbolic";
                  g_clear_pointer (&forge_url, g_free);
                  forge_url = g_strdup (url);
                  break;
                case AS_URL_KIND_CONTRIBUTE:
                  enum_string = C_ ("Project URL Type", "Contribute");
                  icon_name   = "system-users-symbolic";
                  break;
                default:
                  break;
                }

              share_url = g_object_new (
                  BZ_TYPE_URL,
                  "name", enum_string,
                  "url", url,
                  "icon-name", icon_name,
                  NULL);
              g_list_store_append (share_urls, share_url);
            }
        }
      if (g_list_model_get_n_items (G_LIST_MODEL (share_urls)) == 0)
        g_clear_object (&share_urls);

      releases = as_component_load_releases (component, TRUE, error);
      if (releases == NULL)
        return NULL;
      releases_arr = as_release_list_get_entries (releases);
      if (releases_arr != NULL)
        {
          version_history = g_list_store_new (BZ_TYPE_RELEASE);

          for (guint i = 0; i < releases_arr->len; i++)
            {
              AsRelease  *as_release          = NULL;
              GPtrArray  *as_issues           = NULL;
              const char *release_description = NULL;
              g_autoptr (GListStore) issues   = NULL;
              g_autoptr (BzRelease) release   = NULL;

              as_release = g_ptr_array_index (releases_arr, i);
              as_issues  = as_release_get_issues (as_release);

              release_description = as_release_get_description (as_release);

              if (as_issues != NULL && as_issues->len > 0)
                {
                  issues = g_list_store_new (BZ_TYPE_ISSUE);

                  for (guint j = 0; j < as_issues->len; j++)
                    {
                      AsIssue *as_issue         = NULL;
                      g_autoptr (BzIssue) issue = NULL;

                      as_issue = g_ptr_array_index (as_issues, j);

                      issue = g_object_new (
                          BZ_TYPE_ISSUE,
                          "id", as_issue_get_id (as_issue),
                          "url", as_issue_get_url (as_issue),
                          NULL);
                      g_list_store_append (issues, issue);
                    }
                }

              release = g_object_new (
                  BZ_TYPE_RELEASE,
                  "description", release_description,
                  "issues", issues,
                  "timestamp", as_release_get_timestamp (as_release),
                  "url", as_release_get_url (as_release, AS_RELEASE_URL_KIND_DETAILS),
                  "version", as_release_get_version (as_release),
                  NULL);
              g_list_store_append (version_history, release);
            }
        }

      icons = as_component_get_icons (component);
      if (icons != NULL)
        {
          g_autofree char *select          = NULL;
          gboolean         select_is_local = FALSE;
          int              select_width    = 0;
          int              select_height   = 0;

          for (guint i = 0; i < icons->len; i++)
            {
              AsIcon  *icon     = NULL;
              int      width    = 0;
              int      height   = 0;
              gboolean is_local = FALSE;

              icon     = g_ptr_array_index (icons, i);
              width    = as_icon_get_width (icon);
              height   = as_icon_get_height (icon);
              is_local = as_icon_get_kind (icon) != AS_ICON_KIND_REMOTE;

              if (select == NULL ||
                  (is_local && !select_is_local) ||
                  (width > select_width && height > select_height))
                {
                  if (is_local)
                    {
                      const char      *filename   = NULL;
                      g_autofree char *resolution = NULL;
                      g_autofree char *path       = NULL;

                      filename = as_icon_get_filename (icon);
                      if (filename == NULL)
                        continue;

                      resolution = g_strdup_printf ("%dx%d", width, height);
                      path       = g_build_filename (
                          appstream_dir,
                          "icons",
                          "flatpak",
                          resolution,
                          filename,
                          NULL);
                      if (!g_file_test (path, G_FILE_TEST_EXISTS))
                        continue;

                      g_clear_pointer (&select, g_free);
                      select          = g_steal_pointer (&path);
                      select_is_local = TRUE;
                      select_width    = width;
                      select_height   = height;
                    }
                  else
                    {
                      const char *url = NULL;

                      url = as_icon_get_url (icon);
                      if (url == NULL)
                        continue;

                      g_clear_pointer (&select, g_free);
                      select          = g_strdup (url);
                      select_is_local = FALSE;
                      select_width    = width;
                      select_height   = height;
                    }
                }
            }

          if (select != NULL)
            {
              g_autofree char *select_uri  = NULL;
              g_autoptr (GFile) source     = NULL;
              g_autoptr (GFile) cache_into = NULL;
              BzAsyncTexture *texture      = NULL;

              if (select_is_local)
                select_uri = g_strdup_printf ("file://%s", select);
              else
                select_uri = g_steal_pointer (&select);

              source     = g_file_new_for_uri (select_uri);
              cache_into = g_file_new_build_filename (
                  module_dir, unique_id_checksum, "icon-paintable.png", NULL);

              texture        = bz_async_texture_new_lazy (source, cache_into);
              icon_paintable = GDK_PAINTABLE (texture);

              if (select_is_local)
                mini_icon = bz_load_mini_icon_sync (unique_id_checksum, select);
            }
        }

      branding = as_component_get_branding (component);
      if (branding != NULL)
        {
          accent_color_light = as_branding_get_color (
              branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_LIGHT);
          accent_color_dark = as_branding_get_color (
              branding, AS_COLOR_KIND_PRIMARY, AS_COLOR_SCHEME_KIND_DARK);
        }

      content_rating = as_component_get_content_rating (component, "oars-1.1");
      if (content_rating != NULL)
        {
          g_object_ref (content_rating);
        }
      else
        {
          content_rating = as_component_get_content_rating (component, "oars-1.0");
          if (content_rating != NULL)
            g_object_ref (content_rating);
        }

      requires_relations   = as_component_get_requires (component);
      recommends_relations = as_component_get_recommends (component);
      supports_relations   = as_component_get_supports (component);

      if (requires_relations != NULL)
        {
          for (guint i = 0; i < requires_relations->len; i++)
            {
              AsRelation        *relation  = g_ptr_array_index (requires_relations, i);
              AsRelationItemKind item_kind = as_relation_get_item_kind (relation);

              if (item_kind == AS_RELATION_ITEM_KIND_CONTROL)
                {
                  AsControlKind control_kind = as_relation_get_value_control_kind (relation);
                  const char   *control_str  = as_control_kind_to_string (control_kind);
                  if (control_str != NULL)
                    required_controls |= parse_control_value (control_str);
                }
              else if (item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
                {
                  AsRelationCompare compare = as_relation_get_compare (relation);
                  gint              value   = as_relation_get_value_int (relation);
                  if (compare == AS_RELATION_COMPARE_GE)
                    min_display_length = value;
                }
            }
        }

      if (recommends_relations != NULL)
        {
          for (guint i = 0; i < recommends_relations->len; i++)
            {
              AsRelation        *relation  = g_ptr_array_index (recommends_relations, i);
              AsRelationItemKind item_kind = as_relation_get_item_kind (relation);

              if (item_kind == AS_RELATION_ITEM_KIND_CONTROL)
                {
                  AsControlKind control_kind = as_relation_get_value_control_kind (relation);
                  const char   *control_str  = as_control_kind_to_string (control_kind);
                  if (control_str != NULL)
                    recommended_controls |= parse_control_value (control_str);
                }
            }
        }

      if (supports_relations != NULL)
        {
          for (guint i = 0; i < supports_relations->len; i++)
            {
              AsRelation        *relation  = g_ptr_array_index (supports_relations, i);
              AsRelationItemKind item_kind = as_relation_get_item_kind (relation);

              if (item_kind == AS_RELATION_ITEM_KIND_CONTROL)
                {
                  AsControlKind control_kind = as_relation_get_value_control_kind (relation);
                  const char   *control_str  = as_control_kind_to_string (control_kind);
                  if (control_str != NULL)
                    supported_controls |= parse_control_value (control_str);
                }
              else if (item_kind == AS_RELATION_ITEM_KIND_DISPLAY_LENGTH)
                {
                  AsRelationCompare compare = as_relation_get_compare (relation);
                  gint              value   = as_relation_get_value_int (relation);
                  if (compare == AS_RELATION_COMPARE_LE)
                    max_display_length = value;
                }
            }
        }
      is_mobile_friendly = calculate_is_mobile_friendly (required_controls,
                                                         supported_controls,
                                                         min_display_length,
                                                         max_display_length);
    }

  if (icon_paintable == NULL && FLATPAK_IS_BUNDLE_REF (ref))
    {
      for (int size = 128; size > 0; size -= 64)
        {
          g_autoptr (GBytes) icon_bytes = NULL;
          GdkTexture *texture           = NULL;

          icon_bytes = flatpak_bundle_ref_get_icon (FLATPAK_BUNDLE_REF (ref), size);
          if (icon_bytes == NULL)
            continue;

          texture = gdk_texture_new_from_bytes (icon_bytes, NULL);
          /* don't error out even if loading fails */

          if (texture != NULL)
            {
              icon_paintable = GDK_PAINTABLE (texture);
              break;
            }
        }
    }

  if (title == NULL)
    {
      if (self->application_name != NULL)
        title = self->application_name;
      else if (self->runtime_name != NULL)
        title = self->runtime_name;
      else
        title = self->flatpak_id;
    }

  if (FLATPAK_IS_REMOTE_REF (ref))
    eol = flatpak_remote_ref_get_eol (FLATPAK_REMOTE_REF (ref));

  if (as_search_tokens != NULL)
    {
      g_autoptr (GStrvBuilder) builder = NULL;
      g_auto (GStrv) strv              = NULL;

      builder = g_strv_builder_new ();
      for (guint i = 0; i < as_search_tokens->len; i++)
        {
          const char *token = NULL;

          token = g_ptr_array_index (as_search_tokens, i);
          g_strv_builder_add (builder, token);
        }

      strv          = g_strv_builder_end (builder);
      search_tokens = g_strjoinv (" ", strv);
    }

  g_object_set (
      self,
      "kinds", kinds,
      "id", id,
      "unique-id", unique_id,
      "unique-id-checksum", unique_id_checksum,
      "title", title,
      "eol", eol,
      "description", description,
      "long-description", long_description,
      "remote-repo-name", remote_name,
      "url", project_url,
      "size", download_size,
      "search-tokens", search_tokens,
      "metadata-license", metadata_license,
      "project-license", project_license,
      "is-floss", is_floss,
      "project-group", project_group,
      "developer", developer,
      "developer-id", developer_id,
      "icon-paintable", icon_paintable,
      "mini-icon", mini_icon,
      "screenshot-paintables", screenshot_paintables,
      "share-urls", share_urls,
      "donation-url", donation_url,
      "forge-url", forge_url,
      "reviews", native_reviews,
      "average-rating", average_rating,
      "ratings-summary", ratings_summary,
      "version-history", version_history,
      "light-accent-color", accent_color_light,
      "dark-accent-color", accent_color_dark,
      "required-controls", required_controls,
      "recommended-controls", recommended_controls,
      "supported-controls", supported_controls,
      "min-display-length", min_display_length,
      "max-display-length", max_display_length,
      "is-mobile-friendly", is_mobile_friendly,
      "content-rating", content_rating,
      NULL);

  return g_steal_pointer (&self);
}

char *
bz_flatpak_ref_format_unique (FlatpakRef *ref,
                              gboolean    user)
{
  g_autofree char *fmt    = NULL;
  const char      *origin = NULL;

  fmt = flatpak_ref_format_ref (FLATPAK_REF (ref));

  if (FLATPAK_IS_REMOTE_REF (ref))
    origin = flatpak_remote_ref_get_remote_name (FLATPAK_REMOTE_REF (ref));
  else if (FLATPAK_IS_BUNDLE_REF (ref))
    origin = flatpak_bundle_ref_get_origin (FLATPAK_BUNDLE_REF (ref));
  else if (FLATPAK_IS_INSTALLED_REF (ref))
    origin = flatpak_installed_ref_get_origin (FLATPAK_INSTALLED_REF (ref));

  return g_strdup_printf (
      "FLATPAK-%s::%s::%s",
      user ? "USER" : "SYSTEM",
      origin, fmt);
}

FlatpakRef *
bz_flatpak_entry_get_ref (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);

  if (self->ref == NULL)
    self->ref = flatpak_ref_parse (self->flatpak_id, NULL);

  return self->ref;
}

gboolean
bz_flatpak_entry_is_user (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), FALSE);
  return self->user;
}

const char *
bz_flatpak_entry_get_flatpak_name (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->flatpak_name;
}

const char *
bz_flatpak_entry_get_flatpak_id (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->flatpak_id;
}

const char *
bz_flatpak_entry_get_flatpak_version (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->flatpak_version;
}

const char *
bz_flatpak_entry_get_application_name (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->application_name;
}

const char *
bz_flatpak_entry_get_application_runtime (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->application_runtime;
}

const char *
bz_flatpak_entry_get_runtime_name (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->runtime_name;
}

const char *
bz_flatpak_entry_get_addon_extension_of_ref (BzFlatpakEntry *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), NULL);
  return self->addon_extension_of_ref;
}

char *
bz_flatpak_entry_extract_id_from_unique_id (const char *unique_id)
{
  g_auto (GStrv) tokens      = NULL;
  g_autoptr (FlatpakRef) ref = NULL;
  const char *name           = NULL;

  tokens = g_strsplit (unique_id, "::", 3);
  if (g_strv_length (tokens) != 3)
    return NULL;

  ref = flatpak_ref_parse (tokens[2], NULL);
  if (ref == NULL)
    return NULL;

  name = flatpak_ref_get_name (ref);
  if (name == NULL)
    return NULL;

  return g_strdup (name);
}

gboolean
bz_flatpak_entry_launch (BzFlatpakEntry    *self,
                         BzFlatpakInstance *flatpak,
                         GError           **error)
{
  FlatpakRef *ref = NULL;
#ifdef SANDBOXED_LIBFLATPAK
  g_autofree char *fmt     = NULL;
  g_autofree char *cmdline = NULL;
#else
  FlatpakInstallation *installation = NULL;
#endif

  g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (self), FALSE);
  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (flatpak), FALSE);

  ref = bz_flatpak_entry_get_ref (self);

#ifdef SANDBOXED_LIBFLATPAK
  fmt = flatpak_ref_format_ref (FLATPAK_REF (ref));

  if (g_file_test ("/run/systemd", G_FILE_TEST_EXISTS))
    cmdline = g_strdup_printf ("flatpak-spawn --host systemd-run --user --pipe flatpak run %s", fmt);
  else
    cmdline = g_strdup_printf ("flatpak-spawn --host flatpak run %s", fmt);

  return g_spawn_command_line_async (cmdline, error);
#else
  installation =
      self->user
          ? bz_flatpak_instance_get_user_installation (flatpak)
          : bz_flatpak_instance_get_system_installation (flatpak);

  /* async? */
  return flatpak_installation_launch (
      installation,
      flatpak_ref_get_name (ref),
      flatpak_ref_get_arch (ref),
      flatpak_ref_get_branch (ref),
      flatpak_ref_get_commit (ref),
      NULL, error);
#endif
}

static void
clear_entry (BzFlatpakEntry *self)
{
  g_clear_pointer (&self->flatpak_name, g_free);
  g_clear_pointer (&self->flatpak_id, g_free);
  g_clear_pointer (&self->flatpak_version, g_free);
  g_clear_pointer (&self->application_name, g_free);
  g_clear_pointer (&self->application_runtime, g_free);
  g_clear_pointer (&self->application_command, g_free);
  g_clear_pointer (&self->runtime_name, g_free);
  g_clear_pointer (&self->addon_extension_of_ref, g_free);
}

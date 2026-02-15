/* bz-full-view.c
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

#define G_LOG_DOMAIN "BAZAAR::FULL-VIEW-WIDGET"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "bz-addons-dialog.h"
#include "bz-age-rating-dialog.h"
#include "bz-app-size-dialog.h"
#include "bz-app-tile.h"
#include "bz-apps-page.h"
#include "bz-appstream-description-render.h"
#include "bz-context-tile.h"
#include "bz-developer-badge.h"
#include "bz-dynamic-list-view.h"
#include "bz-error.h"
#include "bz-fading-clamp.h"
#include "bz-favorite-button.h"
#include "bz-flatpak-entry.h"
#include "bz-full-view.h"
#include "bz-hardware-support-dialog.h"
#include "bz-license-dialog.h"
#include "bz-releases-list.h"
#include "bz-safety-calculator.h"
#include "bz-safety-dialog.h"
#include "bz-screenshot-page.h"
#include "bz-screenshots-carousel.h"
#include "bz-section-view.h"
#include "bz-share-list.h"
#include "bz-spdx.h"
#include "bz-state-info.h"
#include "bz-stats-dialog.h"
#include "bz-tag-list.h"
#include "bz-template-callbacks.h"

struct _BzFullView
{
  AdwBin parent_instance;

  BzStateInfo          *state;
  BzTransactionManager *transactions;
  BzEntryGroup         *group;
  BzResult             *ui_entry;
  BzResult             *group_model;
  gboolean              show_sidebar;

  GMenuModel *main_menu;

  /* Template widgets */
  GtkScrolledWindow *main_scroll;
  AdwViewStack      *stack;
  GtkWidget         *shadow_overlay;
  GtkToggleButton   *description_toggle;

  GtkWidget *wide_open_button;
  GtkWidget *wide_install_button;
  GtkWidget *narrow_install_button;
  GtkWidget *narrow_open_button;
};

G_DEFINE_FINAL_TYPE (BzFullView, bz_full_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_STATE,
  PROP_ENTRY_GROUP,
  PROP_UI_ENTRY,
  PROP_MAIN_MENU,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,
  SIGNAL_REMOVE,
  SIGNAL_INSTALL_ADDON,
  SIGNAL_REMOVE_ADDON,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
addon_transact_cb (BzFullView     *self,
                   BzEntry        *entry,
                   BzAddonsDialog *dialog);

static void
grab_first_button (BzFullView *self);

static void
bz_full_view_dispose (GObject *object)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  g_clear_object (&self->state);
  g_clear_object (&self->transactions);
  g_clear_object (&self->group);
  g_clear_object (&self->ui_entry);
  g_clear_object (&self->group_model);
  g_clear_object (&self->main_menu);

  G_OBJECT_CLASS (bz_full_view_parent_class)->dispose (object);
}

static void
bz_full_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    case PROP_ENTRY_GROUP:
      g_value_set_object (value, bz_full_view_get_entry_group (self));
      break;
    case PROP_UI_ENTRY:
      g_value_set_object (value, self->ui_entry);
      break;
    case PROP_MAIN_MENU:
      g_value_set_object (value, self->main_menu);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_full_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_clear_object (&self->state);
      self->state = g_value_dup_object (value);
      break;
    case PROP_ENTRY_GROUP:
      bz_full_view_set_entry_group (self, g_value_get_object (value));
      break;
    case PROP_UI_ENTRY:
    case PROP_MAIN_MENU:
      if (self->main_menu)
        g_object_unref (self->main_menu);
      self->main_menu = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
format_with_small_suffix (char *number, const char *suffix)
{
  char *dot = g_strrstr (number, ".");

  if (dot != NULL)
    {
      char *end = dot + strlen (dot) - 1;
      while (end > dot && *end == '0')
        *end-- = '\0';
      if (end == dot)
        *dot = '\0';
    }

  return g_strdup_printf ("%s\xC2\xA0<span font_size='x-small'>%s</span>",
                          number, suffix);
}

static gboolean
is_scrolled_down (gpointer object,
                  double   value)
{
  return value > 100.0;
}

static char *
format_favorites_count (gpointer object,
                        int      favorites_count)
{
  if (favorites_count < 0)
    return g_strdup ("  ");
  return g_strdup_printf ("%d", favorites_count);
}

static char *
format_recent_downloads (gpointer object,
                         int      value)
{
  double result;
  int    digits;

  if (value <= 0)
    return g_strdup (_ ("---"));

  if (value >= 1000000)
    {
      result = value / 1000000.0;
      digits = (int) log10 (result) + 1;
      /* Translators: M is the suffix for millions */
      return g_strdup_printf (_ ("%.*fM"), 3 - digits, result);
    }
  else if (value >= 1000)
    {
      result = value / 1000.0;
      digits = (int) log10 (result) + 1;
      /* Translators: K is the suffix for thousands*/
      return g_strdup_printf (_ ("%.*fK"), 3 - digits, result);
    }
  else
    return g_strdup_printf ("%'d", value);
}

static char *
format_recent_downloads_tooltip (gpointer object,
                                 int      value)
{
  return g_strdup_printf (_ ("%d downloads in the last 30 days"), value);
}

static char *
format_size (gpointer object, guint64 value)
{
  g_autofree char *size_str = g_format_size (value);
  char            *space    = g_strrstr (size_str, "\xC2\xA0");
  char            *decimal  = NULL;
  int              digits   = 0;

  if (value == 0)
    return g_strdup (_ ("N/A"));

  if (space != NULL)
    {
      *space = '\0';
      for (char *p = size_str; *p != '\0' && *p != '.'; p++)
        if (g_ascii_isdigit (*p))
          digits++;
      if (digits >= 3)
        {
          decimal = g_strrstr (size_str, ".");
          if (decimal != NULL)
            *decimal = '\0';
        }
      return format_with_small_suffix (size_str, space + 2);
    }
  return g_strdup (size_str);
}

static char *
get_size_label (gpointer object,
                gboolean is_installable)
{
  // Translators: .
  return g_strdup (is_installable ? _ ("Download") : _ ("Installed"));
}

static guint64
get_size_type (gpointer object,
               BzEntry *entry,
               gboolean is_installable)
{
  if (entry == NULL)
    return 0;

  return is_installable ? bz_entry_get_size (entry) : bz_entry_get_installed_size (entry);
}

static char *
format_size_tooltip (gpointer object, guint64 value)
{
  g_autofree char *size_str = NULL;

  if (value == 0)
    return g_strdup (_ ("Size information unavailable"));

  size_str = g_format_size (value);
  return g_strdup_printf (_ ("Download size of %s"), size_str);
}

static char *
format_age_rating (gpointer         object,
                   AsContentRating *content_rating)
{
  guint age;

  if (content_rating == NULL)
    return g_strdup ("?");

  age = as_content_rating_get_minimum_age (content_rating);

  if (age < 3)
    age = 3;

  /* Translators: Age rating format, e.g. "12+" for ages 12 and up */
  return g_strdup_printf (_ ("%d+"), age);
}

static char *
get_age_rating_label (gpointer         object,
                      AsContentRating *content_rating)
{
  guint age;

  if (content_rating == NULL)
    return g_strdup (_ ("Age Rating"));

  age = as_content_rating_get_minimum_age (content_rating);

  if (age == 0)
    return g_strdup (_ ("All Ages"));
  else
    return g_strdup (_ ("Age Rating"));
}

static char *
get_age_rating_tooltip (gpointer         object,
                        AsContentRating *content_rating)
{
  guint age;

  if (content_rating == NULL)
    return g_strdup (_ ("Age rating information unavailable"));

  age = as_content_rating_get_minimum_age (content_rating);

  if (age == 0)
    return g_strdup (_ ("Suitable for all ages"));

  return g_strdup_printf (_ ("Suitable for ages %d and up"), age);
}

static char *
get_age_rating_style (gpointer         object,
                      AsContentRating *content_rating)
{
  guint age;

  if (content_rating == NULL)
    return g_strdup ("grey");

  age = as_content_rating_get_minimum_age (content_rating);

  if (age >= 18)
    return g_strdup ("error");
  else if (age >= 15)
    return g_strdup ("orange");
  else if (age >= 12)
    return g_strdup ("warning");
  else
    return g_strdup ("grey");
}

static char *
format_license_tooltip (gpointer object,
                        BzEntry *entry)
{
  const char      *license;
  gboolean         is_floss = FALSE;
  g_autofree char *name     = NULL;

  if (entry == NULL)
    return g_strdup (_ ("Unknown"));

  g_object_get (entry, "is-floss", &is_floss, "project-license", &license, NULL);

  if (license == NULL || *license == '\0')
    return g_strdup (_ ("Unknown"));

  if (is_floss && bz_spdx_is_valid (license))
    {
      name = bz_spdx_get_name (license);
      return g_strdup_printf (_ ("Free software licensed under %s"),
                              (name != NULL && *name != '\0') ? name : license);
    }

  if (is_floss)
    return g_strdup (_ ("Free software"));

  if (bz_spdx_is_proprietary (license))
    return g_strdup (_ ("Proprietary Software"));

  name = bz_spdx_get_name (license);
  return g_strdup_printf (_ ("Special License: %s"),
                          (name != NULL && *name != '\0') ? name : license);
}

static char *
get_license_label (gpointer object,
                   BzEntry *entry)
{
  const char *license;
  gboolean    is_floss = FALSE;

  if (entry == NULL)
    return g_strdup (_ ("Unknown"));

  g_object_get (entry, "is-floss", &is_floss, "project-license", &license, NULL);

  if (is_floss)
    return g_strdup (_ ("Free"));

  if (bz_spdx_is_proprietary (license))
    return g_strdup (_ ("Proprietary"));

  if (license == NULL || *license == '\0')
    return g_strdup (_ ("Unknown"));

  return g_strdup (_ ("Special License"));
}

static char *
get_license_icon (gpointer object,
                  gboolean is_floss,
                  int      index)
{
  const char *icons[][2] = {
    {   "license-symbolic", "proprietary-code-symbolic" },
    { "community-symbolic",          "license-symbolic" }
  };

  return g_strdup (icons[is_floss ? 1 : 0][index]);
}

static char *
get_formfactor_label (gpointer object,
                      gboolean is_mobile_friendly)
{
  return g_strdup (is_mobile_friendly ? _ ("Adaptive") : _ ("Desktop Only"));
}

static char *
get_formfactor_tooltip (gpointer object, gboolean is_mobile_friendly)
{
  return g_strdup (is_mobile_friendly ? _ ("Works on desktop, tablets, and phones")
                                      : _ ("May not work on mobile devices"));
}

static char *
format_as_link (gpointer    object,
                const char *value)
{
  if (value != NULL)
    return g_strdup_printf ("<a href=\"%s\" title=\"%s\">%s</a>",
                            value, value, value);
  else
    return g_strdup (_ ("No URL"));
}

static gboolean
has_link (gpointer    object,
          const char *license)
{
  if (license == NULL || *license == '\0')
    return FALSE;

  return bz_spdx_is_valid (license);
}

static char *
pick_license_warning (gpointer object,
                      gboolean value)
{
  return value
             ? g_strdup (_ ("This application has a FLOSS license, meaning the source code can be audited for safety."))
             : g_strdup (_ ("This application has a proprietary license, meaning the source code is developed privately and cannot be audited by an independent third party."));
}

static char *
format_other_apps_label (gpointer object, const char *developer)
{
  if (!developer || *developer == '\0')
    return g_strdup (_ ("More Apps"));
  return g_strdup_printf (_ ("More Apps by %s"), developer);
}

static char *
format_more_other_apps_label (gpointer object, const char *developer)
{
  if (!developer || *developer == '\0')
    return g_strdup (_ ("Other Apps by this Developer"));

  return g_strdup_printf (_ ("Other Apps by %s"), developer);
}

static char *
format_leftover_label (gpointer object, const char *name, guint64 size)
{
  g_autofree char *formatted_size = NULL;

  formatted_size = g_format_size (size);
  return g_strdup_printf (_ ("%s is not installed, but it still has <b>%s</b> of data present."), name, formatted_size);
}

static char *
get_safety_rating_icon (gpointer object,
                        BzEntry *entry,
                        int      index)
{
  char        *icon       = NULL;
  BzImportance importance = 0;

  if (entry == NULL)
    return g_strdup ("app-safety-unknown-symbolic");

  if (index < 0 || index > 2)
    return NULL;

  if (index == 0)
    {
      importance = bz_safety_calculator_calculate_rating (entry);
      switch (importance)
        {
        case BZ_IMPORTANCE_UNIMPORTANT:
        case BZ_IMPORTANCE_NEUTRAL:
          return g_strdup ("app-safety-ok-symbolic");
        case BZ_IMPORTANCE_INFORMATION:
        case BZ_IMPORTANCE_WARNING:
          return NULL;
        case BZ_IMPORTANCE_IMPORTANT:
          return g_strdup ("dialog-warning-symbolic");
        default:
          return NULL;
        }
    }

  icon = bz_safety_calculator_get_top_icon (entry, index - 1);
  return icon;
}

static char *
get_safety_rating_style (gpointer object,
                         BzEntry *entry)
{
  BzImportance importance;

  if (entry == NULL)
    return g_strdup ("grey");

  importance = bz_safety_calculator_calculate_rating (entry);

  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
    case BZ_IMPORTANCE_NEUTRAL:
      return g_strdup ("grey");
    case BZ_IMPORTANCE_INFORMATION:
      return g_strdup ("warning");
    case BZ_IMPORTANCE_WARNING:
      return g_strdup ("orange");
    case BZ_IMPORTANCE_IMPORTANT:
      return g_strdup ("error");
    default:
      return g_strdup ("grey");
    }
}

static char *
get_safety_rating_label (gpointer object,
                         BzEntry *entry)
{
  BzImportance importance;

  if (entry == NULL)
    return g_strdup (_ ("N/A"));

  importance = bz_safety_calculator_calculate_rating (entry);

  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
      return g_strdup (_ ("Safe"));
    case BZ_IMPORTANCE_NEUTRAL:
      return g_strdup (_ ("Low Risk"));
    case BZ_IMPORTANCE_INFORMATION:
      return g_strdup (_ ("Low Risk"));
    case BZ_IMPORTANCE_WARNING:
      return g_strdup (_ ("Medium Risk"));
    case BZ_IMPORTANCE_IMPORTANT:
      return g_strdup (_ ("High Risk"));
    default:
      return g_strdup (_ ("N/A"));
    }
}

static gpointer
filter_own_app_id (BzEntry *entry, GtkStringList *app_ids)
{
  const char *own_id;
  g_autoptr (GtkStringList) filtered = NULL;
  guint n_items                      = 0;

  if (!BZ_IS_ENTRY (entry) || !GTK_IS_STRING_LIST (app_ids))
    return NULL;

  own_id = bz_entry_get_id (entry);
  if (!own_id)
    return NULL;

  filtered = gtk_string_list_new (NULL);
  n_items  = g_list_model_get_n_items (G_LIST_MODEL (app_ids));

  for (guint i = 0; i < n_items; i++)
    {
      const char *id = NULL;

      id = gtk_string_list_get_string (app_ids, i);
      if (g_strcmp0 (id, own_id) != 0)
        gtk_string_list_append (filtered, id);
    }

  if (g_list_model_get_n_items (G_LIST_MODEL (filtered)) > 0)
    return g_steal_pointer (&filtered);
  else
    return NULL;
}

static gboolean
has_other_apps (gpointer object, GtkStringList *app_ids, BzEntry *entry)
{
  g_autoptr (GtkStringList) filtered = filter_own_app_id (BZ_ENTRY (entry), app_ids);
  return filtered != NULL;
}

static GListModel *
get_developer_apps_entries (gpointer object, GtkStringList *app_ids, BzEntry *entry)
{
  BzFullView *self                   = BZ_FULL_VIEW (object);
  g_autoptr (GtkStringList) filtered = filter_own_app_id (BZ_ENTRY (entry), app_ids);
  BzApplicationMapFactory *factory;

  if (!filtered)
    return NULL;

  factory = bz_state_info_get_application_factory (self->state);
  if (!factory)
    return NULL;

  return bz_application_map_factory_generate (factory, G_LIST_MODEL (filtered));
}

static void
apps_page_select_cb (BzFullView        *self,
                     BzEntryGroup      *group,
                     AdwNavigationPage *page)
{
  GtkWidget *nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  adw_navigation_view_pop (ADW_NAVIGATION_VIEW (nav_view));
  bz_full_view_set_entry_group (self, group);
}

static void
more_apps_button_clicked_cb (BzFullView *self,
                             GtkButton  *button)
{
  g_autoptr (GListModel) model = NULL;
  guint              n_items;
  g_autofree char   *title       = NULL;
  g_autofree char   *subtitle    = NULL;
  AdwNavigationPage *apps_page   = NULL;
  GtkWidget         *nav_view    = NULL;
  g_autoptr (GListModel) app_ids = NULL;
  BzEntry    *entry              = NULL;
  const char *developer          = NULL;

  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (GTK_IS_BUTTON (button));

  entry = bz_result_get_object (self->ui_entry);
  if (entry == NULL)
    return;

  g_object_get (entry, "developer-apps", &app_ids, NULL);

  model = bz_application_map_factory_generate (
      bz_state_info_get_application_factory (self->state),
      app_ids);

  n_items = g_list_model_get_n_items (model);

  developer = bz_entry_get_developer (entry);
  if (developer != NULL && *developer != '\0')
    title = g_strdup_printf (_ ("Other Apps by %s"), developer);
  else
    title = g_strdup (_ ("Other Apps"));

  subtitle = g_strdup_printf (ngettext ("%d Application", "%d Applications", n_items), n_items);

  apps_page = bz_apps_page_new (title, model);
  bz_apps_page_set_subtitle (BZ_APPS_PAGE (apps_page), subtitle);

  g_signal_connect_swapped (
      apps_page, "select",
      G_CALLBACK (apps_page_select_cb), self);

  nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  if (nav_view != NULL)
    adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), apps_page);
}

static void
app_tile_clicked_cb (BzFullView *self,
                     BzAppTile  *tile)
{
  BzEntryGroup *group = bz_app_tile_get_group (tile);
  bz_full_view_set_entry_group (self, group);
}

static void
bind_app_tile_cb (BzFullView        *self,
                  BzAppTile         *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked",
                            G_CALLBACK (app_tile_clicked_cb),
                            self);
}

static void
unbind_app_tile_cb (BzFullView        *self,
                    BzAppTile         *tile,
                    BzEntryGroup      *group,
                    BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (app_tile_clicked_cb), self);
}

static void
tag_list_select_cb (BzFullView   *self,
                    BzEntryGroup *group)
{
  bz_full_view_set_entry_group (self, group);
}

static void
open_url_cb (BzFullView   *self,
             AdwActionRow *row)
{
  BzEntry    *entry = NULL;
  const char *url   = NULL;

  entry = BZ_ENTRY (bz_result_get_object (self->ui_entry));
  url   = bz_entry_get_url (entry);

  if (url != NULL && *url != '\0')
    g_app_info_launch_default_for_uri (url, NULL, NULL);
  else
    g_warning ("Invalid or empty URL provided for Flathub URL CB");
}

static void
license_cb (BzFullView *self,
            GtkButton  *button)
{
  AdwDialog *dialog   = NULL;
  BzEntry   *ui_entry = NULL;

  if (self->group == NULL)
    return;

  ui_entry = bz_result_get_object (self->ui_entry);
  if (ui_entry == NULL)
    return;

  dialog = bz_license_dialog_new (ui_entry);
  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
age_rating_cb (BzFullView *self,
               GtkButton  *button)
{
  BzAgeRatingDialog *dialog   = NULL;
  BzEntry           *ui_entry = NULL;

  if (self->group == NULL)
    return;

  ui_entry = bz_result_get_object (self->ui_entry);
  if (ui_entry == NULL)
    return;

  dialog = bz_age_rating_dialog_new (ui_entry);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
dl_stats_cb (BzFullView *self,
             GtkButton  *button)
{
  AdwDialog *dialog   = NULL;
  BzEntry   *ui_entry = NULL;

  if (self->group == NULL)
    return;

  ui_entry = bz_result_get_object (self->ui_entry);

  dialog = bz_stats_dialog_new (NULL, NULL, 0);

  g_object_bind_property (ui_entry, "download-stats", dialog, "model", G_BINDING_SYNC_CREATE);
  g_object_bind_property (ui_entry, "download-stats-per-country", dialog, "country-model", G_BINDING_SYNC_CREATE);
  g_object_bind_property (ui_entry, "total-downloads", dialog, "total-downloads", G_BINDING_SYNC_CREATE);

  adw_dialog_present (dialog, GTK_WIDGET (self));
  bz_stats_dialog_animate_open (BZ_STATS_DIALOG (dialog));
}

static void
screenshot_clicked_cb (BzFullView            *self,
                       guint                  index,
                       BzScreenshotsCarousel *carousel)
{
  GListModel        *screenshots = NULL;
  GListModel        *captions    = NULL;
  AdwNavigationPage *page        = NULL;
  GtkWidget         *nav_view    = NULL;
  BzEntry           *entry       = NULL;

  screenshots = bz_screenshots_carousel_get_model (carousel);
  if (screenshots == NULL)
    return;

  if (self->ui_entry != NULL)
    {
      entry = bz_result_get_object (self->ui_entry);
      if (entry != NULL)
        g_object_get (entry, "screenshot-captions", &captions, NULL);
    }

  page = bz_screenshot_page_new (screenshots, captions, index);

  g_clear_object (&captions);

  nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  if (nav_view != NULL)
    adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), page);
}

static void
size_cb (BzFullView *self,
         GtkButton  *button)
{
  AdwDialog *size_dialog = NULL;

  if (self->group == NULL)
    return;

  size_dialog = bz_app_size_dialog_new (self->group);
  adw_dialog_present (size_dialog, GTK_WIDGET (self));
}

static void
formfactor_cb (BzFullView *self,
               GtkButton  *button)
{
  AdwDialog *dialog   = NULL;
  BzEntry   *ui_entry = NULL;

  if (self->group == NULL)
    return;

  ui_entry = bz_result_get_object (self->ui_entry);
  dialog   = ADW_DIALOG (bz_hardware_support_dialog_new (ui_entry));

  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
safety_cb (BzFullView *self,
           GtkButton  *button)
{
  AdwDialog *dialog = NULL;

  if (self->group == NULL)
    return;

  dialog = ADW_DIALOG (bz_safety_dialog_new (self->group));

  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
run_cb (BzFullView *self,
        GtkButton  *button)
{
  GListModel *model   = NULL;
  guint       n_items = 0;

  if (self->group == NULL || !bz_result_get_resolved (self->group_model))
    return;

  model   = bz_result_get_object (self->group_model);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (model, i);

      if (BZ_IS_FLATPAK_ENTRY (entry) && bz_entry_is_installed (entry))
        {
          g_autoptr (GError) local_error = NULL;
          gboolean result                = FALSE;

          result = bz_flatpak_entry_launch (
              BZ_FLATPAK_ENTRY (entry),
              BZ_FLATPAK_INSTANCE (bz_state_info_get_backend (self->state)),
              &local_error);
          if (!result)
            {
              GtkWidget *window = NULL;

              window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
              if (window != NULL)
                bz_show_error_for_widget (window, _ ("Failed to launch application"), local_error->message);
            }
          break;
        }
    }
}

static void
install_cb (BzFullView *self,
            GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_INSTALL], 0, button);
}

static void
remove_cb (BzFullView *self,
           GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_REMOVE], 0, button);
}

static void
delete_user_data_cb (BzFullView *self,
                     GtkButton  *button)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));

  if (self->group == NULL)
    return;

  bz_entry_group_reap_user_data (self->group);
}

static void
support_cb (BzFullView *self,
            GtkButton  *button)
{
  BzEntry *entry = NULL;

  entry = bz_result_get_object (self->ui_entry);
  if (entry != NULL)
    {
      const char *url = NULL;

      url = bz_entry_get_donation_url (entry);
      g_app_info_launch_default_for_uri (url, NULL, NULL);
    }
}

static void
install_addons_cb (BzFullView *self,
                   GtkButton  *button)
{
  BzEntry    *entry                   = NULL;
  GListModel *model                   = NULL;
  g_autoptr (GListModel) mapped_model = NULL;
  AdwDialog *addons_dialog            = NULL;

  if (self->group == NULL)
    return;

  entry = bz_result_get_object (self->ui_entry);
  if (entry == NULL)
    return;

  model = bz_entry_get_addons (entry);
  if (model == NULL || g_list_model_get_n_items (model) == 0)
    return;

  mapped_model = bz_application_map_factory_generate (
      bz_state_info_get_entry_factory (self->state),
      model);

  addons_dialog = bz_addons_dialog_new (entry, mapped_model);

  g_signal_connect_swapped (
      addons_dialog, "transact",
      G_CALLBACK (addon_transact_cb), self);

  adw_dialog_present (addons_dialog, GTK_WIDGET (self));
}

static void
addon_transact_cb (BzFullView     *self,
                   BzEntry        *entry,
                   BzAddonsDialog *dialog)
{
  gboolean installed = FALSE;

  g_object_get (entry, "installed", &installed, NULL);

  if (installed)
    g_signal_emit (self, signals[SIGNAL_REMOVE_ADDON], 0, entry);
  else
    g_signal_emit (self, signals[SIGNAL_INSTALL_ADDON], 0, entry);
}

static int
get_description_max_height (gpointer object,
                            gboolean active)
{
  return active ? 10000 : 170;
}

static char *
get_description_toggle_text (gpointer object,
                             gboolean active)
{
  return g_strdup (active ? _ ("Show Less") : _ ("Show More"));
}

static void
bz_full_view_class_init (BzFullViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_full_view_dispose;
  object_class->get_property = bz_full_view_get_property;
  object_class->set_property = bz_full_view_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ENTRY_GROUP] =
      g_param_spec_object (
          "entry-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_UI_ENTRY] =
      g_param_spec_object (
          "ui-entry",
          NULL, NULL,
          BZ_TYPE_RESULT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MAIN_MENU] =
      g_param_spec_object (
          "main-menu",
          NULL, NULL,
          G_TYPE_MENU_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 0);
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
          G_TYPE_NONE, 0);
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

  g_type_ensure (BZ_TYPE_APPSTREAM_DESCRIPTION_RENDER);
  g_type_ensure (BZ_TYPE_DEVELOPER_BADGE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_ENTRY);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);
  g_type_ensure (BZ_TYPE_FADING_CLAMP);
  g_type_ensure (BZ_TYPE_FAVORITE_BUTTON);
  g_type_ensure (BZ_TYPE_FLATPAK_ENTRY);
  g_type_ensure (BZ_TYPE_HARDWARE_SUPPORT_DIALOG);
  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_RELEASES_LIST);
  g_type_ensure (BZ_TYPE_SCREENSHOTS_CAROUSEL);
  g_type_ensure (BZ_TYPE_SHARE_LIST);
  g_type_ensure (BZ_TYPE_TAG_LIST);
  g_type_ensure (BZ_TYPE_CONTEXT_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-full-view.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzFullView, stack);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, main_scroll);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, shadow_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, description_toggle);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, wide_open_button);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, wide_install_button);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, narrow_install_button);
  gtk_widget_class_bind_template_child (widget_class, BzFullView, narrow_open_button);
  gtk_widget_class_bind_template_callback (widget_class, is_scrolled_down);
  gtk_widget_class_bind_template_callback (widget_class, format_favorites_count);
  gtk_widget_class_bind_template_callback (widget_class, format_recent_downloads);
  gtk_widget_class_bind_template_callback (widget_class, format_recent_downloads_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, format_size);
  gtk_widget_class_bind_template_callback (widget_class, get_size_label);
  gtk_widget_class_bind_template_callback (widget_class, format_size_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, age_rating_cb);
  gtk_widget_class_bind_template_callback (widget_class, format_age_rating);
  gtk_widget_class_bind_template_callback (widget_class, get_age_rating_label);
  gtk_widget_class_bind_template_callback (widget_class, get_age_rating_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, get_age_rating_style);
  gtk_widget_class_bind_template_callback (widget_class, format_as_link);
  gtk_widget_class_bind_template_callback (widget_class, format_license_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, get_license_label);
  gtk_widget_class_bind_template_callback (widget_class, get_license_icon);
  gtk_widget_class_bind_template_callback (widget_class, get_formfactor_label);
  gtk_widget_class_bind_template_callback (widget_class, get_formfactor_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, get_safety_rating_icon);
  gtk_widget_class_bind_template_callback (widget_class, get_safety_rating_style);
  gtk_widget_class_bind_template_callback (widget_class, get_safety_rating_label);
  gtk_widget_class_bind_template_callback (widget_class, has_link);
  gtk_widget_class_bind_template_callback (widget_class, format_leftover_label);
  gtk_widget_class_bind_template_callback (widget_class, format_other_apps_label);
  gtk_widget_class_bind_template_callback (widget_class, format_more_other_apps_label);
  gtk_widget_class_bind_template_callback (widget_class, get_developer_apps_entries);
  gtk_widget_class_bind_template_callback (widget_class, more_apps_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, has_other_apps);
  gtk_widget_class_bind_template_callback (widget_class, open_url_cb);
  gtk_widget_class_bind_template_callback (widget_class, license_cb);
  gtk_widget_class_bind_template_callback (widget_class, dl_stats_cb);
  gtk_widget_class_bind_template_callback (widget_class, screenshot_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, size_cb);
  gtk_widget_class_bind_template_callback (widget_class, get_size_type);
  gtk_widget_class_bind_template_callback (widget_class, formfactor_cb);
  gtk_widget_class_bind_template_callback (widget_class, safety_cb);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, delete_user_data_cb);
  gtk_widget_class_bind_template_callback (widget_class, support_cb);
  gtk_widget_class_bind_template_callback (widget_class, pick_license_warning);
  gtk_widget_class_bind_template_callback (widget_class, install_addons_cb);
  gtk_widget_class_bind_template_callback (widget_class, addon_transact_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_app_tile_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_app_tile_cb);
  gtk_widget_class_bind_template_callback (widget_class, get_description_max_height);
  gtk_widget_class_bind_template_callback (widget_class, get_description_toggle_text);
  gtk_widget_class_bind_template_callback (widget_class, tag_list_select_cb);
}

static void
bz_full_view_init (BzFullView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_full_view_new (void)
{
  return g_object_new (BZ_TYPE_FULL_VIEW, NULL);
}

static DexFuture *
on_ui_entry_resolved (DexFuture *future,
                      gpointer   user_data)
{
  BzEntry      *ui_entry       = NULL;
  BzResult     *runtime_result = NULL;
  const GValue *value          = NULL;

  value = dex_future_get_value (future, NULL);
  if (value != NULL && G_VALUE_HOLDS_OBJECT (value))
    {
      ui_entry = g_value_get_object (value);

      if (BZ_IS_FLATPAK_ENTRY (ui_entry))
        {
          runtime_result = bz_flatpak_entry_get_runtime (BZ_FLATPAK_ENTRY (ui_entry));

          if (runtime_result != NULL && !bz_result_get_resolved (runtime_result))
            {
              g_autoptr (DexFuture) runtime_future = bz_result_dup_future (runtime_result);
              dex_future_disown (g_steal_pointer (&runtime_future));
            }
        }
    }

  return dex_future_new_for_boolean (TRUE);
}

void
bz_full_view_set_entry_group (BzFullView   *self,
                              BzEntryGroup *group)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (group == NULL ||
                    BZ_IS_ENTRY_GROUP (group));

  if (group == self->group)
    return;

  g_clear_object (&self->group);
  g_clear_object (&self->ui_entry);
  g_clear_object (&self->group_model);
  gtk_toggle_button_set_active (self->description_toggle, FALSE);

  if (group != NULL)
    {
      self->group    = g_object_ref (group);
      self->ui_entry = bz_entry_group_dup_ui_entry (group);

      if (self->ui_entry != NULL && bz_result_get_resolved (self->ui_entry))
        {
          g_autoptr (GListStore) store = NULL;
          g_autoptr (DexFuture) future = NULL;
          BzEntry *entry               = NULL;

          entry = bz_result_get_object (self->ui_entry);
          store = g_list_store_new (BZ_TYPE_ENTRY);
          g_list_store_append (store, entry);

          future            = dex_future_new_for_object (store);
          self->group_model = bz_result_new (future);

          on_ui_entry_resolved (dex_future_new_for_object (entry), self);
        }
      else
        {
          g_autoptr (DexFuture) future = NULL;

          future            = bz_entry_group_dup_all_into_store (group);
          self->group_model = bz_result_new (future);

          if (self->ui_entry != NULL)
            {
              g_autoptr (DexFuture) ui_future = bz_result_dup_future (self->ui_entry);
              ui_future                       = dex_future_then (ui_future, on_ui_entry_resolved, g_object_ref (self), g_object_unref);
              dex_future_disown (g_steal_pointer (&ui_future));
            }
        }

      adw_view_stack_set_visible_child_name (self->stack, "content");
    }
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");

  grab_first_button (self);

  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->main_scroll)), 0.0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY_GROUP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UI_ENTRY]);
}

BzEntryGroup *
bz_full_view_get_entry_group (BzFullView *self)
{
  g_return_val_if_fail (BZ_IS_FULL_VIEW (self), NULL);
  return self->group;
}

static void
grab_first_button (BzFullView *self)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));

  if (gtk_widget_get_visible (self->wide_open_button))
    gtk_widget_grab_focus (self->wide_open_button);
  else if (gtk_widget_get_visible (self->wide_install_button))
    gtk_widget_grab_focus (self->wide_install_button);
  else if (gtk_widget_get_visible (self->narrow_install_button))
    gtk_widget_grab_focus (self->narrow_install_button);
  else if (gtk_widget_get_visible (self->narrow_open_button))
    gtk_widget_grab_focus (self->narrow_open_button);
}

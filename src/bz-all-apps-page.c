/* bz-all-apps-page.c
 *
 * Copyright 2025 Alexander Vanhee
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

#include "bz-all-apps-page.h"
#include "bz-app-tile.h"
#include "bz-entry-group.h"

struct _BzAllAppsPage
{
  AdwNavigationPage parent_instance;

  char       *title;
  GListModel *applications;

  GtkGridView *grid_view;
};

G_DEFINE_FINAL_TYPE (BzAllAppsPage, bz_all_apps_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,

  PROP_PAGE_TITLE,
  PROP_APPLICATIONS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SELECT,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
tile_clicked_cb (GtkListItem *list_item,
                 BzAppTile   *tile);

static void
bz_all_apps_page_dispose (GObject *object)
{
  BzAllAppsPage *self = BZ_ALL_APPS_PAGE (object);

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->applications);

  G_OBJECT_CLASS (bz_all_apps_page_parent_class)->dispose (object);
}

static void
bz_all_apps_page_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzAllAppsPage *self = BZ_ALL_APPS_PAGE (object);

  switch (prop_id)
    {
    case PROP_PAGE_TITLE:
      g_value_set_string (value, self->title);
      break;
    case PROP_APPLICATIONS:
      g_value_set_object (value, self->applications);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_all_apps_page_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzAllAppsPage *self = BZ_ALL_APPS_PAGE (object);

  switch (prop_id)
    {
    case PROP_PAGE_TITLE:
      g_clear_pointer (&self->title, g_free);
      self->title = g_value_dup_string (value);
      break;
    case PROP_APPLICATIONS:
      g_clear_object (&self->applications);
      self->applications = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_all_apps_page_class_init (BzAllAppsPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_all_apps_page_dispose;
  object_class->get_property = bz_all_apps_page_get_property;
  object_class->set_property = bz_all_apps_page_set_property;

  props[PROP_PAGE_TITLE] =
      g_param_spec_string (
          "page-title",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_APPLICATIONS] =
      g_param_spec_object (
          "applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SELECT] =
      g_signal_new (
          "select",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);

  g_type_ensure (BZ_TYPE_APP_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-all-apps-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzAllAppsPage, grid_view);
  gtk_widget_class_bind_template_callback (widget_class, tile_clicked_cb);
}

static void
bz_all_apps_page_init (BzAllAppsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwNavigationPage *
bz_all_apps_page_new (const char *title,
                      GListModel *applications)
{
  BzAllAppsPage     *apps_page       = NULL;
  GtkSelectionModel *selection_model = NULL;

  apps_page = g_object_new (
      BZ_TYPE_ALL_APPS_PAGE,
      "page-title", title,
      "applications", applications,
      NULL);

  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (apps_page), title);

  selection_model = GTK_SELECTION_MODEL (gtk_no_selection_new (applications));
  gtk_grid_view_set_model (apps_page->grid_view, selection_model);
  g_object_unref (selection_model);

  return ADW_NAVIGATION_PAGE (apps_page);
}

static void
tile_clicked_cb (GtkListItem *list_item,
                 BzAppTile   *tile)
{
  BzAllAppsPage *self  = NULL;
  BzEntryGroup  *group = NULL;

  g_assert (GTK_IS_LIST_ITEM (list_item));
  g_assert (BZ_IS_APP_TILE (tile));

  self = BZ_ALL_APPS_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (tile),
                                                    BZ_TYPE_ALL_APPS_PAGE));

  group = gtk_list_item_get_item (list_item);
  if (group == NULL)
    return;

  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}

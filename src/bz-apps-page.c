/* bz-apps-page.c
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

#include "bz-apps-page.h"
#include "bz-app-tile.h"
#include "bz-dynamic-list-view.h"

struct _BzAppsPage
{
  AdwNavigationPage parent_instance;

  char       *title;
  GListModel *applications;
  GListModel *carousel_applications;
  char       *subtitle;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzAppsPage, bz_apps_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,

  PROP_PAGE_TITLE,
  PROP_APPLICATIONS,
  PROP_CAROUSEL_APPLICATIONS,
  PROP_PAGE_SUBTITLE,

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
tile_clicked (BzEntryGroup *group,
              GtkButton    *button);

static void
bz_apps_page_dispose (GObject *object)
{
  BzAppsPage *self = BZ_APPS_PAGE (object);

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->applications);
  g_clear_object (&self->carousel_applications);
  g_clear_pointer (&self->subtitle, g_free);

  G_OBJECT_CLASS (bz_apps_page_parent_class)->dispose (object);
}

static void
bz_apps_page_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzAppsPage *self = BZ_APPS_PAGE (object);

  switch (prop_id)
    {
    case PROP_PAGE_TITLE:
      g_value_set_string (value, self->title);
      break;
    case PROP_APPLICATIONS:
      g_value_set_object (value, self->applications);
      break;
    case PROP_CAROUSEL_APPLICATIONS:
      g_value_set_object (value, self->carousel_applications);
      break;
    case PROP_PAGE_SUBTITLE:
      g_value_set_string (value, self->subtitle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_apps_page_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzAppsPage *self = BZ_APPS_PAGE (object);

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
    case PROP_CAROUSEL_APPLICATIONS:
      g_clear_object (&self->carousel_applications);
      self->carousel_applications = g_value_dup_object (value);
      break;
    case PROP_PAGE_SUBTITLE:
      g_clear_pointer (&self->subtitle, g_free);
      self->subtitle = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bind_widget_cb (BzAppsPage        *self,
                BzAppTile         *tile,
                BzEntryGroup      *group,
                BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzAppsPage        *self,
                  BzAppTile         *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static gboolean
is_not_null (gpointer object,
             GObject *value)
{
  return value != NULL;
}

static gboolean
is_not_empty_string (gpointer    object,
                     const char *str)
{
  return str != NULL && str[0] != '\0';
}

static void
featured_carousel_group_clicked_cb (BzAppsPage   *self,
                                    BzEntryGroup *group,
                                    GtkWidget    *carousel)
{
  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}

static void
bz_apps_page_class_init (BzAppsPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_apps_page_dispose;
  object_class->get_property = bz_apps_page_get_property;
  object_class->set_property = bz_apps_page_set_property;

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

  props[PROP_CAROUSEL_APPLICATIONS] =
      g_param_spec_object (
          "carousel-applications",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  props[PROP_PAGE_SUBTITLE] =
      g_param_spec_string (
          "page-subtitle",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SELECT] =
      g_signal_new (
          "select",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SELECT],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_APP_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-apps-page.ui");
  gtk_widget_class_bind_template_callback (widget_class, is_not_null);
  gtk_widget_class_bind_template_callback (widget_class, is_not_empty_string);
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, featured_carousel_group_clicked_cb);
}

static void
bz_apps_page_init (BzAppsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwNavigationPage *
bz_apps_page_new (const char *title,
                  GListModel *applications)
{
  return bz_apps_page_new_with_carousel (title, applications, NULL);
}

AdwNavigationPage *
bz_apps_page_new_with_carousel (const char *title,
                                GListModel *applications,
                                GListModel *carousel_applications)
{
  BzAppsPage *apps_page = NULL;

  apps_page = g_object_new (
      BZ_TYPE_APPS_PAGE,
      "page-title", title,
      "applications", applications,
      "carousel-applications", carousel_applications,
      NULL);

  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (apps_page), title);

  return ADW_NAVIGATION_PAGE (apps_page);
}

void
bz_apps_page_set_subtitle (BzAppsPage *self,
                           const char *subtitle)
{
  g_return_if_fail (BZ_IS_APPS_PAGE (self));

  if (g_strcmp0 (self->subtitle, subtitle) == 0)
    return;

  g_clear_pointer (&self->subtitle, g_free);
  if (subtitle != NULL)
    self->subtitle = g_strdup (subtitle);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAGE_SUBTITLE]);
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_APPS_PAGE);
  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}

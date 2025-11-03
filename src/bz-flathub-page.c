/* bz-flathub-page.c
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

#include "bz-flathub-page.h"
#include "bz-app-tile.h"
#include "bz-apps-page.h"
#include "bz-detailed-app-tile.h"
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-featured-carousel.h"
#include "bz-flathub-category-section.h"
#include "bz-flathub-category.h"
#include "bz-inhibited-scrollable.h"
#include "bz-patterned-background.h"
#include "bz-section-view.h"
#include "bz-window.h"
#include <glib/gi18n.h>

struct _BzFlathubPage
{
  AdwBin parent_instance;

  BzFlathubState *state;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzFlathubPage, bz_flathub_page, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_SELECTED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button);

static void
show_more_clicked (const char *title,
                   GListModel *model,
                   GtkButton  *button);

static void
apps_page_select_cb (BzFlathubPage *self,
                     BzEntryGroup  *group,
                     BzAppsPage    *page);

static void
category_section_group_selected_cb (BzFlathubPage            *self,
                                    BzEntryGroup             *group,
                                    BzFlathubCategorySection *section);

static void
featured_carousel_group_clicked_cb (BzFlathubPage      *self,
                                    BzEntryGroup       *group,
                                    BzFeaturedCarousel *carousel)
{
  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
bz_flathub_page_dispose (GObject *object)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_flathub_page_parent_class)->dispose (object);
}

static void
bz_flathub_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_flathub_page_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzFlathubPage *self = BZ_FLATHUB_PAGE (object);

  switch (prop_id)
    {
    case PROP_STATE:
      bz_flathub_page_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bind_widget_cb (BzFlathubPage     *self,
                BzAppTile         *tile,
                BzEntryGroup      *group,
                BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzFlathubPage     *self,
                  BzAppTile         *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static void
show_more_trending_clicked_cb (BzFlathubPage *self,
                               GtkButton     *button)
{
  g_autoptr (GListModel) model = NULL;

  model = bz_flathub_state_dup_trending (self->state);
  show_more_clicked (_ ("Trending"), model, button);
}

static void
show_more_recently_updated_clicked_cb (BzFlathubPage *self,
                                       GtkButton     *button)
{
  g_autoptr (GListModel) model = NULL;

  model = bz_flathub_state_dup_recently_updated (self->state);
  show_more_clicked (_ ("Recently Updated"), model, button);
}

static void
show_more_recently_added_clicked_cb (BzFlathubPage *self,
                                     GtkButton     *button)
{
  g_autoptr (GListModel) model = NULL;

  model = bz_flathub_state_dup_recently_added (self->state);
  show_more_clicked (_ ("Recently Added"), model, button);
}

static void
show_more_popular_clicked_cb (BzFlathubPage *self,
                              GtkButton     *button)
{
  g_autoptr (GListModel) model = NULL;

  model = bz_flathub_state_dup_popular (self->state);
  show_more_clicked (_ ("Popular"), model, button);
}

static void
show_more_mobile_clicked_cb (BzFlathubPage *self,
                               GtkButton     *button)
{
  g_autoptr (GListModel) model = NULL;

  model = bz_flathub_state_dup_mobile (self->state);
  show_more_clicked (_ ("Mobile Apps"), model, button);
}

static gpointer
get_category_by_name_cb (gpointer    object,
                         gpointer    categories_obj,
                         const char *name)
{
  GListModel *categories = G_LIST_MODEL (categories_obj);
  guint       n_items;
  guint       i;

  if (categories == NULL)
    return NULL;

  n_items = g_list_model_get_n_items (categories);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr (BzFlathubCategory) category = g_list_model_get_item (categories, i);
      const char *category_name;

      category_name = bz_flathub_category_get_name (category);

      if (g_strcmp0 (category_name, name) == 0)
        return g_object_ref (category);
    }

  return NULL;
}

static void
bz_flathub_page_class_init (BzFlathubPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_flathub_page_dispose;
  object_class->get_property = bz_flathub_page_get_property;
  object_class->set_property = bz_flathub_page_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_FLATHUB_STATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_SELECTED] =
      g_signal_new (
          "group-selected",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_GROUP_SELECTED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_FLATHUB_CATEGORY_SECTION);
  g_type_ensure (BZ_TYPE_PATTERNED_BACKGROUND);
  g_type_ensure (BZ_TYPE_DETAILED_APP_TILE);
  g_type_ensure (BZ_TYPE_INHIBITED_SCROLLABLE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_APP_TILE);
  g_type_ensure (BZ_TYPE_FEATURED_CAROUSEL);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-flathub-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzFlathubPage, stack);
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, category_section_group_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, get_category_by_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_more_trending_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_more_recently_updated_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_more_recently_added_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_more_popular_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_more_mobile_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, featured_carousel_group_clicked_cb);
}

static void
bz_flathub_page_init (BzFlathubPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_flathub_page_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_PAGE, NULL);
}

void
bz_flathub_page_set_state (BzFlathubPage  *self,
                           BzFlathubState *state)
{
  g_return_if_fail (BZ_IS_FLATHUB_PAGE (self));
  g_return_if_fail (state == NULL || BZ_IS_FLATHUB_STATE (state));

  g_clear_object (&self->state);
  if (state != NULL)
    {
      self->state = g_object_ref (state);
      adw_view_stack_set_visible_child_name (self->stack, "content");
    }
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

BzFlathubState *
bz_flathub_page_get_state (BzFlathubPage *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_PAGE (self), NULL);
  return self->state;
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_FLATHUB_PAGE);
  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
show_more_clicked (const char *title,
                   GListModel *model,
                   GtkButton  *button)
{
  GtkWidget         *self      = NULL;
  GtkWidget         *window    = NULL;
  GtkWidget         *nav_view  = NULL;
  AdwNavigationPage *apps_page = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_FLATHUB_PAGE);
  g_assert (self != NULL);

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

  nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  g_assert (nav_view != NULL);

  apps_page = bz_apps_page_new (title, model);

  g_signal_connect_swapped (
      apps_page, "select",
      G_CALLBACK (apps_page_select_cb), self);

  adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), apps_page);
}

static void
apps_page_select_cb (BzFlathubPage *self,
                     BzEntryGroup  *group,
                     BzAppsPage    *page)
{
  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
category_section_group_selected_cb (BzFlathubPage            *self,
                                    BzEntryGroup             *group,
                                    BzFlathubCategorySection *section)
{
  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

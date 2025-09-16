/* bz-category-page.c
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

#include "bz-category-page.h"
#include "bz-app-tile.h"
#include "bz-dynamic-list-view.h"

struct _BzCategoryPage
{
  AdwNavigationPage parent_instance;

  BzFlathubCategory *category;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzCategoryPage, bz_category_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,

  PROP_CATEGORY,

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
bz_category_page_dispose (GObject *object)
{
  BzCategoryPage *self = BZ_CATEGORY_PAGE (object);

  g_clear_object (&self->category);

  G_OBJECT_CLASS (bz_category_page_parent_class)->dispose (object);
}

static void
bz_category_page_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzCategoryPage *self = BZ_CATEGORY_PAGE (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_object (value, self->category);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_category_page_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzCategoryPage *self = BZ_CATEGORY_PAGE (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_clear_object (&self->category);
      self->category = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bind_widget_cb (BzFlathubCategory *self,
                BzAppTile         *tile,
                BzEntryGroup      *group,
                BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzFlathubCategory *self,
                  BzAppTile         *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static void
bz_category_page_class_init (BzCategoryPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_category_page_dispose;
  object_class->get_property = bz_category_page_get_property;
  object_class->set_property = bz_category_page_set_property;

  props[PROP_CATEGORY] =
      g_param_spec_object (
          "category",
          NULL, NULL,
          BZ_TYPE_FLATHUB_CATEGORY,
          G_PARAM_READWRITE);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-category-page.ui");
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
}

static void
bz_category_page_init (BzCategoryPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

AdwNavigationPage *
bz_category_page_new (BzFlathubCategory *category)
{
  BzCategoryPage *category_page = NULL;

  category_page = g_object_new (
      BZ_TYPE_CATEGORY_PAGE,
      "category", category,
      NULL);

  return ADW_NAVIGATION_PAGE (category_page);
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_CATEGORY_PAGE);
  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}
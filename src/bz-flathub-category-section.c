/* bz-flathub-category-section.c
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

#include "bz-flathub-category-section.h"
#include "bz-apps-page.h"
#include "bz-entry-group.h"
#include "bz-flathub-category.h"
#include "bz-flathub-page.h"
#include "bz-window.h"
#include <glib/gi18n.h>

struct _BzFlathubCategorySection
{
  GtkBox parent_instance;

  GtkLabel  *section_title;
  GtkWidget *section_list;
  GtkButton *more_button;

  BzFlathubCategory *category;
  guint              max_items;
  GtkSliceListModel *slice_model;
};

G_DEFINE_FINAL_TYPE (BzFlathubCategorySection, bz_flathub_category_section, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_CATEGORY,
  PROP_MAX_ITEMS,
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
apps_page_select_cb_forwarding (BzFlathubPage *flathub_page,
                                BzEntryGroup  *group,
                                BzAppsPage    *page)
{
  g_signal_emit_by_name (flathub_page, "group-selected", group);
}

static void
apps_page_hiding_cb_forwarding (BzFlathubPage *flathub_page,
                                BzAppsPage    *page)
{
  GtkWidget *window = NULL;

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (flathub_page)));

  if (window != NULL)
    bz_window_set_app_list_view_mode (BZ_WINDOW (window), FALSE);
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  BzFlathubCategorySection *self = NULL;

  self = BZ_FLATHUB_CATEGORY_SECTION (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_FLATHUB_CATEGORY_SECTION));

  if (self != NULL)
    g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
on_more_button_clicked (GtkButton                *button,
                        BzFlathubCategorySection *self)
{
  GtkWidget         *flathub_page       = NULL;
  GtkWidget         *window             = NULL;
  GtkWidget         *nav_view           = NULL;
  AdwNavigationPage *apps_page          = NULL;
  g_autoptr (GListModel) model          = NULL;
  g_autoptr (GListModel) carousel_model = NULL;
  const char      *title                = NULL;
  g_autofree char *subtitle             = NULL;
  int              total_entries        = 0;

  if (self->category == NULL)
    return;

  flathub_page = gtk_widget_get_ancestor (GTK_WIDGET (self), BZ_TYPE_FLATHUB_PAGE);
  if (flathub_page == NULL)
    return;

  window   = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));
  nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);

  if (nav_view == NULL)
    return;

  title          = bz_flathub_category_get_display_name (self->category);
  model          = bz_flathub_category_dup_applications (self->category);
  carousel_model = bz_flathub_category_dup_quality_applications (self->category);
  total_entries  = bz_flathub_category_get_total_entries (self->category);

  if (carousel_model != NULL && g_list_model_get_n_items (carousel_model) > 0)
    {
      apps_page = bz_apps_page_new_with_carousel (title, model, carousel_model);
    }
  else
    {
      apps_page = bz_apps_page_new (title, model);
    }

  if (total_entries > 0)
    {
      subtitle = g_strdup_printf (_ ("%d applications"), total_entries);
      bz_apps_page_set_subtitle (BZ_APPS_PAGE (apps_page), subtitle);
    }

  g_signal_connect_swapped (
      apps_page, "select",
      G_CALLBACK (apps_page_select_cb_forwarding), flathub_page);
  g_signal_connect_swapped (
      apps_page, "hiding",
      G_CALLBACK (apps_page_hiding_cb_forwarding), flathub_page);

  adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), apps_page);

  if (window != NULL)
    bz_window_set_app_list_view_mode (BZ_WINDOW (window), TRUE);
}

static void
bind_widget_cb (BzFlathubCategorySection *self,
                GtkWidget                *tile,
                BzEntryGroup             *group,
                GtkWidget                *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzFlathubCategorySection *self,
                  GtkWidget                *tile,
                  BzEntryGroup             *group,
                  GtkWidget                *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static void
update_model (BzFlathubCategorySection *self)
{
  GtkExpression *expression;

  if (self->category == NULL)
    return;

  if (self->slice_model != NULL)
    {
      gtk_slice_list_model_set_size (self->slice_model, self->max_items);
      return;
    }

  expression        = gtk_property_expression_new (BZ_TYPE_FLATHUB_CATEGORY, NULL, "applications");
  self->slice_model = gtk_slice_list_model_new (NULL, 0, self->max_items);

  gtk_expression_bind (expression, self->slice_model, "model", self->category);

  g_object_set (self->section_list, "model", self->slice_model, NULL);
}

static void
bz_flathub_category_section_dispose (GObject *object)
{
  BzFlathubCategorySection *self = BZ_FLATHUB_CATEGORY_SECTION (object);

  g_clear_object (&self->category);
  g_clear_object (&self->slice_model);

  G_OBJECT_CLASS (bz_flathub_category_section_parent_class)->dispose (object);
}

static void
bz_flathub_category_section_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  BzFlathubCategorySection *self = BZ_FLATHUB_CATEGORY_SECTION (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_object (value, bz_flathub_category_section_get_category (self));
      break;
    case PROP_MAX_ITEMS:
      g_value_set_uint (value, bz_flathub_category_section_get_max_items (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_section_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  BzFlathubCategorySection *self = BZ_FLATHUB_CATEGORY_SECTION (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      bz_flathub_category_section_set_category (self, g_value_get_object (value));
      break;
    case PROP_MAX_ITEMS:
      bz_flathub_category_section_set_max_items (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_flathub_category_section_class_init (BzFlathubCategorySectionClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_flathub_category_section_dispose;
  object_class->get_property = bz_flathub_category_section_get_property;
  object_class->set_property = bz_flathub_category_section_set_property;

  props[PROP_CATEGORY] =
      g_param_spec_object (
          "category",
          NULL, NULL,
          BZ_TYPE_FLATHUB_CATEGORY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MAX_ITEMS] =
      g_param_spec_uint (
          "max-items",
          NULL, NULL,
          1, G_MAXUINT, 12,
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

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-flathub-category-section.ui");

  gtk_widget_class_bind_template_child (widget_class, BzFlathubCategorySection, section_title);
  gtk_widget_class_bind_template_child (widget_class, BzFlathubCategorySection, section_list);
  gtk_widget_class_bind_template_child (widget_class, BzFlathubCategorySection, more_button);

  gtk_widget_class_bind_template_callback (widget_class, on_more_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
}

static void
bz_flathub_category_section_init (BzFlathubCategorySection *self)
{
  self->max_items = 12;

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_flathub_category_section_new (void)
{
  return g_object_new (BZ_TYPE_FLATHUB_CATEGORY_SECTION, NULL);
}

void
bz_flathub_category_section_set_category (BzFlathubCategorySection *self,
                                          BzFlathubCategory        *category)
{
  const char      *display_name;
  g_autofree char *more_label = NULL;

  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY_SECTION (self));
  g_return_if_fail (category == NULL || BZ_IS_FLATHUB_CATEGORY (category));

  if (self->category == category)
    return;

  g_clear_object (&self->category);
  g_clear_object (&self->slice_model);

  if (category != NULL)
    {
      self->category = g_object_ref (category);

      display_name = bz_flathub_category_get_display_name (category);
      gtk_label_set_text (self->section_title, display_name);

      more_label = g_strdup (bz_flathub_category_get_more_of_name (category));
      gtk_button_set_label (self->more_button, more_label);

      update_model (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CATEGORY]);
}

BzFlathubCategory *
bz_flathub_category_section_get_category (BzFlathubCategorySection *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY_SECTION (self), NULL);
  return self->category;
}

void
bz_flathub_category_section_set_max_items (BzFlathubCategorySection *self,
                                           guint                     max_items)
{
  g_return_if_fail (BZ_IS_FLATHUB_CATEGORY_SECTION (self));
  g_return_if_fail (max_items > 0);

  if (self->max_items == max_items)
    return;

  self->max_items = max_items;

  update_model (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAX_ITEMS]);
}

guint
bz_flathub_category_section_get_max_items (BzFlathubCategorySection *self)
{
  g_return_val_if_fail (BZ_IS_FLATHUB_CATEGORY_SECTION (self), 0);
  return self->max_items;
}

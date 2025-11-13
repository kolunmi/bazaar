/* bz-section-view.c
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

#include "bz-section-view.h"
#include "bz-async-texture.h"
#include "bz-content-section.h"
#include "bz-curated-app-tile.h"
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-markdown-render.h"

struct _BzSectionView
{
  AdwBin parent_instance;

  BzContentSection *section;
  GListModel       *classes;

  AdwStyleManager *style_manager;
  GListModel      *applied_classes;

  /* Template widgets */
  GtkOverlay *banner_text_overlay;
  GtkBox     *banner_text_bg;
  GtkBox     *banner_text;
};

G_DEFINE_FINAL_TYPE (BzSectionView, bz_section_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_SECTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_ACTIVATED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button);

static void
dark_changed (BzSectionView   *self,
              GParamSpec      *pspec,
              AdwStyleManager *mgr);

static void
refresh_dark_light_classes (BzSectionView   *self,
                            AdwStyleManager *mgr);

static void
bz_section_view_dispose (GObject *object)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  g_signal_handlers_disconnect_by_func (
      self->style_manager, dark_changed, self);

  g_clear_object (&self->section);
  g_clear_object (&self->classes);
  g_clear_object (&self->style_manager);
  g_clear_object (&self->applied_classes);

  G_OBJECT_CLASS (bz_section_view_parent_class)->dispose (object);
}

static void
bz_section_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      g_value_set_object (value, bz_section_view_get_section (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_section_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzSectionView *self = BZ_SECTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_SECTION:
      bz_section_view_set_section (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static BzAsyncTexture *
get_banner (BzSectionView *self,
            GdkPaintable  *value)
{
  gboolean    is_dark                = FALSE;
  const char *uri                    = NULL;
  g_autoptr (GFile) source           = NULL;
  g_autoptr (GdkPaintable) paintable = NULL;

  if (self->section == NULL)
    return NULL;

  is_dark = adw_style_manager_get_dark (self->style_manager);
  if (is_dark)
    uri = bz_content_section_get_dark_banner (self->section);
  else
    uri = bz_content_section_get_light_banner (self->section);
  if (uri == NULL)
    uri = bz_content_section_get_banner (self->section);
  if (uri == NULL)
    return NULL;

  source = g_file_new_for_uri (uri);
  return bz_async_texture_new_lazy (source, NULL);
}

static void
bind_widget_cb (BzSectionView     *self,
                BzCuratedAppTile  *tile,
                BzEntryGroup      *group,
                BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (tile_clicked), group);
}

static void
unbind_widget_cb (BzSectionView     *self,
                  BzCuratedAppTile  *tile,
                  BzEntryGroup      *group,
                  BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, G_CALLBACK (tile_clicked), group);
}

static void
bz_section_view_class_init (BzSectionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_section_view_dispose;
  object_class->get_property = bz_section_view_get_property;
  object_class->set_property = bz_section_view_set_property;

  props[PROP_SECTION] =
      g_param_spec_object (
          "section",
          NULL, NULL,
          BZ_TYPE_CONTENT_SECTION,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_ACTIVATED] =
      g_signal_new (
          "group-activated",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY_GROUP);
  g_signal_set_va_marshaller (
      signals[SIGNAL_GROUP_ACTIVATED],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_CURATED_APP_TILE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_MARKDOWN_RENDER);
  g_type_ensure (BZ_TYPE_ASYNC_TEXTURE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-section-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text_bg);
  gtk_widget_class_bind_template_child (widget_class, BzSectionView, banner_text);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, get_banner);
  gtk_widget_class_bind_template_callback (widget_class, bind_widget_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_widget_cb);
}

static void
dark_changed (BzSectionView   *self,
              GParamSpec      *pspec,
              AdwStyleManager *mgr)
{
  refresh_dark_light_classes (self, mgr);

  if (self->section != NULL)
    g_object_notify (G_OBJECT (self->section), "banner");
}

static void
bz_section_view_init (BzSectionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_overlay_set_measure_overlay (
      self->banner_text_overlay,
      GTK_WIDGET (self->banner_text),
      TRUE);
  gtk_overlay_set_clip_overlay (
      self->banner_text_overlay,
      GTK_WIDGET (self->banner_text),
      TRUE);

  self->style_manager = g_object_ref (
      adw_style_manager_get_default ());
  g_signal_connect_swapped (
      self->style_manager,
      "notify::dark",
      G_CALLBACK (dark_changed),
      self);
}

GtkWidget *
bz_section_view_new (BzContentSection *section)
{
  return g_object_new (
      BZ_TYPE_SECTION_VIEW,
      "section", section,
      NULL);
}

void
bz_section_view_set_section (BzSectionView    *self,
                             BzContentSection *section)
{
  g_return_if_fail (BZ_IS_SECTION_VIEW (self));
  g_return_if_fail (section == NULL || BZ_IS_CONTENT_SECTION (section));

  g_clear_object (&self->section);

  if (self->classes != NULL)
    {
      guint n_classes = 0;

      n_classes = g_list_model_get_n_items (self->classes);
      for (guint i = 0; i < n_classes; i++)
        {
          g_autoptr (GtkStringObject) string = NULL;
          const char *class                  = NULL;

          string = g_list_model_get_item (self->classes, i);
          class  = gtk_string_object_get_string (string);

          gtk_widget_remove_css_class (GTK_WIDGET (self), class);
        }
    }
  g_clear_object (&self->classes);

  if (section != NULL)
    {
      self->section = g_object_ref (section);
      g_object_get (section, "classes", &self->classes, NULL);

      if (self->classes != NULL)
        {
          guint n_classes = 0;

          n_classes = g_list_model_get_n_items (self->classes);
          for (guint i = 0; i < n_classes; i++)
            {
              g_autoptr (GtkStringObject) string = NULL;
              const char *class                  = NULL;

              string = g_list_model_get_item (self->classes, i);
              class  = gtk_string_object_get_string (string);

              gtk_widget_add_css_class (GTK_WIDGET (self), class);
            }
        }

      refresh_dark_light_classes (self, NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SECTION]);
}

BzContentSection *
bz_section_view_get_section (BzSectionView *self)
{
  g_return_val_if_fail (BZ_IS_SECTION_VIEW (self), NULL);
  return self->section;
}

static void
tile_clicked (BzEntryGroup *group,
              GtkButton    *button)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_SECTION_VIEW);
  g_signal_emit (self, signals[SIGNAL_GROUP_ACTIVATED], 0, group);
}

static void
refresh_dark_light_classes (BzSectionView   *self,
                            AdwStyleManager *mgr)
{
  if (self->applied_classes != NULL)
    {
      guint n_applied_classes = 0;

      n_applied_classes = g_list_model_get_n_items (self->applied_classes);
      for (guint i = 0; i < n_applied_classes; i++)
        {
          g_autoptr (GtkStringObject) string = NULL;
          const char *class                  = NULL;

          string = g_list_model_get_item (self->applied_classes, i);
          class  = gtk_string_object_get_string (string);

          gtk_widget_remove_css_class (GTK_WIDGET (self), class);
        }
    }
  g_clear_object (&self->applied_classes);

  if (self->section == NULL)
    return;

  if (mgr == NULL)
    mgr = adw_style_manager_get_default ();

  if (adw_style_manager_get_dark (mgr))
    g_object_get (self->section, "dark-classes", &self->applied_classes, NULL);
  else
    g_object_get (self->section, "light-classes", &self->applied_classes, NULL);

  if (self->applied_classes != NULL)
    {
      guint n_classes = 0;

      n_classes = g_list_model_get_n_items (self->applied_classes);
      for (guint i = 0; i < n_classes; i++)
        {
          g_autoptr (GtkStringObject) string = NULL;
          const char *class                  = NULL;

          string = g_list_model_get_item (self->applied_classes, i);
          class  = gtk_string_object_get_string (string);

          gtk_widget_add_css_class (GTK_WIDGET (self), class);
        }
    }
}

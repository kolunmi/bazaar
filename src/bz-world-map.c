/* bz-world-map.c
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

#include <glib/gi18n.h>
#include "config.h"

#include <adwaita.h>
#include <math.h>

#include "bz-world-map.h"
#include "bz-world-map-parser.h"
#include "bz-country.h"
#include "bz-country-data-point.h"

#define CARD_EDGE_THRESHOLD 160
#define OPACITY_MULTIPLIER  2

struct _BzWorldMap
{
  GtkWidget parent_instance;

  BzWorldMapParser *parser;
  GListModel       *countries;
  GListModel       *model;

  double min_lon;
  double max_lon;
  double min_lat;
  double max_lat;

  GskPath **country_paths;
  guint    *path_to_country;
  guint     n_paths;

  gboolean cache_valid;

  GtkEventController *motion;
  double              offset_x;
  double              offset_y;
  double              scale;
  int                 hovered_country;
  double              motion_x;
  double              motion_y;

  guint max_downloads;
};

G_DEFINE_FINAL_TYPE (BzWorldMap, bz_world_map, GTK_TYPE_WIDGET)

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static guint
get_downloads_for_country (BzWorldMap *self,
                           const char *iso_code)
{
  guint n_items = 0;

  if (self->model == NULL)
    return 0;

  n_items = g_list_model_get_n_items (self->model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzCountryDataPoint) point = g_list_model_get_item (self->model, i);
      const char *country_code = bz_country_data_point_get_country_code (point);

      if (g_strcmp0 (country_code, iso_code) == 0)
        return bz_country_data_point_get_downloads (point);
    }

  return 0;
}

static void
calculate_max_downloads (BzWorldMap *self)
{
  guint n_items = 0;

  self->max_downloads = 0;

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (self->model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzCountryDataPoint) point = g_list_model_get_item (self->model, i);
      guint downloads = bz_country_data_point_get_downloads (point);

      if (downloads > self->max_downloads)
        self->max_downloads = downloads;
    }
}

static void
calculate_bounds (BzWorldMap *self)
{
  guint n_items = 0;

  if (self->countries == NULL)
    return;

  n_items = g_list_model_get_n_items (self->countries);

  self->min_lon = 180.0;
  self->max_lon = -180.0;
  self->min_lat = 90.0;
  self->max_lat = -90.0;

  for (guint i = 0; i < n_items; i++)
    {
      BzCountry *country = g_list_model_get_item (self->countries, i);
      JsonArray *coordinates = bz_country_get_coordinates (country);

      if (coordinates != NULL)
        {
          for (guint j = 0; j < json_array_get_length (coordinates); j++)
            {
              JsonArray *polygon_array = json_array_get_array_element (coordinates, j);

              for (guint k = 0; k < json_array_get_length (polygon_array); k++)
                {
                  JsonArray *ring_array = json_array_get_array_element (polygon_array, k);

                  for (guint l = 0; l < json_array_get_length (ring_array); l++)
                    {
                      JsonArray *point_array = json_array_get_array_element (ring_array, l);
                      double lon = json_array_get_double_element (point_array, 0);
                      double lat = json_array_get_double_element (point_array, 1);

                      if (lon < self->min_lon)
                        self->min_lon = lon;
                      if (lon > self->max_lon)
                        self->max_lon = lon;
                      if (lat < self->min_lat)
                        self->min_lat = lat;
                      if (lat > self->max_lat)
                        self->max_lat = lat;
                    }
                }
            }
        }

      g_object_unref (country);
    }
}

static void
project_point (BzWorldMap *self,
               double      lon,
               double      lat,
               double      width,
               double      height,
               double     *x,
               double     *y)
{
  double lon_range = self->max_lon - self->min_lon;
  double lat_range = self->max_lat - self->min_lat;

  *x = ((lon - self->min_lon) / lon_range) * width;
  *y = height - ((lat - self->min_lat) / lat_range) * height;
}

static void
calculate_transform (BzWorldMap *self,
                     double      widget_width,
                     double      widget_height,
                     double      map_width,
                     double      map_height)
{
  double scale_x = widget_width / map_width;
  double scale_y = widget_height / map_height;

  self->scale = MIN (scale_x, scale_y);

  self->offset_x = (widget_width - map_width * self->scale) / 2.0;
  self->offset_y = (widget_height - map_height * self->scale) / 2.0;
}

static void
build_paths (BzWorldMap *self,
             double      width,
             double      height)
{
  guint n_items = 0;
  guint path_index = 0;

  if (self->countries == NULL)
    return;

  if (self->country_paths != NULL)
    {
      for (guint i = 0; i < self->n_paths; i++)
        g_clear_pointer (&self->country_paths[i], gsk_path_unref);
      g_free (self->country_paths);
      self->country_paths = NULL;
    }

  if (self->path_to_country != NULL)
    {
      g_free (self->path_to_country);
      self->path_to_country = NULL;
    }

  n_items = g_list_model_get_n_items (self->countries);

  self->n_paths = 0;
  for (guint i = 0; i < n_items; i++)
    {
      BzCountry *country = g_list_model_get_item (self->countries, i);
      JsonArray *coordinates = bz_country_get_coordinates (country);

      if (coordinates != NULL)
        {
          for (guint j = 0; j < json_array_get_length (coordinates); j++)
            {
              JsonArray *polygon_array = json_array_get_array_element (coordinates, j);
              self->n_paths += json_array_get_length (polygon_array);
            }
        }

      g_object_unref (country);
    }

  self->country_paths = g_new0 (GskPath *, self->n_paths);
  self->path_to_country = g_new0 (guint, self->n_paths);

  for (guint i = 0; i < n_items; i++)
    {
      BzCountry *country = g_list_model_get_item (self->countries, i);
      JsonArray *coordinates = bz_country_get_coordinates (country);

      if (coordinates != NULL)
        {
          for (guint j = 0; j < json_array_get_length (coordinates); j++)
            {
              JsonArray *polygon_array = json_array_get_array_element (coordinates, j);

              for (guint k = 0; k < json_array_get_length (polygon_array); k++)
                {
                  JsonArray *ring_array = json_array_get_array_element (polygon_array, k);
                  g_autoptr (GskPathBuilder) builder = gsk_path_builder_new ();
                  gboolean first = TRUE;

                  for (guint l = 0; l < json_array_get_length (ring_array); l++)
                    {
                      JsonArray *point_array = json_array_get_array_element (ring_array, l);
                      double lon = json_array_get_double_element (point_array, 0);
                      double lat = json_array_get_double_element (point_array, 1);
                      double x = 0.0;
                      double y = 0.0;

                      project_point (self, lon, lat, width, height, &x, &y);

                      if (first)
                        {
                          gsk_path_builder_move_to (builder, x, y);
                          first = FALSE;
                        }
                      else
                        {
                          gsk_path_builder_line_to (builder, x, y);
                        }
                    }

                  gsk_path_builder_close (builder);
                  self->country_paths[path_index] = gsk_path_builder_to_path (builder);
                  self->path_to_country[path_index] = i;
                  path_index++;
                }
            }
        }

      g_object_unref (country);
    }

  self->cache_valid = TRUE;
}

static void
invalidate_cache (BzWorldMap *self)
{
  self->cache_valid = FALSE;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
on_style_changed (AdwStyleManager *style_manager,
                  GParamSpec      *pspec,
                  BzWorldMap      *self)
{
  invalidate_cache (self);
}

static void
motion_event (BzWorldMap              *self,
              gdouble                  x,
              gdouble                  y,
              GtkEventControllerMotion *controller)
{
  double map_x = (x - self->offset_x) / self->scale;
  double map_y = (y - self->offset_y) / self->scale;
  int old_hovered = self->hovered_country;

  self->motion_x = x;
  self->motion_y = y;
  self->hovered_country = -1;

  for (guint i = 0; i < self->n_paths; i++)
    {
      if (gsk_path_in_fill (self->country_paths[i],
                           &GRAPHENE_POINT_INIT (map_x, map_y),
                           GSK_FILL_RULE_WINDING))
        {
          self->hovered_country = self->path_to_country[i];
          break;
        }
    }

  if (old_hovered != self->hovered_country || self->hovered_country >= 0)
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
motion_leave (BzWorldMap              *self,
              GtkEventControllerMotion *controller)
{
  if (self->hovered_country != -1)
    {
      self->hovered_country = -1;
      self->motion_x = -1.0;
      self->motion_y = -1.0;
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
bz_world_map_dispose (GObject *object)
{
  BzWorldMap *self = BZ_WORLD_MAP (object);

  g_signal_handlers_disconnect_by_func (adw_style_manager_get_default (),
                                        on_style_changed,
                                        self);

  if (self->country_paths != NULL)
    {
      for (guint i = 0; i < self->n_paths; i++)
        g_clear_pointer (&self->country_paths[i], gsk_path_unref);
      g_free (self->country_paths);
      self->country_paths = NULL;
    }

  if (self->path_to_country != NULL)
    {
      g_free (self->path_to_country);
      self->path_to_country = NULL;
    }

  g_clear_object (&self->countries);
  g_clear_object (&self->model);
  g_clear_object (&self->parser);

  G_OBJECT_CLASS (bz_world_map_parent_class)->dispose (object);
}

static void
bz_world_map_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzWorldMap *self = BZ_WORLD_MAP (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_world_map_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzWorldMap *self = BZ_WORLD_MAP (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_clear_object (&self->model);
      self->model = g_value_dup_object (value);
      calculate_max_downloads (self);
      gtk_widget_queue_draw (GTK_WIDGET (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_world_map_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  BzWorldMap *self = BZ_WORLD_MAP (widget);

  invalidate_cache (self);
}

static void
bz_world_map_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  BzWorldMap              *self          = BZ_WORLD_MAP (widget);
  double                   widget_width  = gtk_widget_get_width (widget);
  double                   widget_height = gtk_widget_get_height (widget);
  AdwStyleManager         *style_manager = adw_style_manager_get_default ();
  g_autoptr (GdkRGBA)      accent_color  = adw_style_manager_get_accent_color_rgba (style_manager);
  GdkRGBA                  stroke_color  = { 0 };
  g_autoptr (GskStroke)    stroke        = gsk_stroke_new (0.5);
  g_autoptr (GskStroke)    hover_stroke  = gsk_stroke_new (1.5);
  double                   map_width     = 1000.0;
  double                   map_height    = 500.0;

  if (self->countries == NULL)
    return;

  gtk_widget_get_color (widget, &stroke_color);
  stroke_color.alpha = 0.3;

  if (!self->cache_valid)
    build_paths (self, map_width, map_height);

  calculate_transform (self, widget_width, widget_height, map_width, map_height);

  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (self->offset_x, self->offset_y));
  gtk_snapshot_scale (snapshot, self->scale, self->scale);

  for (guint i = 0; i < self->n_paths; i++)
    {
      guint country_idx = self->path_to_country[i];
      g_autoptr (BzCountry) country = g_list_model_get_item (self->countries, country_idx);
      const char *iso_code = bz_country_get_iso_code (country);
      guint downloads = get_downloads_for_country (self, iso_code);
      GdkRGBA fill_color = *accent_color;

      if (self->max_downloads > 0 && downloads > 0)
        {
          double ratio = (double) downloads / (double) self->max_downloads;
          fill_color.alpha = CLAMP (ratio * OPACITY_MULTIPLIER, 0.1, 1.0);
        }
      else
        {
          fill_color.alpha = 0.0;
        }

      gtk_snapshot_append_fill (snapshot, self->country_paths[i], GSK_FILL_RULE_WINDING, &fill_color);
      gtk_snapshot_append_stroke (snapshot, self->country_paths[i], stroke, &stroke_color);
    }

  gtk_snapshot_restore (snapshot);

  if (self->hovered_country >= 0)
    {
      GdkRGBA hover_color = stroke_color;
      hover_color.alpha = 1.0;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (self->offset_x, self->offset_y));
      gtk_snapshot_scale (snapshot, self->scale, self->scale);

      for (guint i = 0; i < self->n_paths; i++)
        {
          if (self->path_to_country[i] == (guint) self->hovered_country)
            {
              gtk_snapshot_append_stroke (snapshot, self->country_paths[i], hover_stroke, &hover_color);
            }
        }

      gtk_snapshot_restore (snapshot);
    }

  if (self->hovered_country >= 0 && self->motion_x >= 0.0 && self->motion_y >= 0.0)
    {
      g_autoptr (BzCountry) country        = g_list_model_get_item (self->countries, self->hovered_country);
      const char           *iso_code       = bz_country_get_iso_code (country);
      guint                 download_number = get_downloads_for_country (self, iso_code);
      const char *country_name              = bz_country_get_name (country);
      g_autofree char *card_text            = g_strdup_printf (_("%s: %u downloads"), country_name, download_number);
      g_autoptr (PangoLayout) layout       = pango_layout_new (gtk_widget_get_pango_context (widget));
      PangoRectangle        text_extents   = { 0 };
      double                card_width     = 0.0;
      double                card_height    = 0.0;
      double                card_x         = 0.0;
      double                card_y         = 0.0;
      GskRoundedRect        text_bg_rect   = { { { 0 } } };
      GdkRGBA               text_bg_color  = { 0 };
      GdkRGBA               shadow_color   = { 0 };
      GdkRGBA               text_color     = { 0 };

      pango_layout_set_text (layout, card_text, -1);
      pango_layout_get_pixel_extents (layout, NULL, &text_extents);

      card_width  = text_extents.width + 16.0;
      card_height = text_extents.height + 16.0;

      if (widget_width - self->motion_x < CARD_EDGE_THRESHOLD)
        card_x = self->motion_x - card_width - 10.0;
      else
        card_x = self->motion_x + 10.0;
      card_y = self->motion_y + 10.0;

      gtk_widget_get_color (widget, &text_color);

      if (adw_style_manager_get_dark (style_manager))
        {
          text_bg_color = (GdkRGBA) { 0.18, 0.18, 0.2, 1.0 };
          shadow_color  = (GdkRGBA) { 0.0, 0.0, 0.06, 0.20 };
        }
      else
        {
          text_bg_color = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
          shadow_color  = (GdkRGBA) { 0.0, 0.0, 0.0, 0.20 };
        }

      gsk_rounded_rect_init_from_rect (
          &text_bg_rect,
          &GRAPHENE_RECT_INIT (card_x, card_y, card_width, card_height),
          6.0);

      gtk_snapshot_append_outset_shadow (
          snapshot,
          &text_bg_rect,
          &shadow_color,
          0.0,
          0.0,
          1.0,
          3.0);

      gtk_snapshot_push_rounded_clip (snapshot, &text_bg_rect);
      gtk_snapshot_append_color (snapshot, &text_bg_color, &text_bg_rect.bounds);
      gtk_snapshot_pop (snapshot);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (
          snapshot,
          &GRAPHENE_POINT_INIT (card_x + 8.0, card_y + 8.0));
      gtk_snapshot_append_layout (snapshot, layout, &text_color);
      gtk_snapshot_restore (snapshot);
    }
}

static void
bz_world_map_class_init (BzWorldMapClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_world_map_dispose;
  object_class->get_property = bz_world_map_get_property;
  object_class->set_property = bz_world_map_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->snapshot      = bz_world_map_snapshot;
  widget_class->size_allocate = bz_world_map_size_allocate;
}

static void
bz_world_map_init (BzWorldMap *self)
{
  AdwStyleManager    *style_manager = adw_style_manager_get_default ();
  g_autoptr (GError)  error         = NULL;

  self->parser          = bz_world_map_parser_new ();
  self->hovered_country = -1;
  self->motion_x        = -1.0;
  self->motion_y        = -1.0;
  self->max_downloads   = 0;

  self->motion = gtk_event_controller_motion_new ();
  g_signal_connect_swapped (self->motion, "motion", G_CALLBACK (motion_event), self);
  g_signal_connect_swapped (self->motion, "leave", G_CALLBACK (motion_leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->motion);

  g_signal_connect (style_manager, "notify::dark",
                    G_CALLBACK (on_style_changed), self);
  g_signal_connect (style_manager, "notify::accent-color",
                    G_CALLBACK (on_style_changed), self);

  if (bz_world_map_parser_load_from_resource (self->parser,
                                               "/io/github/kolunmi/Bazaar/countries.json",
                                               &error))
    {
      self->countries = g_object_ref (bz_world_map_parser_get_countries (self->parser));
      calculate_bounds (self);
    }
  else
    {
      g_warning ("BzWorldMap: Failed to load countries: %s", error->message);
    }
}

GtkWidget *
bz_world_map_new (void)
{
  return g_object_new (BZ_TYPE_WORLD_MAP, NULL);
}

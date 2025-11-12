/* bz-featured-carousel.c
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

#include "bz-featured-carousel.h"
#include "bz-entry-group.h"
#include "bz-featured-tile.h"

#define FEATURED_ROTATE_TIME 5

struct _BzFeaturedCarousel
{
  GtkBox parent_instance;

  GListModel   *model;
  gboolean      is_aotd;
  guint         rotation_timer_id;
  unsigned long settings_notify_id;

  AdwCarousel              *carousel;
  GtkButton                *next_button;
  GtkButton                *previous_button;
  AdwCarouselIndicatorDots *dots;
};

G_DEFINE_FINAL_TYPE (BzFeaturedCarousel, bz_featured_carousel, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_IS_AOTD,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = {
  NULL,
};

enum
{
  SIGNAL_GROUP_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {
  0,
};

static void
show_relative_page (BzFeaturedCarousel *self,
                    gint                delta,
                    gboolean            use_custom_spring)
{
  gdouble    current_page;
  guint      n_pages;
  guint      new_page;
  GtkWidget *new_page_widget;
  gboolean   animate;

  current_page = adw_carousel_get_position (self->carousel);
  n_pages      = adw_carousel_get_n_pages (self->carousel);
  animate      = TRUE;

  if (n_pages == 0)
    return;

  new_page        = ((guint) current_page + delta + n_pages) % n_pages;
  new_page_widget = adw_carousel_get_nth_page (self->carousel, new_page);
  g_assert (new_page_widget != NULL);

  if ((new_page == 0 && delta > 0) || (new_page == n_pages - 1 && delta < 0))
    animate = FALSE;

  if (!adw_get_enable_animations (GTK_WIDGET (self)))
    animate = FALSE;

  if (use_custom_spring)
    {
      g_autoptr (AdwSpringParams) spring_params = NULL;
      spring_params                             = adw_spring_params_new (0.90, 1.65, 100.0);
      adw_carousel_set_scroll_params (self->carousel, spring_params);
    }
  else
    {
      g_autoptr (AdwSpringParams) spring_params = NULL;
      spring_params                             = adw_spring_params_new (1, 0.5, 500);
      adw_carousel_set_scroll_params (self->carousel, spring_params);
    }

  adw_carousel_scroll_to (self->carousel, new_page_widget, animate);
}

static gboolean
rotate_cb (gpointer user_data)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (user_data);
  show_relative_page (self, +1, TRUE);

  return G_SOURCE_CONTINUE;
}

static void
start_rotation_timer (BzFeaturedCarousel *self)
{
  if (self->rotation_timer_id == 0)
    {
      self->rotation_timer_id = g_timeout_add_seconds (FEATURED_ROTATE_TIME,
                                                       rotate_cb, self);
    }
}

static void
stop_rotation_timer (BzFeaturedCarousel *self)
{
  if (self->rotation_timer_id != 0)
    {
      g_source_remove (self->rotation_timer_id);
      self->rotation_timer_id = 0;
    }
}

static void
maybe_start_rotation_timer (BzFeaturedCarousel *self)
{
  if (!adw_get_enable_animations (GTK_WIDGET (self)))
    {
      stop_rotation_timer (self);
      return;
    }

  if (self->model != NULL && g_list_model_get_n_items (self->model) > 0 &&
      gtk_widget_get_mapped (GTK_WIDGET (self)))
    start_rotation_timer (self);
}

static void
carousel_notify_position_cb (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (user_data);
  stop_rotation_timer (self);
  maybe_start_rotation_timer (self);
}

static void
carousel_notify_settings_cb (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (user_data);
  maybe_start_rotation_timer (self);
}

static void
next_button_clicked_cb (GtkButton *button,
                        gpointer   user_data)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (user_data);
  show_relative_page (self, +1, FALSE);
}

static void
previous_button_clicked_cb (GtkButton *button,
                            gpointer   user_data)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (user_data);
  show_relative_page (self, -1, FALSE);
}

static void
tile_clicked_cb (BzFeaturedTile *tile,
                 gpointer        user_data)
{
  BzFeaturedCarousel *self;
  BzEntryGroup       *group;

  self  = BZ_FEATURED_CAROUSEL (user_data);
  group = bz_featured_tile_get_group (tile);
  g_signal_emit (self, signals[SIGNAL_GROUP_CLICKED], 0, group);
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                BzFeaturedCarousel    *self)
{
  if (gtk_widget_is_visible (GTK_WIDGET (self->previous_button)) &&
      gtk_widget_is_sensitive (GTK_WIDGET (self->previous_button)) &&
      ((gtk_widget_get_direction (GTK_WIDGET (self->previous_button)) == GTK_TEXT_DIR_LTR && keyval == GDK_KEY_Left) ||
       (gtk_widget_get_direction (GTK_WIDGET (self->previous_button)) == GTK_TEXT_DIR_RTL && keyval == GDK_KEY_Right)))
    {
      gtk_widget_activate (GTK_WIDGET (self->previous_button));
      return GDK_EVENT_STOP;
    }

  if (gtk_widget_is_visible (GTK_WIDGET (self->next_button)) &&
      gtk_widget_is_sensitive (GTK_WIDGET (self->next_button)) &&
      ((gtk_widget_get_direction (GTK_WIDGET (self->next_button)) == GTK_TEXT_DIR_LTR && keyval == GDK_KEY_Right) ||
       (gtk_widget_get_direction (GTK_WIDGET (self->next_button)) == GTK_TEXT_DIR_RTL && keyval == GDK_KEY_Left)))
    {
      gtk_widget_activate (GTK_WIDGET (self->next_button));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
rebuild_carousel (BzFeaturedCarousel *self)
{
  guint n_items;

  stop_rotation_timer (self);

  while (adw_carousel_get_n_pages (self->carousel) > 0)
    adw_carousel_remove (self->carousel, adw_carousel_get_nth_page (self->carousel, 0));

  if (self->model == NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->next_button), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->previous_button), FALSE);
      return;
    }

  n_items = g_list_model_get_n_items (self->model);
  gtk_widget_set_visible (GTK_WIDGET (self), n_items > 0);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntryGroup) group = NULL;
      BzFeaturedTile *tile;

      group = g_list_model_get_item (self->model, i);
      tile  = bz_featured_tile_new (group);

      bz_featured_tile_set_is_aotd (tile, self->is_aotd && (i == 0));

      gtk_widget_set_hexpand (GTK_WIDGET (tile), TRUE);
      gtk_widget_set_vexpand (GTK_WIDGET (tile), TRUE);
      gtk_widget_set_can_focus (GTK_WIDGET (tile), FALSE);

      g_signal_connect (tile, "clicked",
                        G_CALLBACK (tile_clicked_cb), self);

      adw_carousel_append (self->carousel, GTK_WIDGET (tile));
    }

  gtk_widget_set_visible (GTK_WIDGET (self->next_button), n_items > 1);
  gtk_widget_set_visible (GTK_WIDGET (self->previous_button), n_items > 1);

  maybe_start_rotation_timer (self);
}

static void
model_items_changed_cb (BzFeaturedCarousel *self,
                        guint               position,
                        guint               removed,
                        guint               added,
                        GListModel         *model)
{
  rebuild_carousel (self);
}

static void
bz_featured_carousel_map (GtkWidget *widget)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (widget);

  GTK_WIDGET_CLASS (bz_featured_carousel_parent_class)->map (widget);

  maybe_start_rotation_timer (self);
}

static void
bz_featured_carousel_unmap (GtkWidget *widget)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (widget);

  stop_rotation_timer (self);

  GTK_WIDGET_CLASS (bz_featured_carousel_parent_class)->unmap (widget);
}

static void
bz_featured_carousel_dispose (GObject *object)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, model_items_changed_cb, self);

  stop_rotation_timer (self);
  g_clear_signal_handler (&self->settings_notify_id, gtk_widget_get_settings (GTK_WIDGET (self)));
  g_clear_object (&self->model);

  G_OBJECT_CLASS (bz_featured_carousel_parent_class)->dispose (object);
}

static void
bz_featured_carousel_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_featured_carousel_get_model (self));
      break;
    case PROP_IS_AOTD:
      g_value_set_boolean (value, bz_featured_carousel_get_is_aotd (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
bz_featured_carousel_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzFeaturedCarousel *self;

  self = BZ_FEATURED_CAROUSEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_featured_carousel_set_model (self, g_value_get_object (value));
      break;
    case PROP_IS_AOTD:
      bz_featured_carousel_set_is_aotd (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
bz_featured_carousel_class_init (BzFeaturedCarouselClass *klass)
{
  GObjectClass   *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = bz_featured_carousel_get_property;
  object_class->set_property = bz_featured_carousel_set_property;
  object_class->dispose      = bz_featured_carousel_dispose;

  widget_class->map   = bz_featured_carousel_map;
  widget_class->unmap = bz_featured_carousel_unmap;

  props[PROP_MODEL] =
      g_param_spec_object ("model", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_IS_AOTD] =
      g_param_spec_boolean ("is-aotd", NULL, NULL,
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_GROUP_CLICKED] =
      g_signal_new ("group-clicked",
                    G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 1, BZ_TYPE_ENTRY_GROUP);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-featured-carousel.ui");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  gtk_widget_class_bind_template_child (widget_class, BzFeaturedCarousel, carousel);
  gtk_widget_class_bind_template_child (widget_class, BzFeaturedCarousel, next_button);
  gtk_widget_class_bind_template_child (widget_class, BzFeaturedCarousel, previous_button);
  gtk_widget_class_bind_template_child (widget_class, BzFeaturedCarousel, dots);

  gtk_widget_class_bind_template_callback (widget_class, carousel_notify_position_cb);
  gtk_widget_class_bind_template_callback (widget_class, next_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, previous_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, key_pressed_cb);
}

static void
bz_featured_carousel_init (BzFeaturedCarousel *self)
{
  GtkSettings *settings;

  gtk_widget_init_template (GTK_WIDGET (self));

  adw_carousel_set_allow_scroll_wheel (self->carousel, FALSE);

  settings                 = gtk_widget_get_settings (GTK_WIDGET (self));
  self->settings_notify_id = g_signal_connect (settings, "notify::gtk-enable-animations",
                                               G_CALLBACK (carousel_notify_settings_cb),
                                               self);
}

BzFeaturedCarousel *
bz_featured_carousel_new (void)
{
  return g_object_new (BZ_TYPE_FEATURED_CAROUSEL, NULL);
}

GListModel *
bz_featured_carousel_get_model (BzFeaturedCarousel *self)
{
  g_return_val_if_fail (BZ_IS_FEATURED_CAROUSEL (self), NULL);
  return self->model;
}

void
bz_featured_carousel_set_model (BzFeaturedCarousel *self,
                                GListModel         *model)
{
  g_return_if_fail (BZ_IS_FEATURED_CAROUSEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (model != NULL && model == self->model)
    return;

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, model_items_changed_cb, self);

  g_set_object (&self->model, model);

  if (model != NULL)
    g_signal_connect_swapped (model, "items-changed",
                              G_CALLBACK (model_items_changed_cb), self);

  rebuild_carousel (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

gboolean
bz_featured_carousel_get_is_aotd (BzFeaturedCarousel *self)
{
  g_return_val_if_fail (BZ_IS_FEATURED_CAROUSEL (self), FALSE);
  return self->is_aotd;
}

void
bz_featured_carousel_set_is_aotd (BzFeaturedCarousel *self,
                                  gboolean            is_aotd)
{
  g_return_if_fail (BZ_IS_FEATURED_CAROUSEL (self));

  if (self->is_aotd == is_aotd)
    return;

  self->is_aotd = is_aotd;

  rebuild_carousel (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_AOTD]);
}

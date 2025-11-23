/* bz-tag-list.c
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
#include <libdex.h>

#include "bz-apps-page.h"
#include "bz-flathub-state.h"
#include "bz-tag-list.h"
#include "bz-util.h"
#include "bz-window.h"

struct _BzTagList
{
  GtkBox parent_instance;

  GListModel     *model;
  GtkWidget      *prefix;
  BzFlathubState *flathub_state;

  gulong     items_changed_id;
  DexFuture *task;
};

G_DEFINE_FINAL_TYPE (BzTagList, bz_tag_list, GTK_TYPE_BOX);

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_PREFIX,
  PROP_FLATHUB_STATE,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SELECT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
apps_page_select_cb (BzTagList    *self,
                     BzEntryGroup *group,
                     BzAppsPage   *page)
{
  GtkWidget *nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  adw_navigation_view_pop (ADW_NAVIGATION_VIEW (nav_view));
  g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}

static DexFuture *
search_finally (DexFuture *future,
                GWeakRef  *wr)
{
  g_autoptr (BzTagList) self     = NULL;
  g_autoptr (GError) local_error = NULL;
  GtkRoot      *root             = NULL;
  const GValue *value            = NULL;

  bz_weak_get_or_return_reject (self, wr);

  root = gtk_widget_get_root (GTK_WIDGET (self));

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      g_autoptr (GListModel) model = NULL;
      guint n_items                = 0;

      model   = g_value_get_object (value);
      n_items = g_list_model_get_n_items (model);

      if (n_items <= 1)
        {
          AdwToast *toast = adw_toast_new (_ ("No Results Found"));
          bz_window_add_toast (BZ_WINDOW (root), toast);
        }
      else
        {
          const char        *tag       = NULL;
          g_autofree char   *title     = NULL;
          g_autofree char   *subtitle  = NULL;
          AdwNavigationPage *apps_page = NULL;
          GtkWidget         *nav_view  = NULL;

          tag      = g_object_get_data (G_OBJECT (self), "current-tag");
          title    = g_strdup_printf (_ ("Apps Tagged \"%s\""), tag);
          subtitle = g_strdup_printf (_ ("%d applications"), n_items);

          apps_page = bz_apps_page_new (title, model);
          bz_apps_page_set_subtitle (BZ_APPS_PAGE (apps_page), subtitle);

          g_signal_connect_swapped (
              apps_page, "select",
              G_CALLBACK (apps_page_select_cb), self);

          nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
          adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), apps_page);
        }
    }
  else
    {
      AdwToast *toast = adw_toast_new (_ ("Search failed"));
      bz_window_add_toast (BZ_WINDOW (root), toast);
      g_warning ("Search failed: %s", local_error->message);
    }

  dex_clear (&self->task);
  return dex_future_new_true ();
}

static void
tag_button_clicked_cb (BzTagList *self,
                       GtkButton *button)
{
  g_autoptr (BzResult) result  = NULL;
  g_autoptr (DexFuture) future = NULL;
  const char *tag              = NULL;

  g_return_if_fail (BZ_IS_TAG_LIST (self));
  g_return_if_fail (GTK_IS_BUTTON (button));

  if (self->flathub_state == NULL)
    return;
  if (self->task != NULL)
    return;

  tag = gtk_button_get_label (button);
  if (tag == NULL || *tag == '\0')
    return;

  g_object_set_data_full (G_OBJECT (self), "current-tag", g_strdup (tag), g_free);

  result = bz_flathub_state_search_keyword (self->flathub_state, tag);
  if (result == NULL)
    return;

  future = bz_result_dup_future (result);
  future = dex_future_finally (
      future,
      (DexFutureCallback) search_finally,
      bz_track_weak (self),
      bz_weak_release);
  self->task = g_steal_pointer (&future);
}

static void
rebuild_tags (BzTagList *self)
{
  GtkWidget *child;
  guint      n_items;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
    {
      if (child == self->prefix)
        {
          child = gtk_widget_get_next_sibling (child);
          if (child == NULL)
            break;
        }
      gtk_box_remove (GTK_BOX (self), child);
    }

  if (self->model == NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      return;
    }

  n_items = g_list_model_get_n_items (self->model);

  if (n_items == 0)
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      return;
    }

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (GtkStringObject) item = NULL;
      const char *tag;
      GtkWidget  *button;

      item = g_list_model_get_item (self->model, i);
      if (item == NULL)
        continue;

      tag = gtk_string_object_get_string (item);
      if (tag == NULL || *tag == '\0')
        continue;

      button = gtk_button_new_with_label (tag);
      gtk_widget_add_css_class (button, "small-pill");
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK (tag_button_clicked_cb), self);
      gtk_box_append (GTK_BOX (self), button);
    }
}

static void
on_items_changed (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  BzTagList  *self)
{
  rebuild_tags (self);
}

static void
bz_tag_list_dispose (GObject *object)
{
  BzTagList *self = BZ_TAG_LIST (object);

  dex_clear (&self->task);

  if (self->model != NULL && self->items_changed_id != 0)
    {
      g_signal_handler_disconnect (self->model, self->items_changed_id);
      self->items_changed_id = 0;
    }

  g_clear_object (&self->model);
  g_clear_object (&self->flathub_state);
  g_clear_pointer (&self->prefix, gtk_widget_unparent);

  G_OBJECT_CLASS (bz_tag_list_parent_class)->dispose (object);
}

static void
bz_tag_list_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  BzTagList *self = BZ_TAG_LIST (object);
  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_tag_list_get_model (self));
      break;
    case PROP_PREFIX:
      g_value_set_object (value, bz_tag_list_get_prefix (self));
      break;
    case PROP_FLATHUB_STATE:
      g_value_set_object (value, bz_tag_list_get_flathub_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_tag_list_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  BzTagList *self = BZ_TAG_LIST (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_tag_list_set_model (self, g_value_get_object (value));
      break;
    case PROP_PREFIX:
      bz_tag_list_set_prefix (self, g_value_get_object (value));
      break;
    case PROP_FLATHUB_STATE:
      bz_tag_list_set_flathub_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_tag_list_class_init (BzTagListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_tag_list_set_property;
  object_class->get_property = bz_tag_list_get_property;
  object_class->dispose      = bz_tag_list_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREFIX] =
      g_param_spec_object (
          "prefix",
          NULL, NULL,
          GTK_TYPE_WIDGET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FLATHUB_STATE] =
      g_param_spec_object (
          "flathub-state",
          NULL, NULL,
          BZ_TYPE_FLATHUB_STATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SELECT] =
      g_signal_new ("select",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1, BZ_TYPE_ENTRY_GROUP);
}

static void
bz_tag_list_init (BzTagList *self)
{
  GtkLayoutManager *layout;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  layout = GTK_LAYOUT_MANAGER (adw_wrap_layout_new ());
  adw_wrap_layout_set_child_spacing (ADW_WRAP_LAYOUT (layout), 8);
  adw_wrap_layout_set_line_spacing (ADW_WRAP_LAYOUT (layout), 8);
  gtk_widget_set_layout_manager (GTK_WIDGET (self), layout);
}

GtkWidget *
bz_tag_list_new (void)
{
  return g_object_new (BZ_TYPE_TAG_LIST, NULL);
}

GListModel *
bz_tag_list_get_model (BzTagList *self)
{
  g_return_val_if_fail (BZ_IS_TAG_LIST (self), NULL);
  return self->model;
}

void
bz_tag_list_set_model (BzTagList  *self,
                       GListModel *model)
{
  g_return_if_fail (BZ_IS_TAG_LIST (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model != NULL && self->items_changed_id != 0)
    {
      g_signal_handler_disconnect (self->model, self->items_changed_id);
      self->items_changed_id = 0;
    }

  g_clear_object (&self->model);

  if (model != NULL)
    {
      self->model            = g_object_ref (model);
      self->items_changed_id = g_signal_connect (self->model,
                                                 "items-changed",
                                                 G_CALLBACK (on_items_changed),
                                                 self);
    }

  rebuild_tags (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GtkWidget *
bz_tag_list_get_prefix (BzTagList *self)
{
  g_return_val_if_fail (BZ_IS_TAG_LIST (self), NULL);
  return self->prefix;
}

void
bz_tag_list_set_prefix (BzTagList *self,
                        GtkWidget *prefix)
{
  g_return_if_fail (BZ_IS_TAG_LIST (self));
  g_return_if_fail (prefix == NULL || GTK_IS_WIDGET (prefix));

  if (self->prefix != NULL)
    gtk_widget_unparent (self->prefix);

  self->prefix = prefix;

  if (self->prefix != NULL)
    gtk_widget_set_parent (self->prefix, GTK_WIDGET (self));

  rebuild_tags (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREFIX]);
}

BzFlathubState *
bz_tag_list_get_flathub_state (BzTagList *self)
{
  g_return_val_if_fail (BZ_IS_TAG_LIST (self), NULL);
  return self->flathub_state;
}

void
bz_tag_list_set_flathub_state (BzTagList      *self,
                               BzFlathubState *flathub_state)
{
  g_return_if_fail (BZ_IS_TAG_LIST (self));
  g_return_if_fail (flathub_state == NULL || BZ_IS_FLATHUB_STATE (flathub_state));

  g_clear_object (&self->flathub_state);

  if (flathub_state != NULL)
    self->flathub_state = g_object_ref (flathub_state);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FLATHUB_STATE]);
}

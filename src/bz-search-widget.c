/* bz-search-widget.c
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

#include "bz-search-widget.h"
#include <glib/gi18n.h>

#include "bz-apps-page.h"
#include "bz-async-texture.h"
#include "bz-category-tile.h"
#include "bz-dynamic-list-view.h"
#include "bz-flathub-state.h"
#include "bz-group-tile-css-watcher.h"
#include "bz-screenshot.h"
#include "bz-search-result.h"
#include "bz-state-info.h"
#include "bz-util.h"
#include "bz-window.h"

struct _BzSearchWidget
{
  AdwBin parent_instance;

  BzStateInfo  *state;
  BzEntryGroup *selected;
  gboolean      remove;
  BzEntryGroup *previewing;
  gboolean      search_in_progress;

  GListStore         *search_model;
  GtkSingleSelection *selection_model;
  guint               search_update_timeout;
  DexFuture          *search_query;

  /* Template widgets */
  GtkText     *search_bar;
  AdwSpinner  *search_busy;
  GtkBox      *content_box;
  GtkStack    *search_stack;
  GtkListView *list_view;
};

G_DEFINE_FINAL_TYPE (BzSearchWidget, bz_search_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_STATE,
  PROP_PREVIEWING,
  PROP_TEXT,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SELECT,
  SIGNAL_PREVIEW_CHANGED,
  SIGNAL_GROUP_SELECTED,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
search_changed (GtkEditable    *editable,
                BzSearchWidget *self);

static void
search_activate (GtkText        *text,
                 BzSearchWidget *self);

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       BzSearchWidget     *self);

static void
list_activate (GtkListView    *list_view,
               guint           position,
               BzSearchWidget *self);

static DexFuture *
search_query_then (DexFuture *future,
                   GWeakRef  *wr);

static void
update_filter (BzSearchWidget *self);

static void
emit_idx (BzSearchWidget *self,
          GListModel     *model,
          guint           selected_idx);

static void
bz_search_widget_dispose (GObject *object)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  g_clear_handle_id (&self->search_update_timeout, g_source_remove);
  dex_clear (&self->search_query);

  g_clear_object (&self->state);
  g_clear_object (&self->selected);
  g_clear_object (&self->previewing);
  g_clear_object (&self->selection_model);

  G_OBJECT_CLASS (bz_search_widget_parent_class)->dispose (object);
}

static void
bz_search_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    case PROP_TEXT:
      g_value_set_string (value, bz_search_widget_get_text (self));
      break;
    case PROP_PREVIEWING:
      g_value_set_object (value, bz_search_widget_get_previewing (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_clear_object (&self->state);
      self->state = g_value_dup_object (value);
      break;
    case PROP_TEXT:
      bz_search_widget_set_text (self, g_value_get_string (value));
      break;
    case PROP_PREVIEWING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
bz_search_widget_grab_focus (GtkWidget *widget)
{
  BzSearchWidget *self = BZ_SEARCH_WIDGET (widget);
  return gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static gboolean
is_valid_string (gpointer    object,
                 const char *value)
{
  return value != NULL && *value != '\0';
}

static char *
idx_to_string (gpointer object,
               guint    value)
{
  return g_strdup_printf ("%d", value + 1);
}

static char *
score_to_string (gpointer object,
                 double   value)
{
  return g_strdup_printf ("%0.1f", value);
}

static char *
no_results_found_subtitle (gpointer    object,
                           const char *search_text)
{
  if (search_text == NULL || *search_text == '\0')
    return g_strdup ("");

  return g_strdup_printf (_ ("No results found for \"%s\" in Flathub"), search_text);
}

static void
action_move (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  BzSearchWidget    *self     = BZ_SEARCH_WIDGET (widget);
  GtkSelectionModel *model    = NULL;
  guint              selected = 0;
  guint              n_items  = 0;

  model   = gtk_list_view_get_model (self->list_view);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  if (n_items == 0)
    return;

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected == GTK_INVALID_LIST_POSITION)
    selected = 0;
  else
    {
      int offset = 0;

      offset = g_variant_get_int32 (parameter);
      if (offset < 0 && ABS (offset) > selected)
        selected = n_items + (offset % -(int) n_items);
      else
        selected = (selected + offset) % n_items;
    }

  gtk_list_view_scroll_to (
      self->list_view, selected,
      GTK_LIST_SCROLL_SELECT, NULL);
}

static void
apps_page_select_cb (BzSearchWidget *self,
                     BzEntryGroup   *group,
                     BzAppsPage     *page)
{
  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
category_clicked (BzFlathubCategory *category,
                  GtkButton         *button)
{
  GtkWidget         *self               = NULL;
  GtkWidget         *window             = NULL;
  GtkWidget         *nav_view           = NULL;
  AdwNavigationPage *apps_page          = NULL;
  g_autoptr (GListModel) model          = NULL;
  g_autoptr (GListModel) carousel_model = NULL;
  const char *title                     = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_SEARCH_WIDGET);
  g_assert (self != NULL);

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

  nav_view = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_NAVIGATION_VIEW);
  g_assert (nav_view != NULL);

  title          = bz_flathub_category_get_display_name (category);
  model          = bz_flathub_category_dup_applications (category);
  carousel_model = bz_flathub_category_dup_quality_applications (category);

  if (carousel_model != NULL && g_list_model_get_n_items (carousel_model) > 0)
    {
      apps_page = bz_apps_page_new_with_carousel (title, model, carousel_model);
    }
  else
    {
      apps_page = bz_apps_page_new (title, model);
    }

  g_signal_connect_swapped (
      apps_page, "select",
      G_CALLBACK (apps_page_select_cb), self);

  adw_navigation_view_push (ADW_NAVIGATION_VIEW (nav_view), apps_page);

  bz_window_set_app_list_view_mode (BZ_WINDOW (window), TRUE);
}

static void
bind_category_tile_cb (BzSearchWidget    *self,
                       BzCategoryTile    *tile,
                       BzFlathubCategory *category,
                       BzDynamicListView *view)
{
  g_signal_connect_swapped (tile, "clicked", G_CALLBACK (category_clicked), category);
}

static void
unbind_category_tile_cb (BzSearchWidget    *self,
                         BzCategoryTile    *tile,
                         BzFlathubCategory *category,
                         BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (tile, category_clicked, category);
}

static void
bz_search_widget_class_init (BzSearchWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_search_widget_dispose;
  object_class->get_property = bz_search_widget_get_property;
  object_class->set_property = bz_search_widget_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_TEXT] =
      g_param_spec_string (
          "text",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREVIEWING] =
      g_param_spec_object (
          "previewing",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
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
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SELECT],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_PREVIEW_CHANGED] =
      g_signal_new (
          "preview-changed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          NULL,
          G_TYPE_NONE, 2,
          BZ_TYPE_ENTRY_GROUP, G_TYPE_BOOLEAN);

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

  widget_class->grab_focus = bz_search_widget_grab_focus;

  g_type_ensure (BZ_TYPE_ASYNC_TEXTURE);
  g_type_ensure (BZ_TYPE_CATEGORY_TILE);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_GROUP_TILE_CSS_WATCHER);
  g_type_ensure (BZ_TYPE_SCREENSHOT);
  g_type_ensure (BZ_TYPE_SEARCH_RESULT);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-search-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_bar);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_busy);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, content_box);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_stack);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, list_view);
  gtk_widget_class_bind_template_callback (widget_class, bind_category_tile_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_category_tile_cb);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_valid_string);
  gtk_widget_class_bind_template_callback (widget_class, idx_to_string);
  gtk_widget_class_bind_template_callback (widget_class, score_to_string);
  gtk_widget_class_bind_template_callback (widget_class, no_results_found_subtitle);

  gtk_widget_class_install_action (widget_class, "move", "i", action_move);
}

static void
bz_search_widget_init (BzSearchWidget *self)
{
  self->search_model = g_list_store_new (BZ_TYPE_SEARCH_RESULT);

  gtk_widget_init_template (GTK_WIDGET (self));

  /* TODO: move all this to blueprint */

  self->selection_model = gtk_single_selection_new (NULL);
  gtk_list_view_set_model (self->list_view, GTK_SELECTION_MODEL (self->selection_model));
  g_signal_connect (self->selection_model, "notify::selected-item", G_CALLBACK (selected_item_changed), self);

  g_signal_connect (self->search_bar, "changed", G_CALLBACK (search_changed), self);
  g_signal_connect (self->search_bar, "activate", G_CALLBACK (search_activate), self);
  g_signal_connect (self->list_view, "activate", G_CALLBACK (list_activate), self);
}

GtkWidget *
bz_search_widget_new (GListModel *model,
                      const char *initial)
{
  BzSearchWidget *self = NULL;

  self = g_object_new (
      BZ_TYPE_SEARCH_WIDGET,
      "model", model,
      NULL);

  if (initial != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->search_bar), initial);

  return GTK_WIDGET (self);
}

BzEntryGroup *
bz_search_widget_get_selected (BzSearchWidget *self,
                               gboolean       *remove)
{
  g_return_val_if_fail (BZ_IS_SEARCH_WIDGET (self), NULL);

  if (remove != NULL)
    *remove = self->remove;
  return self->selected;
}

BzEntryGroup *
bz_search_widget_get_previewing (BzSearchWidget *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_WIDGET (self), NULL);
  return self->previewing;
}

void
bz_search_widget_set_text (BzSearchWidget *self,
                           const char     *text)
{
  g_return_if_fail (BZ_IS_SEARCH_WIDGET (self));
  gtk_editable_set_text (GTK_EDITABLE (self->search_bar), text);
  gtk_editable_set_position (GTK_EDITABLE (self->search_bar), g_utf8_strlen (text, -1));
}

void
bz_search_widget_refresh (BzSearchWidget *self)
{
  g_return_if_fail (BZ_IS_SEARCH_WIDGET (self));
  update_filter (self);
}

const char *
bz_search_widget_get_text (BzSearchWidget *self)
{
  return gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
}

static void
search_changed (GtkEditable    *editable,
                BzSearchWidget *self)
{
  GSettings *settings = NULL;

  g_clear_handle_id (&self->search_update_timeout, g_source_remove);

  settings = bz_state_info_get_settings (self->state);
  if (settings && g_settings_get_boolean (settings, "search-debounce"))
    {
      self->search_update_timeout = g_timeout_add_once (
          200 /* 200 ms */, (GSourceOnceFunc) update_filter, self);
      gtk_widget_set_visible (GTK_WIDGET (self->search_busy), TRUE);
    }
  else
    update_filter (self);
}

static void
search_activate (GtkText        *text,
                 BzSearchWidget *self)
{
  GtkSelectionModel *model        = NULL;
  guint              selected_idx = 0;

  g_clear_object (&self->selected);

  model        = gtk_list_view_get_model (self->list_view);
  selected_idx = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (model));

  if (selected_idx != GTK_INVALID_LIST_POSITION)
    emit_idx (self, G_LIST_MODEL (model), selected_idx);
}

static void
selected_item_changed (GtkSingleSelection *model,
                       GParamSpec         *pspec,
                       BzSearchWidget     *self)
{
  guint    selected = 0;
  gboolean from_search;

  g_clear_object (&self->previewing);

  selected = gtk_single_selection_get_selected (model);
  if (selected != GTK_INVALID_LIST_POSITION)
    {
      g_autoptr (BzSearchResult) result = NULL;

      result           = g_list_model_get_item (G_LIST_MODEL (model), selected);
      self->previewing = g_object_ref (bz_search_result_get_group (result));

      from_search              = self->search_in_progress;
      self->search_in_progress = FALSE;

      g_signal_emit (self, signals[SIGNAL_PREVIEW_CHANGED], 0, self->previewing, from_search);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREVIEWING]);
}

static void
list_activate (GtkListView    *list_view,
               guint           position,
               BzSearchWidget *self)
{
  GtkSelectionModel *model = NULL;

  model = gtk_list_view_get_model (self->list_view);
  emit_idx (self, G_LIST_MODEL (model), position);
}

static DexFuture *
search_query_then (DexFuture *future,
                   GWeakRef  *wr)
{
  g_autoptr (BzSearchWidget) self = NULL;
  GPtrArray  *results             = NULL;
  guint       old_length          = 0;
  GSettings  *settings            = NULL;
  const char *page_name           = NULL;

  bz_weak_get_or_return_reject (self, wr);

  results    = g_value_get_boxed (dex_future_get_value (future, NULL));
  old_length = g_list_model_get_n_items (G_LIST_MODEL (self->search_model));
  settings   = bz_state_info_get_settings (self->state);

  if (settings && g_settings_get_boolean (settings, "search-only-foss"))
    {
      for (guint i = 0; i < results->len;)
        {
          BzSearchResult *result  = NULL;
          BzEntryGroup   *group   = NULL;
          gboolean        is_foss = FALSE;

          result  = g_ptr_array_index (results, i);
          group   = bz_search_result_get_group (result);
          is_foss = bz_entry_group_get_is_floss (group);

          if (is_foss)
            i++;
          else
            g_ptr_array_remove_index (results, i);
        }
    }

  if (settings && g_settings_get_boolean (settings, "search-only-flathub"))
    {
      for (guint i = 0; i < results->len;)
        {
          BzSearchResult *result     = NULL;
          BzEntryGroup   *group      = NULL;
          gboolean        is_flathub = FALSE;

          result     = g_ptr_array_index (results, i);
          group      = bz_search_result_get_group (result);
          is_flathub = bz_entry_group_get_is_flathub (group);

          if (is_flathub)
            i++;
          else
            g_ptr_array_remove_index (results, i);
        }
    }

  g_list_store_splice (
      self->search_model,
      0, old_length,
      (gpointer *) results->pdata,
      results->len);
  gtk_single_selection_set_model (
      self->selection_model, G_LIST_MODEL (self->search_model));

  gtk_widget_set_visible (GTK_WIDGET (self->search_busy), FALSE);

  if (results->len > 0)
    {
      page_name = "results";
      /* Here to combat weird list view scrolling behavior */
      gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_SELECT, NULL);
    }
  else
    {
      const char *search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
      page_name               = (search_text && *search_text) ? "no-results" : "empty";
    }

  gtk_stack_set_visible_child_name (self->search_stack, page_name);

  dex_clear (&self->search_query);
  return NULL;
}

static void
update_filter (BzSearchWidget *self)
{
  BzSearchEngine *engine           = NULL;
  const char     *search_text      = NULL;
  g_autoptr (GStrvBuilder) builder = NULL;
  guint n_terms                    = 0;
  g_auto (GStrv) terms             = NULL;
  g_autoptr (DexFuture) future     = NULL;
  g_autofree gchar **tokens        = NULL;

  g_clear_handle_id (&self->search_update_timeout, g_source_remove);
  dex_clear (&self->search_query);

  gtk_widget_set_visible (GTK_WIDGET (self->search_busy), FALSE);

  if (self->state == NULL)
    return;
  engine = bz_state_info_get_search_engine (self->state);
  if (engine == NULL)
    return;

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));

  if (search_text == NULL || *search_text == '\0')
    {
      g_list_store_remove_all (self->search_model);
      gtk_single_selection_set_model (
          self->selection_model, G_LIST_MODEL (self->search_model));
      gtk_stack_set_visible_child_name (self->search_stack, "empty");
      return;
    }

  builder = g_strv_builder_new ();

  tokens = g_strsplit_set (search_text, " \t\n", -1);
  for (gchar **token = tokens; *token != NULL; token++)
    {
      if (**token != '\0')
        {
          g_strv_builder_take (builder, *token);
          n_terms++;
        }
      else
        g_free (*token);
    }

  if (n_terms == 0)
    {
      g_list_store_remove_all (self->search_model);
      gtk_single_selection_set_model (
          self->selection_model, G_LIST_MODEL (self->search_model));
      gtk_stack_set_visible_child_name (self->search_stack, "empty");
      return;
    }

  terms = g_strv_builder_end (builder);

  self->search_in_progress = TRUE;

  future = bz_search_engine_query (
      engine,
      (const char *const *) terms);
  gtk_widget_set_visible (
      GTK_WIDGET (self->search_busy),
      dex_future_is_pending (future));

  future = dex_future_then (
      future,
      (DexFutureCallback) search_query_then,
      bz_track_weak (self), bz_weak_release);
  self->search_query = g_steal_pointer (&future);
}

static void
emit_idx (BzSearchWidget *self,
          GListModel     *model,
          guint           selected_idx)
{
  g_autoptr (BzSearchResult) result = NULL;
  BzEntryGroup *group               = NULL;

  result = g_list_model_get_item (G_LIST_MODEL (model), selected_idx);
  group  = bz_search_result_get_group (result);

  if (bz_entry_group_get_installable_and_available (group) > 0 ||
      bz_entry_group_get_removable_and_available (group) > 0)
    {
      g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
      g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
    }
}

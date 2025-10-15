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
#include "bz-async-texture.h"
#include "bz-group-tile-css-watcher.h"
#include "bz-screenshot.h"
#include "bz-search-result.h"
#include "bz-state-info.h"

struct _BzSearchWidget
{
  AdwBin parent_instance;

  BzStateInfo  *state;
  BzEntryGroup *selected;
  gboolean      remove;
  BzEntryGroup *previewing;

  GListStore         *search_model;
  GtkSingleSelection *selection_model;
  guint               search_update_timeout;
  DexFuture          *search_query;

  /* Template widgets */
  GtkText     *search_bar;
  GtkLabel    *search_text;
  GtkImage    *timeout_busy;
  GtkImage    *search_busy;
  GtkBox      *content_box;
  GtkRevealer *entry_list_revealer;
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
search_query_then (DexFuture      *future,
                   BzSearchWidget *self);

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

  widget_class->grab_focus = bz_search_widget_grab_focus;

  g_type_ensure (BZ_TYPE_ASYNC_TEXTURE);
  g_type_ensure (BZ_TYPE_GROUP_TILE_CSS_WATCHER);
  g_type_ensure (BZ_TYPE_SCREENSHOT);
  g_type_ensure (BZ_TYPE_SEARCH_RESULT);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-search-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_bar);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_text);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, timeout_busy);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, search_busy);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, content_box);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, entry_list_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzSearchWidget, list_view);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_valid_string);
  gtk_widget_class_bind_template_callback (widget_class, idx_to_string);
  gtk_widget_class_bind_template_callback (widget_class, score_to_string);

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
      gtk_widget_set_visible (GTK_WIDGET (self->timeout_busy), TRUE);
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
  guint selected = 0;

  g_clear_object (&self->previewing);

  selected = gtk_single_selection_get_selected (model);
  if (selected != GTK_INVALID_LIST_POSITION)
    {
      g_autoptr (BzSearchResult) result = NULL;

      result           = g_list_model_get_item (G_LIST_MODEL (model), selected);
      self->previewing = g_object_ref (bz_search_result_get_group (result));
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
search_query_then (DexFuture      *future,
                   BzSearchWidget *self)
{
  GPtrArray *results    = NULL;
  guint      old_length = 0;
  GSettings *settings   = NULL;

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
  gtk_revealer_set_reveal_child (self->entry_list_revealer, results->len > 0);

  if (results->len > 0)
    /* Here to combat weird list view scrolling behavior */
    gtk_list_view_scroll_to (self->list_view, 0, GTK_LIST_SCROLL_SELECT, NULL);

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

  g_clear_handle_id (&self->search_update_timeout, g_source_remove);
  dex_clear (&self->search_query);

  gtk_widget_set_visible (GTK_WIDGET (self->timeout_busy), FALSE);

  if (self->state == NULL)
    return;
  engine = bz_state_info_get_search_engine (self->state);
  if (engine == NULL)
    return;

  search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_bar));
  builder     = g_strv_builder_new ();

  if (search_text != NULL &&
      *search_text != '\0')
    {
      g_autofree gchar **tokens = NULL;

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
    }

  if (n_terms == 0)
    g_strv_builder_add (builder, "");
  terms = g_strv_builder_end (builder);

  future = bz_search_engine_query (
      engine,
      (const char *const *) terms);
  if (dex_future_is_resolved (future))
    search_query_then (future, self);
  else
    {
      future = dex_future_then (
          future,
          (DexFutureCallback) search_query_then,
          self, NULL);
      self->search_query = g_steal_pointer (&future);
      gtk_widget_set_visible (GTK_WIDGET (self->search_busy), TRUE);
    }
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
    g_signal_emit (self, signals[SIGNAL_SELECT], 0, group);
}

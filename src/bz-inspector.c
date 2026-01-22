/* bz-inspector.c
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

#include "bz-inspector.h"
#include "bz-entry-inspector.h"
#include "bz-window.h"

struct _BzInspector
{
  AdwWindow parent_instance;

  BzStateInfo *state;

  GBinding  *debug_mode_binding;
  GBinding  *disable_blocklists_binding;
  GtkWindow *preview_window;

  GtkCheckButton     *debug_mode_check;
  GtkCheckButton     *disable_blocklists_check;
  GtkEditable        *search_entry;
  GtkFilterListModel *filter_model;
  GtkSingleSelection *groups_selection;
};

G_DEFINE_FINAL_TYPE (BzInspector, bz_inspector, ADW_TYPE_WINDOW);

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static gboolean
filter_func (BzEntryGroup *group,
             BzInspector  *self);

static void
bz_inspector_dispose (GObject *object)
{
  BzInspector *self = BZ_INSPECTOR (object);

  g_clear_pointer (&self->state, g_object_unref);

  g_clear_object (&self->debug_mode_binding);
  g_clear_object (&self->disable_blocklists_binding);
  if (self->preview_window != NULL)
    gtk_window_close (self->preview_window);
  g_clear_object (&self->preview_window);

  G_OBJECT_CLASS (bz_inspector_parent_class)->dispose (object);
}

static void
bz_inspector_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzInspector *self = BZ_INSPECTOR (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_inspector_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_inspector_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzInspector *self = BZ_INSPECTOR (object);

  switch (prop_id)
    {
    case PROP_STATE:
      bz_inspector_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
preview_changed (BzInspector    *self,
                 GParamSpec     *pspec,
                 GtkCheckButton *button)
{
  if (gtk_check_button_get_active (button))
    {
      BzWindow     *window   = NULL;
      BzEntryGroup *selected = NULL;

      g_assert (self->preview_window == NULL);

      window = bz_window_new (self->state);
      gtk_window_set_default_size (GTK_WINDOW (window), 750, 750);
      gtk_window_present (GTK_WINDOW (window));

      selected = gtk_single_selection_get_selected_item (self->groups_selection);
      if (selected != NULL)
        bz_window_show_group (window, selected);

      self->preview_window = (GtkWindow *) g_object_ref_sink (window);
    }
  else
    {
      if (self->preview_window != NULL)
        gtk_window_close (self->preview_window);
      g_clear_object (&self->preview_window);
    }
}

static void
selected_group_changed (BzInspector        *self,
                        GParamSpec         *pspec,
                        GtkSingleSelection *selection)
{
  BzEntryGroup *group = NULL;

  if (self->preview_window == NULL ||
      !gtk_widget_get_mapped (GTK_WIDGET (self->preview_window)))
    return;

  group = gtk_single_selection_get_selected_item (self->groups_selection);
  if (group != NULL)
    bz_window_show_group (BZ_WINDOW (self->preview_window), group);
}

static void
entry_changed (BzInspector *self,
               GtkEditable *editable)
{
  GtkFilter *filter = NULL;

  filter = gtk_filter_list_model_get_filter (self->filter_model);
  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
decache_and_inspect_cb (GtkListItem *list_item,
                        GtkButton   *button)
{
  GtkStringObject *item       = NULL;
  BzInspector     *self       = NULL;
  g_autoptr (BzResult) result = NULL;

  item = gtk_list_item_get_item (list_item);

  self = BZ_INSPECTOR (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSPECTOR));
  g_assert (self != NULL);

  result = bz_application_map_factory_convert_one (
      bz_state_info_get_entry_factory (self->state),
      g_object_ref (item));
  if (result != NULL)
    {
      BzEntryInspector *inspector = NULL;

      inspector = bz_entry_inspector_new ();
      bz_entry_inspector_set_result (inspector, result);

      gtk_window_present (GTK_WINDOW (inspector));
    }
}

static char *
format_uint (gpointer object,
             guint    value)
{
  return g_strdup_printf ("%d", value);
}

static void
bz_inspector_class_init (BzInspectorClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_inspector_set_property;
  object_class->get_property = bz_inspector_get_property;
  object_class->dispose      = bz_inspector_dispose;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-inspector.ui");
  gtk_widget_class_bind_template_child (widget_class, BzInspector, debug_mode_check);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, disable_blocklists_check);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, search_entry);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, filter_model);
  gtk_widget_class_bind_template_child (widget_class, BzInspector, groups_selection);
  gtk_widget_class_bind_template_callback (widget_class, preview_changed);
  gtk_widget_class_bind_template_callback (widget_class, selected_group_changed);
  gtk_widget_class_bind_template_callback (widget_class, decache_and_inspect_cb);
  gtk_widget_class_bind_template_callback (widget_class, entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, format_uint);
}

static void
bz_inspector_init (BzInspector *self)
{
  GtkCustomFilter *filter = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  filter = gtk_custom_filter_new ((GtkCustomFilterFunc) filter_func, self, NULL);
  gtk_filter_list_model_set_filter (self->filter_model, GTK_FILTER (filter));
}

BzInspector *
bz_inspector_new (void)
{
  return g_object_new (BZ_TYPE_INSPECTOR, NULL);
}

BzStateInfo *
bz_inspector_get_state (BzInspector *self)
{
  g_return_val_if_fail (BZ_IS_INSPECTOR (self), NULL);
  return self->state;
}

void
bz_inspector_set_state (BzInspector *self,
                        BzStateInfo *state)
{
  g_return_if_fail (BZ_IS_INSPECTOR (self));

  g_clear_pointer (&self->state, g_object_unref);
  g_clear_pointer (&self->debug_mode_binding, g_object_unref);
  g_clear_pointer (&self->disable_blocklists_binding, g_object_unref);

  if (state != NULL)
    {
      self->state              = g_object_ref (state);
      self->debug_mode_binding = g_object_bind_property (
          state, "debug-mode",
          self->debug_mode_check, "active",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      self->disable_blocklists_binding = g_object_bind_property (
          state, "disable-blocklists",
          self->disable_blocklists_check, "active",
          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

static gboolean
filter_func (BzEntryGroup *group,
             BzInspector  *self)
{
  const char *text  = NULL;
  const char *id    = NULL;
  const char *title = NULL;

  text  = gtk_editable_get_text (self->search_entry);
  id    = bz_entry_group_get_id (group);
  title = bz_entry_group_get_title (group);

  if (text == NULL || *text == '\0')
    return TRUE;

  if (strcasestr (id, text) != NULL)
    return TRUE;
  if (strcasestr (title, text) != NULL)
    return TRUE;

  return FALSE;
}

/* End of bz-inspector.c */

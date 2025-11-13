/* bz-browse-widget.c
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

#include "config.h"

#include "bz-browse-widget.h"
#include "bz-curated-row.h"
#include "bz-dynamic-list-view.h"
#include "bz-entry-group.h"
#include "bz-inhibited-scrollable.h"
#include "bz-section-view.h"

struct _BzBrowseWidget
{
  AdwBin parent_instance;

  BzContentProvider *provider;
  gboolean           online;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzBrowseWidget, bz_browse_widget, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_CONTENT_PROVIDER,
  PROP_ONLINE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_GROUP_SELECTED,
  SIGNAL_BROWSE_FLATHUB,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
items_changed (GListModel     *model,
               guint           position,
               guint           removed,
               guint           added,
               BzBrowseWidget *self);

static void
set_page (BzBrowseWidget *self);

static void
bz_browse_widget_dispose (GObject *object)
{
  BzBrowseWidget *self = BZ_BROWSE_WIDGET (object);

  if (self->provider != NULL)
    g_signal_handlers_disconnect_by_func (
        self->provider, items_changed, self);
  g_clear_object (&self->provider);

  G_OBJECT_CLASS (bz_browse_widget_parent_class)->dispose (object);
}

static void
bz_browse_widget_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzBrowseWidget *self = BZ_BROWSE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CONTENT_PROVIDER:
      g_value_set_object (value, bz_browse_widget_get_content_provider (self));
      break;
    case PROP_ONLINE:
      g_value_set_boolean (value, self->online);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_browse_widget_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzBrowseWidget *self = BZ_BROWSE_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CONTENT_PROVIDER:
      bz_browse_widget_set_content_provider (self, g_value_get_object (value));
      break;
    case PROP_ONLINE:
      self->online = g_value_get_boolean (value);
      if (self->online)
        set_page (self);
      else
        adw_view_stack_set_visible_child_name (self->stack, "offline");

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
group_activated_cb (GtkListItem   *list_item,
                    BzEntryGroup  *group,
                    BzSectionView *view)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (view), BZ_TYPE_BROWSE_WIDGET);
  g_assert (self != NULL);

  g_signal_emit (self, signals[SIGNAL_GROUP_SELECTED], 0, group);
}

static void
bind_section_view_cb (GtkListItem       *list_item,
                      BzSectionView     *section_view,
                      BzContentSection  *section,
                      BzDynamicListView *view)
{
  g_signal_connect_swapped (section_view, "group-activated",
                            G_CALLBACK (group_activated_cb),
                            list_item);
}

static void
unbind_section_view_cb (GtkListItem       *list_item,
                        BzSectionView     *section_view,
                        BzContentSection  *section,
                        BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (section_view, group_activated_cb, list_item);
}

static void
browse_flathub_cb (BzBrowseWidget *self,
                   GtkButton      *button)
{
  g_signal_emit (self, signals[SIGNAL_BROWSE_FLATHUB], 0);
}

static void
bz_browse_widget_class_init (BzBrowseWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_browse_widget_dispose;
  object_class->get_property = bz_browse_widget_get_property;
  object_class->set_property = bz_browse_widget_set_property;

  props[PROP_CONTENT_PROVIDER] =
      g_param_spec_object (
          "content-provider",
          NULL, NULL,
          BZ_TYPE_CONTENT_PROVIDER,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ONLINE] =
      g_param_spec_boolean (
          "online",
          NULL, NULL,
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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

  signals[SIGNAL_BROWSE_FLATHUB] =
      g_signal_new (
          "browse-flathub",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_CURATED_ROW);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_INHIBITED_SCROLLABLE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-browse-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, BzBrowseWidget, stack);
  gtk_widget_class_bind_template_callback (widget_class, bind_section_view_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_section_view_cb);
  gtk_widget_class_bind_template_callback (widget_class, browse_flathub_cb);
}

static void
bz_browse_widget_init (BzBrowseWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_browse_widget_new (void)
{
  return g_object_new (BZ_TYPE_BROWSE_WIDGET, NULL);
}

void
bz_browse_widget_set_content_provider (BzBrowseWidget    *self,
                                       BzContentProvider *provider)
{
  g_return_if_fail (BZ_IS_BROWSE_WIDGET (self));
  g_return_if_fail (provider == NULL || BZ_IS_CONTENT_PROVIDER (provider));

  if (self->provider != NULL)
    g_signal_handlers_disconnect_by_func (
        self->provider, items_changed, self);
  g_clear_object (&self->provider);

  if (provider != NULL)
    {
      self->provider = g_object_ref (provider);
      g_signal_connect (
          self->provider, "items-changed",
          G_CALLBACK (items_changed), self);
    }

  set_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONTENT_PROVIDER]);
}

BzContentProvider *
bz_browse_widget_get_content_provider (BzBrowseWidget *self)
{
  g_return_val_if_fail (BZ_IS_BROWSE_WIDGET (self), NULL);
  return self->provider;
}

static void
items_changed (GListModel     *model,
               guint           position,
               guint           removed,
               guint           added,
               BzBrowseWidget *self)
{
  set_page (self);
}

static void
set_page (BzBrowseWidget *self)
{
  adw_view_stack_set_visible_child_name (
      self->stack,
      self->provider != NULL &&
              g_list_model_get_n_items (G_LIST_MODEL (self->provider)) > 0
          ? "content"
          : "empty");
}

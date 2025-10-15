/* bz-dynamic-list-view.c
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

#include "bz-dynamic-list-view.h"
#include "bz-marshalers.h"

G_DEFINE_ENUM_TYPE (
    BzDynamicListViewKind,
    bz_dynamic_list_view_kind,
    G_DEFINE_ENUM_VALUE (BZ_DYNAMIC_LIST_VIEW_KIND_LIST_BOX, "list-box"),
    G_DEFINE_ENUM_VALUE (BZ_DYNAMIC_LIST_VIEW_KIND_FLOW_BOX, "flow-box"),
    G_DEFINE_ENUM_VALUE (BZ_DYNAMIC_LIST_VIEW_KIND_CAROUSEL, "carousel"))

struct _BzDynamicListView
{
  AdwBin parent_instance;

  GListModel           *model;
  gboolean              scroll;
  BzDynamicListViewKind noscroll_kind;
  GType                 child_type;
  char                 *child_prop;
  char                 *object_prop;
  guint                 max_children_per_line;

  char *child_type_string;
};

G_DEFINE_FINAL_TYPE (BzDynamicListView, bz_dynamic_list_view, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_SCROLL,
  PROP_NOSCROLL_KIND,
  PROP_CHILD_TYPE,
  PROP_CHILD_PROP,
  PROP_OBJECT_PROP,
  PROP_MAX_CHILDREN_PER_LINE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_BIND_WIDGET,
  SIGNAL_UNBIND_WIDGET,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
refresh (BzDynamicListView *self);

static void
list_item_factory_setup (BzDynamicListView        *self,
                         GtkListItem              *item,
                         GtkSignalListItemFactory *factory);

static void
list_item_factory_teardown (BzDynamicListView        *self,
                            GtkListItem              *item,
                            GtkSignalListItemFactory *factory);

static void
list_item_factory_bind (BzDynamicListView        *self,
                        GtkListItem              *item,
                        GtkSignalListItemFactory *factory);

static void
list_item_factory_unbind (BzDynamicListView        *self,
                          GtkListItem              *item,
                          GtkSignalListItemFactory *factory);

static GtkWidget *
create_child_widget (GObject           *object,
                     BzDynamicListView *self);

static void
items_changed (GListModel        *model,
               guint              position,
               guint              removed,
               guint              added,
               BzDynamicListView *self);

static void
bz_dynamic_list_view_dispose (GObject *object)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);

  g_clear_pointer (&self->child_prop, g_free);
  g_clear_pointer (&self->object_prop, g_free);
  g_clear_pointer (&self->child_type_string, g_free);

  G_OBJECT_CLASS (bz_dynamic_list_view_parent_class)->dispose (object);
}

static void
bz_dynamic_list_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_dynamic_list_view_get_model (self));
      break;
    case PROP_SCROLL:
      g_value_set_boolean (value, bz_dynamic_list_view_get_scroll (self));
      break;
    case PROP_NOSCROLL_KIND:
      g_value_set_enum (value, bz_dynamic_list_view_get_noscroll_kind (self));
      break;
    case PROP_CHILD_TYPE:
      g_value_set_string (value, bz_dynamic_list_view_get_child_type (self));
      break;
    case PROP_CHILD_PROP:
      g_value_set_string (value, bz_dynamic_list_view_get_child_prop (self));
      break;
    case PROP_OBJECT_PROP:
      g_value_set_string (value, bz_dynamic_list_view_get_object_prop (self));
      break;
    case PROP_MAX_CHILDREN_PER_LINE:
      g_value_set_uint (value, bz_dynamic_list_view_get_max_children_per_line (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_dynamic_list_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzDynamicListView *self = BZ_DYNAMIC_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_dynamic_list_view_set_model (self, g_value_get_object (value));
      break;
    case PROP_SCROLL:
      bz_dynamic_list_view_set_scroll (self, g_value_get_boolean (value));
      break;
    case PROP_NOSCROLL_KIND:
      bz_dynamic_list_view_set_noscroll_kind (self, g_value_get_enum (value));
      break;
    case PROP_CHILD_TYPE:
      bz_dynamic_list_view_set_child_type (self, g_value_get_string (value));
      break;
    case PROP_CHILD_PROP:
      bz_dynamic_list_view_set_child_prop (self, g_value_get_string (value));
      break;
    case PROP_OBJECT_PROP:
      bz_dynamic_list_view_set_object_prop (self, g_value_get_string (value));
      break;
    case PROP_MAX_CHILDREN_PER_LINE:
      bz_dynamic_list_view_set_max_children_per_line (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_dynamic_list_view_class_init (BzDynamicListViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_dynamic_list_view_set_property;
  object_class->get_property = bz_dynamic_list_view_get_property;
  object_class->dispose      = bz_dynamic_list_view_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SCROLL] =
      g_param_spec_boolean (
          "scroll",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_NOSCROLL_KIND] =
      g_param_spec_enum (
          "noscroll-kind",
          NULL, NULL,
          BZ_TYPE_DYNAMIC_LIST_VIEW_KIND, BZ_DYNAMIC_LIST_VIEW_KIND_LIST_BOX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_MAX_CHILDREN_PER_LINE] =
      g_param_spec_uint (
          "max-children-per-line",
          NULL, NULL,
          1, G_MAXUINT, 4,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CHILD_TYPE] =
      g_param_spec_string (
          "child-type",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CHILD_PROP] =
      g_param_spec_string (
          "child-prop",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_OBJECT_PROP] =
      g_param_spec_string (
          "object-prop",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_BIND_WIDGET] =
      g_signal_new (
          "bind-widget",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          bz_marshal_VOID__OBJECT_OBJECT,
          G_TYPE_NONE,
          1,
          GTK_TYPE_WIDGET);
  g_signal_set_va_marshaller (
      signals[SIGNAL_BIND_WIDGET],
      G_TYPE_FROM_CLASS (klass),
      bz_marshal_VOID__OBJECT_OBJECTv);

  signals[SIGNAL_UNBIND_WIDGET] =
      g_signal_new (
          "unbind-widget",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          bz_marshal_VOID__OBJECT_OBJECT,
          G_TYPE_NONE,
          1,
          GTK_TYPE_WIDGET);
  g_signal_set_va_marshaller (
      signals[SIGNAL_UNBIND_WIDGET],
      G_TYPE_FROM_CLASS (klass),
      bz_marshal_VOID__OBJECT_OBJECTv);
}

static void
bz_dynamic_list_view_init (BzDynamicListView *self)
{
  self->child_type            = G_TYPE_INVALID;
  self->noscroll_kind         = BZ_DYNAMIC_LIST_VIEW_KIND_LIST_BOX;
  self->max_children_per_line = 4;
}

BzDynamicListView *
bz_dynamic_list_view_new (void)
{
  return g_object_new (BZ_TYPE_DYNAMIC_LIST_VIEW, NULL);
}

GListModel *
bz_dynamic_list_view_get_model (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->model;
}

gboolean
bz_dynamic_list_view_get_scroll (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), FALSE);
  return self->scroll;
}

BzDynamicListViewKind
bz_dynamic_list_view_get_noscroll_kind (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), FALSE);
  return self->noscroll_kind;
}

const char *
bz_dynamic_list_view_get_child_type (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->child_type_string;
}

const char *
bz_dynamic_list_view_get_child_prop (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->child_prop;
}

const char *
bz_dynamic_list_view_get_object_prop (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), NULL);
  return self->object_prop;
}

guint
bz_dynamic_list_view_get_max_children_per_line (BzDynamicListView *self)
{
  g_return_val_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self), 4);
  return self->max_children_per_line;
}

void
bz_dynamic_list_view_set_model (BzDynamicListView *self,
                                GListModel        *model)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);

  if (model != NULL)
    self->model = g_object_ref (model);

  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

void
bz_dynamic_list_view_set_scroll (BzDynamicListView *self,
                                 gboolean           scroll)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  self->scroll = scroll;
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCROLL]);
}

void
bz_dynamic_list_view_set_noscroll_kind (BzDynamicListView    *self,
                                        BzDynamicListViewKind noscroll_kind)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));
  g_return_if_fail (noscroll_kind >= 0 && noscroll_kind < BZ_DYNAMIC_LIST_VIEW_N_KINDS);

  self->noscroll_kind = noscroll_kind;
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NOSCROLL_KIND]);
}

void
bz_dynamic_list_view_set_child_type (BzDynamicListView *self,
                                     const char        *child_type)
{
  GType type = G_TYPE_INVALID;

  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));
  if (child_type != NULL)
    {
      type = g_type_from_name (child_type);
      g_return_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET));
    }

  g_clear_pointer (&self->child_type_string, g_free);

  self->child_type = type;
  if (child_type != NULL)
    self->child_type_string = g_strdup (child_type);

  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD_TYPE]);
}

void
bz_dynamic_list_view_set_child_prop (BzDynamicListView *self,
                                     const char        *child_prop)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  g_clear_pointer (&self->child_prop, g_free);
  if (child_prop != NULL)
    self->child_prop = g_strdup (child_prop);
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD_PROP]);
}

void
bz_dynamic_list_view_set_object_prop (BzDynamicListView *self,
                                      const char        *object_prop)
{
  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));

  g_clear_pointer (&self->object_prop, g_free);
  if (object_prop != NULL)
    self->object_prop = g_strdup (object_prop);
  refresh (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OBJECT_PROP]);
}

void
bz_dynamic_list_view_set_max_children_per_line (BzDynamicListView *self,
                                                guint              max_children)
{
  GtkWidget *child;

  g_return_if_fail (BZ_IS_DYNAMIC_LIST_VIEW (self));
  g_return_if_fail (max_children > 0);

  self->max_children_per_line = max_children;

  child = adw_bin_get_child (ADW_BIN (self));
  if (child != NULL &&
      self->noscroll_kind == BZ_DYNAMIC_LIST_VIEW_KIND_FLOW_BOX &&
      !self->scroll)
    {
      gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (child), max_children);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAX_CHILDREN_PER_LINE]);
}

static void
refresh (BzDynamicListView *self)
{
  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  adw_bin_set_child (ADW_BIN (self), NULL);

  if (self->model == NULL ||
      self->child_prop == NULL ||
      self->child_type == G_TYPE_INVALID)
    return;

  if (self->scroll)
    {
      GtkWidget          *window    = NULL;
      GtkNoSelection     *selection = NULL;
      GtkListItemFactory *factory   = NULL;
      GtkWidget          *view      = NULL;

      window    = gtk_scrolled_window_new ();
      selection = gtk_no_selection_new (g_object_ref (self->model));
      factory   = gtk_signal_list_item_factory_new ();
      view      = gtk_list_view_new (GTK_SELECTION_MODEL (selection), factory);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_widget_add_css_class (view, "navigation-sidebar");
      gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (view), TRUE);

      g_signal_connect_swapped (factory, "setup", G_CALLBACK (list_item_factory_setup), self);
      g_signal_connect_swapped (factory, "teardown", G_CALLBACK (list_item_factory_teardown), self);
      g_signal_connect_swapped (factory, "bind", G_CALLBACK (list_item_factory_bind), self);
      g_signal_connect_swapped (factory, "unbind", G_CALLBACK (list_item_factory_unbind), self);

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (window), view);
      adw_bin_set_child (ADW_BIN (self), window);
    }
  else
    {
      switch (self->noscroll_kind)
        {
        case BZ_DYNAMIC_LIST_VIEW_KIND_LIST_BOX:
          {
            GtkWidget *widget = NULL;

            widget = gtk_list_box_new ();
            gtk_list_box_bind_model (
                GTK_LIST_BOX (widget), self->model,
                (GtkListBoxCreateWidgetFunc) create_child_widget,
                self, NULL);

            adw_bin_set_child (ADW_BIN (self), widget);
          }
          break;
        case BZ_DYNAMIC_LIST_VIEW_KIND_FLOW_BOX:
          {
            GtkWidget *widget = NULL;

            widget = gtk_flow_box_new ();
            gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (widget), TRUE);
            gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (widget), self->max_children_per_line);
            gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (widget), GTK_SELECTION_NONE);
            gtk_flow_box_bind_model (
                GTK_FLOW_BOX (widget), self->model,
                (GtkFlowBoxCreateWidgetFunc) create_child_widget,
                self, NULL);

            adw_bin_set_child (ADW_BIN (self), widget);
          }
          break;
        case BZ_DYNAMIC_LIST_VIEW_KIND_CAROUSEL:
          {
            GtkWidget *widget = NULL;

            widget = adw_carousel_new ();
            adw_carousel_set_allow_scroll_wheel (ADW_CAROUSEL (widget), FALSE);
            g_signal_connect (
                self->model, "items-changed",
                G_CALLBACK (items_changed), self);

            adw_bin_set_child (ADW_BIN (self), widget);
            items_changed (self->model, 0, 0, g_list_model_get_n_items (self->model), self);
          }
          break;
        case BZ_DYNAMIC_LIST_VIEW_N_KINDS:
        default:
          g_assert_not_reached ();
        }
    }
}

static void
list_item_factory_setup (BzDynamicListView        *self,
                         GtkListItem              *item,
                         GtkSignalListItemFactory *factory)
{
  GtkWidget *child = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  child = g_object_new (self->child_type, NULL);
  gtk_list_item_set_child (item, child);
}

static void
list_item_factory_teardown (BzDynamicListView        *self,
                            GtkListItem              *item,
                            GtkSignalListItemFactory *factory)
{
  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);
}

static void
list_item_factory_bind (BzDynamicListView        *self,
                        GtkListItem              *item,
                        GtkSignalListItemFactory *factory)
{
  GObject   *object = NULL;
  GtkWidget *child  = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  object = gtk_list_item_get_item (item);
  child  = gtk_list_item_get_child (item);

  gtk_list_item_set_focusable (item, FALSE);
  gtk_list_item_set_selectable (item, FALSE);
  gtk_list_item_set_activatable (item, FALSE);

  if (self->object_prop != NULL)
    {
      GBinding *binding = NULL;

      binding = g_object_bind_property (
          object, self->object_prop,
          child, self->child_prop,
          G_BINDING_SYNC_CREATE);
      g_object_set_data_full (
          G_OBJECT (item),
          "binding", binding,
          g_object_unref);
    }
  else
    g_object_set (child, self->child_prop, object, NULL);

  g_signal_emit (self, signals[SIGNAL_BIND_WIDGET], 0, child, object);
}

static void
list_item_factory_unbind (BzDynamicListView        *self,
                          GtkListItem              *item,
                          GtkSignalListItemFactory *factory)
{
  GtkWidget *child           = NULL;
  GBinding  *binding         = NULL;
  g_autoptr (GObject) object = NULL;

  g_return_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL);

  child   = gtk_list_item_get_child (item);
  binding = g_object_steal_data (G_OBJECT (item), "binding");

  if (binding != NULL)
    {
      object = g_binding_dup_source (binding);
      g_binding_unbind (binding);
      g_object_unref (binding);
    }
  else
    {
      g_object_get (child, self->child_prop, &object, NULL);
      g_object_set (child, self->child_prop, NULL, NULL);
    }

  g_signal_emit (self, signals[SIGNAL_UNBIND_WIDGET], 0, child, object);
}

static GtkWidget *
create_child_widget (GObject           *object,
                     BzDynamicListView *self)
{
  GtkWidget *widget = NULL;

  g_return_val_if_fail (self->child_type != G_TYPE_INVALID && self->child_prop != NULL, NULL);

  widget = g_object_new (self->child_type, NULL);
  if (self->object_prop != NULL)
    g_object_bind_property (
        object, self->object_prop,
        widget, self->child_prop,
        G_BINDING_SYNC_CREATE);
  else
    g_object_set (widget, self->child_prop, object, NULL);

  gtk_widget_set_receives_default (widget, TRUE);
  g_signal_emit (self, signals[SIGNAL_BIND_WIDGET], 0, widget, object);

  if (self->noscroll_kind == BZ_DYNAMIC_LIST_VIEW_KIND_FLOW_BOX)
    {
      GtkWidget *child = NULL;

      child = gtk_flow_box_child_new ();
      gtk_widget_add_css_class (GTK_WIDGET (child), "disable-adw-flow-box-styling");
      gtk_widget_set_focusable (GTK_WIDGET (child), FALSE);
      gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (child), widget);

      return child;
    }
  else
    return widget;
}

static void
items_changed (GListModel        *model,
               guint              position,
               guint              removed,
               guint              added,
               BzDynamicListView *self)
{
  GtkWidget *carousel = NULL;

  carousel = adw_bin_get_child (ADW_BIN (self));
  g_return_if_fail (ADW_IS_CAROUSEL (carousel));

  for (guint i = 0; i < removed; i++)
    adw_carousel_remove (
        ADW_CAROUSEL (carousel),
        adw_carousel_get_nth_page (ADW_CAROUSEL (carousel), position));

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (GObject) object = NULL;
      GtkWidget *widget          = NULL;

      object = g_list_model_get_item (model, position + i);
      widget = create_child_widget (object, self);
      adw_carousel_insert (ADW_CAROUSEL (carousel), widget, position + i);
    }
}

/* End of bz-dynamic-list-view.c */

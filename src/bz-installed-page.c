/* bz-installed-page.c
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

#include <glib/gi18n.h>

#include "bz-addons-dialog.h"
#include "bz-error.h"
#include "bz-flatpak-entry.h"
#include "bz-installed-page.h"
#include "bz-section-view.h"
#include "bz-state-info.h"

struct _BzInstalledPage
{
  AdwBin parent_instance;

  GListModel  *model;
  BzStateInfo *state;

  GtkSortListModel *sorted;

  /* Template widgets */
  AdwViewStack   *stack;
  GtkListView    *list_view;
  GtkNoSelection *no_selection;
};

G_DEFINE_FINAL_TYPE (BzInstalledPage, bz_installed_page, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_MODEL,
  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_REMOVE,
  SIGNAL_INSTALL,
  SIGNAL_SHOW,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
items_changed (BzInstalledPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model);

static gint
cmp_item (BzEntry         *a,
          BzEntry         *b,
          BzInstalledPage *self);

static void
set_page (BzInstalledPage *self);

static void
bz_installed_page_dispose (GObject *object)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  g_clear_object (&self->state);

  g_clear_object (&self->sorted);

  G_OBJECT_CLASS (bz_installed_page_parent_class)->dispose (object);
}

static void
bz_installed_page_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_installed_page_get_model (self));
      break;
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_installed_page_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_installed_page_set_model (self, g_value_get_object (value));
      break;
    case PROP_STATE:
      g_clear_object (&self->state);
      self->state = g_value_dup_object (value);
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

static void
addon_transact_cb (BzInstalledPage *self,
                   BzEntry         *entry,
                   BzAddonsDialog  *dialog)
{
  gboolean installed = FALSE;

  g_object_get (entry, "installed", &installed, NULL);

  if (installed)
    g_signal_emit (self, signals[SIGNAL_REMOVE], 0, entry);
  else
    g_signal_emit (self, signals[SIGNAL_INSTALL], 0, entry);
}

static void
run_cb (GtkListItem *list_item,
        GtkButton   *button)
{
  BzEntry         *entry    = NULL;
  BzInstalledPage *self     = NULL;
  GtkWidget       *menu_btn = NULL;

  entry = gtk_list_item_get_item (list_item);

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  if (BZ_IS_FLATPAK_ENTRY (entry))
    {
      g_autoptr (GError) local_error = NULL;
      gboolean result                = FALSE;

      result = bz_flatpak_entry_launch (
          BZ_FLATPAK_ENTRY (entry),
          BZ_FLATPAK_INSTANCE (bz_state_info_get_backend (self->state)),
          &local_error);
      if (!result)
        {
          GtkWidget *window = NULL;

          window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
          if (window != NULL)
            bz_show_error_for_widget (window, local_error->message);
        }
    }
}

static void
support_cb (GtkListItem *list_item,
            GtkButton   *button)
{
  BzEntry    *entry = NULL;
  const char *url   = NULL;

  entry = gtk_list_item_get_item (list_item);

  url = bz_entry_get_donation_url (entry);
  g_app_info_launch_default_for_uri (url, NULL, NULL);
}

static void
install_addons_cb (GtkListItem *list_item,
                   GtkButton   *button)
{
  BzEntry   *entry               = NULL;
  GtkWidget *menu_btn            = NULL;
  g_autoptr (GListModel) model   = NULL;
  AdwDialog       *addons_dialog = NULL;
  BzInstalledPage *self          = NULL;

  entry = gtk_list_item_get_item (list_item);

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  model = bz_application_map_factory_generate (
      bz_state_info_get_entry_factory (self->state),
      bz_entry_get_addons (entry));

  addons_dialog = bz_addons_dialog_new (entry, model);
  adw_dialog_set_content_width (addons_dialog, 750);
  gtk_widget_set_size_request (GTK_WIDGET (addons_dialog), 350, -1);
  g_signal_connect_swapped (addons_dialog, "transact", G_CALLBACK (addon_transact_cb), self);

  adw_dialog_present (addons_dialog, GTK_WIDGET (self));
}

static void
view_store_page_cb (GtkListItem *list_item,
                    GtkButton   *button)
{
  BzEntry   *entry    = NULL;
  GtkWidget *menu_btn = NULL;
  GtkWidget *self     = NULL;

  entry = gtk_list_item_get_item (list_item);

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSTALLED_PAGE);
  g_assert (self != NULL);

  g_signal_emit (self, signals[SIGNAL_SHOW], 0, entry);
}

static void
remove_cb (GtkListItem *list_item,
           GtkButton   *button)
{
  BzEntry   *entry    = NULL;
  GtkWidget *menu_btn = NULL;
  GtkWidget *self     = NULL;

  entry = gtk_list_item_get_item (list_item);

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  self = gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_INSTALLED_PAGE);
  g_assert (self != NULL);

  g_signal_emit (self, signals[SIGNAL_REMOVE], 0, entry);
}

static void
edit_permissions_cb (GtkListItem *list_item,
                     GtkButton   *button)
{
  // BzEntry   *item     = NULL;
  GtkWidget *menu_btn = NULL;

  // item = gtk_list_item_get_item (list_item);

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  gtk_widget_activate_action (GTK_WIDGET (button), "app.flatseal", NULL);
}

static void
bz_installed_page_class_init (BzInstalledPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_installed_page_dispose;
  object_class->get_property = bz_installed_page_get_property;
  object_class->set_property = bz_installed_page_set_property;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_REMOVE] =
      g_signal_new (
          "remove",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_REMOVE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  signals[SIGNAL_SHOW] =
      g_signal_new (
          "show-entry",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          BZ_TYPE_ENTRY);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SHOW],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_SECTION_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-installed-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, stack);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, list_view);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, no_selection);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
  gtk_widget_class_bind_template_callback (widget_class, support_cb);
  gtk_widget_class_bind_template_callback (widget_class, view_store_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_addons_cb);
  gtk_widget_class_bind_template_callback (widget_class, edit_permissions_cb);
  gtk_widget_class_bind_template_callback (widget_class, addon_transact_cb);
}

static void
bz_installed_page_init (BzInstalledPage *self)
{
  GtkCustomSorter *custom_sorter = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  custom_sorter = gtk_custom_sorter_new ((GCompareDataFunc) cmp_item, self, NULL);
  self->sorted  = gtk_sort_list_model_new (NULL, GTK_SORTER (custom_sorter));
  gtk_no_selection_set_model (self->no_selection, G_LIST_MODEL (self->sorted));
}

GtkWidget *
bz_installed_page_new (void)
{
  return g_object_new (BZ_TYPE_INSTALLED_PAGE, NULL);
}

void
bz_installed_page_set_model (BzInstalledPage *self,
                             GListModel      *model)
{
  g_return_if_fail (BZ_IS_INSTALLED_PAGE (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  if (model != NULL)
    {
      self->model = g_object_ref (model);
      g_signal_connect_swapped (model, "items-changed", G_CALLBACK (items_changed), self);
    }
  gtk_sort_list_model_set_model (self->sorted, model);
  set_page (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

GListModel *
bz_installed_page_get_model (BzInstalledPage *self)
{
  g_return_val_if_fail (BZ_IS_INSTALLED_PAGE (self), NULL);
  return self->model;
}

static void
items_changed (BzInstalledPage *self,
               guint            position,
               guint            removed,
               guint            added,
               GListModel      *model)
{
  set_page (self);
}

static gint
cmp_item (BzEntry         *a,
          BzEntry         *b,
          BzInstalledPage *self)
{
  gboolean    a_is_application = FALSE;
  gboolean    b_is_application = FALSE;
  gboolean    a_is_addon       = FALSE;
  gboolean    b_is_addon       = FALSE;
  const char *title_a          = NULL;
  const char *title_b          = NULL;

  a_is_application = bz_entry_is_of_kinds (a, BZ_ENTRY_KIND_APPLICATION);
  b_is_application = bz_entry_is_of_kinds (b, BZ_ENTRY_KIND_APPLICATION);
  a_is_addon       = bz_entry_is_of_kinds (a, BZ_ENTRY_KIND_ADDON);
  b_is_addon       = bz_entry_is_of_kinds (b, BZ_ENTRY_KIND_ADDON);
  title_a          = bz_entry_get_title (a);
  title_b          = bz_entry_get_title (b);

  if (a_is_application && !b_is_application)
    return -1;
  else if (!a_is_application && b_is_application)
    return 1;
  else if (a_is_addon && !b_is_addon)
    return -1;
  else if (!a_is_addon && b_is_addon)
    return 1;
  else
    return g_strcmp0 (title_a, title_b);
}

static void
set_page (BzInstalledPage *self)
{
  if (self->model != NULL &&
      g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0)
    adw_view_stack_set_visible_child_name (self->stack, "content");
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");
}

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
#include "bz-env.h"
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

  /* Template widgets */
  AdwViewStack *stack;
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

static void
set_page (BzInstalledPage *self);

static BzEntry *
find_entry_in_group (BzEntryGroup *group,
                     gboolean (*test) (BzEntry *entry),
                     GtkWidget *window,
                     GError   **error);

static gboolean
test_is_runnable (BzEntry *entry);

static gboolean
test_is_support (BzEntry *entry);

static gboolean
test_has_addons (BzEntry *entry);

static void
bz_installed_page_dispose (GObject *object)
{
  BzInstalledPage *self = BZ_INSTALLED_PAGE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);
  g_clear_object (&self->state);

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

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
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

static DexFuture *
run_fiber (GtkListItem *list_item)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *self          = NULL;
  GtkWidget       *window        = NULL;
  BzEntryGroup    *group         = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  gboolean result                = FALSE;

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  group = gtk_list_item_get_item (list_item);
  entry = find_entry_in_group (group, test_is_runnable, window, &local_error);
  if (entry == NULL)
    goto err;

  result = bz_flatpak_entry_launch (
      BZ_FLATPAK_ENTRY (entry),
      BZ_FLATPAK_INSTANCE (bz_state_info_get_backend (self->state)),
      &local_error);
  if (!result)
    goto err;

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
run_cb (GtkListItem *list_item,
        GtkButton   *button)
{
  GtkWidget *menu_btn = NULL;

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) run_fiber,
      g_object_ref (list_item),
      g_object_unref));
}

static DexFuture *
support_fiber (GtkListItem *list_item)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *self          = NULL;
  GtkWidget       *window        = NULL;
  BzEntryGroup    *group         = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  const char *url                = NULL;

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  group = gtk_list_item_get_item (list_item);
  entry = find_entry_in_group (group, test_is_support, window, &local_error);
  if (entry == NULL)
    goto err;

  url = bz_entry_get_donation_url (entry);
  g_app_info_launch_default_for_uri (url, NULL, NULL);

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
support_cb (GtkListItem *list_item,
            GtkButton   *button)
{
  GtkWidget *menu_btn = NULL;

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) support_fiber,
      g_object_ref (list_item),
      g_object_unref));
}

static DexFuture *
install_addons_fiber (GtkListItem *list_item)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *self          = NULL;
  GtkWidget       *window        = NULL;
  BzEntryGroup    *group         = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  g_autoptr (GListModel) model   = NULL;
  AdwDialog *addons_dialog       = NULL;

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  group = gtk_list_item_get_item (list_item);
  entry = find_entry_in_group (group, test_has_addons, window, &local_error);
  if (entry == NULL)
    goto err;

  model = bz_application_map_factory_generate (
      bz_state_info_get_entry_factory (self->state),
      bz_entry_get_addons (entry));

  addons_dialog = bz_addons_dialog_new (entry, model);
  adw_dialog_set_content_width (addons_dialog, 750);
  gtk_widget_set_size_request (GTK_WIDGET (addons_dialog), 350, -1);
  g_signal_connect_swapped (addons_dialog, "transact", G_CALLBACK (addon_transact_cb), self);

  adw_dialog_present (addons_dialog, GTK_WIDGET (self));

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
install_addons_cb (GtkListItem *list_item,
                   GtkButton   *button)
{
  GtkWidget *menu_btn = NULL;

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) install_addons_fiber,
      g_object_ref (list_item),
      g_object_unref));
}

static DexFuture *
view_store_page_fiber (GtkListItem *list_item)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *self          = NULL;
  GtkWidget       *window        = NULL;
  BzEntryGroup    *group         = NULL;
  g_autoptr (BzEntry) entry      = NULL;

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  group = gtk_list_item_get_item (list_item);
  entry = find_entry_in_group (group, NULL, window, &local_error);
  if (entry == NULL)
    goto err;

  g_signal_emit (self, signals[SIGNAL_SHOW], 0, entry);

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
view_store_page_cb (GtkListItem *list_item,
                    GtkButton   *button)
{
  GtkWidget *menu_btn = NULL;

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) view_store_page_fiber,
      g_object_ref (list_item),
      g_object_unref));
}

static DexFuture *
remove_fiber (GtkListItem *list_item)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *self          = NULL;
  GtkWidget       *window        = NULL;
  BzEntryGroup    *group         = NULL;
  g_autoptr (BzEntry) entry      = NULL;

  self = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (gtk_list_item_get_child (list_item), BZ_TYPE_INSTALLED_PAGE));
  g_assert (self != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  group = gtk_list_item_get_item (list_item);
  entry = find_entry_in_group (group, NULL, window, &local_error);
  if (entry == NULL)
    goto err;

  g_signal_emit (self, signals[SIGNAL_REMOVE], 0, entry);

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
remove_cb (GtkListItem *list_item,
           GtkButton   *button)
{
  GtkWidget *menu_btn = NULL;

  menu_btn = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU_BUTTON);
  if (menu_btn != NULL)
    gtk_menu_button_set_active (GTK_MENU_BUTTON (menu_btn), FALSE);

  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) remove_fiber,
      g_object_ref (list_item),
      g_object_unref));
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
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-installed-page.ui");
  gtk_widget_class_bind_template_child (widget_class, BzInstalledPage, stack);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
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
  gtk_widget_init_template (GTK_WIDGET (self));
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

static void
set_page (BzInstalledPage *self)
{
  if (self->model != NULL &&
      g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 0)
    adw_view_stack_set_visible_child_name (self->stack, "content");
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");
}

/* Needs to be run in a fiber */
static BzEntry *
find_entry_in_group (BzEntryGroup *group,
                     gboolean (*test) (BzEntry *entry),
                     GtkWidget *window,
                     GError   **error)
{
  g_autoptr (GListModel) model     = NULL;
  guint n_items                    = 0;
  g_autoptr (GPtrArray) candidates = NULL;

  model = dex_await_object (bz_entry_group_dup_all_into_model (group), error);
  if (model == NULL)
    return NULL;
  n_items = g_list_model_get_n_items (model);

  candidates = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (model, i);

      if (bz_entry_is_installed (entry) &&
          (test == NULL || test (entry)))
        g_ptr_array_add (candidates, g_steal_pointer (&entry));
    }

  if (candidates->len == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_UNKNOWN,
                   "BUG: No entry candidates satisfied this test condition");
      return NULL;
    }
  else if (candidates->len == 1)
    return g_ptr_array_steal_index_fast (candidates, 0);
  else if (window != NULL)
    {
      AdwDialog       *alert    = NULL;
      g_autofree char *response = NULL;

      alert = adw_alert_dialog_new (NULL, NULL);
      adw_alert_dialog_set_prefer_wide_layout (ADW_ALERT_DIALOG (alert), TRUE);
      adw_alert_dialog_format_heading (
          ADW_ALERT_DIALOG (alert),
          _ ("Choose an Installation"));
      adw_alert_dialog_format_body (
          ADW_ALERT_DIALOG (alert),
          _ ("You have multiple versions of this app installed. Which "
             "one would you like to proceed with? "));
      adw_alert_dialog_add_responses (
          ADW_ALERT_DIALOG (alert),
          "cancel", _ ("Cancel"),
          NULL);
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");
      adw_alert_dialog_set_response_appearance (
          ADW_ALERT_DIALOG (alert), "cancel", ADW_RESPONSE_DESTRUCTIVE);

      for (guint i = 0; i < candidates->len; i++)
        {
          BzEntry    *entry     = NULL;
          const char *unique_id = NULL;

          entry     = g_ptr_array_index (candidates, i);
          unique_id = bz_entry_get_unique_id (entry);

          adw_alert_dialog_add_responses (
              ADW_ALERT_DIALOG (alert),
              unique_id, unique_id,
              NULL);
          if (i == 0)
            adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), unique_id);
        }

      adw_dialog_present (alert, GTK_WIDGET (window));
      response = dex_await_string (
          bz_make_alert_dialog_future (ADW_ALERT_DIALOG (alert)),
          NULL);

      if (response != NULL)
        {
          for (guint i = 0; i < candidates->len; i++)
            {
              BzEntry    *entry     = NULL;
              const char *unique_id = NULL;

              entry     = g_ptr_array_index (candidates, i);
              unique_id = bz_entry_get_unique_id (entry);

              if (g_strcmp0 (unique_id, response) == 0)
                return g_object_ref (entry);
            }
        }
    }

  return NULL;
}

static gboolean
test_is_runnable (BzEntry *entry)
{
  return BZ_IS_FLATPAK_ENTRY (entry);
}

static gboolean
test_is_support (BzEntry *entry)
{
  return bz_entry_get_donation_url (entry) != NULL;
}

static gboolean
test_has_addons (BzEntry *entry)
{
  GListModel *model = NULL;

  model = bz_entry_get_addons (entry);
  return model != NULL && g_list_model_get_n_items (model) > 0;
}

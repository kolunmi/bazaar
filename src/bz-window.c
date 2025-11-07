/* bz-window.c
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

// This file is an utter mess

#include <glib/gi18n.h>

#include "bz-browse-widget.h"
#include "bz-comet-overlay.h"
#include "bz-entry-group.h"
#include "bz-error.h"
#include "bz-flathub-page.h"
#include "bz-full-view.h"
#include "bz-global-progress.h"
#include "bz-installed-page.h"
#include "bz-progress-bar.h"
#include "bz-search-widget.h"
#include "bz-transaction-manager.h"
#include "bz-update-dialog.h"
#include "bz-util.h"
#include "bz-window.h"

struct _BzWindow
{
  AdwApplicationWindow parent_instance;

  BzStateInfo *state;

  GtkEventController *key_controller;

  gboolean breakpoint_applied;

  DexFuture *transact_future;

  /* Template widgets */
  BzCometOverlay      *comet_overlay;
  AdwOverlaySplitView *split_view;
  AdwViewStack        *transactions_stack;
  AdwNavigationView   *navigation_view;
  BzFullView          *full_view;
  GtkToggleButton     *toggle_transactions;
  GtkToggleButton     *toggle_transactions_sidebar;
  BzSearchWidget      *search_widget;
  GtkButton           *update_button;
  GtkToggleButton     *transactions_pause;
  GtkButton           *transactions_stop;
  GtkButton           *transactions_clear;
  AdwToastOverlay     *toasts;
  AdwViewStack        *main_view_stack;
  GtkStack            *main_stack;
  // GtkButton           *refresh;
};

G_DEFINE_FINAL_TYPE (BzWindow, bz_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
  PROP_0,

  PROP_STATE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    transact,
    Transact,
    {
      BzWindow     *self;
      BzEntryGroup *group;
      gboolean      remove;
      gboolean      auto_confirm;
      GtkWidget    *source;
    },
    BZ_RELEASE_DATA (group, g_object_unref);
    BZ_RELEASE_DATA (source, g_object_unref))

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self);

static void
update_dialog_response (BzUpdateDialog *dialog,
                        const char     *response,
                        BzWindow       *self);

static void
transact (BzWindow  *self,
          BzEntry   *entry,
          gboolean   remove,
          GtkWidget *source);

static void
try_transact (BzWindow     *self,
              BzEntry      *entry,
              BzEntryGroup *group,
              gboolean      remove,
              gboolean      auto_confirm,
              GtkWidget    *source);

static DexFuture *
ready_to_transact (DexFuture    *future,
                   TransactData *data);

static void
update (BzWindow *self,
        BzEntry **updates,
        guint     n_updates);

static void
search (BzWindow   *self,
        const char *text);

static void
check_transactions (BzWindow *self);

static void
set_page (BzWindow *self);

static void
bz_window_dispose (GObject *object)
{
  BzWindow *self = BZ_WINDOW (object);

  dex_clear (&self->transact_future);
  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_window_parent_class)->dispose (object);
}

static void
bz_window_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BzWindow *self = BZ_WINDOW (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, self->state);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_window_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  // BzWindow *self = BZ_WINDOW (object);

  switch (prop_id)
    {
    case PROP_STATE:
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
is_double_zero (gpointer object,
                double   value)
{
  return value == 0.0;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static void
browser_group_selected_cb (BzWindow     *self,
                           BzEntryGroup *group,
                           gpointer      browser)
{
  bz_window_show_group (self, group);
}

static void
search_widget_select_cb (BzWindow       *self,
                         BzEntryGroup   *group,
                         gboolean        should_install,
                         BzSearchWidget *search)
{
  if (should_install)
    {
      int      installable = 0;
      int      removable   = 0;
      gboolean remove      = FALSE;

      g_object_get (
          group,
          "installable", &installable,
          "removable", &removable,
          NULL);

      remove = installable == 0 && removable > 0;
      try_transact (self, NULL, group, remove, FALSE, NULL);
    }
  else
    {
      bz_window_show_group (self, group);
    }
}

static void
full_view_install_cb (BzWindow   *self,
                      GtkWidget  *source,
                      BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), FALSE, TRUE, source);
}

static void
full_view_remove_cb (BzWindow   *self,
                     GtkWidget  *source,
                     BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), TRUE, TRUE, source);
}

static void
install_addon_cb (BzWindow   *self,
                  BzEntry    *entry,
                  BzFullView *view)
{
  try_transact (self, entry, NULL, FALSE, TRUE, NULL);
}

static void
remove_addon_cb (BzWindow   *self,
                 BzEntry    *entry,
                 BzFullView *view)
{
  try_transact (self, entry, NULL, TRUE, TRUE, NULL);
}

static void
installed_page_show_cb (BzWindow   *self,
                        BzEntry    *entry,
                        BzFullView *view)
{
  g_autoptr (BzEntryGroup) group = NULL;

  group = bz_application_map_factory_convert_one (
      bz_state_info_get_application_factory (self->state),
      gtk_string_object_new (bz_entry_get_id (entry)));

  if (group != NULL)
    bz_window_show_group (self, group);
}

static void
page_toggled_cb (BzWindow       *self,
                 GParamSpec     *pspec,
                 AdwToggleGroup *toggles)
{
  set_page (self);
}

static void
update_flathub_style (BzWindow *self)
{
  AdwNavigationPage *visible_page = NULL;
  const char        *page_tag     = NULL;
  const char        *stack_page   = NULL;

  visible_page = adw_navigation_view_get_visible_page (self->navigation_view);

  if (visible_page != NULL)
    {
      page_tag = adw_navigation_page_get_tag (visible_page);

      if (page_tag != NULL && strstr (page_tag, "flathub") != NULL)
        {
          gtk_widget_add_css_class (GTK_WIDGET (self), "flathub");
          return;
        }

      if (g_strcmp0 (page_tag, "main") == 0)
        {
          stack_page = adw_view_stack_get_visible_child_name (self->main_view_stack);
          if (g_strcmp0 (stack_page, "flathub") == 0)
            {
              gtk_widget_add_css_class (GTK_WIDGET (self), "flathub");
              return;
            }
        }
    }

  gtk_widget_remove_css_class (GTK_WIDGET (self), "flathub");
}

static void
visible_page_changed_cb (BzWindow          *self,
                         GParamSpec        *pspec,
                         AdwNavigationView *navigation_view)
{
  update_flathub_style (self);
}

static void
main_view_stack_changed_cb (BzWindow     *self,
                            GParamSpec   *pspec,
                            AdwViewStack *stack)
{
  update_flathub_style (self);
}

static void
browse_flathub_cb (BzWindow       *self,
                   BzBrowseWidget *widget)
{
  adw_view_stack_set_visible_child_name (self->main_view_stack, "flathub");
}

static void
breakpoint_apply_cb (BzWindow      *self,
                     AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = TRUE;

  gtk_widget_add_css_class (GTK_WIDGET (self), "narrow");
}

static void
breakpoint_unapply_cb (BzWindow      *self,
                       AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = FALSE;

  gtk_widget_remove_css_class (GTK_WIDGET (self), "narrow");
}

static void
pause_transactions_cb (BzWindow        *self,
                       GtkToggleButton *toggle)
{
  gboolean paused = FALSE;

  paused = gtk_toggle_button_get_active (toggle);
  bz_transaction_manager_set_paused (
      bz_state_info_get_transaction_manager (self->state), paused);
  check_transactions (self);
}

static void
stop_transactions_cb (BzWindow  *self,
                      GtkButton *button)
{
  bz_transaction_manager_set_paused (bz_state_info_get_transaction_manager (self->state), TRUE);
  bz_transaction_manager_cancel_current (bz_state_info_get_transaction_manager (self->state));
}

static void
update_cb (BzWindow  *self,
           GtkButton *button)
{
  /* if the button is clickable, there have to be updates */
  bz_window_push_update_dialog (self);
}

static void
transactions_clear_cb (BzWindow  *self,
                       GtkButton *button)
{
  bz_transaction_manager_clear_finished (
      bz_state_info_get_transaction_manager (self->state));
}

static void
action_escape (GtkWidget  *widget,
               const char *action_name,
               GVariant   *parameter)
{
  BzWindow   *self    = BZ_WINDOW (widget);
  GListModel *stack   = NULL;
  guint       n_pages = 0;

  stack   = adw_navigation_view_get_navigation_stack (self->navigation_view);
  n_pages = g_list_model_get_n_items (stack);

  adw_navigation_view_pop (self->navigation_view);
  if (n_pages <= 2)
    {
      gtk_toggle_button_set_active (self->toggle_transactions, FALSE);
      set_page (self);
    }
}

static void
bz_window_class_init (BzWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_window_dispose;
  object_class->get_property = bz_window_get_property;
  object_class->set_property = bz_window_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_COMET_OVERLAY);
  g_type_ensure (BZ_TYPE_SEARCH_WIDGET);
  g_type_ensure (BZ_TYPE_GLOBAL_PROGRESS);
  g_type_ensure (BZ_TYPE_PROGRESS_BAR);
  g_type_ensure (BZ_TYPE_BROWSE_WIDGET);
  g_type_ensure (BZ_TYPE_FULL_VIEW);
  g_type_ensure (BZ_TYPE_INSTALLED_PAGE);
  g_type_ensure (BZ_TYPE_FLATHUB_PAGE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-window.ui");
  gtk_widget_class_bind_template_child (widget_class, BzWindow, comet_overlay);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, navigation_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, full_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toasts);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toggle_transactions);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toggle_transactions_sidebar);
  // gtk_widget_class_bind_template_child (widget_class, BzWindow, refresh);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search_widget);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, update_button);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_pause);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stop);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_clear);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, main_view_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, main_stack);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_double_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, browser_group_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_widget_select_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_install_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_addon_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_addon_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_show_cb);
  gtk_widget_class_bind_template_callback (widget_class, page_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_unapply_cb);
  gtk_widget_class_bind_template_callback (widget_class, pause_transactions_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_transactions_cb);
  // gtk_widget_class_bind_template_callback (widget_class, refresh_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_cb);
  gtk_widget_class_bind_template_callback (widget_class, transactions_clear_cb);
  gtk_widget_class_bind_template_callback (widget_class, visible_page_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, main_view_stack_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, browse_flathub_cb);

  gtk_widget_class_install_action (widget_class, "escape", NULL, action_escape);
}

static gboolean
key_pressed (BzWindow              *self,
             guint                  keyval,
             guint                  keycode,
             GdkModifierType        state,
             GtkEventControllerKey *controller)
{
  gunichar unichar = 0;
  char     buf[32] = { 0 };

  /* Ignore if this is a modifier-shortcut of some sort */
  if (state & ~(GDK_NO_MODIFIER_MASK | GDK_SHIFT_MASK))
    return FALSE;

  unichar = gdk_keyval_to_unicode (keyval);
  if (unichar == 0 || !g_unichar_isgraph (unichar))
    return FALSE;
  g_unichar_to_utf8 (unichar, buf);

  adw_view_stack_set_visible_child_name (self->main_view_stack, "search");
  return bz_search_widget_ensure_active (self->search_widget, buf);
}

static void
bz_window_init (BzWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  adw_view_stack_set_visible_child_name (self->main_view_stack, "flathub");

  self->key_controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (self->key_controller, "key-pressed", G_CALLBACK (key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), self->key_controller);
}

static void
app_busy_changed (BzWindow    *self,
                  GParamSpec  *pspec,
                  BzStateInfo *info)
{
  bz_search_widget_refresh (self->search_widget);
  set_page (self);
}

static void
transactions_active_changed (BzWindow             *self,
                             GParamSpec           *pspec,
                             BzTransactionManager *manager)
{
  check_transactions (self);
}

static void
has_transactions_changed (BzWindow             *self,
                          GParamSpec           *pspec,
                          BzTransactionManager *manager)
{
  check_transactions (self);
}

static void
has_inputs_changed (BzWindow          *self,
                    GParamSpec        *pspec,
                    BzContentProvider *provider)
{
  if (!bz_content_provider_get_has_inputs (provider))
    adw_view_stack_set_visible_child_name (self->main_view_stack, "flathub");
}

static void
checking_for_updates_changed (BzWindow    *self,
                              GParamSpec  *pspec,
                              BzStateInfo *info)
{
  gboolean busy                 = FALSE;
  gboolean checking_for_updates = FALSE;
  gboolean has_updates          = FALSE;

  busy                 = bz_state_info_get_busy (info);
  checking_for_updates = bz_state_info_get_checking_for_updates (info);
  has_updates          = bz_state_info_get_available_updates (info) != NULL;

  if (!busy && !checking_for_updates)
    {
      if (has_updates)
        {
          bz_comet_overlay_set_pulse_color (self->comet_overlay, NULL);
          bz_comet_overlay_pulse_child (
              self->comet_overlay,
              GTK_WIDGET (self->update_button));
        }
      /* TODO: this can be intrusive when idling checking for updates */
      // else
      //   adw_toast_overlay_add_toast (
      //       self->toasts,
      //       adw_toast_new_format (_ ("Up to date!")));
    }
}

static void
install_confirmation_response (AdwAlertDialog *alert,
                               gchar          *response,
                               BzWindow       *self)
{
  gboolean should_install               = FALSE;
  gboolean should_remove                = FALSE;
  g_autoptr (GtkWidget) cb_source       = NULL;
  g_autoptr (BzEntry) cb_entry          = NULL;
  g_autoptr (BzEntryGroup) cb_group     = NULL;
  g_autoptr (GListModel) cb_group_model = NULL;

  should_install = g_strcmp0 (response, "install") == 0;
  should_remove  = g_strcmp0 (response, "remove") == 0;

  cb_source      = g_object_steal_data (G_OBJECT (alert), "source");
  cb_entry       = g_object_steal_data (G_OBJECT (alert), "entry");
  cb_group       = g_object_steal_data (G_OBJECT (alert), "group");
  cb_group_model = g_object_steal_data (G_OBJECT (alert), "model");

  if (should_install || should_remove)
    {
      if (cb_group != NULL)
        {
          GPtrArray *checks = NULL;

          checks = g_object_get_data (G_OBJECT (alert), "checks");
          if (checks != NULL)
            {
              guint n_entries           = 0;
              g_autoptr (BzEntry) entry = NULL;

              n_entries = g_list_model_get_n_items (cb_group_model);
              for (guint i = 0; i < n_entries; i++)
                {
                  GtkCheckButton *check = NULL;

                  check = g_ptr_array_index (checks, i);
                  if (check != NULL && gtk_check_button_get_active (check))
                    {
                      entry = g_list_model_get_item (cb_group_model, i);
                      break;
                    }
                }

              if (entry != NULL)
                transact (self, entry, should_remove, cb_source);
            }
        }
      else if (cb_entry != NULL)
        transact (self, cb_entry, should_remove, cb_source);
    }
}

static void
update_dialog_response (BzUpdateDialog *dialog,
                        const char     *response,
                        BzWindow       *self)
{
  g_autoptr (GListModel) accepted_model = NULL;

  accepted_model = bz_update_dialog_was_accepted (dialog);
  if (accepted_model != NULL)
    {
      GListModel          *updates     = NULL;
      guint                n_updates   = 0;
      g_autofree BzEntry **updates_buf = NULL;

      updates     = bz_state_info_get_available_updates (self->state);
      n_updates   = g_list_model_get_n_items (updates);
      updates_buf = g_malloc_n (n_updates, sizeof (*updates_buf));

      for (guint i = 0; i < n_updates; i++)
        updates_buf[i] = g_list_model_get_item (updates, i);
      update (self, updates_buf, n_updates);

      for (guint i = 0; i < n_updates; i++)
        g_object_unref (updates_buf[i]);
      bz_state_info_set_available_updates (
          self->state, NULL);
    }
}

BzWindow *
bz_window_new (BzStateInfo *state)
{
  BzWindow *window = NULL;

  g_return_val_if_fail (BZ_IS_STATE_INFO (state), NULL);

  window        = g_object_new (BZ_TYPE_WINDOW, NULL);
  window->state = g_object_ref (state);

  g_signal_connect_object (state,
                           "notify::busy",
                           G_CALLBACK (app_busy_changed),
                           window, G_CONNECT_SWAPPED);
  g_signal_connect_object (state,
                           "notify::checking-for-updates",
                           G_CALLBACK (checking_for_updates_changed),
                           window, G_CONNECT_SWAPPED);

  /* these seem unsafe but BzApplication never
   * changes the objects we are connecting to
   */
  g_signal_connect_object (bz_state_info_get_transaction_manager (state),
                           "notify::active",
                           G_CALLBACK (transactions_active_changed),
                           window, G_CONNECT_SWAPPED);
  g_signal_connect_object (bz_state_info_get_transaction_manager (state),
                           "notify::has-transactions",
                           G_CALLBACK (has_transactions_changed),
                           window, G_CONNECT_SWAPPED);
  g_signal_connect_object (bz_state_info_get_curated_provider (state),
                           "notify::has-inputs",
                           G_CALLBACK (has_inputs_changed),
                           window, G_CONNECT_SWAPPED);

  g_object_notify_by_pspec (G_OBJECT (window), props[PROP_STATE]);

  set_page (window);
  check_transactions (window);
  return window;
}

void
bz_window_search (BzWindow   *self,
                  const char *text)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  search (self, text);
}

void
bz_window_toggle_transactions (BzWindow *self)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  gtk_toggle_button_set_active (
      self->toggle_transactions,
      !gtk_toggle_button_get_active (
          self->toggle_transactions));
}

void
bz_window_push_update_dialog (BzWindow *self)
{
  GListModel *available_updates = NULL;
  AdwDialog  *update_dialog     = NULL;

  g_return_if_fail (BZ_IS_WINDOW (self));

  available_updates = bz_state_info_get_available_updates (self->state);
  g_return_if_fail (available_updates != NULL);

  update_dialog = bz_update_dialog_new (available_updates);
  adw_dialog_set_content_width (update_dialog, 750);
  g_signal_connect (update_dialog, "response", G_CALLBACK (update_dialog_response), self);

  adw_dialog_present (update_dialog, GTK_WIDGET (self));
}

void
bz_window_show_entry (BzWindow *self,
                      BzEntry  *entry)
{
  /* TODO: IMPLEMENT ME! */
  bz_show_error_for_widget (
      GTK_WIDGET (self),
      _ ("The ability to inspect and install local .flatpak bundle files is coming soon! "
         "In the meantime, try running\n\n"
         "flatpak install --bundle your-bundle.flatpak\n\n"
         "on the command line."));
}

void
bz_window_show_group (BzWindow     *self,
                      BzEntryGroup *group)
{
  AdwNavigationPage *visible_page = NULL;

  g_return_if_fail (BZ_IS_WINDOW (self));
  g_return_if_fail (BZ_IS_ENTRY_GROUP (group));

  bz_full_view_set_entry_group (self->full_view, group);

  visible_page = adw_navigation_view_get_visible_page (self->navigation_view);
  if (visible_page != adw_navigation_view_find_page (self->navigation_view, "view"))
    adw_navigation_view_push_by_tag (self->navigation_view, "view");
}

void
bz_window_add_toast (BzWindow *self,
                     AdwToast *toast)
{
  g_return_if_fail (BZ_IS_WINDOW (self));
  g_return_if_fail (ADW_IS_TOAST (toast));

  adw_toast_overlay_add_toast (self->toasts, toast);
}

BzStateInfo *
bz_window_get_state_info (BzWindow *self)
{
  g_return_val_if_fail (BZ_IS_WINDOW (self), NULL);
  return self->state;
}

static void
transact (BzWindow  *self,
          BzEntry   *entry,
          gboolean   remove,
          GtkWidget *source)
{
  g_autoptr (BzTransaction) transaction = NULL;
  GdkPaintable *icon                    = NULL;
  GtkWidget    *transaction_target      = NULL;

  if (remove)
    transaction = bz_transaction_new_full (
        NULL, 0,
        NULL, 0,
        &entry, 1);
  else
    transaction = bz_transaction_new_full (
        &entry, 1,
        NULL, 0,
        NULL, 0);

  bz_transaction_manager_add (
      bz_state_info_get_transaction_manager (self->state),
      transaction);

  if (source == NULL)
    source = GTK_WIDGET (self->navigation_view);

  if (adw_overlay_split_view_get_show_sidebar (self->split_view))
    transaction_target = GTK_WIDGET (self->toggle_transactions_sidebar);
  else
    transaction_target = GTK_WIDGET (self->toggle_transactions);

  icon = bz_entry_get_icon_paintable (entry);
  if (icon != NULL)
    {
      g_autoptr (BzComet) comet = NULL;

      if (remove)
        {
          AdwStyleManager *style_manager = adw_style_manager_get_default ();
          gboolean         is_dark       = adw_style_manager_get_dark (style_manager);
          GdkRGBA          destructive_color;

          if (is_dark)
            destructive_color = (GdkRGBA) { 0.3, 0.2, 0.21, 0.6 };
          else
            destructive_color = (GdkRGBA) { 0.95, 0.84, 0.84, 0.6 };

          bz_comet_overlay_set_pulse_color (self->comet_overlay, &destructive_color);
        }
      else
        {
          bz_comet_overlay_set_pulse_color (self->comet_overlay, NULL);
        }

      comet = g_object_new (
          BZ_TYPE_COMET,
          "from", remove ? transaction_target : source,
          "to", remove ? source : transaction_target,
          "paintable", icon,
          NULL);
      bz_comet_overlay_spawn (self->comet_overlay, comet);
    }
}

static void
try_transact (BzWindow     *self,
              BzEntry      *entry,
              BzEntryGroup *group,
              gboolean      remove,
              gboolean      auto_confirm,
              GtkWidget    *source)
{
  g_autoptr (DexFuture) base_future = NULL;
  g_autoptr (TransactData) data     = NULL;

  g_return_if_fail (entry != NULL || group != NULL);

  if (bz_state_info_get_busy (self->state))
    {
      adw_toast_overlay_add_toast (
          self->toasts,
          adw_toast_new_format (_ ("Can't do that right now!")));
      return;
    }

  if (group != NULL)
    base_future = bz_entry_group_dup_all_into_model (group);
  else if (entry != NULL)
    base_future = dex_future_new_for_object (entry);

  data               = transact_data_new ();
  data->self         = self;
  data->group        = group != NULL ? g_object_ref (group) : NULL;
  data->remove       = remove;
  data->auto_confirm = auto_confirm;
  data->source       = source != NULL ? g_object_ref (source) : NULL;

  dex_clear (&self->transact_future);
  self->transact_future = dex_future_finally (
      g_steal_pointer (&base_future),
      (DexFutureCallback) ready_to_transact,
      transact_data_ref (data), transact_data_unref);
}

static gboolean
should_skip_entry (BzEntry *entry,
                   gboolean remove)
{
  gboolean is_installed;

  if (bz_entry_is_holding (entry))
    return TRUE;

  is_installed = bz_entry_is_installed (entry);

  return (!remove && is_installed) || (remove && !is_installed);
}

static GtkWidget *
create_entry_radio_button (BzEntry    *entry,
                           GtkWidget **out_radio)
{
  GtkWidget       *row;
  GtkWidget       *radio;
  g_autofree char *label;

  label = g_strdup (bz_entry_get_unique_id (entry));

  row = adw_action_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), label);

  radio = gtk_check_button_new ();
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), radio);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), radio);

  if (out_radio != NULL)
    *out_radio = radio;

  return row;
}

static int
create_entry_radio_buttons (AdwAlertDialog *alert,
                            GListModel     *model,
                            gboolean        remove)
{
  GtkWidget *listbox                = NULL;
  g_autoptr (GPtrArray) radios      = NULL;
  GtkCheckButton *first_valid_radio = NULL;
  guint           n_entries         = 0;
  int             n_valid_radios    = 0;

  listbox = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_widget_add_css_class (listbox, "boxed-list");

  radios            = g_ptr_array_new ();
  first_valid_radio = NULL;

  if (model != NULL)
    n_entries = g_list_model_get_n_items (model);
  n_valid_radios = 0;

  for (guint i = 0; i < n_entries; i++)
    {
      g_autoptr (BzEntry) entry_variant = NULL;
      GtkWidget *row                    = NULL;
      GtkWidget *radio                  = NULL;

      entry_variant = g_list_model_get_item (model, i);

      if (should_skip_entry (entry_variant, remove))
        {
          g_ptr_array_add (radios, NULL);
          continue;
        }

      row = create_entry_radio_button (entry_variant, &radio);
      g_ptr_array_add (radios, radio);

      if (first_valid_radio != NULL)
        gtk_check_button_set_group (GTK_CHECK_BUTTON (radio), first_valid_radio);
      else
        {
          gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
          first_valid_radio = GTK_CHECK_BUTTON (radio);
        }

      gtk_list_box_append (GTK_LIST_BOX (listbox), row);
      n_valid_radios++;
    }

  g_object_set_data_full (
      G_OBJECT (alert), "checks",
      g_steal_pointer (&radios),
      (GDestroyNotify) g_ptr_array_unref);

  if (n_valid_radios > 1)
    adw_alert_dialog_set_extra_child (alert, listbox);

  return n_valid_radios;
}

static void
configure_install_dialog (AdwAlertDialog *alert,
                          const char     *title,
                          const char     *id)
{
  g_autofree char *heading = NULL;

  heading = g_strdup_printf (_ ("Install %s?"), title);

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, _ ("May install additional shared components"));

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("Cancel"),
                                  "install", _ ("Install"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "install", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (alert, "install");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static void
configure_remove_dialog (AdwAlertDialog *alert,
                         const char     *title,
                         const char     *id)
{
  g_autofree char *heading = NULL;

  heading = g_strdup_printf (_ ("Remove %s?"), title);

  adw_alert_dialog_set_heading (alert, heading);
  adw_alert_dialog_set_body (alert, _ ("Settings & user data will be kept"));

  adw_alert_dialog_add_responses (alert,
                                  "cancel", _ ("Cancel"),
                                  "remove", _ ("Remove"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (alert, "remove", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (alert, "remove");
  adw_alert_dialog_set_close_response (alert, "cancel");
}

static AdwDialog *
create_confirmation_dialog (BzWindow     *self,
                            BzEntryGroup *group,
                            GListModel   *model,
                            BzEntry      *entry,
                            GtkWidget    *source,
                            gboolean      remove,
                            int          *out_n_valid_radios)
{
  AdwDialog  *alert          = NULL;
  const char *title          = NULL;
  const char *id             = NULL;
  int         n_valid_radios = 0;

  alert          = adw_alert_dialog_new (NULL, NULL);
  title          = NULL;
  id             = NULL;
  n_valid_radios = 0;

  if (source != NULL)
    g_object_set_data_full (G_OBJECT (alert), "source",
                            g_object_ref (source), g_object_unref);

  if (model != NULL)
    {
      g_object_set_data_full (G_OBJECT (alert), "group",
                              g_object_ref (group), g_object_unref);
      g_object_set_data_full (G_OBJECT (alert), "model",
                              g_object_ref (model), g_object_unref);
      title = bz_entry_group_get_title (group);
      id    = bz_entry_group_get_id (group);
    }
  else
    {
      g_object_set_data_full (G_OBJECT (alert), "entry",
                              g_object_ref (entry), g_object_unref);
      title = bz_entry_get_title (entry);
      id    = bz_entry_get_id (entry);
    }

  if (remove)
    configure_remove_dialog (ADW_ALERT_DIALOG (alert), title, id);
  else
    configure_install_dialog (ADW_ALERT_DIALOG (alert), title, id);

  n_valid_radios = create_entry_radio_buttons (ADW_ALERT_DIALOG (alert), model, remove);

  if (out_n_valid_radios != NULL)
    *out_n_valid_radios = n_valid_radios;

  return alert;
}

static DexFuture *
ready_to_transact (DexFuture    *future,
                   TransactData *data)
{
  BzWindow     *self             = data->self;
  BzEntryGroup *group            = data->group;
  gboolean      remove           = data->remove;
  gboolean      auto_confirm     = data->auto_confirm;
  GtkWidget    *source           = data->source;
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;
  g_autoptr (GListModel) model   = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  AdwDialog *alert               = NULL;
  int        n_valid_radios      = 0;

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      if (G_VALUE_HOLDS (value, G_TYPE_LIST_MODEL))
        model = g_value_dup_object (value);
      else
        entry = g_value_dup_object (value);

      alert = create_confirmation_dialog (self, group, model, entry, source, remove, &n_valid_radios);

      if (auto_confirm && n_valid_radios <= 1)
        {
          const char *response_id = remove ? "remove" : "install";
          g_object_ref_sink (alert);
          install_confirmation_response (ADW_ALERT_DIALOG (alert), (gchar *) response_id, self);
          g_object_unref (alert);
        }
      else
        {
          g_signal_connect (alert, "response",
                            G_CALLBACK (install_confirmation_response), self);
          adw_dialog_present (alert, GTK_WIDGET (self));
        }
    }
  else
    {
      bz_show_error_for_widget (GTK_WIDGET (self), local_error->message);
    }

  dex_clear (&self->transact_future);
  return NULL;
}

static void
update (BzWindow *self,
        BzEntry **updates,
        guint     n_updates)
{
  g_autoptr (BzTransaction) transaction = NULL;

  transaction = bz_transaction_new_full (
      NULL, 0,
      updates, n_updates,
      NULL, 0);
  bz_transaction_manager_add (
      bz_state_info_get_transaction_manager (self->state),
      transaction);
}

static void
search (BzWindow   *self,
        const char *initial)
{
  if (initial != NULL && *initial != '\0')
    {
      bz_search_widget_set_text (self->search_widget, initial);
    }

  adw_view_stack_set_visible_child_name (self->main_view_stack, "search");
  adw_navigation_view_pop_to_tag (self->navigation_view, "main");
  gtk_widget_grab_focus (GTK_WIDGET (self->search_widget));
}

static void
check_transactions (BzWindow *self)
{
  gboolean has_transactions = FALSE;
  gboolean paused           = FALSE;
  gboolean active           = FALSE;

  has_transactions = bz_transaction_manager_get_has_transactions (
      bz_state_info_get_transaction_manager (self->state));
  adw_view_stack_set_visible_child_name (
      self->transactions_stack,
      has_transactions
          ? "content"
          : "empty");

  paused = gtk_toggle_button_get_active (self->transactions_pause);
  active = bz_transaction_manager_get_active (
      bz_state_info_get_transaction_manager (self->state));
  if (paused)
    {
      gtk_button_set_icon_name (GTK_BUTTON (self->transactions_pause), "media-playback-start-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Resume Current Tasks"));
      gtk_widget_add_css_class (GTK_WIDGET (self->transactions_pause), "suggested-action");
    }
  else
    {
      gtk_button_set_icon_name (GTK_BUTTON (self->transactions_pause), "media-playback-pause-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Pause Current Tasks"));
      gtk_widget_remove_css_class (GTK_WIDGET (self->transactions_pause), "suggested-action");
    }

  if (active)
    gtk_widget_add_css_class (GTK_WIDGET (self->transactions_stop), "destructive-action");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->transactions_stop), "destructive-action");
}

static void
set_page (BzWindow *self)
{
  const char *selected_navigation_page_name = NULL;
  const char *visible_child_name            = NULL;

  if (self->state == NULL)
    return;

  if (bz_state_info_get_busy (self->state))
    {
      gtk_stack_set_visible_child_name (self->main_stack, "loading");
      adw_navigation_view_pop_to_tag (self->navigation_view, "main");
    }
  else
    {
      gtk_stack_set_visible_child_name (self->main_stack, "main");
    }

  selected_navigation_page_name = adw_navigation_view_get_visible_page_tag (self->navigation_view);

  if (g_strcmp0 (selected_navigation_page_name, "view") != 0)
    bz_full_view_set_entry_group (self->full_view, NULL);
}

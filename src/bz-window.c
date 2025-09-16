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

  GBinding *search_to_view_binding;
  gboolean  breakpoint_applied;

  DexFuture *transact_future;

  /* Template widgets */
  BzCometOverlay      *comet_overlay;
  AdwOverlaySplitView *split_view;
  AdwOverlaySplitView *search_split;
  AdwViewStack        *transactions_stack;
  AdwNavigationView   *main_stack;
  BzFullView          *full_view;
  GtkToggleButton     *toggle_transactions;
  GtkButton           *go_back;
  GtkButton           *search;
  BzSearchWidget      *search_widget;
  GtkButton           *update_button;
  GtkRevealer         *title_revealer;
  AdwToggleGroup      *title_toggle_group;
  GtkToggleButton     *transactions_pause;
  GtkButton           *transactions_stop;
  GtkButton           *transactions_clear;
  AdwToastOverlay     *toasts;
  AdwToolbarView      *toolbar_view;
  AdwHeaderBar        *top_header_bar;
  AdwHeaderBar        *bottom_header_bar;
  AdwToggle           *curated_toggle;
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
set_bottom_bar (BzWindow *self);

static void
bz_window_dispose (GObject *object)
{
  BzWindow *self = BZ_WINDOW (object);

  dex_clear (&self->transact_future);
  g_clear_object (&self->state);
  g_clear_object (&self->search_to_view_binding);

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
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static char *
get_search_icon_from_state (gpointer object,
                            gboolean value)
{
  return g_strdup (
      value
          ? "go-previous-symbolic"
          : "system-search-symbolic");
}

static void
browser_group_selected_cb (BzWindow     *self,
                           BzEntryGroup *group,
                           gpointer      browser)
{
  bz_window_show_group (self, group);
}

static void
search_split_open_changed_cb (BzWindow            *self,
                              GParamSpec          *pspec,
                              AdwOverlaySplitView *view)
{
  gboolean show_sidebar = FALSE;

  g_clear_object (&self->search_to_view_binding);
  show_sidebar = adw_overlay_split_view_get_show_sidebar (view);

  if (show_sidebar)
    self->search_to_view_binding = g_object_bind_property (
        self->search_widget, "previewing",
        self->full_view, "entry-group",
        G_BINDING_SYNC_CREATE);

  set_page (self);
}

static void
search_widget_select_cb (BzWindow       *self,
                         BzEntryGroup   *group,
                         BzSearchWidget *search)
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
  try_transact (self, NULL, group, remove, NULL);
}

static void
full_view_install_cb (BzWindow   *self,
                      GtkWidget  *source,
                      BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), FALSE, source);
}

static void
full_view_remove_cb (BzWindow   *self,
                     GtkWidget  *source,
                     BzFullView *view)
{
  try_transact (self, NULL, bz_full_view_get_entry_group (view), TRUE, source);
}

static void
installed_page_install_cb (BzWindow   *self,
                           BzEntry    *entry,
                           BzFullView *view)
{
  try_transact (self, entry, NULL, FALSE, NULL);
}

static void
installed_page_remove_cb (BzWindow   *self,
                          BzEntry    *entry,
                          BzFullView *view)
{
  try_transact (self, entry, NULL, TRUE, NULL);
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
visible_page_changed_cb (BzWindow          *self,
                         GParamSpec        *pspec,
                         AdwNavigationView *navigation_view)
{
  AdwNavigationPage *visible_page = NULL;
  const char        *page_tag     = NULL;

  visible_page = adw_navigation_view_get_visible_page (navigation_view);

  if (visible_page != NULL)
    {
      page_tag = adw_navigation_page_get_tag (visible_page);

      if (page_tag != NULL && strstr (page_tag, "flathub") != NULL)
        gtk_widget_add_css_class (GTK_WIDGET (self), "flathub");
      else
        gtk_widget_remove_css_class (GTK_WIDGET (self), "flathub");
    }
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "flathub");
}

static void
breakpoint_apply_cb (BzWindow      *self,
                     AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = TRUE;

  adw_header_bar_set_title_widget (self->top_header_bar, NULL);
  adw_header_bar_set_title_widget (self->bottom_header_bar, NULL);
  adw_header_bar_set_title_widget (self->bottom_header_bar, GTK_WIDGET (self->title_revealer));

  set_bottom_bar (self);
}

static void
breakpoint_unapply_cb (BzWindow      *self,
                       AdwBreakpoint *breakpoint)
{
  self->breakpoint_applied = FALSE;

  adw_header_bar_set_title_widget (self->top_header_bar, NULL);
  adw_header_bar_set_title_widget (self->bottom_header_bar, NULL);
  adw_header_bar_set_title_widget (self->top_header_bar, GTK_WIDGET (self->title_revealer));

  set_bottom_bar (self);
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
go_back_cb (BzWindow  *self,
            GtkButton *button)
{
  set_page (self);
}

// static void
// refresh_cb (BzWindow  *self,
//             GtkButton *button)
// {
//   gtk_widget_activate_action (GTK_WIDGET (self), "app.refresh", NULL);
// }

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
  BzWindow *self = BZ_WINDOW (widget);

  adw_overlay_split_view_set_show_sidebar (self->search_split, FALSE);
  gtk_toggle_button_set_active (self->toggle_transactions, FALSE);
  set_page (self);
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
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search_split);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, full_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toasts);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toggle_transactions);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, go_back);
  // gtk_widget_class_bind_template_child (widget_class, BzWindow, refresh);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, search_widget);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, update_button);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, title_revealer);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, title_toggle_group);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_pause);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_stop);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, transactions_clear);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, toolbar_view);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, top_header_bar);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, bottom_header_bar);
  gtk_widget_class_bind_template_child (widget_class, BzWindow, curated_toggle);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, get_search_icon_from_state);
  gtk_widget_class_bind_template_callback (widget_class, browser_group_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_widget_select_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_install_cb);
  gtk_widget_class_bind_template_callback (widget_class, full_view_remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_install_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, installed_page_show_cb);
  gtk_widget_class_bind_template_callback (widget_class, page_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_apply_cb);
  gtk_widget_class_bind_template_callback (widget_class, breakpoint_unapply_cb);
  gtk_widget_class_bind_template_callback (widget_class, pause_transactions_cb);
  gtk_widget_class_bind_template_callback (widget_class, stop_transactions_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_split_open_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, go_back_cb);
  // gtk_widget_class_bind_template_callback (widget_class, refresh_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_cb);
  gtk_widget_class_bind_template_callback (widget_class, transactions_clear_cb);
  gtk_widget_class_bind_template_callback (widget_class, visible_page_changed_cb);

  gtk_widget_class_install_action (widget_class, "escape", NULL, action_escape);
}

static gboolean
key_pressed (BzWindow              *self,
             guint                  keyval,
             guint                  keycode,
             GdkModifierType        state,
             GtkEventControllerKey *controller)
{
  guint32 unichar = 0;
  char    buf[32] = { 0 };

  /* Ignore if this is a modifier-shortcut of some sort */
  if (state & ~(GDK_NO_MODIFIER_MASK | GDK_SHIFT_MASK))
    return FALSE;

  /* Ignore if we are already inside search  */
  if (adw_overlay_split_view_get_show_sidebar (self->search_split))
    return FALSE;

  unichar = gdk_keyval_to_unicode (keyval);
  if (unichar == 0 || !g_unichar_isgraph (unichar))
    return FALSE;

  adw_overlay_split_view_set_show_sidebar (self->search_split, TRUE);

  g_unichar_to_utf8 (unichar, buf);
  bz_search_widget_set_text (self->search_widget, buf);

  return TRUE;
}

static void
bz_window_init (BzWindow *self)
{
  // const char *desktop = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  // desktop = g_getenv ("XDG_CURRENT_DESKTOP");
  // if (desktop != NULL)
  //   {
  //     if (g_strcmp0 (desktop, "GNOME") == 0)
  //       gtk_widget_set_visible (GTK_WIDGET (self->support_gnome), TRUE);
  //     else if (g_strcmp0 (desktop, "KDE") == 0)
  //       gtk_widget_set_visible (GTK_WIDGET (self->support_kde), TRUE);
  //   }

  adw_toggle_group_set_active_name (self->title_toggle_group, "flathub");

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
    adw_toggle_group_set_active_name (self->title_toggle_group, "flathub");
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
        bz_comet_overlay_pulse_child (
            self->comet_overlay,
            GTK_WIDGET (self->update_button));
      else
        adw_toast_overlay_add_toast (
            self->toasts,
            adw_toast_new_format (_ ("Up to date!")));
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
  g_return_if_fail (BZ_IS_WINDOW (self));
  g_return_if_fail (BZ_IS_ENTRY_GROUP (group));

  bz_full_view_set_entry_group (self->full_view, group);
  adw_navigation_view_replace_with_tags (self->main_stack, (const char *[]) { "view" }, 1);
  gtk_widget_set_visible (GTK_WIDGET (self->go_back), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->search), FALSE);
  gtk_revealer_set_reveal_child (self->title_revealer, FALSE);
}

void
bz_window_set_category_view_mode (BzWindow *self,
                                  gboolean  enabled)
{
  g_return_if_fail (BZ_IS_WINDOW (self));

  gtk_widget_set_visible (GTK_WIDGET (self->go_back), enabled);
  gtk_widget_set_visible (GTK_WIDGET (self->search), !enabled);
  gtk_revealer_set_reveal_child (self->title_revealer, !enabled);
}

static void
transact (BzWindow  *self,
          BzEntry   *entry,
          gboolean   remove,
          GtkWidget *source)
{
  g_autoptr (BzTransaction) transaction = NULL;
  GdkPaintable *icon                    = NULL;

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
    source = GTK_WIDGET (self->main_stack);

  icon = bz_entry_get_icon_paintable (entry);
  if (icon != NULL)
    {
      g_autoptr (BzComet) comet = NULL;

      comet = g_object_new (
          BZ_TYPE_COMET,
          "from", remove ? GTK_WIDGET (self->toggle_transactions) : source,
          "to", remove ? source : GTK_WIDGET (self->toggle_transactions),
          "paintable", icon,
          NULL);
      bz_comet_overlay_spawn (self->comet_overlay, comet);
    }

  // adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
}

static void
try_transact (BzWindow     *self,
              BzEntry      *entry,
              BzEntryGroup *group,
              gboolean      remove,
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

  data         = transact_data_new ();
  data->self   = self;
  data->group  = group != NULL ? g_object_ref (group) : NULL;
  data->remove = remove;
  data->source = source != NULL ? g_object_ref (source) : NULL;

  dex_clear (&self->transact_future);
  self->transact_future = dex_future_finally (
      g_steal_pointer (&base_future),
      (DexFutureCallback) ready_to_transact,
      transact_data_ref (data), transact_data_unref);
}

static DexFuture *
ready_to_transact (DexFuture    *future,
                   TransactData *data)
{
  BzWindow     *self             = data->self;
  BzEntryGroup *group            = data->group;
  gboolean      remove           = data->remove;
  GtkWidget    *source           = data->source;
  g_autoptr (GError) local_error = NULL;
  const GValue *value            = NULL;

  value = dex_future_get_value (future, &local_error);
  if (value != NULL)
    {
      g_autoptr (GListModel) model = NULL;
      g_autoptr (BzEntry) entry    = NULL;
      AdwDialog  *alert            = NULL;
      const char *title            = NULL;
      const char *id               = NULL;

      if (G_VALUE_HOLDS (value, G_TYPE_LIST_MODEL))
        model = g_value_dup_object (value);
      else
        entry = g_value_dup_object (value);

      alert = adw_alert_dialog_new (NULL, NULL);
      if (source != NULL)
        g_object_set_data_full (
            G_OBJECT (alert),
            "source", g_object_ref (source),
            g_object_unref);

      adw_alert_dialog_set_heading (
          ADW_ALERT_DIALOG (alert), _ ("Confirm Action"));

      if (model != NULL)
        {
          g_object_set_data_full (
              G_OBJECT (alert),
              "group", g_object_ref (group),
              g_object_unref);
          g_object_set_data_full (
              G_OBJECT (alert),
              "model", g_object_ref (model),
              g_object_unref);
          title = bz_entry_group_get_title (group);
          id    = bz_entry_group_get_id (group);
        }
      else
        {
          g_object_set_data_full (
              G_OBJECT (alert),
              "entry", g_object_ref (entry),
              g_object_unref);
          title = bz_entry_get_title (entry);
          id    = bz_entry_get_id (entry);
        }

      if (remove)
        {
          adw_alert_dialog_format_body_markup (
              ADW_ALERT_DIALOG (alert),
              _ ("You are about to remove the following Flatpak:\n\n"
                 "<b>%s</b>\n"
                 "<tt>%s</tt>\n\n"
                 "Are you sure?"),
              title, id);
          adw_alert_dialog_add_responses (
              ADW_ALERT_DIALOG (alert),
              "cancel", _ ("Cancel"),
              "remove", _ ("Remove"),
              NULL);
          adw_alert_dialog_set_response_appearance (
              ADW_ALERT_DIALOG (alert), "remove", ADW_RESPONSE_DESTRUCTIVE);
          adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "remove");
          adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");
        }
      else
        {
          adw_alert_dialog_format_body_markup (
              ADW_ALERT_DIALOG (alert),
              _ ("You are about to install the following Flatpak:\n\n"
                 "<b>%s</b>\n"
                 "<tt>%s</tt>\n\n"
                 "Are you sure?"),
              title, id);
          adw_alert_dialog_add_responses (
              ADW_ALERT_DIALOG (alert),
              "cancel", _ ("Cancel"),
              "install", _ ("Install"),
              NULL);
          adw_alert_dialog_set_response_appearance (
              ADW_ALERT_DIALOG (alert), "install", ADW_RESPONSE_SUGGESTED);
          adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (alert), "install");
          adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (alert), "cancel");
        }

      if (group != NULL)
        {
          guint n_entries = 0;

          n_entries = g_list_model_get_n_items (model);
          if (n_entries > 0)
            {
              GtkWidget      *box               = NULL;
              GtkWidget      *expander          = NULL;
              GPtrArray      *checks            = NULL;
              GtkCheckButton *first_valid_check = NULL;
              int             n_valid_checks    = FALSE;

              box      = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
              expander = gtk_expander_new (_ ("More details"));

              gtk_expander_set_child (GTK_EXPANDER (expander), box);
              adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (alert), expander);

              checks = g_ptr_array_new ();

              for (guint i = 0; i < n_entries; i++)
                {
                  g_autoptr (BzEntry) entry_variant = NULL;
                  gboolean         is_installed     = FALSE;
                  g_autofree char *label            = NULL;
                  GtkWidget       *check            = NULL;
                  GtkWidget       *check_label      = NULL;

                  entry_variant = g_list_model_get_item (model, i);
                  is_installed  = bz_entry_is_installed (entry_variant);

                  if (bz_entry_is_holding (entry_variant) ||
                      (!remove && is_installed) ||
                      (remove && !is_installed))
                    {
                      g_ptr_array_add (checks, NULL);
                      continue;
                    }

                  label = g_strdup (bz_entry_get_unique_id (entry_variant));

                  check = gtk_check_button_new ();
                  g_ptr_array_add (checks, check);

                  gtk_widget_set_has_tooltip (check, TRUE);
                  gtk_widget_set_tooltip_text (check, label);

                  check_label = gtk_label_new (label);
                  gtk_label_set_wrap (GTK_LABEL (check_label), TRUE);
                  gtk_label_set_wrap_mode (GTK_LABEL (check_label), PANGO_WRAP_WORD_CHAR);
                  gtk_check_button_set_child (GTK_CHECK_BUTTON (check), check_label);

                  if (first_valid_check != NULL)
                    gtk_check_button_set_group (GTK_CHECK_BUTTON (check), first_valid_check);
                  else
                    {
                      gtk_check_button_set_active (GTK_CHECK_BUTTON (check), TRUE);
                      first_valid_check = GTK_CHECK_BUTTON (check);
                    }

                  gtk_box_append (GTK_BOX (box), check);
                  n_valid_checks++;
                }

              g_object_set_data_full (
                  G_OBJECT (alert),
                  "checks",
                  checks,
                  (GDestroyNotify) g_ptr_array_unref);

              if (n_valid_checks == 1)
                gtk_widget_set_sensitive (GTK_WIDGET (first_valid_check), FALSE);
            }
        }

      g_signal_connect (alert, "response", G_CALLBACK (install_confirmation_response), self);
      adw_dialog_present (alert, GTK_WIDGET (self));
    }
  else
    bz_show_error_for_widget (GTK_WIDGET (self), local_error->message);

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
  gboolean open_sidebar = FALSE;

  if (initial != NULL && *initial != '\0')
    {
      bz_search_widget_set_text (self->search_widget, initial);
      open_sidebar = TRUE;
    }
  else
    open_sidebar = !adw_overlay_split_view_get_show_sidebar (self->search_split);

  adw_overlay_split_view_set_show_sidebar (self->search_split, open_sidebar);
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
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Resume the execution of transactions"));
      gtk_widget_add_css_class (GTK_WIDGET (self->transactions_pause), "suggested-action");
    }
  else
    {
      gtk_button_set_icon_name (GTK_BUTTON (self->transactions_pause), "media-playback-pause-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->transactions_pause), _ ("Pause the execution of transactions"));
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
  const char *active_name   = NULL;
  gboolean    show_search   = FALSE;
  const char *visible_child = NULL;

  if (self->state == NULL)
    return;

  active_name = adw_toggle_group_get_active_name (self->title_toggle_group);
  show_search = adw_overlay_split_view_get_show_sidebar (self->search_split);

  if (bz_state_info_get_busy (self->state))
    visible_child = "loading";
  else if (show_search)
    visible_child = "view";
  else if (g_strcmp0 (active_name, "installed") == 0)
    visible_child = "installed";
  else if (g_strcmp0 (active_name, "curated") == 0)
    visible_child = bz_state_info_get_online (self->state) ? "browse" : "offline";
  else if (g_strcmp0 (active_name, "flathub") == 0)
    visible_child = bz_state_info_get_online (self->state) ? "flathub" : "offline";
  else
    visible_child = "flathub";

  adw_navigation_view_replace_with_tags (self->main_stack, (const char *[]) { visible_child }, 1);
  gtk_widget_set_sensitive (GTK_WIDGET (self->title_toggle_group), !bz_state_info_get_busy (self->state));
  gtk_revealer_set_reveal_child (self->title_revealer, !show_search);
  set_bottom_bar (self);

  gtk_widget_set_visible (GTK_WIDGET (self->go_back), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->search), TRUE);

  if (show_search)
    gtk_widget_grab_focus (GTK_WIDGET (self->search_widget));
  else
    bz_full_view_set_entry_group (self->full_view, NULL);
}

static void
set_bottom_bar (BzWindow *self)
{
  gboolean showing_search  = FALSE;
  gboolean show_bottom_bar = FALSE;

  showing_search  = adw_overlay_split_view_get_show_sidebar (self->search_split);
  show_bottom_bar = (self->breakpoint_applied && !showing_search);
  adw_toolbar_view_set_reveal_bottom_bars (self->toolbar_view, show_bottom_bar);
}

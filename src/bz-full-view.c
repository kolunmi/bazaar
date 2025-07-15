/* bz-full-view.c
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

#include <glib/gi18n.h>

#include "bz-async-texture.h"
#include "bz-dynamic-list-view.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-flatpak-entry.h"
#include "bz-full-view.h"
#include "bz-paintable-model.h"
#include "bz-screenshot.h"
#include "bz-section-view.h"
#include "bz-share-dialog.h"
#include "bz-stats-dialog.h"
#include "bz-util.h"

struct _BzFullView
{
  AdwBin parent_instance;

  BzTransactionManager *transactions;
  BzEntryGroup         *group;
  BzEntryGroup         *debounced_group;

  DexFuture *loading_image_viewer;
  guint      debounce_timeout;

  /* Template widgets */
  AdwViewStack *stack;
};

G_DEFINE_FINAL_TYPE (BzFullView, bz_full_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_TRANSACTION_MANAGER,
  PROP_ENTRY_GROUP,
  PROP_DEBOUNCED_GROUP,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_INSTALL,
  SIGNAL_REMOVE,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
debounce_timeout (BzFullView *self);

BZ_DEFINE_DATA (
    open_screenshots_external,
    OpenScreenshotsExternal,
    {
      GPtrArray *textures;
      guint      initial;
    },
    BZ_RELEASE_DATA (textures, g_ptr_array_unref));
static DexFuture *
open_screenshots_external_fiber (OpenScreenshotsExternalData *data);
static DexFuture *
open_screenshots_external_finally (DexFuture  *future,
                                   BzFullView *self);

BZ_DEFINE_DATA (
    save_single_screenshot,
    SaveSingleScreenshot,
    {
      GdkTexture *texture;
      GFile      *output;
    },
    BZ_RELEASE_DATA (texture, g_object_unref);
    BZ_RELEASE_DATA (output, g_object_unref));
static DexFuture *
save_single_screenshot_fiber (SaveSingleScreenshotData *data);

static void
bz_full_view_dispose (GObject *object)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  g_clear_object (&self->transactions);
  g_clear_object (&self->group);
  g_clear_object (&self->debounced_group);

  dex_clear (&self->loading_image_viewer);
  g_clear_handle_id (&self->debounce_timeout, g_source_remove);

  G_OBJECT_CLASS (bz_full_view_parent_class)->dispose (object);
}

static void
bz_full_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION_MANAGER:
      g_value_set_object (value, bz_full_view_get_transaction_manager (self));
      break;
    case PROP_ENTRY_GROUP:
      g_value_set_object (value, bz_full_view_get_entry_group (self));
      break;
    case PROP_DEBOUNCED_GROUP:
      g_value_set_object (value, self->debounced_group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_full_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  BzFullView *self = BZ_FULL_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION_MANAGER:
      bz_full_view_set_transaction_manager (self, g_value_get_object (value));
      break;
    case PROP_ENTRY_GROUP:
      bz_full_view_set_entry_group (self, g_value_get_object (value));
      break;
    case PROP_DEBOUNCED_GROUP:
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

static char *
format_recent_downloads (gpointer object,
                         int      value)
{
  if (value > 0)
    return g_strdup_printf ("%d", value);
  else
    return g_strdup ("---");
}

static char *
format_size (gpointer object,
             guint64  value)
{
  g_autofree char *size = NULL;

  size = g_format_size (value);
  return g_strdup_printf ("%s Download", size);
}

static char *
format_timestamp (gpointer object,
                  guint64  value)
{
  g_autoptr (GDateTime) date = NULL;

  date = g_date_time_new_from_unix_utc (value);
  return g_date_time_format (date, _ ("Released %x"));
}

static char *
format_as_link (gpointer    object,
                const char *value)
{
  if (value != NULL)
    return g_strdup_printf ("<a href=\"%s\" title=\"%s\">%s</a>",
                            value, value, value);
  else
    return g_strdup (_ ("No URL"));
}

static char *
pick_license_warning (gpointer object,
                      gboolean value)
{
  return value
             ? g_strdup (_ ("This application has a FLOSS license, meaning the source code can be audited for safety."))
             : g_strdup (_ ("This application has a proprietary license, meaning the source code is developed privately and cannot be audited by an independent third party."));
}

static void
share_cb (BzFullView *self,
          GtkButton  *button)
{
  AdwDialog *share_dialog = NULL;

  if (self->group == NULL)
    return;

  share_dialog = bz_share_dialog_new (bz_entry_group_get_ui_entry (self->group));
  gtk_widget_set_size_request (GTK_WIDGET (share_dialog), 400, -1);

  adw_dialog_present (share_dialog, GTK_WIDGET (self));
}

static void
dl_stats_cb (BzFullView *self,
             GtkButton  *button)
{
  BzEntry   *ui_entry = NULL;
  AdwDialog *dialog   = NULL;

  if (self->group == NULL)
    return;

  ui_entry = bz_entry_group_get_ui_entry (self->group);
  dialog   = bz_stats_dialog_new (NULL);
  adw_dialog_set_content_width (dialog, 2000);
  adw_dialog_set_content_height (dialog, 1500);
  g_object_bind_property (ui_entry, "download-stats", dialog, "model", G_BINDING_SYNC_CREATE);

  adw_dialog_present (dialog, GTK_WIDGET (self));
  bz_stats_dialog_animate_open (BZ_STATS_DIALOG (dialog));
}

static void
run_cb (BzFullView *self,
        GtkButton  *button)
{
  GListModel *model   = NULL;
  guint       n_items = 0;

  if (self->group == NULL)
    return;

  model   = bz_entry_group_get_model (self->group);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = NULL;

      entry = g_list_model_get_item (model, i);

      if (BZ_IS_FLATPAK_ENTRY (entry) &&
          bz_entry_group_query_removable (self->group, entry))
        {
          g_autoptr (GError) local_error = NULL;
          gboolean result                = FALSE;

          result = bz_flatpak_entry_launch (BZ_FLATPAK_ENTRY (entry), &local_error);
          if (!result)
            {
              GtkWidget *window = NULL;

              window = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW);
              if (window != NULL)
                bz_show_error_for_widget (window, local_error->message);
            }
          break;
        }
    }
}

static void
install_cb (BzFullView *self,
            GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_INSTALL], 0);
}

static void
remove_cb (BzFullView *self,
           GtkButton  *button)
{
  g_signal_emit (self, signals[SIGNAL_REMOVE], 0);
}

static void
support_cb (BzFullView *self,
            GtkButton  *button)
{
  BzEntry *ui_entry = NULL;

  ui_entry = bz_entry_group_get_ui_entry (self->group);
  if (ui_entry != NULL)
    {
      const char *url = NULL;

      url = bz_entry_get_donation_url (ui_entry);
      g_app_info_launch_default_for_uri (url, NULL, NULL);
    }
}

static void
screenshot_activate_cb (BzFullView  *self,
                        guint        position,
                        GtkListView *list_view)
{
  DexScheduler      *scheduler                 = NULL;
  GtkSelectionModel *model                     = NULL;
  guint              n_items                   = 0;
  g_autoptr (OpenScreenshotsExternalData) data = NULL;
  DexFuture *future                            = NULL;

  if (self->loading_image_viewer != NULL)
    return;

  scheduler = dex_thread_pool_scheduler_get_default ();
  model     = gtk_list_view_get_model (list_view);
  n_items   = g_list_model_get_n_items (G_LIST_MODEL (model));

  data           = open_screenshots_external_data_new ();
  data->textures = g_ptr_array_new_with_free_func (g_object_unref);
  data->initial  = position;

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzAsyncTexture) async_tex;

      async_tex = g_list_model_get_item (G_LIST_MODEL (model), i);
      if (bz_async_texture_get_loaded (async_tex))
        {
          GdkTexture *texture = NULL;

          texture = bz_async_texture_get_texture (async_tex);
          if (texture != NULL)
            g_ptr_array_add (data->textures, g_object_ref (texture));
        }
    }
  if (data->textures->len == 0)
    return;

  future = dex_scheduler_spawn (
      scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) open_screenshots_external_fiber,
      open_screenshots_external_data_ref (data),
      open_screenshots_external_data_unref);
  future = dex_future_finally (
      future,
      (DexFutureCallback) open_screenshots_external_finally,
      self, NULL);

  self->loading_image_viewer = future;
  // gtk_widget_set_visible (GTK_WIDGET (self->loading_screenshots_external), TRUE);
}

static void
bz_full_view_class_init (BzFullViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_full_view_dispose;
  object_class->get_property = bz_full_view_get_property;
  object_class->set_property = bz_full_view_set_property;

  props[PROP_TRANSACTION_MANAGER] =
      g_param_spec_object (
          "transaction-manager",
          NULL, NULL,
          BZ_TYPE_TRANSACTION_MANAGER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ENTRY_GROUP] =
      g_param_spec_object (
          "entry-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEBOUNCED_GROUP] =
      g_param_spec_object (
          "debounced-group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_INSTALL] =
      g_signal_new (
          "install",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_INSTALL],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__VOIDv);

  signals[SIGNAL_REMOVE] =
      g_signal_new (
          "remove",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_REMOVE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__VOIDv);

  g_type_ensure (BZ_TYPE_SCREENSHOT);
  g_type_ensure (BZ_TYPE_SECTION_VIEW);
  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-full-view.ui");
  gtk_widget_class_bind_template_child (widget_class, BzFullView, stack);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, format_recent_downloads);
  gtk_widget_class_bind_template_callback (widget_class, format_size);
  gtk_widget_class_bind_template_callback (widget_class, format_timestamp);
  gtk_widget_class_bind_template_callback (widget_class, format_as_link);
  gtk_widget_class_bind_template_callback (widget_class, share_cb);
  gtk_widget_class_bind_template_callback (widget_class, dl_stats_cb);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);
  gtk_widget_class_bind_template_callback (widget_class, support_cb);
  gtk_widget_class_bind_template_callback (widget_class, screenshot_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, pick_license_warning);
}

static void
bz_full_view_init (BzFullView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_full_view_new (void)
{
  return g_object_new (BZ_TYPE_FULL_VIEW, NULL);
}

void
bz_full_view_set_transaction_manager (BzFullView           *self,
                                      BzTransactionManager *transactions)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (transactions == NULL ||
                    BZ_IS_TRANSACTION_MANAGER (transactions));

  g_clear_object (&self->transactions);
  if (transactions != NULL)
    self->transactions = g_object_ref (transactions);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSACTION_MANAGER]);
}

BzTransactionManager *
bz_full_view_get_transaction_manager (BzFullView *self)
{
  g_return_val_if_fail (BZ_IS_FULL_VIEW (self), NULL);
  return self->transactions;
}

void
bz_full_view_set_entry_group (BzFullView   *self,
                              BzEntryGroup *group)
{
  g_return_if_fail (BZ_IS_FULL_VIEW (self));
  g_return_if_fail (group == NULL ||
                    BZ_IS_ENTRY_GROUP (group));

  g_clear_object (&self->group);
  g_clear_handle_id (&self->debounce_timeout, g_source_remove);
  g_clear_object (&self->debounced_group);

  if (group != NULL)
    {
      BzEntry    *ui_entry    = NULL;
      GListModel *screenshots = NULL;

      self->group = g_object_ref (group);

      ui_entry    = bz_entry_group_get_ui_entry (group);
      screenshots = bz_entry_get_screenshot_paintables (ui_entry);

      if (BZ_IS_PAINTABLE_MODEL (screenshots) &&
          bz_paintable_model_is_fully_loaded (BZ_PAINTABLE_MODEL (screenshots)))
        self->debounced_group = g_object_ref (group);
      else
        self->debounce_timeout = g_timeout_add_once (
            500, (GSourceOnceFunc) debounce_timeout, self);

      adw_view_stack_set_visible_child_name (self->stack, "content");
    }
  else
    adw_view_stack_set_visible_child_name (self->stack, "empty");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENTRY_GROUP]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEBOUNCED_GROUP]);
}

BzEntryGroup *
bz_full_view_get_entry_group (BzFullView *self)
{
  g_return_val_if_fail (BZ_IS_FULL_VIEW (self), NULL);
  return self->group;
}

static void
debounce_timeout (BzFullView *self)
{
  self->debounce_timeout = 0;
  if (self->group == NULL)
    return;

  g_clear_object (&self->debounced_group);
  self->debounced_group = g_object_ref (self->group);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEBOUNCED_GROUP]);
}

static DexFuture *
open_screenshots_external_fiber (OpenScreenshotsExternalData *data)
{
  GPtrArray *textures               = data->textures;
  guint      initial                = data->initial;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GAppInfo) appinfo      = NULL;
  g_autofree char       *tmp_dir    = NULL;
  g_autofree DexFuture **jobs       = NULL;
  GList                 *image_uris = NULL;
  gboolean               result     = FALSE;

  appinfo = g_app_info_get_default_for_type ("image/png", TRUE);
  /* early check that an image viewer even exists */
  if (appinfo == NULL)
    return dex_future_new_false ();

  tmp_dir = g_dir_make_tmp (NULL, &local_error);
  if (tmp_dir == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  jobs = g_malloc0_n (data->textures->len, sizeof (*jobs));

  for (guint i = 0; i < textures->len; i++)
    {
      GdkTexture *texture                            = NULL;
      char        basename[32]                       = { 0 };
      g_autoptr (GFile) download_file                = NULL;
      g_autoptr (SaveSingleScreenshotData) save_data = NULL;

      texture = g_ptr_array_index (textures, i);

      g_snprintf (basename, sizeof (basename), "%d.png", i);
      download_file = g_file_new_build_filename (tmp_dir, basename, NULL);

      save_data          = save_single_screenshot_data_new ();
      save_data->texture = g_object_ref (texture);
      save_data->output  = g_object_ref (download_file);

      jobs[i] = dex_scheduler_spawn (
          dex_scheduler_get_thread_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) save_single_screenshot_fiber,
          save_single_screenshot_data_ref (save_data),
          save_single_screenshot_data_unref);

      if (i == initial)
        image_uris = g_list_prepend (image_uris, g_file_get_uri (download_file));
      else
        image_uris = g_list_append (image_uris, g_file_get_uri (download_file));
    }

  for (guint i = 0; i < textures->len; i++)
    {
      if (!dex_await (jobs[i], &local_error))
        {
          g_list_free_full (image_uris, g_free);
          return dex_future_new_for_error (g_steal_pointer (&local_error));
        }
    }

  result = g_app_info_launch_uris (appinfo, image_uris, NULL, &local_error);
  g_list_free_full (image_uris, g_free);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_true ();
}

static DexFuture *
save_single_screenshot_fiber (SaveSingleScreenshotData *data)
{
  GdkTexture *texture            = data->texture;
  GFile      *output_file        = data->output;
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GBytes) png_bytes   = NULL;
  g_autoptr (GFileIOStream) io   = NULL;
  GOutputStream *output          = NULL;
  gboolean       result          = FALSE;

  png_bytes = gdk_texture_save_to_png_bytes (texture);

  io = g_file_create_readwrite (
      output_file,
      G_FILE_CREATE_REPLACE_DESTINATION,
      NULL,
      &local_error);
  if (io == NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));
  output = g_io_stream_get_output_stream (G_IO_STREAM (io));

  g_output_stream_write_bytes (output, png_bytes, NULL, &local_error);
  if (local_error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  result = g_io_stream_close (G_IO_STREAM (io), NULL, &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  return dex_future_new_true ();
}

static DexFuture *
open_screenshots_external_finally (DexFuture  *future,
                                   BzFullView *self)
{
  g_autoptr (GError) local_error = NULL;

  // dex_future_get_value (future, &local_error);
  // if (local_error != NULL)
  //   {
  //     gtk_widget_set_visible (GTK_WIDGET (self->open_screenshot_error), TRUE);
  //     gtk_label_set_label (self->open_screenshot_error, local_error->message);
  //   }

  // gtk_widget_set_visible (GTK_WIDGET (self->loading_screenshots_external), FALSE);

  self->loading_image_viewer = NULL;
  return NULL;
}

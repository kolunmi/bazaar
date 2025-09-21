/* bz-backend.c
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

#include "bz-backend.h"
#include "bz-env.h"
#include "bz-io.h"
#include "bz-transaction.h"
#include "bz-util.h"

G_DEFINE_INTERFACE (BzBackend, bz_backend, G_TYPE_OBJECT)

BZ_DEFINE_DATA (
    retrieve_with_blocklists,
    RetrieveWithBlocklists,
    {
      BzBackend     *backend;
      DexChannel    *channel;
      GPtrArray     *blocklists;
      GCancellable  *cancellable;
      gpointer       user_data;
      GDestroyNotify destroy_user_data;
    },
    BZ_RELEASE_DATA (channel, dex_unref);
    BZ_RELEASE_DATA (blocklists, g_ptr_array_unref);
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
retrieve_with_blocklists_fiber (RetrieveWithBlocklistsData *data);

static DexChannel *
bz_backend_real_create_notification_channel (BzBackend *self)
{
  return NULL;
}

static DexFuture *
bz_backend_real_load_local_package (BzBackend    *self,
                                    GFile        *file,
                                    GCancellable *cancellable)
{
  return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unimplemented");
}

static DexFuture *
bz_backend_real_retrieve_remote_entries (BzBackend     *self,
                                         DexChannel    *channel,
                                         GPtrArray     *blocked_names,
                                         GCancellable  *cancellable,
                                         gpointer       user_data,
                                         GDestroyNotify destroy_user_data)
{
  return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unimplemented");
}

static DexFuture *
bz_backend_real_retrieve_install_ids (BzBackend    *self,
                                      GCancellable *cancellable)
{
  return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unimplemented");
}

static DexFuture *
bz_backend_real_retrieve_update_ids (BzBackend    *self,
                                     GCancellable *cancellable)
{
  return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unimplemented");
}

static DexFuture *
bz_backend_real_schedule_transaction (BzBackend    *self,
                                      BzEntry     **installs,
                                      guint         n_installs,
                                      BzEntry     **updates,
                                      guint         n_updates,
                                      BzEntry     **removals,
                                      guint         n_removals,
                                      DexChannel   *channel,
                                      GCancellable *cancellable)
{
  return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "Unimplemented");
}

static void
bz_backend_default_init (BzBackendInterface *iface)
{
  iface->create_notification_channel = bz_backend_real_create_notification_channel;
  iface->load_local_package          = bz_backend_real_load_local_package;
  iface->retrieve_remote_entries     = bz_backend_real_retrieve_remote_entries;
  iface->retrieve_install_ids        = bz_backend_real_retrieve_install_ids;
  iface->retrieve_update_ids         = bz_backend_real_retrieve_update_ids;
  iface->schedule_transaction        = bz_backend_real_schedule_transaction;
}

DexChannel *
bz_backend_create_notification_channel (BzBackend *self)
{
  g_return_val_if_fail (BZ_IS_BACKEND (self), NULL);

  return BZ_BACKEND_GET_IFACE (self)->create_notification_channel (self);
}

DexFuture *
bz_backend_load_local_package (BzBackend    *self,
                               GFile        *file,
                               GCancellable *cancellable)
{
  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  dex_return_error_if_fail (G_IS_FILE (file));
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (self));

  return BZ_BACKEND_GET_IFACE (self)->load_local_package (self, file, cancellable);
}

DexFuture *
bz_backend_retrieve_remote_entries (BzBackend     *self,
                                    DexChannel    *channel,
                                    GPtrArray     *blocked_names,
                                    GCancellable  *cancellable,
                                    gpointer       user_data,
                                    GDestroyNotify destroy_user_data)
{
  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  dex_return_error_if_fail (DEX_IS_CHANNEL (channel));

  return BZ_BACKEND_GET_IFACE (self)->retrieve_remote_entries (
      self,
      channel,
      blocked_names,
      cancellable,
      user_data,
      destroy_user_data);
}

DexFuture *
bz_backend_retrieve_remote_entries_with_blocklists (BzBackend     *self,
                                                    DexChannel    *channel,
                                                    GListModel    *blocklists,
                                                    GCancellable  *cancellable,
                                                    gpointer       user_data,
                                                    GDestroyNotify destroy_user_data)
{
  g_autoptr (RetrieveWithBlocklistsData) data = NULL;
  guint n_blocklists                          = 0;

  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  dex_return_error_if_fail (DEX_IS_CHANNEL (channel));
  dex_return_error_if_fail (G_LIST_MODEL (blocklists));

  data                    = retrieve_with_blocklists_data_new ();
  data->backend           = self;
  data->channel           = dex_ref (channel);
  data->blocklists        = g_ptr_array_new_with_free_func (g_free);
  data->cancellable       = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;

  n_blocklists = g_list_model_get_n_items (blocklists);
  for (guint i = 0; i < n_blocklists; i++)
    {
      g_autoptr (GtkStringObject) string = NULL;
      const char *path                   = NULL;

      string = g_list_model_get_item (blocklists, i);
      path   = gtk_string_object_get_string (string);

      g_ptr_array_add (data->blocklists, g_strdup (path));
    }

  return dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) retrieve_with_blocklists_fiber,
      retrieve_with_blocklists_data_ref (data),
      retrieve_with_blocklists_data_unref);
}

DexFuture *
bz_backend_retrieve_install_ids (BzBackend    *self,
                                 GCancellable *cancellable)
{
  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  return BZ_BACKEND_GET_IFACE (self)->retrieve_install_ids (self, cancellable);
}

DexFuture *
bz_backend_retrieve_update_ids (BzBackend    *self,
                                GCancellable *cancellable)
{
  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  return BZ_BACKEND_GET_IFACE (self)->retrieve_update_ids (self, cancellable);
}

DexFuture *
bz_backend_schedule_transaction (BzBackend    *self,
                                 BzEntry     **installs,
                                 guint         n_installs,
                                 BzEntry     **updates,
                                 guint         n_updates,
                                 BzEntry     **removals,
                                 guint         n_removals,
                                 DexChannel   *channel,
                                 GCancellable *cancellable)
{
  dex_return_error_if_fail (BZ_IS_BACKEND (self));
  dex_return_error_if_fail ((installs != NULL && n_installs > 0) ||
                            (updates != NULL && n_updates > 0) ||
                            (removals != NULL && n_removals));
  if (installs != NULL)
    {
      for (guint i = 0; i < n_installs; i++)
        dex_return_error_if_fail (BZ_IS_ENTRY (installs[i]));
    }
  if (updates != NULL)
    {
      for (guint i = 0; i < n_updates; i++)
        dex_return_error_if_fail (BZ_IS_ENTRY (updates[i]));
    }
  if (removals != NULL)
    {
      for (guint i = 0; i < n_removals; i++)
        dex_return_error_if_fail (BZ_IS_ENTRY (removals[i]));
    }

  return BZ_BACKEND_GET_IFACE (self)->schedule_transaction (
      self,
      installs,
      n_installs,
      updates,
      n_updates,
      removals,
      n_removals,
      channel,
      cancellable);
}

DexFuture *
bz_backend_merge_and_schedule_transactions (BzBackend    *self,
                                            GListModel   *transactions,
                                            DexChannel   *channel,
                                            GCancellable *cancellable)
{
  guint n_items                     = 0;
  g_autoptr (GPtrArray) installs_pa = NULL;
  g_autoptr (GPtrArray) updates_pa  = NULL;
  g_autoptr (GPtrArray) removals_pa = NULL;

  dex_return_error_if_fail (G_IS_LIST_MODEL (transactions));

  n_items = g_list_model_get_n_items (transactions);
  dex_return_error_if_fail (n_items > 0);

  installs_pa = g_ptr_array_new_with_free_func (g_object_unref);
  updates_pa  = g_ptr_array_new_with_free_func (g_object_unref);
  removals_pa = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzTransaction) transaction = NULL;
      GListModel *installs                  = NULL;
      GListModel *updates                   = NULL;
      GListModel *removals                  = NULL;
      guint       n_installs                = 0;
      guint       n_updates                 = 0;
      guint       n_removals                = 0;

      transaction = g_list_model_get_item (transactions, i);
      installs    = bz_transaction_get_installs (transaction);
      updates     = bz_transaction_get_updates (transaction);
      removals    = bz_transaction_get_removals (transaction);

      if (installs != NULL)
        n_installs = g_list_model_get_n_items (installs);
      if (updates != NULL)
        n_updates = g_list_model_get_n_items (updates);
      if (removals != NULL)
        n_removals = g_list_model_get_n_items (removals);

      for (guint j = 0; j < n_installs; j++)
        g_ptr_array_add (installs_pa, g_list_model_get_item (installs, j));
      for (guint j = 0; j < n_updates; j++)
        g_ptr_array_add (updates_pa, g_list_model_get_item (updates, j));
      for (guint j = 0; j < n_removals; j++)
        g_ptr_array_add (removals_pa, g_list_model_get_item (removals, j));
    }

  return bz_backend_schedule_transaction (
      self,
      (BzEntry **) installs_pa->pdata,
      installs_pa->len,
      (BzEntry **) updates_pa->pdata,
      updates_pa->len,
      (BzEntry **) removals_pa->pdata,
      removals_pa->len,
      channel,
      cancellable);
}

static DexFuture *
retrieve_with_blocklists_fiber (RetrieveWithBlocklistsData *data)
{
  GPtrArray *blocklists               = data->blocklists;
  g_autoptr (GError) local_error      = NULL;
  g_autoptr (GPtrArray) blocked_names = NULL;

  blocked_names = g_ptr_array_new_with_free_func (g_free);

  for (guint i = 0; i < blocklists->len; i++)
    {
      const char *path       = NULL;
      g_autoptr (GFile) file = NULL;

      path = g_ptr_array_index (blocklists, i);
      file = g_file_new_for_path (path);

      if (file != NULL)
        {
          g_autoptr (GBytes) bytes     = NULL;
          const char       *bytes_data = NULL;
          g_autofree char **lines      = NULL;

          bytes = dex_await_boxed (dex_file_load_contents_bytes (file), &local_error);
          if (bytes == NULL)
            {
              g_critical ("Failed to load blocklist from path '%s': %s",
                          path, local_error->message);
              g_clear_pointer (&local_error, g_error_free);
              continue;
            }

          bytes_data = g_bytes_get_data (bytes, NULL);
          lines      = g_strsplit (bytes_data, "\n", -1);

          for (char **line = lines; *line != NULL; line++)
            {
              if (**line != '\0')
                g_ptr_array_add (blocked_names, *line);
              else
                g_free (*line);
            }
        }
    }

  return BZ_BACKEND_GET_IFACE (data->backend)
      ->retrieve_remote_entries (
          data->backend,
          data->channel,
          blocked_names,
          data->cancellable,
          g_steal_pointer (&data->user_data),
          data->destroy_user_data);
}

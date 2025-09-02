/* bz-flatpak-instance.c
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

#define G_LOG_DOMAIN  "BAZAAR::FLATPAK"
#define BAZAAR_MODULE "flatpak"

#include <xmlb.h>

#include "bz-backend.h"
#include "bz-env.h"
#include "bz-flatpak-private.h"
#include "bz-global-state.h"
#include "bz-io.h"
#include "bz-util.h"

/* clang-format off */
G_DEFINE_QUARK (bz-flatpak-error-quark, bz_flatpak_error);
/* clang-format on */

struct _BzFlatpakInstance
{
  GObject parent_instance;

  DexScheduler        *scheduler;
  FlatpakInstallation *system;
  FlatpakInstallation *user;
  GPtrArray           *cache_dirs;
};

static void
backend_iface_init (BzBackendInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    BzFlatpakInstance,
    bz_flatpak_instance,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (BZ_TYPE_BACKEND, backend_iface_init));

BZ_DEFINE_DATA (
    init,
    Init,
    { BzFlatpakInstance *instance; },
    BZ_RELEASE_DATA (instance, g_object_unref))
static DexFuture *
init_fiber (InitData *data);

BZ_DEFINE_DATA (
    check_has_flathub,
    CheckHasFlathub,
    {
      GCancellable      *cancellable;
      BzFlatpakInstance *instance;
    },
    BZ_RELEASE_DATA (cancellable, g_object_unref));
static DexFuture *
check_has_flathub_fiber (CheckHasFlathubData *data);

BZ_DEFINE_DATA (
    ensure_flathub,
    EnsureFlathub,
    {
      GCancellable      *cancellable;
      BzFlatpakInstance *instance;
    },
    BZ_RELEASE_DATA (cancellable, g_object_unref));
static DexFuture *
ensure_flathub_fiber (EnsureFlathubData *data);

BZ_DEFINE_DATA (
    load_local_ref,
    LoadLocalRef,
    {
      GCancellable      *cancellable;
      BzFlatpakInstance *instance;
      GFile             *file;
    },
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    BZ_RELEASE_DATA (file, g_object_unref));
static DexFuture *
load_local_ref_fiber (LoadLocalRefData *data);

BZ_DEFINE_DATA (
    gather_refs,
    GatherRefs,
    {
      GCancellable      *cancellable;
      BzFlatpakInstance *instance;
      DexChannel        *channel;
      GPtrArray         *blocked_names;
      gpointer           user_data;
      GDestroyNotify     destroy_user_data;
      guint              total;
    },
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    BZ_RELEASE_DATA (channel, dex_unref);
    BZ_RELEASE_DATA (blocked_names, g_ptr_array_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
retrieve_remote_refs_fiber (GatherRefsData *data);
static DexFuture *
retrieve_installs_fiber (GatherRefsData *data);
static DexFuture *
retrieve_updates_fiber (GatherRefsData *data);

BZ_DEFINE_DATA (
    retrieve_refs_for_remote,
    RetrieveRefsForRemote,
    {
      GatherRefsData      *parent;
      FlatpakInstallation *installation;
      FlatpakRemote       *remote;
      GHashTable          *blocked_names_hash;
    },
    BZ_RELEASE_DATA (parent, gather_refs_data_unref);
    BZ_RELEASE_DATA (installation, g_object_unref);
    BZ_RELEASE_DATA (remote, g_object_unref);
    BZ_RELEASE_DATA (blocked_names_hash, g_hash_table_unref));
static DexFuture *
retrieve_refs_for_remote_fiber (RetrieveRefsForRemoteData *data);

static void
gather_refs_update_progress (const char     *status,
                             guint           progress,
                             gboolean        estimating,
                             GatherRefsData *data);

BZ_DEFINE_DATA (
    transaction,
    Transaction,
    {
      GCancellable                    *cancellable;
      BzFlatpakInstance               *instance;
      GPtrArray                       *installs;
      GPtrArray                       *updates;
      GPtrArray                       *removals;
      GHashTable                      *ref_to_entry_hash;
      BzBackendTransactionProgressFunc progress_func;
      gpointer                         user_data;
      GDestroyNotify                   destroy_user_data;
      int                              n_operations;
      int                              n_finished_operations;
    },
    BZ_RELEASE_DATA (cancellable, g_object_unref);
    BZ_RELEASE_DATA (installs, g_ptr_array_unref);
    BZ_RELEASE_DATA (updates, g_ptr_array_unref);
    BZ_RELEASE_DATA (removals, g_ptr_array_unref);
    BZ_RELEASE_DATA (ref_to_entry_hash, g_hash_table_unref);
    BZ_RELEASE_DATA (user_data, self->destroy_user_data))
static DexFuture *
transaction_fiber (TransactionData *data);
static void
transaction_new_operation (FlatpakTransaction          *object,
                           FlatpakTransactionOperation *operation,
                           FlatpakTransactionProgress  *progress,
                           TransactionData             *data);

static BzFlatpakEntry *
find_entry_from_operation (TransactionData             *data,
                           FlatpakTransactionOperation *operation,
                           int                         *n_operations);

BZ_DEFINE_DATA (
    transaction_operation,
    TransactionOperation,
    {
      TransactionData *parent;
      BzFlatpakEntry  *entry;
      guint            timeout_handle;
    },
    BZ_RELEASE_DATA (parent, transaction_data_unref);
    BZ_RELEASE_DATA (entry, g_object_unref);
    BZ_RELEASE_UTAG (timeout_handle, g_source_remove))
static void
transaction_progress_changed (FlatpakTransactionProgress *object,
                              TransactionOperationData   *data);

BZ_DEFINE_DATA (
    idle_transaction,
    IdleTransaction,
    {
      BzFlatpakInstance        *hold;
      TransactionOperationData *parent;
      char                     *status;
      gboolean                  is_estimating;
      double                    progress;
      guint64                   bytes_transferred;
      guint64                   start_time;
    },
    BZ_RELEASE_DATA (hold, g_object_unref);
    BZ_RELEASE_DATA (parent, transaction_operation_data_unref);
    BZ_RELEASE_DATA (status, g_free));
static gboolean
transaction_progress_idle (IdleTransactionData *data);
static gboolean
transaction_progress_timeout (IdleTransactionData *data);

BZ_DEFINE_DATA (
    add_cache_dir,
    AddCacheDir,
    {
      BzFlatpakInstance *instance;
      char              *cache_dir;
    },
    BZ_RELEASE_DATA (instance, g_object_unref);
    BZ_RELEASE_DATA (cache_dir, g_free));
static DexFuture *
add_cache_dir_fiber (AddCacheDirData *data);

static void
add_cache_dir (BzFlatpakInstance *self,
               const char        *cache_dir);
static void
destroy_cache_dir (gpointer ptr);
static DexFuture *
remove_cache_dir_fiber (const char *cache_dir);

static gint
cmp_rref (FlatpakRemoteRef *a,
          FlatpakRemoteRef *b,
          GHashTable       *hash);

static void
bz_flatpak_instance_dispose (GObject *object)
{
  BzFlatpakInstance *self = BZ_FLATPAK_INSTANCE (object);

  dex_clear (&self->scheduler);
  g_clear_object (&self->system);
  g_clear_object (&self->user);
  g_clear_pointer (&self->cache_dirs, g_ptr_array_unref);

  G_OBJECT_CLASS (bz_flatpak_instance_parent_class)->dispose (object);
}

static void
bz_flatpak_instance_class_init (BzFlatpakInstanceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = bz_flatpak_instance_dispose;
}

static void
bz_flatpak_instance_init (BzFlatpakInstance *self)
{
  self->scheduler  = dex_thread_pool_scheduler_new ();
  self->cache_dirs = g_ptr_array_new_with_free_func (destroy_cache_dir);
}

static DexFuture *
bz_flatpak_instance_load_local_package (BzBackend    *backend,
                                        GFile        *file,
                                        GCancellable *cancellable)
{
  BzFlatpakInstance *self           = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (LoadLocalRefData) data = NULL;

  data              = load_local_ref_data_new ();
  data->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->instance    = self;
  data->file        = g_object_ref (file);

  return dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) load_local_ref_fiber,
      load_local_ref_data_ref (data), load_local_ref_data_unref);
}

static DexFuture *
bz_flatpak_instance_retrieve_remote_refs (BzBackend     *backend,
                                          DexChannel    *channel,
                                          GPtrArray     *blocked_names,
                                          GCancellable  *cancellable,
                                          gpointer       user_data,
                                          GDestroyNotify destroy_user_data)
{
  BzFlatpakInstance *self         = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherRefsData) data = NULL;

  data                    = gather_refs_data_new ();
  data->cancellable       = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->instance          = self;
  data->channel           = dex_ref (channel);
  data->blocked_names     = g_ptr_array_ref (blocked_names);
  data->user_data         = user_data;
  data->destroy_user_data = destroy_user_data;
  data->total             = 0;

  return dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) retrieve_remote_refs_fiber,
      gather_refs_data_ref (data), gather_refs_data_unref);
}

static DexFuture *
bz_flatpak_instance_retrieve_install_ids (BzBackend    *backend,
                                          GCancellable *cancellable)
{
  BzFlatpakInstance *self         = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherRefsData) data = NULL;

  data                    = gather_refs_data_new ();
  data->cancellable       = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->instance          = self;
  data->user_data         = NULL;
  data->destroy_user_data = NULL;

  return dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) retrieve_installs_fiber,
      gather_refs_data_ref (data), gather_refs_data_unref);
}

static DexFuture *
bz_flatpak_instance_retrieve_update_ids (BzBackend    *backend,
                                         GCancellable *cancellable)
{
  BzFlatpakInstance *self         = BZ_FLATPAK_INSTANCE (backend);
  g_autoptr (GatherRefsData) data = NULL;

  data                    = gather_refs_data_new ();
  data->cancellable       = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->instance          = self;
  data->user_data         = NULL;
  data->destroy_user_data = NULL;

  return dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) retrieve_updates_fiber,
      gather_refs_data_ref (data), gather_refs_data_unref);
}

static DexFuture *
bz_flatpak_instance_schedule_transaction (BzBackend                       *backend,
                                          BzEntry                        **installs,
                                          guint                            n_installs,
                                          BzEntry                        **updates,
                                          guint                            n_updates,
                                          BzEntry                        **removals,
                                          guint                            n_removals,
                                          BzBackendTransactionProgressFunc progress_func,
                                          GCancellable                    *cancellable,
                                          gpointer                         user_data,
                                          GDestroyNotify                   destroy_user_data)
{
  BzFlatpakInstance *self          = BZ_FLATPAK_INSTANCE (backend);
  BzFlatpakEntry   **installs_dup  = NULL;
  BzFlatpakEntry   **updates_dup   = NULL;
  BzFlatpakEntry   **removals_dup  = NULL;
  g_autoptr (TransactionData) data = NULL;

  if (n_installs > 0)
    {
      for (guint i = 0; i < n_installs; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (installs[i]), NULL);
    }
  if (n_updates > 0)
    {
      for (guint i = 0; i < n_updates; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (updates[i]), NULL);
    }
  if (n_removals > 0)
    {
      for (guint i = 0; i < n_removals; i++)
        g_return_val_if_fail (BZ_IS_FLATPAK_ENTRY (removals[i]), NULL);
    }

  if (n_installs > 0)
    {
      installs_dup = g_malloc0_n (n_installs, sizeof (*installs_dup));
      for (guint i = 0; i < n_installs; i++)
        installs_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (installs[i]));
    }
  if (n_updates > 0)
    {
      updates_dup = g_malloc0_n (n_updates, sizeof (*updates_dup));
      for (guint i = 0; i < n_updates; i++)
        updates_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (updates[i]));
    }
  if (n_removals > 0)
    {
      removals_dup = g_malloc0_n (n_removals, sizeof (*removals_dup));
      for (guint i = 0; i < n_removals; i++)
        removals_dup[i] = g_object_ref (BZ_FLATPAK_ENTRY (removals[i]));
    }

  data                        = transaction_data_new ();
  data->cancellable           = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->instance              = self;
  data->installs              = installs_dup != NULL ? g_ptr_array_new_take ((gpointer *) installs_dup, n_installs, g_object_unref) : NULL;
  data->updates               = updates_dup != NULL ? g_ptr_array_new_take ((gpointer *) updates_dup, n_updates, g_object_unref) : NULL;
  data->removals              = removals_dup != NULL ? g_ptr_array_new_take ((gpointer *) removals_dup, n_removals, g_object_unref) : NULL;
  data->ref_to_entry_hash     = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  data->progress_func         = progress_func;
  data->user_data             = user_data;
  data->destroy_user_data     = destroy_user_data;
  data->n_operations          = 0;
  data->n_finished_operations = -1;

  return dex_scheduler_spawn (
      self->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) transaction_fiber,
      transaction_data_ref (data), transaction_data_unref);
}

static void
backend_iface_init (BzBackendInterface *iface)
{
  iface->load_local_package      = bz_flatpak_instance_load_local_package;
  iface->retrieve_remote_entries = bz_flatpak_instance_retrieve_remote_refs;
  iface->retrieve_install_ids    = bz_flatpak_instance_retrieve_install_ids;
  iface->retrieve_update_ids     = bz_flatpak_instance_retrieve_update_ids;
  iface->schedule_transaction    = bz_flatpak_instance_schedule_transaction;
}

FlatpakInstallation *
bz_flatpak_instance_get_system_installation (BzFlatpakInstance *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (self), NULL);
  return self->system;
}

FlatpakInstallation *
bz_flatpak_instance_get_user_installation (BzFlatpakInstance *self)
{
  g_return_val_if_fail (BZ_IS_FLATPAK_INSTANCE (self), NULL);
  return self->user;
}

DexFuture *
bz_flatpak_instance_new (void)
{
  g_autoptr (InitData) data = NULL;

  data           = init_data_new ();
  data->instance = g_object_new (BZ_TYPE_FLATPAK_INSTANCE, NULL);

  return dex_scheduler_spawn (
      data->instance->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) init_fiber,
      init_data_ref (data), init_data_unref);
}

DexFuture *
bz_flatpak_instance_has_flathub (BzFlatpakInstance *self,
                                 GCancellable      *cancellable)
{
  g_autoptr (CheckHasFlathubData) data = NULL;

  dex_return_error_if_fail (BZ_IS_FLATPAK_INSTANCE (self));
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data              = check_has_flathub_data_new ();
  data->instance    = self;
  data->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;

  return dex_scheduler_spawn (
      data->instance->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) check_has_flathub_fiber,
      check_has_flathub_data_ref (data), check_has_flathub_data_unref);
}

DexFuture *
bz_flatpak_instance_ensure_has_flathub (BzFlatpakInstance *self,
                                        GCancellable      *cancellable)
{
  g_autoptr (EnsureFlathubData) data = NULL;

  dex_return_error_if_fail (BZ_IS_FLATPAK_INSTANCE (self));
  dex_return_error_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  data              = ensure_flathub_data_new ();
  data->instance    = self;
  data->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;

  return dex_scheduler_spawn (
      data->instance->scheduler,
      bz_get_dex_stack_size (),
      (DexFiberFunc) ensure_flathub_fiber,
      ensure_flathub_data_ref (data), ensure_flathub_data_unref);
}

static DexFuture *
init_fiber (InitData *data)
{
  BzFlatpakInstance *instance    = data->instance;
  g_autoptr (GError) local_error = NULL;
  g_autofree char *main_cache    = NULL;

  bz_discard_module_dir ();

  instance->system = flatpak_installation_new_system (NULL, &local_error);
  if (instance->system == NULL)
    {
      g_warning ("Failed to initialize system installation: %s",
                 local_error->message);
      g_clear_pointer (&local_error, g_error_free);
    }

  instance->user = flatpak_installation_new_user (NULL, &local_error);
  if (instance->user == NULL)
    {
      g_warning ("Failed to initialize user installation: %s",
                 local_error->message);
      g_clear_pointer (&local_error, g_error_free);
    }

  if (instance->system == NULL && instance->user == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
        "Failed to initialize any flatpak installations");

  return dex_future_new_for_object (instance);
}

static DexFuture *
check_has_flathub_fiber (CheckHasFlathubData *data)
{
  BzFlatpakInstance *instance          = data->instance;
  GCancellable      *cancellable       = data->cancellable;
  g_autoptr (GError) local_error       = NULL;
  g_autoptr (GPtrArray) system_remotes = NULL;
  guint n_system_remotes               = 0;
  g_autoptr (GPtrArray) user_remotes   = NULL;
  guint n_user_remotes                 = 0;

  if (instance->system != NULL)
    {
      system_remotes = flatpak_installation_list_remotes (
          instance->system, cancellable, &local_error);
      if (system_remotes == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
            "Failed to enumerate remotes for system installation: %s",
            local_error->message);
      n_system_remotes = system_remotes->len;
    }

  if (instance->user != NULL)
    {
      user_remotes = flatpak_installation_list_remotes (
          instance->user, cancellable, &local_error);
      if (user_remotes == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
            "Failed to enumerate remotes for user installation: %s",
            local_error->message);
      n_user_remotes = user_remotes->len;
    }

  for (guint i = 0; i < n_system_remotes + n_user_remotes; i++)
    {
      FlatpakRemote *remote = NULL;
      const char    *name   = NULL;

      if (i < n_system_remotes)
        remote = g_ptr_array_index (system_remotes, i);
      else
        remote = g_ptr_array_index (user_remotes, i - n_system_remotes);

      if (flatpak_remote_get_disabled (remote) ||
          flatpak_remote_get_noenumerate (remote))
        continue;

      name = flatpak_remote_get_name (remote);
      if (g_strcmp0 (name, "flathub") == 0)
        return dex_future_new_true ();
    }
  return dex_future_new_false ();
}

static DexFuture *
ensure_flathub_fiber (EnsureFlathubData *data)
{
  BzFlatpakInstance *instance          = data->instance;
  GCancellable      *cancellable       = data->cancellable;
  g_autoptr (GError) local_error       = NULL;
  g_autoptr (FlatpakRemote) sys_remote = NULL;
  g_autoptr (FlatpakRemote) usr_remote = NULL;
  gboolean result                      = FALSE;
  g_autoptr (FlatpakRemote) remote     = NULL;

#define REPO_URL "https://dl.flathub.org/repo/flathub.flatpakrepo"

  if (instance->system != NULL)
    sys_remote = flatpak_installation_get_remote_by_name (
        instance->system, "flathub", cancellable, NULL);
  if (instance->user != NULL)
    usr_remote = flatpak_installation_get_remote_by_name (
        instance->user, "flathub", cancellable, NULL);

  if (sys_remote != NULL)
    remote = g_steal_pointer (&sys_remote);
  else if (usr_remote != NULL)
    remote = g_steal_pointer (&usr_remote);

  if (remote != NULL)
    {
      flatpak_remote_set_disabled (remote, FALSE);
      flatpak_remote_set_noenumerate (remote, FALSE);
      flatpak_remote_set_gpg_verify (remote, TRUE);
    }
  else
    {
      g_autoptr (SoupMessage) message  = NULL;
      g_autoptr (GOutputStream) output = NULL;
      g_autoptr (GBytes) bytes         = NULL;

      message = soup_message_new (SOUP_METHOD_GET, REPO_URL);
      output  = g_memory_output_stream_new_resizable ();
      result  = dex_await (
          bz_send_with_global_http_session_then_splice_into (message, output),
          &local_error);
      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to retrieve flatpakrepo file from %s: %s",
            REPO_URL, local_error->message);

      bytes  = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
      remote = flatpak_remote_new_from_file ("flathub", bytes, &local_error);
      if (remote == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to construct flatpak remote from flatpakrepo file %s: %s",
            REPO_URL, local_error->message);

      flatpak_remote_set_gpg_verify (remote, TRUE);

      result = flatpak_installation_add_remote (
          instance->system != NULL ? instance->system : instance->user,
          remote,
          TRUE,
          cancellable,
          &local_error);
      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
            "Failed to add flathub to flatpak installation: %s",
            local_error->message);
    }

  return dex_future_new_true ();
}

static DexFuture *
load_local_ref_fiber (LoadLocalRefData *data)
{
  // GCancellable      *cancellable    = data->cancellable;
  BzFlatpakInstance *instance       = data->instance;
  GFile             *file           = data->file;
  g_autoptr (GError) local_error    = NULL;
  g_autofree char *uri              = NULL;
  g_autofree char *path             = NULL;
  g_autoptr (FlatpakBundleRef) bref = NULL;
  g_autoptr (BzFlatpakEntry) entry  = NULL;

  uri  = g_file_get_uri (file);
  path = g_file_get_path (file);
  if (uri == NULL)
    uri = g_strdup_printf ("file://%s", path);

  if (g_str_has_suffix (uri, ".flatpakref"))
    {
      const char *resolved_uri      = NULL;
      g_autoptr (GKeyFile) key_file = g_key_file_new ();
      gboolean         result       = FALSE;
      g_autofree char *name         = NULL;

      if (g_str_has_prefix (uri, "flatpak+https"))
        resolved_uri = uri + strlen ("flatpak+");
      else
        resolved_uri = uri;

      key_file = g_key_file_new ();

      if (g_str_has_prefix (resolved_uri, "http"))
        {
          g_autoptr (SoupMessage) message  = NULL;
          g_autoptr (GOutputStream) output = NULL;
          g_autoptr (GBytes) bytes         = NULL;

          message = soup_message_new (SOUP_METHOD_GET, resolved_uri);
          output  = g_memory_output_stream_new_resizable ();
          result  = dex_await (
              bz_send_with_global_http_session_then_splice_into (message, output),
              &local_error);
          if (!result)
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
                "Failed to retrieve flatpakref file from %s: %s",
                resolved_uri, local_error->message);

          bytes  = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (output));
          result = g_key_file_load_from_bytes (key_file, bytes, G_KEY_FILE_NONE, &local_error);
        }
      else if (path != NULL)
        result = g_key_file_load_from_file (
            key_file, path, G_KEY_FILE_NONE, &local_error);
      else
        local_error = g_error_new (
            G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Cannot handle URIs of this type");

      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to load flatpakref '%s' into a key file: %s",
            uri, local_error->message);

      name = g_key_file_get_string (key_file, "Flatpak Ref", "Name", &local_error);
      if (name == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to load locate \"Name\" key in flatpakref '%s': %s",
            uri, local_error->message);

      return dex_future_new_take_string (g_steal_pointer (&name));
    }

  bref = flatpak_bundle_ref_new (file, &local_error);
  if (bref == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to load local flatpak bundle '%s': %s",
        path,
        local_error->message);

  entry = bz_flatpak_entry_new_for_ref (
      instance,
      FALSE,
      NULL,
      FLATPAK_REF (bref),
      NULL,
      NULL,
      NULL,
      &local_error);
  if (entry == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to parse information from flatpak bundle '%s': %s",
        path,
        local_error->message);

  return dex_future_new_for_object (entry);
}

static DexFuture *
retrieve_remote_refs_fiber (GatherRefsData *data)
{
  GCancellable      *cancellable            = data->cancellable;
  BzFlatpakInstance *instance               = data->instance;
  GPtrArray         *blocked_names          = data->blocked_names;
  DexChannel        *channel                = data->channel;
  g_autoptr (GError) local_error            = NULL;
  g_autoptr (GPtrArray) system_remotes      = NULL;
  guint n_system_remotes                    = 0;
  g_autoptr (GPtrArray) user_remotes        = NULL;
  guint n_user_remotes                      = 0;
  g_autoptr (GHashTable) blocked_names_hash = NULL;
  g_autoptr (GPtrArray) jobs                = NULL;
  g_autoptr (GPtrArray) job_names           = NULL;
  g_autoptr (DexFuture) future              = NULL;
  gboolean result                           = FALSE;
  g_autoptr (GString) error_string          = NULL;

  while (instance->cache_dirs->len > 0)
    {
      char *cache_dir = NULL;

      cache_dir = g_ptr_array_steal_index_fast (instance->cache_dirs, 0);
      bz_reap_path (cache_dir);
      g_free (cache_dir);
    }

  if (instance->system != NULL)
    {
      system_remotes = flatpak_installation_list_remotes (
          instance->system, cancellable, &local_error);
      if (system_remotes == NULL)
        {
          dex_channel_close_send (channel);
          return dex_future_new_reject (
              BZ_FLATPAK_ERROR,
              BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
              "Failed to enumerate remotes for system installation: %s",
              local_error->message);
        }
      n_system_remotes = system_remotes->len;
    }

  if (instance->user != NULL)
    {
      user_remotes = flatpak_installation_list_remotes (
          instance->user, cancellable, &local_error);
      if (user_remotes == NULL)
        {
          dex_channel_close_send (channel);
          return dex_future_new_reject (
              BZ_FLATPAK_ERROR,
              BZ_FLATPAK_ERROR_CANNOT_INITIALIZE,
              "Failed to enumerate remotes for user installation: %s",
              local_error->message);
        }
      n_user_remotes = user_remotes->len;
    }

  if (n_user_remotes + n_system_remotes == 0)
    {
      dex_channel_close_send (channel);
      return dex_future_new_true ();
    }

  if (blocked_names != NULL)
    {
      blocked_names_hash = g_hash_table_new (g_str_hash, g_str_equal);
      for (guint i = 0; i < blocked_names->len; i++)
        {
          const char *name = NULL;

          name = g_ptr_array_index (blocked_names, i);
          g_hash_table_add (blocked_names_hash, (gpointer) name);
        }
    }

  jobs      = g_ptr_array_new_with_free_func (dex_unref);
  job_names = g_ptr_array_new_with_free_func (g_free);

  for (guint i = 0; i < n_system_remotes + n_user_remotes; i++)
    {
      FlatpakInstallation *installation              = NULL;
      FlatpakRemote       *remote                    = NULL;
      const char          *name                      = NULL;
      g_autoptr (RetrieveRefsForRemoteData) job_data = NULL;
      g_autoptr (DexFuture) job_future               = NULL;

      if (i < n_system_remotes)
        {
          installation = instance->system;
          remote       = g_ptr_array_index (system_remotes, i);
        }
      else
        {
          installation = instance->user;
          remote       = g_ptr_array_index (user_remotes, i - n_system_remotes);
        }

      name = flatpak_remote_get_name (remote);

      if (flatpak_remote_get_disabled (remote) ||
          flatpak_remote_get_noenumerate (remote))
        {
          g_debug ("Skipping remote %s", name);
          continue;
        }

      if (g_strstr_len (name, -1, "fedora") != NULL)
        {
          g_debug ("Skipping remote %s", name);
          /* the fedora flatpak repos cause too many issues */
          continue;
        }

      job_data                     = retrieve_refs_for_remote_data_new ();
      job_data->parent             = gather_refs_data_ref (data);
      job_data->installation       = g_object_ref (installation);
      job_data->remote             = g_object_ref (remote);
      job_data->blocked_names_hash = blocked_names_hash != NULL ? g_hash_table_ref (blocked_names_hash) : NULL;

      job_future = dex_scheduler_spawn (
          instance->scheduler,
          bz_get_dex_stack_size (),
          (DexFiberFunc) retrieve_refs_for_remote_fiber,
          retrieve_refs_for_remote_data_ref (job_data),
          retrieve_refs_for_remote_data_unref);

      g_ptr_array_add (jobs, g_steal_pointer (&job_future));
      g_ptr_array_add (job_names, g_strdup (name));
    }

  if (jobs->len == 0)
    {
      dex_channel_close_send (channel);
      return dex_future_new_true ();
    }

  result = dex_await (dex_future_allv (
                          (DexFuture *const *) jobs->pdata,
                          jobs->len),
                      NULL);
  dex_channel_close_send (channel);
  if (!result)
    error_string = g_string_new ("No remotes could be synchronized:\n\n");

  for (guint i = 0; i < jobs->len; i++)
    {
      DexFuture *job_future = NULL;
      char      *name       = NULL;

      job_future = g_ptr_array_index (jobs, i);
      name       = g_ptr_array_index (job_names, i);

      dex_future_get_value (job_future, &local_error);
      if (local_error != NULL)
        {
          if (error_string == NULL)
            error_string = g_string_new ("Some remotes couldn't be fully sychronized:\n");
          g_string_append_printf (error_string, "\n%s failed because: %s\n", name, local_error->message);
        }
      g_clear_pointer (&local_error, g_error_free);
    }

  if (result)
    {
      if (error_string != NULL)
        return dex_future_new_take_string (
            g_string_free_and_steal (g_steal_pointer (&error_string)));
      else
        return dex_future_new_true ();
    }
  else
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "%s", error_string->str);
}

static void
gather_refs_update_progress (const char     *status,
                             guint           progress,
                             gboolean        estimating,
                             GatherRefsData *data)
{
}

static DexFuture *
retrieve_refs_for_remote_fiber (RetrieveRefsForRemoteData *data)
{
  GCancellable        *cancellable        = data->parent->cancellable;
  BzFlatpakInstance   *instance           = data->parent->instance;
  DexChannel          *channel            = data->parent->channel;
  FlatpakInstallation *installation       = data->installation;
  FlatpakRemote       *remote             = data->remote;
  GHashTable          *blocked_names_hash = data->blocked_names_hash;
  g_autoptr (GError) local_error          = NULL;
  g_autoptr (DexFuture) error_future      = NULL;
  const char *remote_name                 = NULL;
  gboolean    result                      = FALSE;
  g_autoptr (GFile) appstream_dir         = NULL;
  g_autofree char *appstream_dir_path     = NULL;
  g_autofree char *appstream_xml_path     = NULL;
  g_autoptr (GFile) appstream_xml         = NULL;
  g_autoptr (XbBuilderSource) source      = NULL;
  g_autoptr (XbBuilder) builder           = NULL;
  const gchar *const *locales             = NULL;
  g_autoptr (XbSilo) silo                 = NULL;
  g_autoptr (XbNode) root                 = NULL;
  g_autoptr (GPtrArray) children          = NULL;
  g_autoptr (AsMetadata) metadata         = NULL;
  AsComponentBox *components              = NULL;
  g_autoptr (GHashTable) component_hash   = NULL;
  // g_autofree char *remote_icon_name       = NULL;
  g_autoptr (GdkPaintable) remote_icon = NULL;
  g_autoptr (GPtrArray) refs           = NULL;
  g_autofree char *main_cache          = NULL;
  g_autofree char *output_dir_path     = NULL;
  g_autoptr (GFile) output_dir_file    = NULL;

  remote_name = flatpak_remote_get_name (remote);

  result = flatpak_installation_update_remote_sync (
      installation,
      remote_name,
      cancellable,
      &local_error);
  if (!result)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "Failed to synchronize remote '%s': %s",
        remote_name,
        local_error->message);

  result = flatpak_installation_update_appstream_full_sync (
      installation,
      remote_name,
      NULL,
      (FlatpakProgressCallback) gather_refs_update_progress,
      data,
      NULL,
      cancellable,
      &local_error);
  if (!result)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "Failed to synchronize appstream data for remote '%s': %s",
        remote_name,
        local_error->message);

  appstream_dir = flatpak_remote_get_appstream_dir (remote, NULL);
  if (appstream_dir == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to locate appstream directory for remote '%s': %s",
        remote_name,
        local_error->message);

  appstream_dir_path = g_file_get_path (appstream_dir);
  appstream_xml_path = g_build_filename (appstream_dir_path, "appstream.xml.gz", NULL);
  if (!g_file_test (appstream_xml_path, G_FILE_TEST_EXISTS))
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to verify existence of appstream bundle download at path %s for remote '%s'",
        appstream_xml_path,
        remote_name);

  appstream_xml = g_file_new_for_path (appstream_xml_path);

  source = xb_builder_source_new ();
  result = xb_builder_source_load_file (
      source,
      appstream_xml,
      XB_BUILDER_SOURCE_FLAG_WATCH_FILE |
          XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT,
      cancellable,
      &local_error);
  if (!result)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to load binary xml from appstream bundle download at path %s for remote '%s': %s",
        appstream_xml_path,
        remote_name,
        local_error->message);

  builder = xb_builder_new ();
  locales = g_get_language_names ();
  for (guint i = 0; locales[i] != NULL; i++)
    xb_builder_add_locale (builder, locales[i]);
  xb_builder_import_source (builder, source);

  silo = xb_builder_compile (
      builder,

      /* This was causing issues */
      // // fallback for locales should be handled by AppStream as_component_get_name
      // XB_BUILDER_COMPILE_FLAG_NONE,

      /* This seems to work better */
      XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS,
      cancellable,
      &local_error);
  if (silo == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
        "Failed to compile binary xml silo from appstream bundle download at path %s for remote '%s': %s",
        appstream_xml_path,
        remote_name,
        local_error->message);

  root     = xb_silo_get_root (silo);
  children = xb_node_get_children (root);
  metadata = as_metadata_new ();

  for (guint i = 0; i < children->len; i++)
    {
      XbNode          *component_node = NULL;
      g_autofree char *component_xml  = NULL;

      component_node = g_ptr_array_index (children, i);

      component_xml = xb_node_export (
          component_node, XB_NODE_EXPORT_FLAG_NONE, &local_error);
      if (component_xml == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to export plain xml from appstream bundle silo "
            "originating from download at path %s for remote '%s': %s",
            appstream_xml_path,
            remote_name,
            local_error->message);

      result = as_metadata_parse_data (
          metadata, component_xml, -1,
          AS_FORMAT_KIND_XML, &local_error);
      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_APPSTREAM_FAILURE,
            "Failed to create appstream metadata from appstream bundle silo "
            "originating from download at path %s for remote '%s': %s",
            appstream_xml_path,
            remote_name,
            local_error->message);
    }

  components     = as_metadata_get_components (metadata);
  component_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  for (guint i = 0; i < as_component_box_len (components); i++)
    {
      AsComponent *component = NULL;
      const char  *id        = NULL;

      component = as_component_box_index (components, i);
      id        = as_component_get_id (component);

      if (!g_hash_table_contains (component_hash, id) &&
          (blocked_names_hash == NULL || !g_hash_table_contains (blocked_names_hash, id)))
        g_hash_table_replace (component_hash, g_strdup (id), g_object_ref (component));
    }

  /* Disabled for now, as it is causing issues and
   * we shouldn't be using GFile for http
   */

  // remote_icon_name = flatpak_remote_get_icon (remote);
  // if (remote_icon_name != NULL)
  //   {
  //     g_autoptr (GFile) remote_icon_file = NULL;

  //     remote_icon_file = g_file_new_for_uri (remote_icon_name);
  //     if (remote_icon_file != NULL)
  //       {
  //         g_autoptr (GlyLoader) loader = NULL;
  //         g_autoptr (GlyImage) image   = NULL;
  //         g_autoptr (GlyFrame) frame   = NULL;
  //         GdkTexture *texture          = NULL;

  //         loader = gly_loader_new (remote_icon_file);
  //         image  = gly_loader_load (loader, &local_error);
  //         if (image == NULL)
  //           {
  //             error_future = dex_future_new_reject (
  //                 BZ_FLATPAK_ERROR,
  //                 BZ_FLATPAK_ERROR_GLYCIN_FAILURE,
  //                 "Failed to download icon from uri %s for remote '%s': %s",
  //                 remote_icon_name,
  //                 remote_name,
  //                 local_error->message);
  //             goto done;
  //           }

  //         frame = gly_image_next_frame (image, &local_error);
  //         if (frame == NULL)
  //           {
  //             error_future = dex_future_new_reject (
  //                 BZ_FLATPAK_ERROR,
  //                 BZ_FLATPAK_ERROR_GLYCIN_FAILURE,
  //                 "Failed to decode frame from downloaded icon from uri %s for remote '%s': %s",
  //                 remote_icon_name,
  //                 remote_name,
  //                 local_error->message);
  //             goto done;
  //           }

  //         texture = gly_gtk_frame_get_texture (frame);
  //         if (texture != NULL)
  //           remote_icon = GDK_PAINTABLE (texture);
  //       }
  //   }

  refs = flatpak_installation_list_remote_refs_sync (
      installation, remote_name, cancellable, &local_error);
  if (refs == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "Failed to enumerate refs for remote '%s': %s",
        remote_name,
        local_error->message);

  main_cache      = bz_dup_module_dir ();
  output_dir_path = g_build_filename (main_cache, remote_name, NULL);
  output_dir_file = g_file_new_for_path (output_dir_path);

  result = g_file_make_directory_with_parents (output_dir_file, cancellable, &local_error);
  if (!result)
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        g_clear_pointer (&local_error, g_error_free);
      else
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_IO_MISBEHAVIOR,
            "Failed to create cache dir for remote '%s': %s",
            remote_name,
            local_error->message);
    }
  add_cache_dir (instance, output_dir_path);

  for (guint i = 0; i < refs->len;)
    {
      FlatpakRemoteRef *rref = NULL;
      const char       *name = NULL;

      rref = g_ptr_array_index (refs, i);
      name = flatpak_ref_get_name (FLATPAK_REF (rref));

      if (flatpak_remote_ref_get_eol (rref) != NULL ||
          flatpak_remote_ref_get_eol_rebase (rref) != NULL ||
          (blocked_names_hash != NULL &&
           g_hash_table_contains (blocked_names_hash, name)))
        g_ptr_array_remove_index_fast (refs, i);
      else
        i++;
    }
  if (refs->len == 0)
    {
      destroy_cache_dir (g_steal_pointer (&output_dir_path));
      return dex_future_new_true ();
    }

  result = dex_await (dex_channel_send (
                          channel, dex_future_new_for_int (refs->len)),
                      &local_error);
  if (!result)
    return dex_future_new_reject (
        DEX_ERROR,
        DEX_ERROR_UNKNOWN,
        "Failed to communicate across channel: %s",
        local_error->message);

  /* Ensure the receiving side of the channel gets
   * runtimes first, then addons, then applications
   */
  g_ptr_array_sort_values_with_data (
      refs, (GCompareDataFunc) cmp_rref, component_hash);

  for (guint i = 0; i < refs->len; i++)
    {
      FlatpakRemoteRef *rref               = NULL;
      const char       *name               = NULL;
      AsComponent      *component          = NULL;
      g_autoptr (BzFlatpakEntry) entry     = NULL;
      g_autoptr (DexFuture) channel_future = NULL;

      rref      = g_ptr_array_index (refs, i);
      name      = flatpak_ref_get_name (FLATPAK_REF (rref));
      component = g_hash_table_lookup (component_hash, name);
      if (component == NULL)
        {
          g_autofree char *desktop_id = NULL;

          desktop_id = g_strdup_printf ("%s.desktop", name);
          component  = g_hash_table_lookup (component_hash, desktop_id);
        }

      entry = bz_flatpak_entry_new_for_ref (
          instance,
          installation == instance->user,
          remote,
          FLATPAK_REF (rref),
          component,
          appstream_dir_path,
          remote_icon,
          NULL);
      if (entry != NULL)
        result = dex_await (
            dex_channel_send (channel, dex_future_new_for_object (entry)),
            &local_error);
      else
        result = dex_await (
            dex_channel_send (channel, dex_future_new_for_int (-1)),
            &local_error);
      if (!result)
        return dex_future_new_reject (
            DEX_ERROR,
            DEX_ERROR_UNKNOWN,
            "Failed to communicate across channel: %s",
            local_error->message);
    }

  return dex_future_new_true ();
}

static DexFuture *
retrieve_installs_fiber (GatherRefsData *data)
{
  GCancellable      *cancellable    = data->cancellable;
  BzFlatpakInstance *instance       = data->instance;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GPtrArray) system_refs = NULL;
  guint n_system_refs               = 0;
  g_autoptr (GPtrArray) user_refs   = NULL;
  guint n_user_refs                 = 0;
  g_autoptr (GHashTable) ids        = NULL;

  if (instance->system != NULL)
    {
      system_refs = flatpak_installation_list_installed_refs (
          instance->system, cancellable, &local_error);
      if (system_refs == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_LOCAL_SYNCHRONIZATION_FAILURE,
            "Failed to discover installed refs for system installation: %s",
            local_error->message);
      n_system_refs = system_refs->len;
    }

  if (instance->user != NULL)
    {
      user_refs = flatpak_installation_list_installed_refs (
          instance->user, cancellable, &local_error);
      if (user_refs == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_LOCAL_SYNCHRONIZATION_FAILURE,
            "Failed to discover installed refs for user installation: %s",
            local_error->message);
      n_user_refs = user_refs->len;
    }

  ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (guint i = 0; i < n_system_refs + n_user_refs; i++)
    {
      gboolean             user = FALSE;
      FlatpakInstalledRef *iref = NULL;

      if (i < n_system_refs)
        {
          user = FALSE;
          iref = g_ptr_array_index (system_refs, i);
        }
      else
        {
          user = TRUE;
          iref = g_ptr_array_index (user_refs, i - n_system_refs);
        }

      g_hash_table_add (ids, bz_flatpak_ref_format_unique (FLATPAK_REF (iref), user));
    }

  return dex_future_new_take_boxed (
      G_TYPE_HASH_TABLE, g_steal_pointer (&ids));
}

static DexFuture *
retrieve_updates_fiber (GatherRefsData *data)
{
  GCancellable      *cancellable    = data->cancellable;
  BzFlatpakInstance *instance       = data->instance;
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GPtrArray) system_refs = NULL;
  g_autoptr (GPtrArray) user_refs   = NULL;
  g_autoptr (GPtrArray) ids         = NULL;

  system_refs = flatpak_installation_list_installed_refs_for_update (
      instance->system, cancellable, &local_error);
  if (system_refs == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "Failed to discover update-elligible refs for system installation: %s",
        local_error->message);

  user_refs = flatpak_installation_list_installed_refs_for_update (
      instance->user, cancellable, &local_error);
  if (user_refs == NULL)
    return dex_future_new_reject (
        BZ_FLATPAK_ERROR,
        BZ_FLATPAK_ERROR_REMOTE_SYNCHRONIZATION_FAILURE,
        "Failed to discover update-elligible refs for user installation: %s",
        local_error->message);

  ids = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_set_size (ids, system_refs->len + user_refs->len);

  for (guint i = 0; i < system_refs->len + user_refs->len; i++)
    {
      gboolean             user = FALSE;
      FlatpakInstalledRef *iref = NULL;

      if (i < system_refs->len)
        {
          user = FALSE;
          iref = g_ptr_array_index (system_refs, i);
        }
      else
        {
          user = TRUE;
          iref = g_ptr_array_index (user_refs, i - system_refs->len);
        }

      g_ptr_array_index (ids, i) =
          bz_flatpak_ref_format_unique (FLATPAK_REF (iref), user);
    }

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY, g_steal_pointer (&ids));
}

static DexFuture *
transaction_fiber (TransactionData *data)
{
  GCancellable      *cancellable                    = data->cancellable;
  BzFlatpakInstance *instance                       = data->instance;
  GPtrArray         *installations                  = data->installs;
  GPtrArray         *updates                        = data->updates;
  GPtrArray         *removals                       = data->removals;
  g_autoptr (GError) local_error                    = NULL;
  g_autoptr (FlatpakTransaction) system_transaction = NULL;
  g_autoptr (FlatpakTransaction) user_transaction   = NULL;
  gboolean result                                   = FALSE;
  g_autoptr (FlatpakTransactionProgress) progress   = NULL;

  if (instance->system != NULL)
    {
      system_transaction = flatpak_transaction_new_for_installation (
          instance->system, cancellable, &local_error);
      if (system_transaction == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
            "Failed to initialize potential transaction for system installation: %s",
            local_error->message);
    }

  if (instance->user != NULL)
    {
      user_transaction = flatpak_transaction_new_for_installation (
          instance->user, cancellable, &local_error);
      if (user_transaction == NULL)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
            "Failed to initialize potential transaction for system installation: %s",
            local_error->message);
    }

  if (installations != NULL)
    {
      for (guint i = 0; i < installations->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          gboolean         is_user = FALSE;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (installations, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          is_user = bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry));
          ref_fmt = flatpak_ref_format_ref (ref);

          if ((is_user && instance->user == NULL) ||
              (!is_user && instance->system == NULL))
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the update of %s to transaction "
                "because its installation couldn't be found",
                ref_fmt);

          result = flatpak_transaction_add_install (
              is_user
                  ? user_transaction
                  : system_transaction,
              bz_entry_get_remote_repo_name (BZ_ENTRY (entry)),
              ref_fmt,
              NULL,
              &local_error);
          if (!result)
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the installation of %s to transaction: %s",
                ref_fmt,
                local_error->message);

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (updates != NULL)
    {
      for (guint i = 0; i < updates->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          gboolean         is_user = FALSE;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (updates, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          is_user = bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry));
          ref_fmt = flatpak_ref_format_ref (ref);

          if ((is_user && instance->user == NULL) ||
              (!is_user && instance->system == NULL))
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the update of %s to transaction "
                "because its installation couldn't be found",
                ref_fmt);

          result = flatpak_transaction_add_update (
              is_user
                  ? user_transaction
                  : system_transaction,
              ref_fmt,
              NULL,
              NULL,
              &local_error);
          if (!result)
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the update of %s to transaction: %s",
                ref_fmt,
                local_error->message);

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (removals != NULL)
    {
      for (guint i = 0; i < removals->len; i++)
        {
          BzFlatpakEntry  *entry   = NULL;
          FlatpakRef      *ref     = NULL;
          gboolean         is_user = FALSE;
          g_autofree char *ref_fmt = NULL;

          entry   = g_ptr_array_index (removals, i);
          ref     = bz_flatpak_entry_get_ref (entry);
          is_user = bz_flatpak_entry_is_user (BZ_FLATPAK_ENTRY (entry));
          ref_fmt = flatpak_ref_format_ref (ref);

          if ((is_user && instance->user == NULL) ||
              (!is_user && instance->system == NULL))
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the removal of %s to transaction "
                "because its installation couldn't be found",
                ref_fmt);

          result = flatpak_transaction_add_uninstall (
              is_user
                  ? user_transaction
                  : system_transaction,
              ref_fmt,
              &local_error);
          if (!result)
            return dex_future_new_reject (
                BZ_FLATPAK_ERROR,
                BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
                "Failed to append the removal of %s to transaction: %s",
                ref_fmt,
                local_error->message);

          g_hash_table_replace (data->ref_to_entry_hash,
                                g_steal_pointer (&ref_fmt),
                                g_object_ref (entry));
        }
    }

  if (system_transaction != NULL &&
      !flatpak_transaction_is_empty (system_transaction))
    {
      g_signal_connect (system_transaction, "new-operation", G_CALLBACK (transaction_new_operation), data);
      result = flatpak_transaction_run (system_transaction, cancellable, &local_error);
      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
            "Failed to run flatpak transaction on system installation: %s",
            local_error->message);
    }

  if (user_transaction != NULL &&
      !flatpak_transaction_is_empty (user_transaction))
    {
      g_signal_connect (user_transaction, "new-operation", G_CALLBACK (transaction_new_operation), data);
      result = flatpak_transaction_run (user_transaction, cancellable, &local_error);
      if (!result)
        return dex_future_new_reject (
            BZ_FLATPAK_ERROR,
            BZ_FLATPAK_ERROR_TRANSACTION_FAILURE,
            "Failed to run flatpak transaction on user installation: %s",
            local_error->message);
    }

  return dex_future_new_true ();
}

static void
transaction_new_operation (FlatpakTransaction          *transaction,
                           FlatpakTransactionOperation *operation,
                           FlatpakTransactionProgress  *progress,
                           TransactionData             *data)
{
  BzFlatpakEntry *entry                               = NULL;
  g_autoptr (TransactionOperationData) operation_data = NULL;

  data->n_finished_operations++;

  if (data->progress_func == NULL)
    return;

  flatpak_transaction_progress_set_update_frequency (progress, 150);
  entry = find_entry_from_operation (data, operation, &data->n_operations);

  operation_data                 = transaction_operation_data_new ();
  operation_data->parent         = transaction_data_ref (data);
  operation_data->entry          = entry != NULL ? g_object_ref (entry) : NULL;
  operation_data->timeout_handle = 0;

  g_signal_connect_data (
      progress, "changed",
      G_CALLBACK (transaction_progress_changed),
      transaction_operation_data_ref (operation_data),
      transaction_operation_data_unref_closure,
      G_CONNECT_DEFAULT);
}

static BzFlatpakEntry *
find_entry_from_operation (TransactionData             *data,
                           FlatpakTransactionOperation *operation,
                           int                         *n_operations)
{
  GPtrArray *related_to_ops = NULL;

  related_to_ops = flatpak_transaction_operation_get_related_to_ops (operation);
  /* count all deps if applicable */
  if (n_operations != NULL && related_to_ops != NULL)
    {
      *n_operations += related_to_ops->len;

      for (guint i = 0; i < related_to_ops->len; i++)
        {
          FlatpakTransactionOperation *related_op = NULL;

          related_op = g_ptr_array_index (related_to_ops, i);
          find_entry_from_operation (NULL, related_op, n_operations);
        }
    }

  if (data != NULL)
    {
      const char     *ref_fmt = NULL;
      BzFlatpakEntry *entry   = NULL;

      ref_fmt = flatpak_transaction_operation_get_ref (operation);
      entry   = g_hash_table_lookup (data->ref_to_entry_hash, ref_fmt);
      if (entry != NULL)
        return entry;

      if (related_to_ops != NULL)
        {
          for (guint i = 0; i < related_to_ops->len; i++)
            {
              FlatpakTransactionOperation *related_op = NULL;

              related_op = g_ptr_array_index (related_to_ops, i);
              entry      = find_entry_from_operation (data, related_op, NULL);
              if (entry != NULL)
                break;
            }
        }

      return entry;
    }
  else
    return NULL;
}

static void
transaction_progress_changed (FlatpakTransactionProgress *progress,
                              TransactionOperationData   *data)
{
  g_autoptr (IdleTransactionData) idle_data = NULL;

  idle_data                = idle_transaction_data_new ();
  idle_data->hold          = g_object_ref (data->parent->instance);
  idle_data->parent        = transaction_operation_data_ref (data);
  idle_data->status        = flatpak_transaction_progress_get_status (progress);
  idle_data->is_estimating = flatpak_transaction_progress_get_is_estimating (progress);
  idle_data->progress      = (double) (flatpak_transaction_progress_get_progress (progress) +
                                  (100 * data->parent->n_finished_operations)) /
                        (100.0 * MAX (1.0, (double) (data->parent->n_operations)));
  idle_data->bytes_transferred = flatpak_transaction_progress_get_bytes_transferred (progress);
  idle_data->start_time        = flatpak_transaction_progress_get_start_time (progress);

  g_idle_add_full (
      G_PRIORITY_DEFAULT,
      (GSourceFunc) transaction_progress_idle,
      idle_transaction_data_ref (idle_data),
      idle_transaction_data_unref);

  if (idle_data->is_estimating)
    {
      if (data->timeout_handle == 0)
        /* We'll send an update periodically so the UI can pulse */
        data->timeout_handle = g_timeout_add_full (
            G_PRIORITY_HIGH_IDLE,
            150,
            (GSourceFunc) transaction_progress_timeout,
            idle_transaction_data_ref (idle_data),
            idle_transaction_data_unref);
    }
  else
    g_clear_handle_id (&data->timeout_handle,
                       g_source_remove);
}

static gboolean
transaction_progress_idle (IdleTransactionData *data)
{
  data->parent->parent->progress_func (
      data->parent->entry != NULL
          ? BZ_ENTRY (data->parent->entry)
          : NULL,
      data->status,
      data->is_estimating,
      data->progress,
      data->bytes_transferred,
      data->start_time,
      data->parent->parent->user_data);

  return G_SOURCE_REMOVE;
}

static gboolean
transaction_progress_timeout (IdleTransactionData *data)
{
  data->parent->parent->progress_func (
      data->parent->entry != NULL
          ? BZ_ENTRY (data->parent->entry)
          : NULL,
      data->status,
      data->is_estimating,
      data->progress,
      data->bytes_transferred,
      data->start_time,
      data->parent->parent->user_data);

  return G_SOURCE_CONTINUE;
}

static void
add_cache_dir (BzFlatpakInstance *self,
               const char        *cache_dir)
{
  g_autoptr (AddCacheDirData) data = NULL;

  data            = add_cache_dir_data_new ();
  data->instance  = g_object_ref (self);
  data->cache_dir = g_strdup (cache_dir);

  dex_await (dex_scheduler_spawn (
                 dex_scheduler_get_default (),
                 bz_get_dex_stack_size (),
                 (DexFiberFunc) add_cache_dir_fiber,
                 add_cache_dir_data_ref (data), add_cache_dir_data_unref),
             NULL);
}

static DexFuture *
add_cache_dir_fiber (AddCacheDirData *data)
{
  g_ptr_array_add (
      data->instance->cache_dirs,
      g_steal_pointer (&data->cache_dir));
  return NULL;
}

static void
destroy_cache_dir (gpointer ptr)
{
  char *cache_dir = ptr;

  dex_future_disown (dex_scheduler_spawn (
      bz_get_io_scheduler (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) remove_cache_dir_fiber,
      cache_dir, g_free));
}

static DexFuture *
remove_cache_dir_fiber (const char *cache_dir)
{
  bz_reap_path (cache_dir);
  return NULL;
}

static gint
cmp_rref (FlatpakRemoteRef *a,
          FlatpakRemoteRef *b,
          GHashTable       *hash)
{
  AsComponent    *a_comp = NULL;
  AsComponent    *b_comp = NULL;
  AsComponentKind a_kind = AS_COMPONENT_KIND_UNKNOWN;
  AsComponentKind b_kind = AS_COMPONENT_KIND_UNKNOWN;

  a_comp = g_hash_table_lookup (hash, flatpak_ref_get_name (FLATPAK_REF (a)));
  b_comp = g_hash_table_lookup (hash, flatpak_ref_get_name (FLATPAK_REF (b)));

  if (a_comp == NULL)
    return -1;
  if (b_comp == NULL)
    return 1;

  a_kind = as_component_get_kind (a_comp);
  b_kind = as_component_get_kind (b_comp);

  if (a_kind == AS_COMPONENT_KIND_RUNTIME)
    return -1;
  if (b_kind == AS_COMPONENT_KIND_RUNTIME)
    return 1;

  if (a_kind == AS_COMPONENT_KIND_ADDON)
    return -1;
  if (b_kind == AS_COMPONENT_KIND_ADDON)
    return 1;

  if (a_kind == AS_COMPONENT_KIND_DESKTOP_APP ||
      a_kind == AS_COMPONENT_KIND_CONSOLE_APP ||
      a_kind == AS_COMPONENT_KIND_WEB_APP)
    return 1;
  if (b_kind == AS_COMPONENT_KIND_DESKTOP_APP ||
      b_kind == AS_COMPONENT_KIND_CONSOLE_APP ||
      b_kind == AS_COMPONENT_KIND_WEB_APP)
    return -1;

  return 0;
}

/* bz-transaction-manager.c
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

#define G_LOG_DOMAIN "BAZAAR::TRANSACTIONS"

#include <glib/gi18n.h>

#include "bz-backend-transaction-op-payload.h"
#include "bz-backend-transaction-op-progress-payload.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-marshalers.h"
#include "bz-transaction-manager.h"
#include "bz-transaction-view.h"
#include "bz-util.h"

/* clang-format off */
G_DEFINE_QUARK (bz-transaction-mgr-error-quark, bz_transaction_mgr_error);
/* clang-format on */

enum
{
  HOOK_CONTINUE,
  HOOK_STOP,
  HOOK_CONFIRM,
  HOOK_DENY,
};

struct _BzTransactionManager
{
  GObject parent_instance;

  BzMainConfig *config;
  BzBackend    *backend;

  gboolean    paused;
  GListStore *transactions;
  double      current_progress;
  gboolean    pending;

  DexPromise *current;

  GQueue queue;
};

G_DEFINE_FINAL_TYPE (BzTransactionManager, bz_transaction_manager, G_TYPE_OBJECT)

enum
{
  PROP_0,

  PROP_CONFIG,
  PROP_BACKEND,
  PROP_PAUSED,
  PROP_TRANSACTIONS,
  PROP_HAS_TRANSACTIONS,
  PROP_ACTIVE,
  PROP_PENDING,
  PROP_CURRENT_PROGRESS,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_SUCCESS,
  SIGNAL_FAILURE,

  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static inline void
finish_queued_schedule_data (gpointer ptr);

BZ_DEFINE_DATA (
    queued_schedule,
    QueuedSchedule,
    {
      GWeakRef      *self;
      BzTransaction *transaction;
      DexPromise    *promise;
      GTimer        *timer;
    },
    finish_queued_schedule_data (self);)

BZ_DEFINE_DATA (
    dialog,
    Dialog,
    {
      char      *id;
      AdwDialog *dialog;
    },
    BZ_RELEASE_DATA (id, g_free);
    BZ_RELEASE_DATA (dialog, g_object_unref));

static DexFuture *
transaction_fiber (QueuedScheduleData *data);

static int
execute_hook (BzTransactionManager *self,
              BzHook               *hook,
              const char           *hook_type,
              const char           *ts_type,
              const char           *ts_appid);

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data);

static DexFuture *
then_loop_cb (DexFuture *future,
              GWeakRef  *wr);

static DexFuture *
dispatch_next (BzTransactionManager *self);

static void
bz_transaction_manager_dispose (GObject *object)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  g_clear_object (&self->config);
  g_clear_object (&self->backend);
  g_clear_object (&self->transactions);
  g_queue_clear_full (&self->queue, queued_schedule_data_unref);
  dex_clear (&self->current);

  G_OBJECT_CLASS (bz_transaction_manager_parent_class)->dispose (object);
}

static void
bz_transaction_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_boxed (value, bz_transaction_manager_get_config (self));
      break;
    case PROP_BACKEND:
      g_value_set_object (value, bz_transaction_manager_get_backend (self));
      break;
    case PROP_PAUSED:
      g_value_set_boolean (value, bz_transaction_manager_get_paused (self));
      break;
    case PROP_TRANSACTIONS:
      g_value_set_object (value, self->transactions);
      break;
    case PROP_HAS_TRANSACTIONS:
      g_value_set_boolean (value, bz_transaction_manager_get_has_transactions (self));
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, bz_transaction_manager_get_active (self));
      break;
    case PROP_PENDING:
      g_value_set_boolean (value, bz_transaction_manager_get_pending (self));
      break;
    case PROP_CURRENT_PROGRESS:
      g_value_set_double (value, self->current_progress);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  BzTransactionManager *self = BZ_TRANSACTION_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      bz_transaction_manager_set_config (self, g_value_get_boxed (value));
      break;
    case PROP_BACKEND:
      bz_transaction_manager_set_backend (self, g_value_get_object (value));
      break;
    case PROP_PAUSED:
      bz_transaction_manager_set_paused (self, g_value_get_boolean (value));
      break;
    case PROP_TRANSACTIONS:
    case PROP_HAS_TRANSACTIONS:
    case PROP_ACTIVE:
    case PROP_PENDING:
    case PROP_CURRENT_PROGRESS:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_manager_class_init (BzTransactionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_transaction_manager_dispose;
  object_class->get_property = bz_transaction_manager_get_property;
  object_class->set_property = bz_transaction_manager_set_property;

  props[PROP_CONFIG] =
      g_param_spec_object (
          "config",
          NULL, NULL,
          BZ_TYPE_MAIN_CONFIG,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_BACKEND] =
      g_param_spec_object (
          "backend",
          NULL, NULL,
          BZ_TYPE_BACKEND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PAUSED] =
      g_param_spec_boolean (
          "paused",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSACTIONS] =
      g_param_spec_object (
          "transactions",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_HAS_TRANSACTIONS] =
      g_param_spec_boolean (
          "has-transactions",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACTIVE] =
      g_param_spec_boolean (
          "active",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_PENDING] =
      g_param_spec_boolean (
          "pending",
          NULL, NULL, FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  props[PROP_CURRENT_PROGRESS] =
      g_param_spec_double (
          "current-progress",
          NULL, NULL,
          0.0, 1.0, 0.0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_type_ensure (BZ_TYPE_TRANSACTION_VIEW);
  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_SUCCESS] =
      g_signal_new (
          "success",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE,
          1, BZ_TYPE_TRANSACTION, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_SUCCESS],
      G_TYPE_FROM_CLASS (klass),
      bz_marshal_VOID__OBJECT_BOXEDv);

  signals[SIGNAL_FAILURE] =
      g_signal_new (
          "failure",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE,
          1, BZ_TYPE_TRANSACTION, 0);
  g_signal_set_va_marshaller (
      signals[SIGNAL_FAILURE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);
}

static void
bz_transaction_manager_init (BzTransactionManager *self)
{
  self->transactions = g_list_store_new (BZ_TYPE_TRANSACTION);
  g_queue_init (&self->queue);
}

BzTransactionManager *
bz_transaction_manager_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_MANAGER, NULL);
}

void
bz_transaction_manager_set_config (BzTransactionManager *self,
                                   BzMainConfig         *config)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  g_clear_object (&self->config);
  if (config != NULL)
    self->config = g_object_ref (config);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONFIG]);
}

BzMainConfig *
bz_transaction_manager_get_config (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), NULL);
  return self->config;
}

void
bz_transaction_manager_set_backend (BzTransactionManager *self,
                                    BzBackend            *backend)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  g_return_if_fail (backend == NULL || BZ_IS_BACKEND (backend));

  g_clear_object (&self->backend);
  if (backend != NULL)
    self->backend = g_object_ref (backend);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BACKEND]);
}

BzBackend *
bz_transaction_manager_get_backend (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), NULL);
  return self->backend;
}

void
bz_transaction_manager_set_paused (BzTransactionManager *self,
                                   gboolean              paused)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  if (!!self->paused == !!paused)
    return;

  self->paused = paused;
  if (!paused)
    dex_future_disown (dispatch_next (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAUSED]);
}

gboolean
bz_transaction_manager_get_paused (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return self->paused;
}

gboolean
bz_transaction_manager_get_active (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return self->current != NULL;
}

gboolean
bz_transaction_manager_get_pending (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return self->current != NULL && self->pending;
}

gboolean
bz_transaction_manager_get_has_transactions (BzTransactionManager *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_MANAGER (self), FALSE);
  return g_list_model_get_n_items (G_LIST_MODEL (self->transactions)) > 0;
}

DexFuture *
bz_transaction_manager_add (BzTransactionManager *self,
                            BzTransaction        *transaction)
{
  g_autoptr (QueuedScheduleData) data = NULL;

  dex_return_error_if_fail (BZ_IS_TRANSACTION_MANAGER (self));
  dex_return_error_if_fail (self->backend != NULL);
  dex_return_error_if_fail (BZ_IS_TRANSACTION (transaction));

  bz_transaction_hold (transaction);

  if (self->queue.length > 0)
    {
      BzTransaction *to_merge[2] = { 0 };
      guint          position    = 0;

      data = g_queue_pop_head (&self->queue);

      to_merge[0] = data->transaction;
      to_merge[1] = g_steal_pointer (&transaction);
      transaction = bz_transaction_new_merged (to_merge, G_N_ELEMENTS (to_merge));

      g_list_store_find (self->transactions, data->transaction, &position);
      g_list_store_splice (self->transactions, position, 1, (gpointer *) &transaction, 1);

      g_clear_object (&data->transaction);
      data->transaction = transaction;
    }
  else
    {
      data              = queued_schedule_data_new ();
      data->self        = bz_track_weak (self);
      data->transaction = g_object_ref (transaction);
      data->promise     = dex_promise_new_cancellable ();

      g_list_store_insert (self->transactions, 0, transaction);
    }

  g_queue_push_head (&self->queue, queued_schedule_data_ref (data));
  if (self->current == NULL && !self->paused)
    dex_future_disown (dispatch_next (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_TRANSACTIONS]);
  return dex_ref (data->promise);
}

void
bz_transaction_manager_cancel_current (BzTransactionManager *self)
{
  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  if (self->current == NULL)
    return;

  dex_promise_reject (
      self->current,
      g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled by API"));
  dex_clear (&self->current);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
}

void
bz_transaction_manager_clear_finished (BzTransactionManager *self)
{
  guint    n_items   = 0;
  gboolean had_items = FALSE;

  g_return_if_fail (BZ_IS_TRANSACTION_MANAGER (self));

  n_items   = g_list_model_get_n_items (G_LIST_MODEL (self->transactions));
  had_items = n_items > 0;

  for (guint i = 0; i < n_items;)
    {
      g_autoptr (BzTransaction) transaction = NULL;
      gboolean finished                     = FALSE;

      transaction = g_list_model_get_item (G_LIST_MODEL (self->transactions), i);
      g_object_get (transaction, "finished", &finished, NULL);

      if (finished)
        {
          g_list_store_remove (self->transactions, i);
          n_items--;
        }
      else
        i++;
    }

  if (had_items && n_items == 0)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_TRANSACTIONS]);
}

static DexFuture *
transaction_fiber (QueuedScheduleData *data)
{
  g_autoptr (BzTransactionManager) self = NULL;
  BzTransaction *transaction            = data->transaction;
  DexPromise    *promise                = data->promise;
  g_autoptr (GError) local_error        = NULL;
  gboolean result                       = FALSE;
  guint    n_installs                   = 0;
  guint    n_updates                    = 0;
  guint    n_removals                   = 0;
  g_autoptr (GListStore) store          = NULL;
  g_autoptr (DexChannel) channel        = NULL;
  g_autoptr (DexFuture) future          = NULL;
  g_autoptr (GHashTable) op_set         = NULL;
  g_autoptr (GHashTable) pending_set    = NULL;

  bz_weak_get_or_return_reject (self, data->self);

  g_object_set (
      transaction,
      "status", "Starting up...",
      "progress", 0.0,
      NULL);

  self->current_progress = 0.0;
  self->pending          = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_PROGRESS]);

#define COUNT(type)                                  \
  G_STMT_START                                       \
  {                                                  \
    GListModel *model = NULL;                        \
                                                     \
    model = bz_transaction_get_##type (transaction); \
    if (model != NULL)                               \
      n_##type = g_list_model_get_n_items (model);   \
  }                                                  \
  G_STMT_END

  COUNT (installs);
  COUNT (updates);
  COUNT (removals);

#undef COUNT

  /* TODO: make reading config less bad */
  if (self->config != NULL &&
      bz_main_config_get_hooks (self->config) != NULL)
    {
      GListModel *hooks   = NULL;
      guint       n_hooks = 0;

      hooks   = bz_main_config_get_hooks (self->config);
      n_hooks = g_list_model_get_n_items (hooks);

      for (guint i = 0; i < n_installs + n_updates + n_removals; i++)
        {
          const char *ts_kind       = NULL;
          g_autoptr (BzEntry) entry = NULL;
          const char *ts_appid      = NULL;

          if (i < n_installs)
            {
              ts_kind = "install";
              entry   = g_list_model_get_item (
                  bz_transaction_get_installs (transaction),
                  i);
            }
          else if (i < n_installs + n_updates)
            {
              ts_kind = "update";
              entry   = g_list_model_get_item (
                  bz_transaction_get_updates (transaction),
                  i - n_installs);
            }
          else
            {
              ts_kind = "removal";
              entry   = g_list_model_get_item (
                  bz_transaction_get_removals (transaction),
                  i - n_updates - n_installs);
            }
          ts_appid = bz_entry_get_id (entry);

          for (guint j = 0; j < n_hooks; j++)
            {
              g_autoptr (BzHook) hook  = NULL;
              BzHookSignal when        = 0;
              int          hook_result = HOOK_CONTINUE;

              hook = g_list_model_get_item (hooks, j);
              when = bz_hook_get_when (hook);

              if (when == BZ_HOOK_SIGNAL_BEFORE_TRANSACTION)
                hook_result = execute_hook (self, hook, "before-transaction", ts_kind, ts_appid);

              if (hook_result == HOOK_CONFIRM ||
                  hook_result == HOOK_STOP)
                break;
              else if (hook_result == HOOK_DENY)
                return dex_future_new_reject (
                    BZ_TRANSACTION_MGR_ERROR,
                    BZ_TRANSACTION_MGR_ERROR_CANCELLED_BY_HOOK,
                    "The transaction was prevented by a configured hook");
            }
        }
    }

  store = g_list_store_new (BZ_TYPE_TRANSACTION);
  g_list_store_append (store, transaction);

  channel = dex_channel_new (0);
  future  = bz_backend_merge_and_schedule_transactions (
      self->backend,
      G_LIST_MODEL (store),
      channel,
      dex_promise_get_cancellable (promise));

  op_set      = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
  pending_set = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
  for (;;)
    {
      g_autoptr (GObject) object = NULL;

      object = dex_await_object (dex_channel_receive (channel), NULL);
      if (object == NULL)
        break;

      if (BZ_IS_BACKEND_TRANSACTION_OP_PAYLOAD (object))
        {
          if (g_hash_table_contains (op_set, object))
            {
              g_autofree char *error = NULL;

              error = g_object_steal_data (object, "error");
              if (error != NULL)
                bz_transaction_error_out_task (
                    transaction, BZ_BACKEND_TRANSACTION_OP_PAYLOAD (object), error);
              else
                bz_transaction_finish_task (
                    transaction, BZ_BACKEND_TRANSACTION_OP_PAYLOAD (object));
              g_hash_table_remove (op_set, object);

              if (g_hash_table_contains (pending_set, object))
                {
                  g_hash_table_remove (pending_set, object);
                  self->pending = g_hash_table_size (pending_set) ==
                                  g_hash_table_size (op_set);
                  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
                }
            }
          else
            {
              bz_transaction_add_task (
                  transaction, BZ_BACKEND_TRANSACTION_OP_PAYLOAD (object));
              g_hash_table_add (op_set, g_object_ref (object));
            }
        }
      else if (BZ_IS_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object))
        {
          const char *status         = NULL;
          gboolean    is_estimating  = FALSE;
          double      total_progress = 0.0;

          bz_transaction_update_task (
              transaction, BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object));

          status = bz_backend_transaction_op_progress_payload_get_status (
              BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object));
          is_estimating = bz_backend_transaction_op_progress_payload_get_is_estimating (
              BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object));
          total_progress = bz_backend_transaction_op_progress_payload_get_total_progress (
              BZ_BACKEND_TRANSACTION_OP_PROGRESS_PAYLOAD (object));

          g_object_set (
              transaction,
              "pending", is_estimating,
              "status", status,
              "progress", total_progress,
              NULL);

          self->current_progress = total_progress;
          g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_PROGRESS]);

          if (is_estimating && !g_hash_table_contains (pending_set, object))
            {
              g_hash_table_add (pending_set, g_object_ref (object));
              self->pending = g_hash_table_size (pending_set) ==
                              g_hash_table_size (op_set);
              g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
            }
          else if (!is_estimating && g_hash_table_contains (pending_set, object))
            {
              g_hash_table_remove (pending_set, object);
              self->pending = g_hash_table_size (pending_set) ==
                              g_hash_table_size (op_set);
              g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
            }
        }
    }

  result = dex_await (g_steal_pointer (&future), &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  /* FIXME: duplicate code */
  if (self->config != NULL &&
      bz_main_config_get_hooks (self->config) != NULL)
    {
      GListModel *hooks   = NULL;
      guint       n_hooks = 0;

      hooks   = bz_main_config_get_hooks (self->config);
      n_hooks = g_list_model_get_n_items (hooks);

      for (guint i = 0; i < n_installs + n_updates + n_removals; i++)
        {
          const char *ts_kind       = NULL;
          g_autoptr (BzEntry) entry = NULL;
          const char *ts_appid      = NULL;

          if (i < n_installs)
            {
              ts_kind = "install";
              entry   = g_list_model_get_item (
                  bz_transaction_get_installs (transaction),
                  i);
            }
          else if (i < n_installs + n_updates)
            {
              ts_kind = "update";
              entry   = g_list_model_get_item (
                  bz_transaction_get_updates (transaction),
                  i - n_installs);
            }
          else
            {
              ts_kind = "removal";
              entry   = g_list_model_get_item (
                  bz_transaction_get_removals (transaction),
                  i - n_updates - n_installs);
            }
          ts_appid = bz_entry_get_id (entry);

          for (guint j = 0; j < n_hooks; j++)
            {
              g_autoptr (BzHook) hook  = NULL;
              BzHookSignal when        = 0;
              int          hook_result = HOOK_CONTINUE;

              hook = g_list_model_get_item (hooks, j);
              when = bz_hook_get_when (hook);

              if (when == BZ_HOOK_SIGNAL_AFTER_TRANSACTION)
                hook_result = execute_hook (self, hook, "after-transaction", ts_kind, ts_appid);

              if (hook_result == HOOK_STOP)
                break;
            }
        }
    }

  return dex_future_new_true ();
}

static int
execute_hook (BzTransactionManager *self,
              BzHook               *hook,
              const char           *hook_type,
              const char           *ts_type,
              const char           *ts_appid)
{
  g_autoptr (GDateTime) date            = NULL;
  g_autofree char *timestamp_sec        = NULL;
  g_autofree char *timestamp_usec       = NULL;
  const char      *id                   = NULL;
  const char      *shell                = NULL;
  g_autoptr (GPtrArray) dialogs         = NULL;
  g_autoptr (DialogData) current_dialog = NULL;
  gboolean hook_aborted                 = FALSE;
  gboolean finish                       = FALSE;

  date           = g_date_time_new_now_utc ();
  timestamp_sec  = g_strdup_printf ("%zu", g_date_time_to_unix (date));
  timestamp_usec = g_strdup_printf ("%zu", g_date_time_to_unix_usec (date));

  id    = bz_hook_get_id (hook);
  shell = bz_hook_get_shell (hook);
  if (shell == NULL)
    {
      g_warning ("Main Config: hook definition must have shell code, skipping this hook");
      return HOOK_CONTINUE;
    }

  dialogs = g_ptr_array_new_with_free_func (dialog_data_unref);
  if (bz_hook_get_dialogs (hook) != NULL)
    {
      GListModel *config_dialogs = NULL;
      guint       n_dialogs      = 0;

      config_dialogs = bz_hook_get_dialogs (hook);
      n_dialogs      = g_list_model_get_n_items (config_dialogs);

      for (guint i = 0; i < n_dialogs; i++)
        {
          g_autoptr (BzHookDialog) config_dialog = NULL;
          const char *dialog_id                  = NULL;
          const char *dialog_title               = NULL;
          const char *dialog_body                = NULL;
          gboolean    dialog_body_use_markup     = FALSE;
          const char *dialog_default_response    = NULL;
          g_autoptr (AdwDialog) dialog           = NULL;
          guint n_opts                           = 0;
          g_autoptr (DialogData) data            = NULL;

          config_dialog           = g_list_model_get_item (config_dialogs, i);
          dialog_id               = bz_hook_dialog_get_id (config_dialog);
          dialog_title            = bz_hook_dialog_get_title (config_dialog);
          dialog_body             = bz_hook_dialog_get_body (config_dialog);
          dialog_body_use_markup  = bz_hook_dialog_get_body_use_markup (config_dialog);
          dialog_default_response = bz_hook_dialog_get_default_response_id (config_dialog);

          if (dialog_title == NULL ||
              dialog_body == NULL)
            {
              g_warning ("Main Config: dialog definition must have a title and body, skipping this hook");
              return HOOK_CONTINUE;
            }
          if (dialog_default_response == NULL)
            {
              g_warning ("Main Config: dialog definition must have a default response, skipping this hook");
              return HOOK_CONTINUE;
            }
          dialog = g_object_ref_sink (adw_alert_dialog_new (dialog_title, dialog_body));

          if (bz_hook_dialog_get_options (config_dialog) != NULL)
            {
              GListModel *config_opts = NULL;
              guint       n_options   = 0;

              config_opts = bz_hook_dialog_get_options (config_dialog);
              n_options   = g_list_model_get_n_items (config_opts);

              for (guint j = 0; j < n_options; j++)
                {
                  g_autoptr (BzHookDialogOption) config_opt = NULL;
                  const char *opt_id                        = NULL;
                  const char *opt_string                    = NULL;
                  const char *opt_style                     = NULL;

                  config_opt = g_list_model_get_item (config_opts, j);

                  opt_id = bz_hook_dialog_option_get_id (config_opt);
                  if (opt_id == NULL)
                    {
                      g_warning ("Main Config: dialog option definition must have an id, skipping this hook");
                      return HOOK_CONTINUE;
                    }

                  opt_string = bz_hook_dialog_option_get_string (config_opt);
                  if (opt_string == NULL)
                    {
                      g_warning ("Main Config: dialog option definition must have a string, skipping this hook");
                      return HOOK_CONTINUE;
                    }

                  opt_style = bz_hook_dialog_option_get_style (config_opt);

                  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), opt_id, opt_string);
                  if (opt_style != NULL)
                    {
                      AdwResponseAppearance appearance = ADW_RESPONSE_DEFAULT;

                      if (g_strcmp0 (opt_style, "suggested") == 0)
                        appearance = ADW_RESPONSE_SUGGESTED;
                      else if (g_strcmp0 (opt_style, "destructive") == 0)
                        appearance = ADW_RESPONSE_DESTRUCTIVE;
                      else
                        g_warning ("Main Config: dialog option definition appearance can be "
                                   "\"suggested\" or \"destructive\". \"%s\" is invalid.",
                                   opt_style);

                      adw_alert_dialog_set_response_appearance (
                          ADW_ALERT_DIALOG (dialog),
                          opt_id,
                          appearance);
                    }

                  n_opts++;
                }
            }
          if (n_opts == 0)
            {
              g_warning ("Main Config: dialog definition must have options, skipping this hook");
              return HOOK_CONTINUE;
            }

          adw_alert_dialog_set_body_use_markup (
              ADW_ALERT_DIALOG (dialog),
              dialog_body_use_markup);
          adw_alert_dialog_set_default_response (
              ADW_ALERT_DIALOG (dialog),
              dialog_default_response);

          data         = dialog_data_new ();
          data->id     = dialog_id != NULL ? g_strdup (dialog_id) : NULL;
          data->dialog = g_steal_pointer (&dialog);
          g_ptr_array_add (dialogs, g_steal_pointer (&data));
        }
    }

  for (guint stage = 0;; stage++)
    {
      g_autoptr (GError) local_error           = NULL;
      g_autoptr (GSubprocessLauncher) launcher = NULL;
      g_autofree char *stage_str               = NULL;
      const char      *hook_stage              = NULL;
      g_autoptr (GSubprocess) subprocess       = NULL;
      gboolean      result                     = FALSE;
      GInputStream *stdout_pipe                = NULL;
      g_autoptr (GBytes) stdout_bytes          = NULL;
      gsize            stdout_size             = 0;
      gconstpointer    stdout_data             = NULL;
      g_autofree char *stdout_str              = NULL;
      char            *stdout_newline          = NULL;

      launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
      g_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_INITIATED_UNIX_STAMP", timestamp_sec, TRUE);
      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_INITIATED_UNIX_STAMP_USEC", timestamp_usec, TRUE);

      stage_str = g_strdup_printf ("%d", stage);
      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_STAGE_IDX", stage_str, TRUE);

      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_ID", id, TRUE);
      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_TYPE", hook_type, TRUE);

      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_WAS_ABORTED", hook_aborted ? "true" : "false", TRUE);

      g_subprocess_launcher_setenv (launcher, "BAZAAR_TS_APPID", ts_appid, TRUE);
      g_subprocess_launcher_setenv (launcher, "BAZAAR_TS_TYPE", ts_type, TRUE);

      if (finish)
        hook_stage = "teardown";
      else if (hook_aborted)
        hook_stage = "catch";
      else if (stage == 0)
        hook_stage = "setup";
      else if (current_dialog != NULL)
        {
          GApplication    *application = NULL;
          GtkWindow       *window      = NULL;
          g_autofree char *response    = NULL;

          hook_stage = "teardown-dialog";

          application = g_application_get_default ();
          window      = gtk_application_get_active_window (GTK_APPLICATION (application));

          if (window != NULL)
            {
              adw_dialog_present (current_dialog->dialog, GTK_WIDGET (window));
              response = dex_await_string (
                  bz_make_alert_dialog_future (ADW_ALERT_DIALOG (current_dialog->dialog)),
                  &local_error);
              if (response == NULL)
                g_warning ("Failed to resolve response from dialog "
                           "\"%s\", assuming default response \"%s\": %s",
                           current_dialog->id,
                           adw_alert_dialog_get_default_response (
                               ADW_ALERT_DIALOG (current_dialog->dialog)),
                           local_error->message);
              g_clear_pointer (&local_error, g_error_free);
            }
          else
            g_warning ("A window was not available to present dialog "
                       "\"%s\" on, assuming default response \"%s\"",
                       current_dialog->id,
                       adw_alert_dialog_get_default_response (
                           ADW_ALERT_DIALOG (current_dialog->dialog)));

          g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_DIALOG_ID", current_dialog->id, TRUE);
          g_subprocess_launcher_setenv (
              launcher,
              "BAZAAR_HOOK_DIALOG_RESPONSE_ID",
              response != NULL
                  ? response
                  : adw_alert_dialog_get_default_response (
                        ADW_ALERT_DIALOG (current_dialog->dialog)),
              TRUE);
          g_clear_pointer (&current_dialog, dialog_data_unref);
        }
      else if (dialogs->len > 0)
        {
          hook_stage = "setup-dialog";

          current_dialog = g_ptr_array_steal_index (dialogs, 0);
          g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_DIALOG_ID", current_dialog->id, TRUE);
        }
      else
        hook_stage = "action";
      g_subprocess_launcher_setenv (launcher, "BAZAAR_HOOK_STAGE", hook_stage, TRUE);

      subprocess = g_subprocess_launcher_spawn (
          launcher,
          &local_error,
          "/bin/sh",
          "-c",
          shell,
          NULL);
      if (subprocess == NULL)
        {
          g_warning ("Hook failed to spawn, abandoning it now: %s", local_error->message);
          return HOOK_CONTINUE;
        }

      result = dex_await (
          dex_subprocess_wait_check (subprocess),
          &local_error);
      if (!result)
        {
          g_warning ("Hook failed to exit cleanly, abandoning it now: %s", local_error->message);
          return HOOK_CONTINUE;
        }

      stdout_pipe  = g_subprocess_get_stdout_pipe (subprocess);
      stdout_bytes = g_input_stream_read_bytes (stdout_pipe, 1024, NULL, &local_error);
      if (!stdout_bytes)
        {
          g_warning ("Failed to read stdout pipe of hook, abandoning it now: %s", local_error->message);
          return HOOK_CONTINUE;
        }

      stdout_data = g_bytes_get_data (stdout_bytes, &stdout_size);
      stdout_str  = g_malloc (stdout_size + 1);

      memcpy (stdout_str, stdout_data, stdout_size);
      stdout_str[stdout_size] = '\0';

      stdout_newline = strchr (stdout_str, '\n');
      if (stdout_newline != NULL)
        *stdout_newline = '\0';

      if (g_strcmp0 (hook_stage, "setup") == 0)
        {
          if (g_strcmp0 (stdout_str, "ok") == 0)
            continue;
          else if (g_strcmp0 (stdout_str, "pass") == 0)
            return HOOK_CONTINUE;
        }
      else if (g_strcmp0 (hook_stage, "setup-dialog") == 0)
        {
          if (g_strcmp0 (stdout_str, "ok") == 0)
            continue;
          else if (g_strcmp0 (stdout_str, "pass") == 0)
            {
              g_clear_pointer (&current_dialog, dialog_data_unref);
              continue;
            }
        }
      else if (g_strcmp0 (hook_stage, "teardown-dialog") == 0)
        {
          if (g_strcmp0 (stdout_str, "ok") == 0)
            continue;
          else if (g_strcmp0 (stdout_str, "abort") == 0)
            {
              hook_aborted = TRUE;
              continue;
            }
        }
      else if (g_strcmp0 (hook_stage, "catch") == 0)
        {
          if (g_strcmp0 (stdout_str, "recover") == 0)
            {
              hook_aborted = FALSE;
              continue;
            }
          else if (g_strcmp0 (stdout_str, "abort") == 0)
            {
              finish = TRUE;
              continue;
            }
        }
      else if (g_strcmp0 (hook_stage, "action") == 0)
        {
          finish = TRUE;
          continue;
        }
      else if (g_strcmp0 (hook_stage, "teardown") == 0)
        {
          if (g_strcmp0 (stdout_str, "continue") == 0)
            return HOOK_CONTINUE;
          else if (g_strcmp0 (stdout_str, "stop") == 0)
            return HOOK_STOP;
          else if (g_strcmp0 (stdout_str, "confirm") == 0)
            return HOOK_CONFIRM;
          else if (g_strcmp0 (stdout_str, "deny") == 0)
            return HOOK_DENY;
        }
      else
        g_assert_not_reached ();

      g_warning ("Received invalid response from hook for stage \"%s\", abandoning it now",
                 hook_stage);
      return HOOK_CONTINUE;
    }
}

static DexFuture *
transaction_finally (DexFuture          *future,
                     QueuedScheduleData *data)
{
  g_autoptr (BzTransactionManager) self = NULL;
  g_autoptr (GError) local_error        = NULL;
  BzTransaction   *transaction          = data->transaction;
  DexPromise      *promise              = data->promise;
  GTimer          *timer                = data->timer;
  const GValue    *value                = NULL;
  g_autofree char *status               = NULL;

  bz_weak_get_or_return_reject (self, data->self);

  g_timer_stop (timer);
  status = g_strdup_printf (
      _ ("Finished in %.02f seconds"),
      g_timer_elapsed (data->timer, NULL));

  value = dex_future_get_value (future, &local_error);
  g_object_set (
      transaction,
      "status", status,
      "progress", 1.0,
      "finished", TRUE,
      "success", value != NULL,
      "error", local_error != NULL ? local_error->message : NULL,
      NULL);

  self->current_progress = 1.0;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_PROGRESS]);

  if (value != NULL)
    {
      g_signal_emit (self, signals[SIGNAL_SUCCESS], 0, transaction);
      dex_promise_resolve_boolean (promise, TRUE);
    }
  else
    {
      g_warning ("Transaction failed to complete: %s", local_error->message);
      g_signal_emit (self, signals[SIGNAL_FAILURE], 0, transaction);
      dex_promise_resolve_boolean (promise, FALSE);
    }

  return dex_future_new_true ();
}

static DexFuture *
then_loop_cb (DexFuture *future,
              GWeakRef  *wr)
{
  g_autoptr (BzTransactionManager) self = NULL;

  bz_weak_get_or_return_reject (self, wr);

  dex_clear (&self->current);
  if (self->paused)
    {
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
      return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_PENDING, "Paused");
    }
  return dispatch_next (self);
}

static DexFuture *
dispatch_next (BzTransactionManager *self)
{
  g_autoptr (QueuedScheduleData) data = NULL;
  g_autoptr (DexFuture) future        = NULL;

  if (self->queue.length == 0)
    {
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PENDING]);
      return dex_future_new_reject (G_IO_ERROR, G_IO_ERROR_UNKNOWN, "No more futures in queue");
    }

  data = g_queue_pop_tail (&self->queue);

  g_clear_pointer (&data->timer, g_timer_destroy);
  data->timer = g_timer_new ();

  future = dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) transaction_fiber,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  future = dex_future_finally (
      future, (DexFutureCallback) transaction_finally,
      queued_schedule_data_ref (data),
      queued_schedule_data_unref);
  future = dex_future_first (
      future,
      dex_ref (data->promise),
      NULL);
  future = dex_future_then_loop (
      future,
      (DexFutureCallback) then_loop_cb,
      bz_track_weak (self),
      bz_weak_release);
  dex_future_disown (g_steal_pointer (&future));

  dex_clear (&self->current);
  self->current = dex_ref (data->promise);

  return dex_ref (data->promise);
}

static inline void
finish_queued_schedule_data (gpointer ptr)
{
  QueuedScheduleData *data = ptr;

  g_clear_pointer (&data->self, bz_weak_release);

  if (data->transaction != NULL)
    bz_transaction_release (data->transaction);
  g_clear_object (&data->transaction);

  if (data->promise != NULL &&
      dex_future_is_pending (DEX_FUTURE (data->promise)))
    dex_promise_reject (
        data->promise,
        g_error_new (G_IO_ERROR,
                     G_IO_ERROR_CANCELLED,
                     "User data was destroyed"));
  dex_clear (&data->promise);

  g_clear_pointer (&data->timer, g_timer_destroy);
}

/* bz-util.h
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

#pragma once

#include <libdex.h>

#define BZ_RELEASE_DATA(name, unref) \
  if ((unref) != NULL)               \
    g_clear_pointer (&self->name, (unref));

#define BZ_RELEASE_UTAG(name, remove) \
  if ((remove) != NULL)               \
    g_clear_handle_id (&self->name, (remove));

/* va args = releases */
#define BZ_DEFINE_DATA(name, Name, layout, ...)    \
  typedef struct _##Name##Data Name##Data;         \
  struct _##Name##Data                             \
  {                                                \
    gatomicrefcount rc;                            \
    struct layout;                                 \
  };                                               \
  G_GNUC_UNUSED                                    \
  static inline Name##Data *                       \
  name##_data_new (void)                           \
  {                                                \
    Name##Data *data = NULL;                       \
    data             = g_new0 (typeof (*data), 1); \
    g_atomic_ref_count_init (&data->rc);           \
    return data;                                   \
  }                                                \
  G_GNUC_UNUSED                                    \
  static inline Name##Data *                       \
  name##_data_ref (gpointer ptr)                   \
  {                                                \
    Name##Data *self = ptr;                        \
    g_atomic_ref_count_inc (&self->rc);            \
    return self;                                   \
  }                                                \
  G_GNUC_UNUSED                                    \
  static void                                      \
  name##_data_deinit (gpointer ptr)                \
  {                                                \
    Name##Data *self = ptr;                        \
    __VA_ARGS__                                    \
  }                                                \
  G_GNUC_UNUSED                                    \
  static void                                      \
  name##_data_unref (gpointer ptr)                 \
  {                                                \
    Name##Data *self = ptr;                        \
    if (g_atomic_ref_count_dec (&self->rc))        \
      {                                            \
        name##_data_deinit (self);                 \
        g_free (self);                             \
      }                                            \
  }                                                \
  G_GNUC_UNUSED                                    \
  static void                                      \
  name##_data_unref_closure (gpointer  data,       \
                             GClosure *closure)    \
  {                                                \
    name##_data_unref (data);                      \
  }                                                \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (Name##Data, name##_data_unref);

/* Be careful with deadlocks */
typedef DexFuture BzGuard;
static inline void
bz_guard_destroy (BzGuard *guard)
{
  if (dex_future_is_pending (guard))
    dex_promise_resolve_boolean (DEX_PROMISE (guard), TRUE);
  dex_unref (guard);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (BzGuard, bz_guard_destroy);
#define bz_clear_guard(_pp) g_clear_pointer (_pp, bz_guard_destroy)

#define BZ_BEGIN_GUARD_WITH_CONTEXT(_guard, _mutex, _gate) \
  G_STMT_START                                             \
  {                                                        \
    g_autoptr (GMutexLocker) _locker = NULL;               \
    g_autoptr (DexFuture) _wait      = NULL;               \
                                                           \
    _locker = g_mutex_locker_new (_mutex);                 \
    if (*(_guard) == NULL)                                 \
      *(_guard) = (DexFuture *) dex_promise_new ();        \
    if (*(_gate) != NULL)                                  \
      {                                                    \
        if (dex_future_is_pending (*(_gate)))              \
          _wait = g_steal_pointer (_gate);                 \
        else                                               \
          dex_clear (_gate);                               \
      }                                                    \
    *(_gate) = dex_ref (*(_guard));                        \
    g_clear_pointer (&_locker, g_mutex_locker_free);       \
                                                           \
    if (_wait != NULL)                                     \
      dex_await (g_steal_pointer (&_wait), NULL);          \
  }                                                        \
  G_STMT_END

#define BZ_BEGIN_GUARD(_guard)                             \
  G_STMT_START                                             \
  {                                                        \
    static GMutex   _mutex = { 0 };                        \
    static BzGuard *_gate  = NULL;                         \
    BZ_BEGIN_GUARD_WITH_CONTEXT (_guard, &_mutex, &_gate); \
  }                                                        \
  G_STMT_END

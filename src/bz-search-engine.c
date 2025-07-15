/* bz-search-engine.c
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

#include "bz-search-engine.h"
#include "bz-entry-group.h"
#include "bz-env.h"
#include "bz-util.h"

struct _BzSearchEngine
{
  GObject parent_instance;

  GListModel *model;

  DexScheduler *scheduler;
};

G_DEFINE_FINAL_TYPE (BzSearchEngine, bz_search_engine, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

BZ_DEFINE_DATA (
    entry,
    Entry,
    {
      BzEntryGroup *group;
      GPtrArray    *tokens;
      gint          score;
    },
    BZ_RELEASE_DATA (group, g_object_unref);
    BZ_RELEASE_DATA (tokens, g_ptr_array_unref))

BZ_DEFINE_DATA (
    query_task,
    QueryTask,
    {
      GArray *entries;
      char  **terms;
    },
    BZ_RELEASE_DATA (entries, g_array_unref);
    BZ_RELEASE_DATA (terms, g_strfreev))
static DexFuture *
query_task_fiber (QueryTaskData *data);

static gint
cmp_entry_data (EntryData *a,
                EntryData *b);

static void
bz_search_engine_dispose (GObject *object)
{
  BzSearchEngine *self = BZ_SEARCH_ENGINE (object);

  g_clear_object (&self->model);
  dex_clear (&self->scheduler);

  G_OBJECT_CLASS (bz_search_engine_parent_class)->dispose (object);
}

static void
bz_search_engine_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSearchEngine *self = BZ_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, bz_search_engine_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_engine_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSearchEngine *self = BZ_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      bz_search_engine_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_engine_class_init (BzSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_search_engine_set_property;
  object_class->get_property = bz_search_engine_get_property;
  object_class->dispose      = bz_search_engine_dispose;

  props[PROP_MODEL] =
      g_param_spec_object (
          "model",
          NULL, NULL,
          G_TYPE_LIST_MODEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_search_engine_init (BzSearchEngine *self)
{
  // self->scheduler = dex_thread_pool_scheduler_new ();
}

BzSearchEngine *
bz_search_engine_new (void)
{
  return g_object_new (BZ_TYPE_SEARCH_ENGINE, NULL);
}

GListModel *
bz_search_engine_get_model (BzSearchEngine *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_ENGINE (self), NULL);
  return self->model;
}

void
bz_search_engine_set_model (BzSearchEngine *self,
                            GListModel     *model)
{
  g_return_if_fail (BZ_IS_SEARCH_ENGINE (self));

  g_clear_pointer (&self->model, g_object_unref);
  if (model != NULL)
    self->model = g_object_ref (model);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

DexFuture *
bz_search_engine_query (BzSearchEngine    *self,
                        const char *const *terms)
{
  guint n_entries                = 0;
  g_autoptr (GArray) entries     = NULL;
  g_autoptr (QueryTaskData) data = NULL;

  g_return_val_if_fail (BZ_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (terms != NULL && *terms != NULL, NULL);

  if (self->model != NULL)
    n_entries = g_list_model_get_n_items (self->model);
  if (n_entries == 0 || **terms == '\0')
    return dex_future_new_take_boxed (
        G_TYPE_PTR_ARRAY,
        g_ptr_array_new_with_free_func (g_object_unref));

  entries = g_array_new (FALSE, TRUE, sizeof (EntryData));
  g_array_set_clear_func (entries, entry_data_deinit);

  g_array_set_size (entries, n_entries);
  for (guint i = 0; i < n_entries; i++)
    {
      EntryData *addr     = NULL;
      BzEntry   *ui_entry = NULL;
      GPtrArray *tokens   = NULL;

      addr = &g_array_index (entries, EntryData, i);

      addr->group = g_list_model_get_item (self->model, i);
      ui_entry    = bz_entry_group_get_ui_entry (addr->group);

      /* We understand BzEntry -> search-tokens to be immutable */
      tokens       = bz_entry_get_search_tokens (ui_entry);
      addr->tokens = g_ptr_array_ref (tokens);

      addr->score = 0;
    }

  data          = query_task_data_new ();
  data->entries = g_steal_pointer (&entries);
  data->terms   = g_strdupv ((gchar **) terms);

  return dex_scheduler_spawn (
      dex_thread_pool_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) query_task_fiber,
      query_task_data_ref (data), query_task_data_unref);
}

static DexFuture *
query_task_fiber (QueryTaskData *data)
{
  GArray *entries               = data->entries;
  char  **terms                 = data->terms;
  g_autoptr (GPtrArray) results = NULL;

  for (guint i = 0; i < entries->len;)
    {
      EntryData *edata = NULL;

      edata = &g_array_index (entries, EntryData, i);

      for (char **term = terms; *term != NULL; term++)
        {
          gint term_len   = 0;
          gint term_score = 0;

          term_len = strlen (*term);

          for (guint j = 0; j < edata->tokens->len; j++)
            {
              const char *token = NULL;

              token = g_ptr_array_index (edata->tokens, j);
              // g_assert (*token != '\0');

              /* See BzSearchWidget */
              if (g_strcmp0 (*term, token) == 0)
                term_score += term_len * 5;
              else if (g_str_match_string (*term, token, TRUE))
                {
                  gint token_len = 0;

                  token_len = strlen (token);
                  if (token_len == term_len)
                    term_score += term_len * 4;
                  else
                    {
                      double len_ratio = 0.0;
                      double add       = 0.0;

                      len_ratio = (double) term_len / (double) token_len;
                      add       = (double) term_len * len_ratio;
                      term_score += (gint) MAX (round (add), 1.0);
                    }
                }
            }

          if (term_score > 0)
            edata->score += term_score;
          else
            {
              edata->score = 0;
              break;
            }
        }

      if (edata->score > 0)
        i++;
      else
        g_array_remove_index_fast (entries, i);
    }

  if (entries->len > 0)
    g_array_sort (entries, (GCompareFunc) cmp_entry_data);

  results = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_set_size (results, entries->len);
  for (guint i = 0; i < entries->len; i++)
    {
      EntryData *edata = NULL;

      edata                          = &g_array_index (entries, EntryData, i);
      g_ptr_array_index (results, i) = g_steal_pointer (&edata->group);
    }

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY,
      g_steal_pointer (&results));
}

static gint
cmp_entry_data (EntryData *a,
                EntryData *b)
{
  return b->score - a->score;
}

/* End of bz-search-engine.c */

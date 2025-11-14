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
#include "bz-search-result.h"
#include "bz-util.h"

struct _BzSearchEngine
{
  GObject parent_instance;

  GListModel *model;
};

G_DEFINE_FINAL_TYPE (BzSearchEngine, bz_search_engine, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static inline double
test_strings (const char *query,
              const char *against);

typedef struct
{
  guint  idx;
  double val;
} Score;

static gint
cmp_scores (Score *a,
            Score *b);

#define PERFECT        1.0
#define ALMOST_PERFECT 0.95
#define SAME_CLASS     0.2
#define SAME_CLUSTER   0.1
#define NO_MATCH       0.0

BZ_DEFINE_DATA (
    query_task,
    QueryTask,
    {
      char     **terms;
      GPtrArray *snapshot;
    },
    BZ_RELEASE_DATA (terms, g_strfreev);
    BZ_RELEASE_DATA (snapshot, g_ptr_array_unref))
static DexFuture *
query_task_fiber (QueryTaskData *data);

BZ_DEFINE_DATA (
    query_sub_task,
    QuerySubTask,
    {
      char      *query_utf8;
      GPtrArray *shallow_mirror;
      double     threshold;
      guint      work_offset;
      guint      work_length;
    },
    BZ_RELEASE_DATA (query_utf8, g_free);
    BZ_RELEASE_DATA (shallow_mirror, g_ptr_array_unref));
static DexFuture *
query_sub_task_fiber (QuerySubTaskData *data);

static void
bz_search_engine_dispose (GObject *object)
{
  BzSearchEngine *self = BZ_SEARCH_ENGINE (object);

  g_clear_object (&self->model);

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
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  g_clear_object (&self->model);
  if (model != NULL)
    self->model = g_object_ref (model);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

DexFuture *
bz_search_engine_query (BzSearchEngine    *self,
                        const char *const *terms)
{
  guint n_groups = 0;

  dex_return_error_if_fail (BZ_IS_SEARCH_ENGINE (self));
  dex_return_error_if_fail (terms != NULL && *terms != NULL);

  if (self->model != NULL)
    n_groups = g_list_model_get_n_items (self->model);

  if (self->model == NULL ||
      n_groups == 0 ||
      **terms == '\0')
    {
      g_autoptr (GPtrArray) ret = NULL;

      ret = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_set_size (ret, n_groups);

      for (guint i = 0; i < ret->len; i++)
        {
          g_autoptr (BzEntryGroup) group    = NULL;
          g_autoptr (BzSearchResult) result = NULL;

          group = g_list_model_get_item (self->model, i);

          result = bz_search_result_new ();
          bz_search_result_set_group (result, group);
          bz_search_result_set_original_index (result, i);
          g_ptr_array_index (ret, i) = g_steal_pointer (&result);
        }

      return dex_future_new_take_boxed (
          G_TYPE_PTR_ARRAY,
          g_steal_pointer (&ret));
    }
  else
    {
      g_autoptr (GPtrArray) snapshot = NULL;
      g_autoptr (QueryTaskData) data = NULL;

      snapshot = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_set_size (snapshot, n_groups);

      for (guint i = 0; i < snapshot->len; i++)
        g_ptr_array_index (snapshot, i) = g_list_model_get_item (self->model, i);

      data           = query_task_data_new ();
      data->terms    = g_strdupv ((gchar **) terms);
      data->snapshot = g_steal_pointer (&snapshot);

      return dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) query_task_fiber,
          query_task_data_ref (data), query_task_data_unref);
    }
}

static DexFuture *
query_task_fiber (QueryTaskData *data)
{
  char     **terms                  = data->terms;
  GPtrArray *shallow_mirror         = data->snapshot;
  g_autoptr (GError) local_error    = NULL;
  gboolean         result           = FALSE;
  g_autofree char *query_utf8       = NULL;
  double           threshold        = 0.0;
  guint            n_sub_tasks      = 0;
  guint            scores_per_task  = 0;
  g_autoptr (GPtrArray) sub_futures = NULL;
  g_autoptr (GArray) scores         = NULL;
  g_autoptr (GPtrArray) results     = NULL;

  query_utf8 = g_strjoinv (" ", terms);
  // threshold = (double) strlen (joined) / (double) g_strv_length (terms);
  threshold = (double) strlen (query_utf8);

  n_sub_tasks     = MAX (1, MIN (shallow_mirror->len / 512, g_get_num_processors ()));
  scores_per_task = shallow_mirror->len / n_sub_tasks;

  sub_futures = g_ptr_array_new_with_free_func (dex_unref);
  for (guint i = 0; i < n_sub_tasks; i++)
    {
      g_autoptr (QuerySubTaskData) sub_data = NULL;
      g_autoptr (DexFuture) future          = NULL;

      sub_data                 = query_sub_task_data_new ();
      sub_data->query_utf8     = g_strdup (query_utf8);
      sub_data->shallow_mirror = g_ptr_array_ref (shallow_mirror);
      sub_data->threshold      = threshold;
      sub_data->work_offset    = i * scores_per_task;
      sub_data->work_length    = scores_per_task;

      if (i >= n_sub_tasks - 1)
        sub_data->work_length += shallow_mirror->len % n_sub_tasks;

      future = dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) query_sub_task_fiber,
          query_sub_task_data_ref (sub_data),
          query_sub_task_data_unref);

      g_ptr_array_add (sub_futures, g_steal_pointer (&future));
    }

  result = dex_await (dex_future_allv (
                          (DexFuture *const *) sub_futures->pdata, sub_futures->len),
                      &local_error);
  if (!result)
    return dex_future_new_for_error (g_steal_pointer (&local_error));

  scores = g_array_new (FALSE, FALSE, sizeof (Score));
  for (guint i = 0; i < sub_futures->len; i++)
    {
      DexFuture *future     = NULL;
      GArray    *scores_out = NULL;

      future     = g_ptr_array_index (sub_futures, i);
      scores_out = g_value_get_boxed (dex_future_get_value (future, NULL));

      if (scores_out->len > 0)
        g_array_append_vals (scores, scores_out->data, scores_out->len);
    }
  if (scores->len > 0)
    g_array_sort (scores, (GCompareFunc) cmp_scores);

  results = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_set_size (results, scores->len);
  for (guint i = 0; i < scores->len; i++)
    {
      Score        *score                      = NULL;
      BzEntryGroup *group                      = NULL;
      g_autoptr (BzSearchResult) search_result = NULL;

      score = &g_array_index (scores, Score, i);
      group = g_ptr_array_index (shallow_mirror, score->idx);

      search_result = bz_search_result_new ();
      bz_search_result_set_group (search_result, group);
      bz_search_result_set_original_index (search_result, score->idx);
      bz_search_result_set_score (search_result, score->val);

      g_ptr_array_index (results, i) = g_steal_pointer (&search_result);
    }

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY,
      g_steal_pointer (&results));
}

static DexFuture *
query_sub_task_fiber (QuerySubTaskData *data)
{
  GPtrArray *shallow_mirror     = data->shallow_mirror;
  char      *query_utf8         = data->query_utf8;
  double     threshold          = data->threshold;
  guint      work_offset        = data->work_offset;
  guint      work_length        = data->work_length;
  g_autoptr (GArray) scores_out = NULL;

  scores_out = g_array_new (FALSE, FALSE, sizeof (Score));

  for (guint i = 0; i < work_length; i++)
    {
      g_autoptr (GMutexLocker) locker = NULL;
      BzEntryGroup *group             = NULL;
      const char   *id                = NULL;
      const char   *title             = NULL;
      const char   *developer         = NULL;
      const char   *description       = NULL;
      GPtrArray    *search_tokens     = NULL;
      double        score             = 0.0;

      group  = g_ptr_array_index (shallow_mirror, work_offset + i);
      locker = bz_entry_group_lock (group);

      id            = bz_entry_group_get_id (group);
      title         = bz_entry_group_get_title (group);
      developer     = bz_entry_group_get_developer (group);
      description   = bz_entry_group_get_description (group);
      search_tokens = bz_entry_group_get_search_tokens (group);

#define EVALUATE_STRING(_s, _weight)                          \
  if ((_s) != NULL)                                           \
    {                                                         \
      double token_score = 0.0;                               \
                                                              \
      if (strstr ((_s), query_utf8) != NULL)                  \
        token_score += threshold *                            \
                       (1.0 + ((double) strlen (query_utf8) / \
                               (double) strlen ((_s))));      \
      else if ((_weight) > 0.0)                               \
        token_score += test_strings (query_utf8, (_s));       \
      if ((_weight) > 0.0)                                    \
        token_score *= (_weight);                             \
                                                              \
      score += token_score;                                   \
    }

      EVALUATE_STRING (id, -1.0);
      EVALUATE_STRING (title, 1.0);
      EVALUATE_STRING (developer, 1.0);
      EVALUATE_STRING (description, -1.0);

      if (search_tokens != NULL)
        {
          for (guint j = 0; j < search_tokens->len; j++)
            {
              const char *token = NULL;

              token = g_ptr_array_index (search_tokens, j);
              EVALUATE_STRING (token, -1.0);
            }
        }

#undef EVALUATE_STRING

      if (score > threshold)
        {
          Score append = { 0 };

          append.idx = work_offset + i;
          append.val = score;
          g_array_append_val (scores_out, append);
        }
    }

  return dex_future_new_take_boxed (G_TYPE_ARRAY, g_steal_pointer (&scores_out));
}

static inline double
test_strings (const char *query,
              const char *against)
{
  const char *last_best_char = NULL;
  guint       misses         = 0;
  double      score          = 0.0;

  for (const char *q = query;
       q != NULL && *q != '\0';
       q = g_utf8_next_char (q))
    {
      gunichar    query_ch   = 0;
      const char *best_char  = NULL;
      double      best_score = 0.0;

      query_ch = g_utf8_get_char (q);

      for (const char *a = against + strlen (against);
           a != NULL && a >= against;
           a = g_utf8_prev_char (a))
        {
          gunichar against_ch = 0;
          double   tmp_score  = 0;

          against_ch = g_utf8_get_char (a);

          tmp_score = against_ch == query_ch ? PERFECT : NO_MATCH;
          if (tmp_score > NO_MATCH &&
              (tmp_score > best_score ||
               ((last_best_char == NULL ||
                 last_best_char < best_char) &&
                tmp_score >= best_score)))
            {
              best_char  = a;
              best_score = tmp_score;
            }
        }

      if (best_char != NULL)
        {
          if (last_best_char != NULL)
            {
              gssize diff = 0;

              diff = (gssize) best_char - (gssize) last_best_char;
              if (diff > 1)
                /* Penalize the query for fragmentation */
                best_score /= (double) diff / 2.0;
              else if (diff < 0)
                /* Penalize the query more harshly for
                 * transposing and fragmentation
                 */
                best_score /= (double) ABS (diff);
            }

          score += best_score;
          last_best_char = best_char;
        }
      else
        misses++;
    }

  /* Penalize the query for including chars that didn't match at all */
  score /= (double) (misses + 1);

  return score;
}

static gint
cmp_scores (Score *a,
            Score *b)
{
  return (b->val - a->val < 0.0) ? -1 : 1;
}

/* End of bz-search-engine.c */

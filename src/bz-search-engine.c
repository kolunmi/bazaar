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
  GPtrArray  *mirror;
};

G_DEFINE_FINAL_TYPE (BzSearchEngine, bz_search_engine, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_MODEL,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
items_changed (BzSearchEngine *self,
               guint           position,
               guint           removed,
               guint           added,
               GListModel     *model);

BZ_DEFINE_DATA (
    query_task,
    QueryTask,
    {
      char     **terms;
      GPtrArray *shallow_mirror;
    },
    BZ_RELEASE_DATA (terms, g_strfreev);
    BZ_RELEASE_DATA (shallow_mirror, g_ptr_array_unref))
static DexFuture *
query_task_fiber (QueryTaskData *data);

typedef struct
{
  gunichar     ch;
  GUnicodeType type;
} IndexedChar;

BZ_DEFINE_DATA (
    indexed_string,
    IndexedString,
    {
      char        *ptr;
      glong        utf8_len;
      IndexedChar *chars;
      double       weight;
    },
    BZ_RELEASE_DATA (ptr, g_free);
    BZ_RELEASE_DATA (chars, g_free))

static inline void
index_string (const char        *s,
              IndexedStringData *out);

static inline double
test_strings (IndexedStringData *query,
              IndexedStringData *against);

BZ_DEFINE_DATA (
    group,
    Group,
    {
      BzEntryGroup   *group;
      GArray         *istrings;
      BzSearchResult *default_result;
    },
    BZ_RELEASE_DATA (group, g_object_unref);
    BZ_RELEASE_DATA (istrings, g_array_unref);
    BZ_RELEASE_DATA (default_result, g_object_unref))

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

static void
bz_search_engine_dispose (GObject *object)
{
  BzSearchEngine *self = BZ_SEARCH_ENGINE (object);

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);

  g_clear_pointer (&self->mirror, g_ptr_array_unref);

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
  self->mirror = g_ptr_array_new_with_free_func (group_data_unref);
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

  if (self->model != NULL)
    g_signal_handlers_disconnect_by_func (self->model, items_changed, self);
  g_clear_object (&self->model);

  if (self->mirror->len > 0)
    g_ptr_array_remove_range (self->mirror, 0, self->mirror->len);

  if (model != NULL)
    {
      self->model = g_object_ref (model);
      items_changed (self, 0, 0, g_list_model_get_n_items (model), model);
      g_signal_connect_swapped (model, "items-changed", G_CALLBACK (items_changed), self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

DexFuture *
bz_search_engine_query (BzSearchEngine    *self,
                        const char *const *terms)
{
  g_return_val_if_fail (BZ_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (terms != NULL && *terms != NULL, NULL);

  if (self->mirror->len == 0 || **terms == '\0')
    {
      g_autoptr (GPtrArray) ret = NULL;

      ret = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_set_size (ret, self->mirror->len);

      for (guint i = 0; i < ret->len; i++)
        {
          GroupData *data = NULL;

          data = g_ptr_array_index (self->mirror, i);
          /* Set original index here to ensure it is always up to date */
          bz_search_result_set_original_index (data->default_result, i);
          g_ptr_array_index (ret, i) = g_object_ref (data->default_result);
        }

      return dex_future_new_take_boxed (
          G_TYPE_PTR_ARRAY,
          g_steal_pointer (&ret));
    }
  else
    {
      g_autoptr (GPtrArray) shallow_mirror = NULL;
      g_autoptr (QueryTaskData) data       = NULL;

      shallow_mirror = g_ptr_array_new_with_free_func (group_data_unref);
      g_ptr_array_set_size (shallow_mirror, self->mirror->len);

      for (guint i = 0; i < shallow_mirror->len; i++)
        g_ptr_array_index (shallow_mirror, i) =
            group_data_ref (g_ptr_array_index (self->mirror, i));

      data                 = query_task_data_new ();
      data->terms          = g_strdupv ((gchar **) terms);
      data->shallow_mirror = g_steal_pointer (&shallow_mirror);

      return dex_scheduler_spawn (
          dex_thread_pool_scheduler_get_default (),
          bz_get_dex_stack_size (),
          (DexFiberFunc) query_task_fiber,
          query_task_data_ref (data), query_task_data_unref);
    }
}

static void
items_changed (BzSearchEngine *self,
               guint           position,
               guint           removed,
               guint           added,
               GListModel     *model)
{
  if (removed > 0)
    g_ptr_array_remove_range (self->mirror, position, removed);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (BzEntryGroup) group = NULL;
      const char *id                 = NULL;
      const char *title              = NULL;
      const char *developer          = NULL;
      const char *description        = NULL;
      GPtrArray  *search_tokens      = NULL;
      g_autoptr (GroupData) data     = NULL;

      group         = g_list_model_get_item (model, position + i);
      id            = bz_entry_group_get_id (group);
      title         = bz_entry_group_get_title (group);
      developer     = bz_entry_group_get_developer (group);
      description   = bz_entry_group_get_description (group);
      search_tokens = bz_entry_group_get_search_tokens (group);

      data        = group_data_new ();
      data->group = g_object_ref (group);

      data->istrings = g_array_new (FALSE, TRUE, sizeof (IndexedStringData));
      g_array_set_clear_func (data->istrings, indexed_string_data_deinit);

#define ADD_INDEXED_STRING(_s, _weight)            \
  if ((_s) != NULL)                                \
    {                                              \
      IndexedStringData append = { 0 };            \
                                                   \
      index_string ((_s), &append);                \
      append.weight = (_weight);                   \
      g_array_append_val (data->istrings, append); \
    }

      ADD_INDEXED_STRING (id, -1.0);
      ADD_INDEXED_STRING (title, 1.0);
      ADD_INDEXED_STRING (developer, 1.0);
      ADD_INDEXED_STRING (description, -1.0);

#undef ADD_INDEXED_STRING

      if (search_tokens != NULL)
        {
          guint old_len = 0;

          old_len = data->istrings->len;
          g_array_set_size (data->istrings, old_len + search_tokens->len);

          for (guint j = 0; j < search_tokens->len; j++)
            {
              const char        *token   = NULL;
              IndexedStringData *istring = NULL;

              token   = g_ptr_array_index (search_tokens, j);
              istring = &g_array_index (data->istrings, IndexedStringData, old_len + j);

              index_string (token, istring);
              istring->weight = -1.0;
            }
        }

      data->default_result = bz_search_result_new ();
      bz_search_result_set_group (data->default_result, group);

      g_ptr_array_insert (self->mirror, position + i, g_steal_pointer (&data));
    }
}

static DexFuture *
query_task_fiber (QueryTaskData *data)
{
  char           **terms                      = data->terms;
  GPtrArray       *shallow_mirror             = data->shallow_mirror;
  g_autofree char *joined                     = NULL;
  double           threshold                  = 0.0;
  g_autoptr (IndexedStringData) query_istring = NULL;
  g_autoptr (GArray) scores                   = NULL;
  g_autoptr (GPtrArray) results               = NULL;

  joined    = g_strjoinv (" ", terms);
  threshold = (double) strlen (joined) / (double) g_strv_length (terms);

  query_istring = indexed_string_data_new ();
  index_string (joined, query_istring);

  scores = g_array_new (FALSE, FALSE, sizeof (Score));
  for (guint i = 0; i < shallow_mirror->len; i++)
    {
      GroupData *group_data = NULL;
      double     score      = 0.0;

      group_data = g_ptr_array_index (shallow_mirror, i);
      for (guint j = 0; j < group_data->istrings->len; j++)
        {
          IndexedStringData *token_istring = NULL;
          double             token_score   = 0.0;

          token_istring = &g_array_index (group_data->istrings, IndexedStringData, j);
          if (strstr (token_istring->ptr, query_istring->ptr) != NULL)
            token_score = threshold * (1.0 + ((double) query_istring->utf8_len /
                                              (double) token_istring->utf8_len));
          else if (token_istring->weight > 0.0)
            token_score = test_strings (query_istring, token_istring);

          if (token_istring->weight > 0.0)
            token_score *= token_istring->weight;

          score += token_score;
        }

      if (score > threshold)
        {
          Score append = { 0 };

          append.idx = i;
          append.val = score;
          g_array_append_val (scores, append);
        }
    }

  if (scores->len > 0)
    g_array_sort (scores, (GCompareFunc) cmp_scores);

  results = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_set_size (results, scores->len);
  for (guint i = 0; i < scores->len; i++)
    {
      Score     *score                  = NULL;
      GroupData *group_data             = NULL;
      g_autoptr (BzSearchResult) result = NULL;

      score      = &g_array_index (scores, Score, i);
      group_data = g_ptr_array_index (shallow_mirror, score->idx);

      result = bz_search_result_new ();
      bz_search_result_set_group (result, group_data->group);
      bz_search_result_set_original_index (result, score->idx);
      bz_search_result_set_score (result, score->val);

      g_ptr_array_index (results, i) = g_steal_pointer (&result);
    }

  return dex_future_new_take_boxed (
      G_TYPE_PTR_ARRAY,
      g_steal_pointer (&results));
}

static inline void
index_string (const char        *s,
              IndexedStringData *out)
{
  g_autofree char *normalized = NULL;
  g_autofree char *casefolded = NULL;
  guint            i          = 0;

  normalized = g_utf8_normalize (s, -1, G_NORMALIZE_ALL);
  casefolded = g_utf8_casefold (normalized, -1);

  out->ptr      = g_steal_pointer (&casefolded);
  out->utf8_len = g_utf8_strlen (out->ptr, -1);
  out->chars    = g_malloc0_n (out->utf8_len, sizeof (*out->chars));

  for (char *ch = out->ptr; *ch != '\0'; ch = g_utf8_next_char (ch), i++)
    {
      out->chars[i].ch   = g_utf8_get_char (ch);
      out->chars[i].type = g_unichar_type (out->chars[i].ch);
    }
}

static inline double
test_chars (IndexedChar *a,
            IndexedChar *b)
{
  if (a->ch == b->ch)
    return PERFECT;

  // if ((a->type == G_UNICODE_LOWERCASE_LETTER ||
  //      a->type == G_UNICODE_MODIFIER_LETTER ||
  //      a->type == G_UNICODE_OTHER_LETTER ||
  //      a->type == G_UNICODE_TITLECASE_LETTER ||
  //      a->type == G_UNICODE_LOWERCASE_LETTER) &&

  //     (b->type == G_UNICODE_LOWERCASE_LETTER ||
  //      b->type == G_UNICODE_MODIFIER_LETTER ||
  //      b->type == G_UNICODE_OTHER_LETTER ||
  //      b->type == G_UNICODE_TITLECASE_LETTER ||
  //      b->type == G_UNICODE_LOWERCASE_LETTER))
  //   return SAME_CLUSTER;

  // if ((a->type == G_UNICODE_DECIMAL_NUMBER ||
  //      a->type == G_UNICODE_LETTER_NUMBER ||
  //      a->type == G_UNICODE_OTHER_NUMBER) &&

  //     (b->type == G_UNICODE_DECIMAL_NUMBER ||
  //      b->type == G_UNICODE_LETTER_NUMBER ||
  //      b->type == G_UNICODE_OTHER_NUMBER))
  //   return SAME_CLUSTER;

  // if ((a->type == G_UNICODE_CONNECT_PUNCTUATION ||
  //      a->type == G_UNICODE_DASH_PUNCTUATION ||
  //      a->type == G_UNICODE_CLOSE_PUNCTUATION ||
  //      a->type == G_UNICODE_FINAL_PUNCTUATION ||
  //      a->type == G_UNICODE_INITIAL_PUNCTUATION ||
  //      a->type == G_UNICODE_OTHER_PUNCTUATION ||
  //      a->type == G_UNICODE_OPEN_PUNCTUATION) &&

  //     (b->type == G_UNICODE_CONNECT_PUNCTUATION ||
  //      b->type == G_UNICODE_DASH_PUNCTUATION ||
  //      b->type == G_UNICODE_CLOSE_PUNCTUATION ||
  //      b->type == G_UNICODE_FINAL_PUNCTUATION ||
  //      b->type == G_UNICODE_INITIAL_PUNCTUATION ||
  //      b->type == G_UNICODE_OTHER_PUNCTUATION ||
  //      b->type == G_UNICODE_OPEN_PUNCTUATION))
  //   return SAME_CLUSTER;

  return NO_MATCH;
}

static inline double
test_strings (IndexedStringData *query,
              IndexedStringData *against)
{
  guint  last_best_idx = G_MAXUINT;
  guint  misses        = 0;
  double score         = 0.0;

  for (guint i = 0; i < query->utf8_len; i++)
    {
      guint  best_idx   = G_MAXUINT;
      double best_score = 0.0;

      for (guint j = against->utf8_len; j > 1; j--)
        {
          double tmp_score = 0;

          tmp_score = test_chars (&query->chars[i], &against->chars[j - 1]);
          if (tmp_score > NO_MATCH &&
              (tmp_score > best_score ||
               ((last_best_idx == G_MAXUINT ||
                 j - 1 > last_best_idx) &&
                tmp_score >= best_score)))
            {
              best_idx   = j - 1;
              best_score = tmp_score;
            }
        }

      if (best_idx != G_MAXUINT)
        {
          if (last_best_idx != G_MAXUINT)
            {
              int diff = 0;

              diff = (int) best_idx - (int) last_best_idx;
              if (diff > 1)
                /* Penalize the query for fragmentation */
                best_score /= (double) diff;
              else if (diff < 0)
                /* Penalize the query more harshly for
                 * transposing and fragmentation
                 */
                best_score /= 1.5 * (double) ABS (diff);
            }

          score += best_score;
          last_best_idx = best_idx;
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

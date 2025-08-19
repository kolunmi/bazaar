/* bz-search-result.c
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

#include "bz-search-result.h"

struct _BzSearchResult
{
  GObject parent_instance;

  BzEntryGroup *group;
  guint         original_index;
  double        score;
  char         *title_markup;
};

G_DEFINE_FINAL_TYPE (BzSearchResult, bz_search_result, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_GROUP,
  PROP_ORIGINAL_INDEX,
  PROP_SCORE,
  PROP_TITLE_MARKUP,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_search_result_dispose (GObject *object)
{
  BzSearchResult *self = BZ_SEARCH_RESULT (object);

  g_clear_pointer (&self->group, g_object_unref);
  g_clear_pointer (&self->title_markup, g_free);

  G_OBJECT_CLASS (bz_search_result_parent_class)->dispose (object);
}

static void
bz_search_result_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  BzSearchResult *self = BZ_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, bz_search_result_get_group (self));
      break;
    case PROP_ORIGINAL_INDEX:
      g_value_set_uint (value, bz_search_result_get_original_index (self));
      break;
    case PROP_SCORE:
      g_value_set_double (value, bz_search_result_get_score (self));
      break;
    case PROP_TITLE_MARKUP:
      g_value_set_string (value, bz_search_result_get_title_markup (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_result_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  BzSearchResult *self = BZ_SEARCH_RESULT (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      bz_search_result_set_group (self, g_value_get_object (value));
      break;
    case PROP_ORIGINAL_INDEX:
      bz_search_result_set_original_index (self, g_value_get_uint (value));
      break;
    case PROP_SCORE:
      bz_search_result_set_score (self, g_value_get_double (value));
      break;
    case PROP_TITLE_MARKUP:
      bz_search_result_set_title_markup (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_search_result_class_init (BzSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = bz_search_result_set_property;
  object_class->get_property = bz_search_result_get_property;
  object_class->dispose      = bz_search_result_dispose;

  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ORIGINAL_INDEX] =
      g_param_spec_uint (
          "original-index",
          NULL, NULL,
          0, G_MAXUINT, (guint) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SCORE] =
      g_param_spec_double (
          "score",
          NULL, NULL,
          0.0, G_MAXDOUBLE, (double) 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TITLE_MARKUP] =
      g_param_spec_string (
          "title-markup",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_search_result_init (BzSearchResult *self)
{
}

BzSearchResult *
bz_search_result_new (void)
{
  return g_object_new (BZ_TYPE_SEARCH_RESULT, NULL);
}

BzEntryGroup *
bz_search_result_get_group (BzSearchResult *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_RESULT (self), NULL);
  return self->group;
}

guint
bz_search_result_get_original_index (BzSearchResult *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_RESULT (self), 0);
  return self->original_index;
}

double
bz_search_result_get_score (BzSearchResult *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_RESULT (self), 0.0);
  return self->score;
}

const char *
bz_search_result_get_title_markup (BzSearchResult *self)
{
  g_return_val_if_fail (BZ_IS_SEARCH_RESULT (self), NULL);
  return self->title_markup;
}

void
bz_search_result_set_group (BzSearchResult *self,
                            BzEntryGroup   *group)
{
  g_return_if_fail (BZ_IS_SEARCH_RESULT (self));

  g_clear_pointer (&self->group, g_object_unref);
  if (group != NULL)
    self->group = g_object_ref (group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

void
bz_search_result_set_original_index (BzSearchResult *self,
                                     guint           original_index)
{
  g_return_if_fail (BZ_IS_SEARCH_RESULT (self));

  self->original_index = original_index;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ORIGINAL_INDEX]);
}

void
bz_search_result_set_score (BzSearchResult *self,
                            double          score)
{
  g_return_if_fail (BZ_IS_SEARCH_RESULT (self));

  self->score = score;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SCORE]);
}

void
bz_search_result_set_title_markup (BzSearchResult *self,
                                   const char     *title_markup)
{
  g_return_if_fail (BZ_IS_SEARCH_RESULT (self));

  g_clear_pointer (&self->title_markup, g_free);
  if (title_markup != NULL)
    self->title_markup = g_strdup (title_markup);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE_MARKUP]);
}

/* End of bz-search-result.c */

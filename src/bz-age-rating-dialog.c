/* bz-age-rating-dialog.c
 *
 * Copyright 2021 Endless OS Foundation LLC
 * Copyright 2025 Alexander Vanhee
 *
 * Author: Philip Withnall <pwithnall@endlessos.org> (GNOME Software)
 * Adapted for Bazaar by Alexander Vanhee
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

#include <appstream.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "bz-age-rating-attribute.h"
#include "bz-age-rating-dialog.h"
#include "bz-context-row.h"
#include "bz-lozenge.h"

struct _BzAgeRatingDialog
{
  AdwDialog parent_instance;

  BzEntry    *entry;
  BzLozenge  *lozenge;
  GtkListBox *list;
};

G_DEFINE_FINAL_TYPE (BzAgeRatingDialog, bz_age_rating_dialog, ADW_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_ENTRY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL };

typedef enum
{
  BZ_AGE_RATING_GROUP_TYPE_DRUGS,
  BZ_AGE_RATING_GROUP_TYPE_LANGUAGE,
  BZ_AGE_RATING_GROUP_TYPE_MONEY,
  BZ_AGE_RATING_GROUP_TYPE_SEX,
  BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
  BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
} BzAgeRatingGroupType;

#define BZ_AGE_RATING_GROUP_TYPE_COUNT (BZ_AGE_RATING_GROUP_TYPE_VIOLENCE + 1)

typedef struct
{
  GList *attributes;
} BzAgeRatingGroup;

typedef void (*AttributeCallback) (const gchar         *attribute,
                                   AsContentRatingValue value,
                                   gpointer             user_data);

static const struct
{
  const gchar         *id;
  BzAgeRatingGroupType group_type;
  const gchar         *title;
  const gchar         *unknown_description;
  const gchar         *icon_name;
  const gchar         *icon_name_negative;
} attribute_details[] = {
  {        "violence-cartoon", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Cartoon Violence"),
   N_ ("No information regarding cartoon violence"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {        "violence-fantasy", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Fantasy Violence"),
   N_ ("No information regarding fantasy violence"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {      "violence-realistic", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Realistic Violence"),
   N_ ("No information regarding realistic violence"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {      "violence-bloodshed", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Violence Depicting Bloodshed"),
   N_ ("No information regarding bloodshed"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {         "violence-sexual", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Sexual Violence"),
   N_ ("No information regarding sexual violence"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {           "drugs-alcohol",    BZ_AGE_RATING_GROUP_TYPE_DRUGS,
   N_ ("Alcohol"),
   N_ ("No information regarding references to alcohol"),
   "alcohol-use-symbolic",           "alcohol-use-none-symbolic"             },
  {         "drugs-narcotics",    BZ_AGE_RATING_GROUP_TYPE_DRUGS,
   N_ ("Narcotics"),
   N_ ("No information regarding references to illicit drugs"),
   "drug-use-symbolic",              "drug-use-none-symbolic"                },
  {           "drugs-tobacco",    BZ_AGE_RATING_GROUP_TYPE_DRUGS,
   N_ ("Tobacco"),
   N_ ("No information regarding references to tobacco products"),
   "smoking-symbolic",               "smoking-none-symbolic"                 },
  {              "sex-nudity",      BZ_AGE_RATING_GROUP_TYPE_SEX,
   N_ ("Nudity"),
   N_ ("No information regarding nudity of any sort"),
   "nudity-symbolic",                "nudity-none-symbolic"                  },
  {              "sex-themes",      BZ_AGE_RATING_GROUP_TYPE_SEX,
   N_ ("Sexual Themes"),
   N_ ("No information regarding references to or depictions of sexual nature"),
   "nudity-symbolic",                "nudity-none-symbolic"                  },
  {      "language-profanity", BZ_AGE_RATING_GROUP_TYPE_LANGUAGE,
   N_ ("Profanity"),
   N_ ("No information regarding profanity of any kind"),
   "strong-language-symbolic",       "strong-language-none-symbolic"         },
  {          "language-humor", BZ_AGE_RATING_GROUP_TYPE_LANGUAGE,
   N_ ("Inappropriate Humor"),
   N_ ("No information regarding inappropriate humor"),
   "strong-language-symbolic",       "strong-language-none-symbolic"         },
  { "language-discrimination",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Discrimination"),
   N_ ("No information regarding discriminatory language of any kind"),
   "strong-language-symbolic",       "strong-language-none-symbolic"         },
  {       "money-advertising",    BZ_AGE_RATING_GROUP_TYPE_MONEY,
   N_ ("Advertising"),
   N_ ("No information regarding advertising of any kind"),
   "advertising-symbolic",           "advertising-none-symbolic"             },
  {          "money-gambling",    BZ_AGE_RATING_GROUP_TYPE_MONEY,
   N_ ("Gambling"),
   N_ ("No information regarding gambling of any kind"),
   "gambling-symbolic",              "gambling-none-symbolic"                },
  {        "money-purchasing",    BZ_AGE_RATING_GROUP_TYPE_MONEY,
   N_ ("Purchasing"),
   N_ ("No information regarding the ability to spend money"),
   "money-symbolic",                 "money-none-symbolic"                   },
  {             "social-chat",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Chat Between Users"),
   N_ ("No information regarding ways to chat with other users"),
   "messaging-symbolic",             "messaging-none-symbolic"               },
  {            "social-audio",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Audio Chat Between Users"),
   N_ ("No information regarding ways to talk with other users"),
   "audio-chat-symbolic",            "audio-chat-none-symbolic"              },
  {         "social-contacts",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Contact Details"),
   N_ ("No information regarding sharing of social network usernames or email addresses"),
   "contacts-symbolic",                                  NULL                },
  {             "social-info",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Identifying Information"),
   N_ ("No information regarding sharing of user information with third parties"),
   "social-info-symbolic",                                  NULL             },
  {         "social-location",   BZ_AGE_RATING_GROUP_TYPE_SOCIAL,
   N_ ("Location Sharing"),
   N_ ("No information regarding sharing of physical location with other users"),
   "location-services-active-symbolic", "location-services-disabled-symbolic" },
  {        "sex-prostitution",      BZ_AGE_RATING_GROUP_TYPE_SEX,
   N_ ("Prostitution"),
   N_ ("No information regarding references to prostitution"),
   "nudity-symbolic",                "nudity-none-symbolic"                  },
  {            "sex-adultery",      BZ_AGE_RATING_GROUP_TYPE_SEX,
   N_ ("Adultery"),
   N_ ("No information regarding references to adultery"),
   "nudity-symbolic",                "nudity-none-symbolic"                  },
  {          "sex-appearance",      BZ_AGE_RATING_GROUP_TYPE_SEX,
   N_ ("Sexualized Characters"),
   N_ ("No information regarding sexualized characters"),
   "nudity-symbolic",                "nudity-none-symbolic"                  },
  {        "violence-worship", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Desecration"),
   N_ ("No information regarding references to desecration"),
   "violence-symbolic",              "violence-none-symbolic"                },
  {    "violence-desecration", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Human Remains"),
   N_ ("No information regarding visible dead human remains"),
   "human-remains-symbolic",                                  NULL           },
  {        "violence-slavery", BZ_AGE_RATING_GROUP_TYPE_VIOLENCE,
   N_ ("Slavery"),
   N_ ("No information regarding references to slavery"),
   "violence-symbolic",              "violence-none-symbolic"                },
};

static const gchar         *content_rating_attribute_get_icon_name (const gchar *attribute,
                                                                    gboolean     negative_version);
static const gchar         *content_rating_attribute_get_title (const gchar *attribute);
static const gchar         *content_rating_attribute_get_unknown_description (const gchar *attribute);
static BzAgeRatingGroupType content_rating_attribute_get_group_type (const gchar *attribute);
static const gchar         *content_rating_group_get_description (BzAgeRatingGroupType group_type);
static const gchar         *content_rating_group_get_icon_name (BzAgeRatingGroupType group_type,
                                                                gboolean             negative_version);
static const gchar         *content_rating_group_get_title (BzAgeRatingGroupType group_type);
static BzImportance         content_rating_value_get_importance (AsContentRatingValue value);
static gint                 attributes_compare (BzAgeRatingAttribute *attr1,
                                                BzAgeRatingAttribute *attr2);
static void                 collect_attribute (const gchar         *attribute,
                                               AsContentRatingValue value,
                                               gpointer             user_data);
static void                 process_attributes (AsContentRating  *content_rating,
                                                gboolean          show_worst_only,
                                                AttributeCallback callback,
                                                gpointer          user_data);
static gchar               *format_age_short (AsContentRatingSystem system,
                                              guint                 age);
static void                 update_lozenge (BzAgeRatingDialog *self,
                                            AsContentRating   *content_rating);
static void                 update_list (BzAgeRatingDialog *self);

static void
bz_age_rating_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzAgeRatingDialog *self = NULL;

  self = BZ_AGE_RATING_DIALOG (object);

  switch (prop_id)
    {
    case PROP_ENTRY:
      self->entry = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_age_rating_dialog_constructed (GObject *object)
{
  BzAgeRatingDialog *self = NULL;

  self = BZ_AGE_RATING_DIALOG (object);

  G_OBJECT_CLASS (bz_age_rating_dialog_parent_class)->constructed (object);

  if (self->entry != NULL)
    update_list (self);
}

static void
bz_age_rating_dialog_dispose (GObject *object)
{
  BzAgeRatingDialog *self = NULL;

  self = BZ_AGE_RATING_DIALOG (object);

  g_clear_object (&self->entry);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_AGE_RATING_DIALOG);
  G_OBJECT_CLASS (bz_age_rating_dialog_parent_class)->dispose (object);
}

static void
bz_age_rating_dialog_class_init (BzAgeRatingDialogClass *klass)
{
  GObjectClass   *object_class = NULL;
  GtkWidgetClass *widget_class = NULL;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_age_rating_dialog_set_property;
  object_class->constructed  = bz_age_rating_dialog_constructed;
  object_class->dispose      = bz_age_rating_dialog_dispose;

  props[PROP_ENTRY] =
      g_param_spec_object (
          "entry",
          NULL, NULL,
          BZ_TYPE_ENTRY,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_LOZENGE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-age-rating-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzAgeRatingDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzAgeRatingDialog, list);
}

static void
bz_age_rating_dialog_init (BzAgeRatingDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzAgeRatingDialog *
bz_age_rating_dialog_new (BzEntry *entry)
{
  return g_object_new (BZ_TYPE_AGE_RATING_DIALOG,
                       "entry", entry,
                       NULL);
}

static const gchar *
content_rating_attribute_get_icon_name (const gchar *attribute,
                                        gboolean     negative_version)
{
  for (gsize i = 0; i < G_N_ELEMENTS (attribute_details); i++)
    {
      if (g_str_equal (attribute, attribute_details[i].id))
        {
          if (negative_version && attribute_details[i].icon_name_negative != NULL)
            return attribute_details[i].icon_name_negative;
          return attribute_details[i].icon_name;
        }
    }

  g_assert_not_reached ();
}

static const gchar *
content_rating_attribute_get_title (const gchar *attribute)
{
  for (gsize i = 0; i < G_N_ELEMENTS (attribute_details); i++)
    {
      if (g_str_equal (attribute, attribute_details[i].id))
        return _ (attribute_details[i].title);
    }

  g_assert_not_reached ();
}

static const gchar *
content_rating_attribute_get_unknown_description (const gchar *attribute)
{
  for (gsize i = 0; i < G_N_ELEMENTS (attribute_details); i++)
    {
      if (g_str_equal (attribute, attribute_details[i].id))
        return _ (attribute_details[i].unknown_description);
    }

  g_assert_not_reached ();
}

static BzAgeRatingGroupType
content_rating_attribute_get_group_type (const gchar *attribute)
{
  for (gsize i = 0; i < G_N_ELEMENTS (attribute_details); i++)
    {
      if (g_str_equal (attribute, attribute_details[i].id))
        return attribute_details[i].group_type;
    }

  g_assert_not_reached ();
}

static const gchar *
content_rating_group_get_description (BzAgeRatingGroupType group_type)
{
  switch (group_type)
    {
    case BZ_AGE_RATING_GROUP_TYPE_DRUGS:
      return _ ("Does not include references to drugs");
    case BZ_AGE_RATING_GROUP_TYPE_LANGUAGE:
      return _ ("Does not include swearing, profanity, and other kinds of strong language");
    case BZ_AGE_RATING_GROUP_TYPE_MONEY:
      return _ ("Does not include ads or monetary transactions");
    case BZ_AGE_RATING_GROUP_TYPE_SEX:
      return _ ("Does not include sex or nudity");
    case BZ_AGE_RATING_GROUP_TYPE_SOCIAL:
      return _ ("Does not include uncontrolled chat functionality");
    case BZ_AGE_RATING_GROUP_TYPE_VIOLENCE:
      return _ ("Does not include violence");
    default:
      g_assert_not_reached ();
    }
}

static const gchar *
content_rating_group_get_icon_name (BzAgeRatingGroupType group_type,
                                    gboolean             negative_version)
{
  switch (group_type)
    {
    case BZ_AGE_RATING_GROUP_TYPE_DRUGS:
      return negative_version ? "smoking-none-symbolic" : "smoking-symbolic";
    case BZ_AGE_RATING_GROUP_TYPE_LANGUAGE:
      return negative_version ? "strong-language-none-symbolic" : "strong-language-symbolic";
    case BZ_AGE_RATING_GROUP_TYPE_MONEY:
      return negative_version ? "money-none-symbolic" : "money-symbolic";
    case BZ_AGE_RATING_GROUP_TYPE_SEX:
      return negative_version ? "nudity-none-symbolic" : "nudity-symbolic";
    case BZ_AGE_RATING_GROUP_TYPE_SOCIAL:
      return negative_version ? "messaging-none-symbolic" : "messaging-symbolic";
    case BZ_AGE_RATING_GROUP_TYPE_VIOLENCE:
      return negative_version ? "violence-none-symbolic" : "violence-symbolic";
    default:
      g_assert_not_reached ();
    }
}

static const gchar *
content_rating_group_get_title (BzAgeRatingGroupType group_type)
{
  switch (group_type)
    {
    case BZ_AGE_RATING_GROUP_TYPE_DRUGS:
      return _ ("Drugs");
    case BZ_AGE_RATING_GROUP_TYPE_LANGUAGE:
      return _ ("Strong Language");
    case BZ_AGE_RATING_GROUP_TYPE_MONEY:
      return _ ("Money");
    case BZ_AGE_RATING_GROUP_TYPE_SEX:
      return _ ("Nudity");
    case BZ_AGE_RATING_GROUP_TYPE_SOCIAL:
      return _ ("Social");
    case BZ_AGE_RATING_GROUP_TYPE_VIOLENCE:
      return _ ("Violence");
    default:
      g_assert_not_reached ();
    }
}

static BzImportance
content_rating_value_get_importance (AsContentRatingValue value)
{
  switch (value)
    {
    case AS_CONTENT_RATING_VALUE_NONE:
      return BZ_IMPORTANCE_UNIMPORTANT;
    case AS_CONTENT_RATING_VALUE_UNKNOWN:
      return BZ_IMPORTANCE_NEUTRAL;
    case AS_CONTENT_RATING_VALUE_MILD:
      return BZ_IMPORTANCE_INFORMATION;
    case AS_CONTENT_RATING_VALUE_MODERATE:
      return BZ_IMPORTANCE_WARNING;
    case AS_CONTENT_RATING_VALUE_INTENSE:
      return BZ_IMPORTANCE_IMPORTANT;
    case AS_CONTENT_RATING_VALUE_LAST:
    default:
      return BZ_IMPORTANCE_NEUTRAL;
    }
}

static gint
attributes_compare (BzAgeRatingAttribute *attr1,
                    BzAgeRatingAttribute *attr2)
{
  BzImportance importance1 = 0;
  BzImportance importance2 = 0;
  const gchar *id1         = NULL;
  const gchar *id2         = NULL;

  importance1 = bz_age_rating_attribute_get_importance (attr1);
  importance2 = bz_age_rating_attribute_get_importance (attr2);

  if (importance1 != importance2)
    {
      if (importance1 == BZ_IMPORTANCE_NEUTRAL &&
          importance2 == BZ_IMPORTANCE_UNIMPORTANT)
        return -1;
      if (importance1 == BZ_IMPORTANCE_UNIMPORTANT &&
          importance2 == BZ_IMPORTANCE_NEUTRAL)
        return 1;

      return importance2 - importance1;
    }
  else
    {
      id1 = bz_age_rating_attribute_get_id (attr1);
      id2 = bz_age_rating_attribute_get_id (attr2);
      return g_strcmp0 (id1, id2);
    }
}

static void
collect_attribute (const gchar         *attribute,
                   AsContentRatingValue value,
                   gpointer             user_data)
{
  BzAgeRatingGroup     *groups      = NULL;
  BzAgeRatingGroupType  group_type  = 0;
  BzImportance          rating      = 0;
  const gchar          *icon_name   = NULL;
  const gchar          *title       = NULL;
  const gchar          *description = NULL;
  BzAgeRatingAttribute *attr        = NULL;

  groups     = user_data;
  group_type = content_rating_attribute_get_group_type (attribute);
  rating     = content_rating_value_get_importance (value);
  icon_name  = content_rating_attribute_get_icon_name (attribute, value == AS_CONTENT_RATING_VALUE_NONE);
  title      = content_rating_attribute_get_title (attribute);

  if (value == AS_CONTENT_RATING_VALUE_UNKNOWN)
    description = content_rating_attribute_get_unknown_description (attribute);
  else
    description = as_content_rating_attribute_get_description (attribute, value);

  attr                          = g_object_new (BZ_TYPE_AGE_RATING_ATTRIBUTE,
                                                "id", attribute,
                                                "icon-name", icon_name,
                                                "importance", rating,
                                                "title", title,
                                                "description", description,
                                                NULL);
  groups[group_type].attributes = g_list_insert_sorted (groups[group_type].attributes,
                                                        attr,
                                                        (GCompareFunc) attributes_compare);
}

static void
process_attributes (AsContentRating  *content_rating,
                    gboolean          show_worst_only,
                    AttributeCallback callback,
                    gpointer          user_data)
{
  g_autofree const gchar **rating_ids   = NULL;
  AsContentRatingValue     value_bad    = AS_CONTENT_RATING_VALUE_NONE;
  guint                    age_bad      = 0;
  guint                    rating_age   = 0;
  AsContentRatingValue     rating_value = 0;

  const gchar *const violence_group[] = {
    "violence-bloodshed",
    "violence-realistic",
    "violence-fantasy",
    "violence-cartoon",
    NULL
  };
  const gchar *const social_group[] = {
    "social-audio",
    "social-chat",
    "social-contacts",
    "social-info",
    NULL
  };
  const gchar *const coalesce_groups[] = {
    "sex-themes",
    "sex-homosexuality",
    NULL
  };

  rating_ids = as_content_rating_get_all_rating_ids ();

  for (gsize i = 0; rating_ids[i] != NULL; i++)
    {
      rating_value = as_content_rating_get_value (content_rating, rating_ids[i]);
      rating_age   = as_content_rating_attribute_to_csm_age (rating_ids[i], rating_value);

      if (rating_age > age_bad)
        age_bad = rating_age;
      if (rating_value > value_bad)
        value_bad = rating_value;
    }

  if (show_worst_only && (value_bad == AS_CONTENT_RATING_VALUE_NONE || age_bad == 0))
    {
      callback (NULL, AS_CONTENT_RATING_VALUE_UNKNOWN, user_data);
      return;
    }

  for (gsize i = 0; rating_ids[i] != NULL; i++)
    {
      if (g_strv_contains (violence_group, rating_ids[i]) ||
          g_strv_contains (social_group, rating_ids[i]))
        continue;

      rating_value = as_content_rating_get_value (content_rating, rating_ids[i]);
      rating_age   = as_content_rating_attribute_to_csm_age (rating_ids[i], rating_value);

      if (show_worst_only && rating_age < age_bad)
        continue;

      if (g_strv_contains (coalesce_groups + 1, rating_ids[i]) &&
          as_content_rating_attribute_to_csm_age (coalesce_groups[0],
                                                  as_content_rating_get_value (content_rating,
                                                                               coalesce_groups[0])) >= rating_age)
        continue;

      callback (rating_ids[i], rating_value, user_data);
    }

  for (gsize i = 0; violence_group[i] != NULL; i++)
    {
      rating_value = as_content_rating_get_value (content_rating, violence_group[i]);
      rating_age   = as_content_rating_attribute_to_csm_age (violence_group[i], rating_value);

      if (show_worst_only && rating_age < age_bad)
        continue;

      callback (violence_group[i], rating_value, user_data);
    }

  for (gsize i = 0; social_group[i] != NULL; i++)
    {
      rating_value = as_content_rating_get_value (content_rating, social_group[i]);
      rating_age   = as_content_rating_attribute_to_csm_age (social_group[i], rating_value);

      if (show_worst_only && rating_age < age_bad)
        continue;

      callback (social_group[i], rating_value, user_data);
    }
}

static gchar *
format_age_short (AsContentRatingSystem system,
                  guint                 age)
{
  if (age < 3)
    age = 3;

  /* Translators: Age rating format, e.g. "12+" for ages 12 and up */
  return g_strdup_printf (_ ("%d+"), age);
}

static void
update_lozenge (BzAgeRatingDialog *self,
                AsContentRating   *content_rating)
{
  const gchar          *locale     = NULL;
  AsContentRatingSystem system     = 0;
  guint                 age        = G_MAXUINT;
  g_autofree gchar     *age_text   = NULL;
  g_autofree gchar     *title_text = NULL;
  BzImportance          importance = BZ_IMPORTANCE_NEUTRAL;
  gboolean              is_unknown = FALSE;

  locale = setlocale (LC_MESSAGES, NULL);
  system = as_content_rating_system_from_locale (locale);

  if (content_rating != NULL)
    age = as_content_rating_get_minimum_age (content_rating);

  if (age != G_MAXUINT)
    age_text = format_age_short (system, age);

  if (content_rating != NULL && age_text == NULL && age == 0)
    age_text = g_strdup (C_ ("Age rating", "All"));

  if (age_text == NULL ||
      (content_rating != NULL &&
       g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.0") != 0 &&
       g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.1") != 0))
    {
      g_clear_pointer (&age_text, g_free);
      age_text   = g_strdup (_ ("?"));
      importance = BZ_IMPORTANCE_NEUTRAL;
    }
  else
    {
      if (age >= 18)
        importance = BZ_IMPORTANCE_IMPORTANT;
      else if (age >= 15)
        importance = BZ_IMPORTANCE_WARNING;
      else if (age >= 12)
        importance = BZ_IMPORTANCE_INFORMATION;
      else
        importance = BZ_IMPORTANCE_NEUTRAL;
    }

  if (self->entry == NULL)
    {
      title_text = g_strdup (_ ("Age Rating"));
    }
  else
    {
      is_unknown = (content_rating == NULL ||
                    (g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.0") != 0 &&
                     g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.1") != 0) ||
                    age == G_MAXUINT);

      if (is_unknown)
        {
          title_text = g_strdup_printf (_ ("%s has an unknown age rating"),
                                        bz_entry_get_title (self->entry));
        }
      else
        {
          if (age <= 3)
            title_text = g_strdup_printf (_ ("%s is suitable for everyone"),
                                          bz_entry_get_title (self->entry));
          else if (age <= 5)
            title_text = g_strdup_printf (_ ("%s is suitable for young children"),
                                          bz_entry_get_title (self->entry));
          else if (age <= 12)
            title_text = g_strdup_printf (_ ("%s is suitable for children"),
                                          bz_entry_get_title (self->entry));
          else if (age <= 18)
            title_text = g_strdup_printf (_ ("%s is suitable for teenagers"),
                                          bz_entry_get_title (self->entry));
          else if (age < G_MAXUINT)
            title_text = g_strdup_printf (_ ("%s is suitable for adults"),
                                          bz_entry_get_title (self->entry));
          else
            title_text = g_strdup_printf (_ ("%s is suitable for %s"),
                                          bz_entry_get_title (self->entry),
                                          age_text);
        }
    }

  bz_lozenge_set_label (self->lozenge, age_text);
  bz_lozenge_set_title (self->lozenge, title_text);
  bz_lozenge_set_importance (self->lozenge, importance);
}

static void
update_list (BzAgeRatingDialog *self)
{
  AsContentRating      *content_rating                         = NULL;
  BzAgeRatingGroup      groups[BZ_AGE_RATING_GROUP_TYPE_COUNT] = { 0 };
  guint                 attr_count                             = 0;
  BzAgeRatingAttribute *attr                                   = NULL;
  AdwActionRow         *row                                    = NULL;
  BzImportance          max_importance                         = 0;
  BzImportance          attr_importance                        = 0;
  const gchar          *group_icon                             = NULL;
  const gchar          *group_title                            = NULL;
  const gchar          *group_description                      = NULL;
  const gchar          *attr_description                       = NULL;
  g_autofree gchar     *description                            = NULL;
  g_autoptr (GList) l                                          = NULL;
  g_autofree gchar *tmp                                        = NULL;

  content_rating = bz_entry_get_content_rating (self->entry);
  update_lozenge (self, content_rating);

  process_attributes (content_rating, FALSE, collect_attribute, groups);

  for (gsize i = 0; i < BZ_AGE_RATING_GROUP_TYPE_COUNT; i++)
    {
      if (groups[i].attributes == NULL)
        continue;

      attr_count = g_list_length (groups[i].attributes);
      row        = NULL;

      if (attr_count == 1)
        {
          attr = (BzAgeRatingAttribute *) groups[i].attributes->data;
          row  = bz_context_row_new (bz_age_rating_attribute_get_icon_name (attr),
                                     bz_age_rating_attribute_get_importance (attr),
                                     bz_age_rating_attribute_get_title (attr),
                                     bz_age_rating_attribute_get_description (attr));
        }
      else
        {
          max_importance = BZ_IMPORTANCE_UNIMPORTANT;

          for (l = groups[i].attributes; l != NULL; l = l->next)
            {
              attr            = (BzAgeRatingAttribute *) l->data;
              attr_importance = bz_age_rating_attribute_get_importance (attr);
              if (attr_importance > max_importance)
                max_importance = attr_importance;
            }

          if (max_importance == BZ_IMPORTANCE_UNIMPORTANT)
            {
              group_icon        = content_rating_group_get_icon_name (i, TRUE);
              group_title       = content_rating_group_get_title (i);
              group_description = content_rating_group_get_description (i);
              row               = bz_context_row_new (group_icon, BZ_IMPORTANCE_UNIMPORTANT, group_title, group_description);
            }
          else
            {
              group_icon  = content_rating_group_get_icon_name (i, FALSE);
              group_title = content_rating_group_get_title (i);
              g_clear_pointer (&description, g_free);

              for (l = groups[i].attributes; l != NULL; l = l->next)
                {
                  attr            = (BzAgeRatingAttribute *) l->data;
                  attr_importance = bz_age_rating_attribute_get_importance (attr);

                  if (attr_importance == BZ_IMPORTANCE_UNIMPORTANT)
                    continue;

                  attr_description = bz_age_rating_attribute_get_description (attr);

                  if (description == NULL)
                    {
                      description = g_strdup (attr_description);
                    }
                  else
                    {
                      tmp = g_strdup_printf (_ ("%s • %s"), description, attr_description);
                      g_clear_pointer (&description, g_free);
                      description = g_steal_pointer (&tmp);
                    }
                }

              row = bz_context_row_new (group_icon, max_importance, group_title, description);
            }
        }

      gtk_list_box_append (self->list, GTK_WIDGET (row));
      g_list_free_full (g_steal_pointer (&groups[i].attributes), g_object_unref);
    }
}

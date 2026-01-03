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

#include "bz-age-rating-dialog.h"
#include <appstream.h>
#include <glib/gi18n.h>
#include <locale.h>

struct _BzAgeRatingDialog
{
  AdwDialog parent_instance;

  BzEntry *entry;
  gulong   entry_notify_handler_content_rating;
  gulong   entry_notify_handler_title;

  GtkWidget  *lozenge;
  GtkLabel   *title_label;
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

typedef enum
{
  BZ_IMPORTANCE_UNIMPORTANT,
  BZ_IMPORTANCE_NEUTRAL,
  BZ_IMPORTANCE_INFORMATION,
  BZ_IMPORTANCE_WARNING,
  BZ_IMPORTANCE_IMPORTANT,
} BzImportance;

typedef struct
{
  gchar       *id;
  gchar       *icon_name;
  BzImportance importance;
  gchar       *title;
  gchar       *description;
} BzAgeRatingAttribute;

typedef struct
{
  BzAgeRatingDialog   *dialog;
  BzAgeRatingGroupType group_type;
  AdwActionRow        *row;
  GList               *attributes;
} BzAgeRatingGroup;

typedef void (*AttributeCallback) (const gchar         *attribute,
                                   AsContentRatingValue value,
                                   gpointer             user_data);

static BzAgeRatingAttribute *
bz_age_rating_attribute_new (const gchar *id,
                             const gchar *icon_name,
                             BzImportance importance,
                             const gchar *title,
                             const gchar *description)
{
  BzAgeRatingAttribute *attribute;

  g_assert (icon_name != NULL);
  g_assert (title != NULL);
  g_assert (description != NULL);

  attribute              = g_new0 (BzAgeRatingAttribute, 1);
  attribute->id          = g_strdup (id);
  attribute->icon_name   = g_strdup (icon_name);
  attribute->importance  = importance;
  attribute->title       = g_strdup (title);
  attribute->description = g_strdup (description);

  return attribute;
}

static void
bz_age_rating_attribute_free (BzAgeRatingAttribute *attribute)
{
  g_free (attribute->id);
  g_free (attribute->icon_name);
  g_free (attribute->title);
  g_free (attribute->description);
  g_free (attribute);
}

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
  if (attr1->importance != attr2->importance)
    {
      if (attr1->importance == BZ_IMPORTANCE_NEUTRAL &&
          attr2->importance == BZ_IMPORTANCE_UNIMPORTANT)
        return -1;
      if (attr1->importance == BZ_IMPORTANCE_UNIMPORTANT &&
          attr2->importance == BZ_IMPORTANCE_NEUTRAL)
        return 1;

      return attr2->importance - attr1->importance;
    }
  else
    {
      return g_strcmp0 (attr1->id, attr2->id);
    }
}

static AdwActionRow *
create_attribute_row (const gchar *icon_name,
                      BzImportance importance,
                      const gchar *title,
                      const gchar *subtitle)
{
  AdwActionRow *row;
  GtkWidget    *icon;
  const gchar  *css_class;

  row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);

  if (subtitle != NULL)
    adw_action_row_set_subtitle (row, subtitle);

  icon = gtk_image_new_from_icon_name (icon_name);
  gtk_widget_set_valign (icon, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (icon, "circular-lozenge");

  switch (importance)
    {
    case BZ_IMPORTANCE_UNIMPORTANT:
      css_class = "green";
      break;
    case BZ_IMPORTANCE_NEUTRAL:
      css_class = "grey";
      break;
    case BZ_IMPORTANCE_INFORMATION:
      css_class = "grey";
      break;
    case BZ_IMPORTANCE_WARNING:
      css_class = "yellow";
      break;
    case BZ_IMPORTANCE_IMPORTANT:
      css_class = "red";
      break;
    default:
      css_class = "grey";
      break;
    }

  gtk_widget_add_css_class (icon, css_class);
  adw_action_row_add_prefix (row, icon);

  return row;
}

static void
update_attribute_row (BzAgeRatingDialog   *self,
                      BzAgeRatingGroupType group_type,
                      AdwActionRow        *row,
                      GList               *attributes)
{
  const BzAgeRatingAttribute *first;
  const gchar                *group_icon_name;
  const gchar                *group_title;
  const gchar                *group_description;
  g_autofree gchar           *new_description = NULL;
  GtkWidget                  *icon;

  first = (BzAgeRatingAttribute *) attributes->data;

  if (g_list_length (attributes) == 1)
    {
      const gchar *css_classes[] = { "success", "grey", "warning", "error" };

      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), first->title);
      adw_action_row_set_subtitle (row, first->description);

      icon = gtk_widget_get_first_child (GTK_WIDGET (row));
      while (icon != NULL && !GTK_IS_IMAGE (icon))
        icon = gtk_widget_get_next_sibling (icon);

      if (icon != NULL)
        {
          gtk_image_set_from_icon_name (GTK_IMAGE (icon), first->icon_name);

          for (gsize i = 0; i < G_N_ELEMENTS (css_classes); i++)
            gtk_widget_remove_css_class (icon, css_classes[i]);

          switch (first->importance)
            {
            case BZ_IMPORTANCE_UNIMPORTANT:
              gtk_widget_add_css_class (icon, "success");
              break;
            case BZ_IMPORTANCE_NEUTRAL:
              gtk_widget_add_css_class (icon, "grey");
              break;
            case BZ_IMPORTANCE_INFORMATION:
              gtk_widget_add_css_class (icon, "grey");
              break;
            case BZ_IMPORTANCE_WARNING:
              gtk_widget_add_css_class (icon, "warning");
              break;
            case BZ_IMPORTANCE_IMPORTANT:
              gtk_widget_add_css_class (icon, "error");
              break;
            default:
              gtk_widget_add_css_class (icon, "grey");
              break;
            }
        }

      return;
    }

  if (first->importance == BZ_IMPORTANCE_UNIMPORTANT)
    {
      gboolean only_unimportant = TRUE;

      for (GList *l = attributes->next; l; l = l->next)
        {
          BzAgeRatingAttribute *attribute = (BzAgeRatingAttribute *) l->data;

          if (attribute->importance != BZ_IMPORTANCE_UNIMPORTANT)
            {
              only_unimportant = FALSE;
              break;
            }
        }

      if (only_unimportant)
        {
          group_icon_name   = content_rating_group_get_icon_name (group_type, TRUE);
          group_title       = content_rating_group_get_title (group_type);
          group_description = content_rating_group_get_description (group_type);

          adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), group_title);
          adw_action_row_set_subtitle (row, group_description);

          icon = gtk_widget_get_first_child (GTK_WIDGET (row));
          while (icon != NULL && !GTK_IS_IMAGE (icon))
            icon = gtk_widget_get_next_sibling (icon);

          if (icon != NULL)
            gtk_image_set_from_icon_name (GTK_IMAGE (icon), group_icon_name);

          return;
        }
    }

  group_icon_name = content_rating_group_get_icon_name (group_type, FALSE);
  group_title     = content_rating_group_get_title (group_type);
  new_description = g_strdup (first->description);

  for (GList *l = attributes->next; l; l = l->next)
    {
      BzAgeRatingAttribute *attribute = (BzAgeRatingAttribute *) l->data;
      gchar                *s;

      if (attribute->importance == BZ_IMPORTANCE_UNIMPORTANT)
        break;

      s = g_strdup_printf (_ ("%s • %s"),
                           new_description,
                           attribute->description);
      g_free (new_description);
      new_description = s;
    }

  icon = gtk_widget_get_first_child (GTK_WIDGET (row));
  while (icon != NULL && !GTK_IS_IMAGE (icon))
    icon = gtk_widget_get_next_sibling (icon);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), group_title);
  adw_action_row_set_subtitle (row, new_description);

  if (icon != NULL)
    gtk_image_set_from_icon_name (GTK_IMAGE (icon), group_icon_name);
}

static void
add_attribute_row (BzAgeRatingDialog   *self,
                   BzAgeRatingGroup    *groups,
                   const gchar         *attribute,
                   AsContentRatingValue value)
{
  BzAgeRatingGroupType  group_type;
  BzImportance          rating;
  const gchar          *icon_name;
  const gchar          *title;
  const gchar          *description;
  BzAgeRatingAttribute *attr;

  group_type = content_rating_attribute_get_group_type (attribute);
  rating     = content_rating_value_get_importance (value);
  icon_name  = content_rating_attribute_get_icon_name (attribute, value == AS_CONTENT_RATING_VALUE_NONE);
  title      = content_rating_attribute_get_title (attribute);

  if (value == AS_CONTENT_RATING_VALUE_UNKNOWN)
    description = content_rating_attribute_get_unknown_description (attribute);
  else
    description = as_content_rating_attribute_get_description (attribute, value);

  attr = bz_age_rating_attribute_new (attribute, icon_name, rating, title, description);

  if (groups[group_type].attributes != NULL)
    {
      groups[group_type].attributes = g_list_insert_sorted (groups[group_type].attributes,
                                                            attr,
                                                            (GCompareFunc) attributes_compare);

      update_attribute_row (self, group_type, groups[group_type].row, groups[group_type].attributes);
    }
  else
    {
      groups[group_type].attributes = g_list_prepend (groups[group_type].attributes, attr);
      groups[group_type].row        = create_attribute_row (icon_name, rating, title, description);
      gtk_list_box_append (self->list, GTK_WIDGET (groups[group_type].row));
    }
}

typedef struct
{
  BzAgeRatingDialog *dialog;
  BzAgeRatingGroup  *groups;
} AddAttributeData;

static void
add_attribute_rows_cb (const gchar         *attribute,
                       AsContentRatingValue value,
                       gpointer             user_data)
{
  AddAttributeData *data = user_data;

  add_attribute_row (data->dialog, data->groups, attribute, value);
}

static void
process_attributes (AsContentRating  *content_rating,
                    gboolean          show_worst_only,
                    AttributeCallback callback,
                    gpointer          user_data)
{
  g_autofree const gchar **rating_ids = NULL;
  AsContentRatingValue     value_bad  = AS_CONTENT_RATING_VALUE_NONE;
  guint                    age_bad    = 0;

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
      guint                rating_age;
      AsContentRatingValue rating_value;

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
      guint                rating_age;
      AsContentRatingValue rating_value;

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
      guint                rating_age;
      AsContentRatingValue rating_value;

      rating_value = as_content_rating_get_value (content_rating, violence_group[i]);
      rating_age   = as_content_rating_attribute_to_csm_age (violence_group[i], rating_value);

      if (show_worst_only && rating_age < age_bad)
        continue;

      callback (violence_group[i], rating_value, user_data);
    }

  for (gsize i = 0; social_group[i] != NULL; i++)
    {
      guint                rating_age;
      AsContentRatingValue rating_value;

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
  const gchar          *css_class;
  const gchar          *locale;
  AsContentRatingSystem system;
  guint                 age      = G_MAXUINT;
  g_autofree gchar     *age_text = NULL;

  const gchar *css_age_classes[] = {
    "error",
    "warning",
    "dark-blue",
    "grey"
  };

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
      g_free (age_text);
      age_text  = g_strdup (_ ("?"));
      css_class = "grey";
    }
  else
    {
      if (age >= 18)
        css_class = "error";
      else if (age >= 15)
        css_class = "warning";
      else if (age >= 12)
        css_class = "dark-blue";
      else
        css_class = "grey";
    }

  gtk_label_set_text (GTK_LABEL (self->lozenge), age_text);

  for (gsize i = 0; i < G_N_ELEMENTS (css_age_classes); i++)
    gtk_widget_remove_css_class (self->lozenge, css_age_classes[i]);

  gtk_widget_add_css_class (self->lozenge, css_class);
}

static void
update_list (BzAgeRatingDialog *self)
{
  GtkWidget        *child;
  AsContentRating  *content_rating = NULL;
  gboolean          is_unknown;
  g_autofree gchar *title_text                             = NULL;
  BzAgeRatingGroup  groups[BZ_AGE_RATING_GROUP_TYPE_COUNT] = { 0 };
  AddAttributeData  data;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->list))) != NULL)
    gtk_list_box_remove (self->list, child);

  if (self->entry == NULL)
    return;

  content_rating = bz_entry_get_content_rating (self->entry);
  update_lozenge (self, content_rating);

  if (content_rating == NULL ||
      (g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.0") != 0 &&
       g_strcmp0 (as_content_rating_get_kind (content_rating), "oars-1.1") != 0))
    {
      is_unknown = TRUE;
    }
  else
    {
      guint age  = as_content_rating_get_minimum_age (content_rating);
      is_unknown = (age == G_MAXUINT);
    }

  if (is_unknown)
    {
      title_text = g_strdup_printf (_ ("%s has an unknown age rating"),
                                    bz_entry_get_title (self->entry));
    }
  else
    {
      guint age = as_content_rating_get_minimum_age (content_rating);

      if (age == 0)
        title_text = g_strdup_printf (_ ("%s is suitable for everyone"),
                                      bz_entry_get_title (self->entry));
      else if (age <= 3)
        title_text = g_strdup_printf (_ ("%s is suitable for toddlers"),
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
                                      gtk_label_get_text (GTK_LABEL (self->lozenge)));
    }

  gtk_label_set_text (self->title_label, title_text);

  for (gsize i = 0; i < BZ_AGE_RATING_GROUP_TYPE_COUNT; i++)
    {
      groups[i].dialog     = self;
      groups[i].group_type = i;
      groups[i].row        = NULL;
      groups[i].attributes = NULL;
    }

  data.dialog = self;
  data.groups = groups;

  process_attributes (content_rating, FALSE, add_attribute_rows_cb, &data);

  for (gsize i = 0; i < BZ_AGE_RATING_GROUP_TYPE_COUNT; i++)
    {
      if (groups[i].attributes != NULL)
        g_list_free_full (groups[i].attributes, (GDestroyNotify) bz_age_rating_attribute_free);
    }
}

static void
entry_notify_cb (GObject    *obj,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  BzAgeRatingDialog *self = BZ_AGE_RATING_DIALOG (user_data);
  update_list (self);
}

static gint
sort_cb (GtkListBoxRow *row1,
         GtkListBoxRow *row2,
         gpointer       user_data)
{
  const gchar *title1, *title2;

  title1 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row1));
  title2 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row2));

  return g_strcmp0 (title1, title2);
}

static void
bz_age_rating_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  BzAgeRatingDialog *self = BZ_AGE_RATING_DIALOG (object);

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
  BzAgeRatingDialog *self = BZ_AGE_RATING_DIALOG (object);

  G_OBJECT_CLASS (bz_age_rating_dialog_parent_class)->constructed (object);

  if (self->entry != NULL)
    {
      self->entry_notify_handler_content_rating = g_signal_connect (self->entry, "notify::content-rating",
                                                                    G_CALLBACK (entry_notify_cb), self);
      self->entry_notify_handler_title          = g_signal_connect (self->entry, "notify::title",
                                                                    G_CALLBACK (entry_notify_cb), self);
      update_list (self);
    }
}

static void
bz_age_rating_dialog_dispose (GObject *object)
{
  BzAgeRatingDialog *self = BZ_AGE_RATING_DIALOG (object);

  g_clear_signal_handler (&self->entry_notify_handler_content_rating, self->entry);
  g_clear_signal_handler (&self->entry_notify_handler_title, self->entry);
  g_clear_object (&self->entry);

  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_AGE_RATING_DIALOG);

  G_OBJECT_CLASS (bz_age_rating_dialog_parent_class)->dispose (object);
}

static void
bz_age_rating_dialog_class_init (BzAgeRatingDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-age-rating-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, BzAgeRatingDialog, lozenge);
  gtk_widget_class_bind_template_child (widget_class, BzAgeRatingDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, BzAgeRatingDialog, list);
}

static void
bz_age_rating_dialog_init (BzAgeRatingDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->list, sort_cb, NULL, NULL);
}

BzAgeRatingDialog *
bz_age_rating_dialog_new (BzEntry *entry)
{
  return g_object_new (BZ_TYPE_AGE_RATING_DIALOG,
                       "entry", entry,
                       NULL);
}

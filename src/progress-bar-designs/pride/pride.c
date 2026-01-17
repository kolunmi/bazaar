/* progress-bar-designs/pride/pride.c
 *
 * Copyright 2025 Eva M
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

#include "bz-parser.h"
#include "bz-pride-flag-config.h"
#include "bz-yaml-parser.h"
#include "pride.h"

GtkStyleProvider *
bz_get_pride_style_provider (void)
{
  static GtkCssProvider *provider = NULL;

  if (g_once_init_enter_pointer (&provider))
    {
      g_autoptr (GError) local_error       = NULL;
      g_autoptr (GBytes) config_bytes      = NULL;
      g_autoptr (BzYamlParser) parser      = NULL;
      g_autoptr (GHashTable) parse_results = NULL;
      g_autoptr (BzPrideFlagConfig) config = NULL;
      g_autoptr (GtkCssProvider) tmp       = NULL;

      config_bytes = g_resources_lookup_data (
          "/io/github/kolunmi/Bazaar/pride-flags.yaml",
          G_RESOURCE_LOOKUP_FLAGS_NONE,
          NULL);
      g_assert (config_bytes != NULL);

      g_type_ensure (BZ_TYPE_PRIDE_FLAG_CONFIG);
      g_type_ensure (BZ_TYPE_PRIDE_FLAG_SPEC);
      g_type_ensure (BZ_TYPE_PRIDE_FLAG_STRIPE_SPEC);

      parser = bz_yaml_parser_new_for_resource_schema (
          "/io/github/kolunmi/Bazaar/pride-flag-config-schema.xml");
      parse_results = bz_parser_process_bytes (
          BZ_PARSER (parser), config_bytes, &local_error);
      if (parse_results == NULL)
        g_critical ("Could not parse internal 'pride-flags.yaml': %s", local_error->message);
      g_assert (parse_results != NULL);

      config = g_value_dup_object (g_hash_table_lookup (parse_results, "/"));
      tmp    = gtk_css_provider_new ();
      if (config != NULL)
        {
          GListModel *flag_specs = NULL;

          flag_specs = bz_pride_flag_config_get_flag_specs (config);
          if (flag_specs != NULL)
            {
              g_autoptr (GString) css = NULL;
              guint n_flag_specs      = 0;

              css = g_string_new (NULL);

              n_flag_specs = g_list_model_get_n_items (flag_specs);
              for (guint i = 0; i < n_flag_specs; i++)
                {
                  g_autoptr (BzPrideFlagSpec) flag_spec = NULL;
                  const char *id                        = NULL;
                  const char *name                      = NULL;
                  gboolean    homogeneous               = FALSE;
                  GListModel *stripes                   = NULL;
                  const char *direction                 = NULL;
                  g_autoptr (GString) stripe_css        = NULL;
                  guint    n_stripes                    = 0;
                  double   cur_offset                   = 0.0;
                  gboolean skip                         = FALSE;

                  flag_spec = g_list_model_get_item (flag_specs, i);

                  id          = bz_pride_flag_spec_get_id (flag_spec);
                  name        = bz_pride_flag_spec_get_name (flag_spec);
                  homogeneous = bz_pride_flag_spec_get_homogeneous (flag_spec);
                  stripes     = bz_pride_flag_spec_get_stripes (flag_spec);
                  direction   = bz_pride_flag_spec_get_direction (flag_spec);

                  if (id == NULL)
                    {
                      g_critical ("Flag spec with index %d lacks an id, skipping it", i);
                      continue;
                    }
                  if (name == NULL)
                    {
                      g_critical ("Flag spec \"%s\" lacks an name, skipping it", id);
                      continue;
                    }
                  if (stripes == NULL)
                    {
                      g_critical ("Flag spec \"%s\" lacks a strip list, skipping it", id);
                      continue;
                    }

                  if (direction == NULL)
                    direction = "to bottom";

                  stripe_css = g_string_new (NULL);
                  g_string_append_printf (stripe_css, ".%s-theme { ", id);

                  g_string_append_printf (stripe_css, "--flag-gradient: linear-gradient(%s", direction);
                  n_stripes = g_list_model_get_n_items (stripes);
                  for (guint j = 0; j < n_stripes; j++)
                    {
                      g_autoptr (BzPrideFlagStripeSpec) stripe_spec = NULL;
                      const char *rgba_spec                         = NULL;
                      double      size                              = 0.0;
                      GdkRGBA     rgba                              = { 0 };

                      stripe_spec = g_list_model_get_item (stripes, j);

                      rgba_spec = bz_pride_flag_stripe_spec_get_rgba (stripe_spec);
                      size      = bz_pride_flag_stripe_spec_get_size (stripe_spec);

                      if (rgba_spec == NULL)
                        {
                          g_critical ("Flag spec \"%s\" has a stripe spec which lacks an rgba spec, skipping it", id);
                          skip = TRUE;
                          break;
                        }
                      if (!gdk_rgba_parse (&rgba, rgba_spec))
                        {
                          g_critical ("Flag spec \"%s\" has a stripe spec which has an invalid rgba spec, skipping it", id);
                          skip = TRUE;
                          break;
                        }
                      if (!homogeneous &&
                          (size <= 0.0 || size > 1.0))
                        {
                          g_critical ("Flag spec \"%s\" has a stripe spec which has an out of bounds size, skipping it", id);
                          skip = TRUE;
                          break;
                        }

                      g_string_append_printf (stripe_css, ", %s %d%%", rgba_spec, (int) round (cur_offset * 100.0));
                      cur_offset += homogeneous ? 1.0 / (double) n_stripes : size;
                      if (cur_offset > 1.0)
                        {
                          g_critical ("Flag spec \"%s\" has a stripe spec which exceeds the height of the flag, skipping it", id);
                          skip = TRUE;
                          break;
                        }
                      g_string_append_printf (stripe_css, ", %s %d%%", rgba_spec, (int) round (cur_offset * 100.0));
                    }
                  if (skip)
                    continue;
                  g_string_append (stripe_css, "); }\n");

                  g_string_append_len (css, stripe_css->str, stripe_css->len);
                }

              if (css->len > 0)
                gtk_css_provider_load_from_string (tmp, css->str);
            }
        }

      g_once_init_leave_pointer (&provider, g_steal_pointer (&tmp));
    }

  return GTK_STYLE_PROVIDER (provider);
}

char *
bz_dup_css_class_for_pride_id (const char *id)
{
  g_return_val_if_fail (id != NULL, NULL);
  return g_strdup_printf ("%s-theme", id);
}

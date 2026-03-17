/* bz-permissions-page.c
 *
 * Copyright 2026 Alexander Vanhee
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

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "bz-application.h"
#include "bz-permission-entry-row.h"
#include "bz-permission-toggle-row.h"
#include "bz-permissions-page.h"
#include "bz-portal-permissions.h"
#include "bz-state-info.h"
#include "bz-template-callbacks.h"
#include "bz-window.h"

#define REGEX_FILESYSTEM "^(~|/|xdg-[a-z-]+)[^ ]*(:ro|:rw|:create)?$"
#define REGEX_DBUS_NAME  "^[a-zA-Z_][a-zA-Z0-9_]*(\\.[a-zA-Z_][a-zA-Z0-9_]*)+(\\.\\*)?$"
#define REGEX_ENV_VAR    "^[a-zA-Z_][a-zA-Z0-9_]*=.*$"

typedef struct
{
  BzPermissionEntryRow *entry_row;
  const char           *kf_group;
  const char           *policy_type;
} BusPolicyMapping;

struct _BzPermissionsPage
{
  AdwNavigationPage parent_instance;

  BzEntryGroup *entry_group;
  gboolean      is_default;

  GKeyFile *metadata;
  GKeyFile *overrides;
  char     *app_id;
  char     *override_path;

  GPtrArray *rows;

  AdwBanner *banner;

  BzPermissionEntryRow *other_files;
  BusPolicyMapping      bus_entries[4];
  BzPermissionEntryRow *session_bus_talk;
  BzPermissionEntryRow *session_bus_own;
  BzPermissionEntryRow *system_bus_talk;
  BzPermissionEntryRow *system_bus_own;
  BzPermissionEntryRow *environment_vars;
};

G_DEFINE_FINAL_TYPE (BzPermissionsPage, bz_permissions_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  PROP_0,
  PROP_ENTRY_GROUP,
  PROP_IS_DEFAULT,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void
collect_permission_rows (GtkWidget *widget,
                         GPtrArray *rows)
{
  GtkWidget *child = NULL;

  if (BZ_IS_PERMISSION_TOGGLE_ROW (widget))
    {
      g_ptr_array_add (rows, widget);
      return;
    }

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    collect_permission_rows (child, rows);
}

static char *
get_override_path (const char *app_id)
{
  return g_build_filename (g_get_user_data_dir (),
                           "flatpak", "overrides",
                           app_id, NULL);
}

static GKeyFile *
load_key_file_from_path (const char *path)
{
  GKeyFile *kf = NULL;
  kf           = g_key_file_new ();

  if (path != NULL && g_file_test (path, G_FILE_TEST_EXISTS))
    g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL);

  return kf;
}

static GKeyFile *
load_metadata_for_app (const char *app_id)
{
  g_autofree char *path = NULL;
  GKeyFile        *kf   = NULL;
  kf                    = g_key_file_new ();

  path = g_build_filename (g_get_user_data_dir (),
                           "flatpak", "app", app_id,
                           "current", "active", "metadata",
                           NULL);

  if (!g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL))
    {
      g_clear_pointer (&path, g_free);
      path = g_build_filename ("/var/lib/flatpak/app",
                               app_id, "current", "active",
                               "metadata", NULL);
      g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL);
    }

  return kf;
}

static void
parse_permission_key (const char  *row_key,
                      char       **out_kf_key,
                      const char **out_value)
{
  const char *eq = NULL;
  eq             = strchr (row_key, '=');

  if (eq != NULL)
    {
      *out_kf_key = g_strndup (row_key, eq - row_key);
      *out_value  = eq + 1;
    }
  else
    {
      *out_kf_key = g_strdup (row_key);
      *out_value  = row_key;
    }
}

static gint
ptr_array_find_str (GPtrArray  *arr,
                    const char *needle)
{
  for (guint i = 0; i < arr->len; i++)
    {
      if (g_strcmp0 (g_ptr_array_index (arr, i), needle) == 0)
        return (gint) i;
    }

  return -1;
}

static gboolean
metadata_has_value (GKeyFile   *metadata,
                    const char *kf_group,
                    const char *kf_key,
                    const char *value)
{
  g_auto (GStrv) values = NULL;

  values = g_key_file_get_string_list (metadata, kf_group, kf_key, NULL, NULL);
  return values != NULL && g_strv_contains ((const char *const *) values, value);
}

static gboolean
override_has_value (GKeyFile   *overrides,
                    const char *kf_group,
                    const char *kf_key,
                    const char *value,
                    gboolean   *out_negated)
{
  g_auto (GStrv) values    = NULL;
  g_autofree char *negated = NULL;

  values = g_key_file_get_string_list (overrides, kf_group, kf_key, NULL, NULL);
  if (values == NULL)
    return FALSE;

  negated = g_strdup_printf ("!%s", value);

  for (guint i = 0; values[i] != NULL; i++)
    {
      if (g_strcmp0 (values[i], value) == 0)
        {
          if (out_negated != NULL)
            *out_negated = FALSE;
          return TRUE;
        }

      if (g_strcmp0 (values[i], negated) == 0)
        {
          if (out_negated != NULL)
            *out_negated = TRUE;
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
is_well_known_filesystem (const char *value)
{
  static const char *known[] = { "home", "host", "xdg-download", NULL };
  const char        *suffix  = NULL;

  for (guint i = 0; known[i] != NULL; i++)
    {
      if (!g_str_has_prefix (value, known[i]))
        continue;

      suffix = value + strlen (known[i]);

      if (suffix[0] == '\0' ||
          g_strcmp0 (suffix, ":ro") == 0 ||
          g_strcmp0 (suffix, ":rw") == 0 ||
          g_strcmp0 (suffix, ":create") == 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
env_entries_contain_key (const char *const *entries,
                         const char        *key)
{
  if (entries == NULL)
    return FALSE;

  for (guint i = 0; entries[i] != NULL; i++)
    {
      g_autofree char *entry_key = NULL;
      const char      *eq        = NULL;
      eq                         = strchr (entries[i], '=');

      if (eq != NULL)
        entry_key = g_strndup (entries[i], eq - entries[i]);
      else
        entry_key = g_strdup (entries[i]);

      if (g_strcmp0 (entry_key, key) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
load_row_state (BzPermissionsPage     *self,
                BzPermissionToggleRow *row)
{
  const char      *group      = NULL;
  const char      *row_key    = NULL;
  g_autofree char *kf_key     = NULL;
  const char      *value      = NULL;
  gboolean         base_value = FALSE;
  gboolean         effective  = FALSE;
  gboolean         negated    = FALSE;

  group   = bz_permission_toggle_row_get_group (row);
  row_key = bz_permission_toggle_row_get_key (row);

  parse_permission_key (row_key, &kf_key, &value);

  if (self->metadata != NULL)
    base_value = metadata_has_value (self->metadata, group, kf_key, value);

  effective = base_value;

  if (override_has_value (self->overrides, group, kf_key, value, &negated))
    effective = !negated;

  bz_permission_toggle_row_set_default_value (row, base_value);
  bz_permission_toggle_row_set_active (row, effective);
}

static void
load_filesystem_state (BzPermissionsPage *self)
{
  g_auto (GStrv) meta_values          = NULL;
  g_auto (GStrv) ovr_values           = NULL;
  g_autoptr (GPtrArray) paths         = NULL;
  g_autoptr (GPtrArray) default_paths = NULL;
  gint idx                            = -1;

  meta_values   = g_key_file_get_string_list (self->metadata, "Context",
                                              "filesystems", NULL, NULL);
  ovr_values    = g_key_file_get_string_list (self->overrides, "Context",
                                              "filesystems", NULL, NULL);
  paths         = g_ptr_array_new_with_free_func (g_free);
  default_paths = g_ptr_array_new_with_free_func (g_free);

  if (meta_values != NULL)
    {
      for (guint i = 0; meta_values[i] != NULL; i++)
        {
          if (!is_well_known_filesystem (meta_values[i]))
            {
              g_ptr_array_add (paths, g_strdup (meta_values[i]));
              g_ptr_array_add (default_paths, g_strdup (meta_values[i]));
            }
        }
    }

  if (ovr_values != NULL)
    {
      for (guint i = 0; ovr_values[i] != NULL; i++)
        {
          const char *val = ovr_values[i];

          if (val[0] == '!')
            {
              idx = ptr_array_find_str (paths, val + 1);
              if (idx >= 0)
                g_ptr_array_remove_index (paths, idx);
            }
          else if (!is_well_known_filesystem (val) &&
                   ptr_array_find_str (paths, val) < 0)
            {
              g_ptr_array_add (paths, g_strdup (val));
            }
        }
    }

  g_ptr_array_add (default_paths, NULL);
  bz_permission_entry_row_set_default_values (self->other_files,
                                              (const char *const *) default_paths->pdata);

  g_ptr_array_add (paths, NULL);
  bz_permission_entry_row_set_values (self->other_files,
                                      (const char *const *) paths->pdata);
}

static void
load_bus_policy_state (BzPermissionsPage    *self,
                       BzPermissionEntryRow *entry_row,
                       const char           *kf_group,
                       const char           *policy_type)
{
  g_auto (GStrv) meta_keys            = NULL;
  g_auto (GStrv) ovr_keys             = NULL;
  g_autoptr (GPtrArray) names         = NULL;
  g_autoptr (GPtrArray) default_names = NULL;
  gint idx                            = -1;

  meta_keys     = g_key_file_get_keys (self->metadata, kf_group, NULL, NULL);
  ovr_keys      = g_key_file_get_keys (self->overrides, kf_group, NULL, NULL);
  names         = g_ptr_array_new_with_free_func (g_free);
  default_names = g_ptr_array_new_with_free_func (g_free);

  if (meta_keys != NULL)
    {
      for (guint i = 0; meta_keys[i] != NULL; i++)
        {
          g_autofree char *val = NULL;

          val = g_key_file_get_string (self->metadata, kf_group,
                                       meta_keys[i], NULL);
          if (g_strcmp0 (val, policy_type) == 0)
            {
              g_ptr_array_add (names, g_strdup (meta_keys[i]));
              g_ptr_array_add (default_names, g_strdup (meta_keys[i]));
            }
        }
    }

  if (ovr_keys != NULL)
    {
      for (guint i = 0; ovr_keys[i] != NULL; i++)
        {
          g_autofree char *val = NULL;

          val = g_key_file_get_string (self->overrides, kf_group,
                                       ovr_keys[i], NULL);

          if (g_strcmp0 (val, policy_type) == 0)
            {
              if (ptr_array_find_str (names, ovr_keys[i]) < 0)
                g_ptr_array_add (names, g_strdup (ovr_keys[i]));
            }
          else if (g_strcmp0 (val, "none") == 0)
            {
              idx = ptr_array_find_str (names, ovr_keys[i]);
              if (idx >= 0)
                g_ptr_array_remove_index (names, idx);
            }
        }
    }

  g_ptr_array_add (default_names, NULL);
  bz_permission_entry_row_set_default_values (entry_row,
                                              (const char *const *) default_names->pdata);

  g_ptr_array_add (names, NULL);
  bz_permission_entry_row_set_values (entry_row,
                                      (const char *const *) names->pdata);
}

static void
load_environment_state (BzPermissionsPage *self)
{
  g_auto (GStrv) meta_keys              = NULL;
  g_auto (GStrv) ovr_keys               = NULL;
  g_autoptr (GHashTable) env_map        = NULL;
  g_autoptr (GPtrArray) entries         = NULL;
  g_autoptr (GPtrArray) default_entries = NULL;
  GHashTableIter iter                   = { 0 };
  gpointer       ht_key                 = NULL;
  gpointer       ht_val                 = NULL;

  meta_keys       = g_key_file_get_keys (self->metadata, "Environment", NULL, NULL);
  ovr_keys        = g_key_file_get_keys (self->overrides, "Environment", NULL, NULL);
  env_map         = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  default_entries = g_ptr_array_new_with_free_func (g_free);

  if (meta_keys != NULL)
    {
      for (guint i = 0; meta_keys[i] != NULL; i++)
        {
          g_autofree char *val = NULL;

          val = g_key_file_get_string (self->metadata, "Environment",
                                       meta_keys[i], NULL);
          if (val != NULL)
            {
              g_hash_table_insert (env_map, g_strdup (meta_keys[i]),
                                   g_steal_pointer (&val));
              g_ptr_array_add (default_entries,
                               g_strdup_printf ("%s=%s",
                                                meta_keys[i],
                                                (const char *) g_hash_table_lookup (env_map, meta_keys[i])));
            }
        }
    }

  if (ovr_keys != NULL)
    {
      for (guint i = 0; ovr_keys[i] != NULL; i++)
        {
          g_autofree char *val = NULL;

          val = g_key_file_get_string (self->overrides, "Environment",
                                       ovr_keys[i], NULL);
          if (val != NULL && val[0] != '\0')
            g_hash_table_insert (env_map, g_strdup (ovr_keys[i]),
                                 g_steal_pointer (&val));
          else
            g_hash_table_remove (env_map, ovr_keys[i]);
        }
    }

  entries = g_ptr_array_new_with_free_func (g_free);

  g_hash_table_iter_init (&iter, env_map);
  while (g_hash_table_iter_next (&iter, &ht_key, &ht_val))
    g_ptr_array_add (entries,
                     g_strdup_printf ("%s=%s",
                                      (const char *) ht_key,
                                      (const char *) ht_val));

  g_ptr_array_add (default_entries, NULL);
  bz_permission_entry_row_set_default_values (self->environment_vars,
                                              (const char *const *) default_entries->pdata);

  g_ptr_array_add (entries, NULL);
  bz_permission_entry_row_set_values (self->environment_vars,
                                      (const char *const *) entries->pdata);
}

static void
load_all_entry_row_states (BzPermissionsPage *self)
{
  load_filesystem_state (self);

  for (guint b = 0; b < G_N_ELEMENTS (self->bus_entries); b++)
    load_bus_policy_state (self, self->bus_entries[b].entry_row,
                           self->bus_entries[b].kf_group,
                           self->bus_entries[b].policy_type);

  load_environment_state (self);
}

static void
update_is_default (BzPermissionsPage *self)
{
  self->is_default = !g_file_test (self->override_path, G_FILE_TEST_EXISTS);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_DEFAULT]);
}

static void
save_overrides (BzPermissionsPage *self)
{
  g_autoptr (GKeyFile) kf             = NULL;
  g_autoptr (GHashTable) group_values = NULL;
  g_autoptr (GError) error            = NULL;
  g_autofree char *dir                = NULL;
  GHashTableIter   iter               = { 0 };
  gpointer         ht_key             = NULL;
  gpointer         ht_val             = NULL;

  kf           = g_key_file_new ();
  group_values = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free, (GDestroyNotify) g_ptr_array_unref);

  for (guint i = 0; i < self->rows->len; i++)
    {
      BzPermissionToggleRow *row      = NULL;
      const char            *group    = NULL;
      const char            *row_key  = NULL;
      gboolean               active   = FALSE;
      g_autofree char       *kf_key   = NULL;
      const char            *value    = NULL;
      g_autofree char       *dict_key = NULL;
      GPtrArray             *arr      = NULL;

      row = g_ptr_array_index (self->rows, i);

      if (!bz_permission_toggle_row_get_is_modified (row))
        continue;

      group   = bz_permission_toggle_row_get_group (row);
      row_key = bz_permission_toggle_row_get_key (row);
      active  = bz_permission_toggle_row_get_active (row);

      parse_permission_key (row_key, &kf_key, &value);

      dict_key = g_strdup_printf ("%s/%s", group, kf_key);
      arr      = g_hash_table_lookup (group_values, dict_key);

      if (arr == NULL)
        {
          arr = g_ptr_array_new_with_free_func (g_free);
          g_hash_table_insert (group_values, g_steal_pointer (&dict_key), arr);
        }

      g_ptr_array_add (arr, active
                                ? g_strdup (value)
                                : g_strdup_printf ("!%s", value));
    }

  {
    g_auto (GStrv) fs_values        = NULL;
    g_auto (GStrv) meta_filesystems = NULL;
    const char *dict_key_str        = "Context/filesystems";
    GPtrArray  *fs_arr              = NULL;

    fs_values        = bz_permission_entry_row_get_values (self->other_files);
    meta_filesystems = g_key_file_get_string_list (self->metadata, "Context",
                                                   "filesystems", NULL, NULL);
    fs_arr           = g_hash_table_lookup (group_values, dict_key_str);

    if (fs_arr == NULL)
      {
        fs_arr = g_ptr_array_new_with_free_func (g_free);
        g_hash_table_insert (group_values, g_strdup (dict_key_str), fs_arr);
      }

    if (fs_values != NULL)
      {
        for (guint i = 0; fs_values[i] != NULL; i++)
          {
            if (fs_values[i][0] == '\0')
              continue;

            if (!is_well_known_filesystem (fs_values[i]) &&
                metadata_has_value (self->metadata, "Context",
                                    "filesystems", fs_values[i]))
              continue;

            g_ptr_array_add (fs_arr, g_strdup (fs_values[i]));
          }
      }

    if (meta_filesystems != NULL)
      {
        for (guint i = 0; meta_filesystems[i] != NULL; i++)
          {
            if (is_well_known_filesystem (meta_filesystems[i]))
              continue;

            if (fs_values == NULL ||
                !g_strv_contains ((const char *const *) fs_values, meta_filesystems[i]))
              g_ptr_array_add (fs_arr,
                               g_strdup_printf ("!%s", meta_filesystems[i]));
          }
      }
  }

  g_hash_table_iter_init (&iter, group_values);
  while (g_hash_table_iter_next (&iter, &ht_key, &ht_val))
    {
      const char      *dict_key   = ht_key;
      GPtrArray       *arr        = ht_val;
      const char      *slash      = NULL;
      g_autofree char *group_name = NULL;

      slash = strchr (dict_key, '/');
      if (slash == NULL || arr->len == 0)
        continue;

      group_name = g_strndup (dict_key, slash - dict_key);

      g_ptr_array_add (arr, NULL);
      g_key_file_set_string_list (kf, group_name, slash + 1,
                                  (const char *const *) arr->pdata,
                                  arr->len - 1);
    }

  {
    for (guint b = 0; b < G_N_ELEMENTS (self->bus_entries); b++)
      {
        g_auto (GStrv) current_names = NULL;
        g_auto (GStrv) meta_keys     = NULL;

        current_names = bz_permission_entry_row_get_values (self->bus_entries[b].entry_row);
        meta_keys     = g_key_file_get_keys (self->metadata,
                                             self->bus_entries[b].kf_group,
                                             NULL, NULL);

        if (current_names != NULL)
          {
            for (guint i = 0; current_names[i] != NULL; i++)
              {
                g_autofree char *meta_val = NULL;

                if (current_names[i][0] == '\0')
                  continue;

                meta_val = g_key_file_get_string (self->metadata,
                                                  self->bus_entries[b].kf_group,
                                                  current_names[i], NULL);
                if (g_strcmp0 (meta_val, self->bus_entries[b].policy_type) == 0)
                  continue;

                g_key_file_set_string (kf, self->bus_entries[b].kf_group,
                                       current_names[i],
                                       self->bus_entries[b].policy_type);
              }
          }

        if (meta_keys != NULL)
          {
            for (guint i = 0; meta_keys[i] != NULL; i++)
              {
                g_autofree char *meta_val = NULL;

                meta_val = g_key_file_get_string (self->metadata,
                                                  self->bus_entries[b].kf_group,
                                                  meta_keys[i], NULL);
                if (g_strcmp0 (meta_val, self->bus_entries[b].policy_type) != 0)
                  continue;

                if (current_names == NULL ||
                    !g_strv_contains ((const char *const *) current_names, meta_keys[i]))
                  g_key_file_set_string (kf, self->bus_entries[b].kf_group,
                                         meta_keys[i], "none");
              }
          }
      }
  }

  {
    g_auto (GStrv) current_entries = NULL;
    g_auto (GStrv) meta_keys       = NULL;

    current_entries = bz_permission_entry_row_get_values (self->environment_vars);
    meta_keys       = g_key_file_get_keys (self->metadata, "Environment",
                                           NULL, NULL);

    if (current_entries != NULL)
      {
        for (guint i = 0; current_entries[i] != NULL; i++)
          {
            const char      *eq       = NULL;
            g_autofree char *env_key  = NULL;
            g_autofree char *meta_val = NULL;

            if (current_entries[i][0] == '\0')
              continue;

            eq = strchr (current_entries[i], '=');
            if (eq != NULL)
              {
                env_key  = g_strndup (current_entries[i], eq - current_entries[i]);
                meta_val = g_key_file_get_string (self->metadata, "Environment",
                                                  env_key, NULL);
                if (g_strcmp0 (meta_val, eq + 1) != 0)
                  g_key_file_set_string (kf, "Environment", env_key, eq + 1);
              }
            else
              {
                meta_val = g_key_file_get_string (self->metadata, "Environment",
                                                  current_entries[i], NULL);
                if (meta_val != NULL)
                  g_key_file_set_string (kf, "Environment",
                                         current_entries[i], "");
              }
          }
      }

    if (meta_keys != NULL)
      {
        for (guint i = 0; meta_keys[i] != NULL; i++)
          {
            if (!env_entries_contain_key ((const char *const *) current_entries,
                                          meta_keys[i]))
              g_key_file_set_string (kf, "Environment", meta_keys[i], "");
          }
      }
  }

  dir = g_path_get_dirname (self->override_path);
  g_mkdir_with_parents (dir, 0755);

  if (!g_key_file_save_to_file (kf, self->override_path, &error))
    g_warning ("Failed to save overrides: %s", error->message);

  update_is_default (self);
}

static void
row_changed_cb (BzPermissionToggleRow *row,
                BzPermissionsPage     *self)
{
  save_overrides (self);
}

static void
entry_row_changed_cb (BzPermissionEntryRow *entry_row,
                      BzPermissionsPage    *self)
{
  save_overrides (self);
}

static void
reset_button_clicked_cb (GtkButton         *button,
                         BzPermissionsPage *self)
{
  BzWindow *window = NULL;
  AdwToast *toast = NULL;

  if (self->override_path != NULL)
    g_remove (self->override_path);

  g_clear_pointer (&self->overrides, g_key_file_unref);
  self->overrides = g_key_file_new ();

  for (guint i = 0; i < self->rows->len; i++)
    load_row_state (self, g_ptr_array_index (self->rows, i));

  load_all_entry_row_states (self);
  update_is_default (self);

  window = BZ_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  toast = adw_toast_new (_("Permissions reset!"));
  bz_window_add_toast (window, toast);
}

static void
banner_clicked_cb (AdwBanner         *banner,
                   BzPermissionsPage *self)
{
  g_settings_set_boolean (bz_state_info_get_settings (bz_state_info_get_default ()),
                          "show-permission-banner", FALSE);
}

static void
run_cb (GtkButton         *button,
        BzPermissionsPage *self)
{
  if (self->entry_group == NULL)
    return;

  gtk_widget_activate_action (GTK_WIDGET (self), "window.launch-group", "s",
                              bz_entry_group_get_id (self->entry_group));
}

static char *
get_show_more_label (gpointer object,
                     gboolean active)
{
  return g_strdup (active ? _ ("Hide Developer Options") : _ ("Show Developer Options"));
}

static void
bz_permissions_page_dispose (GObject *object)
{
  BzPermissionsPage *self = BZ_PERMISSIONS_PAGE (object);

  g_clear_object (&self->entry_group);
  g_clear_pointer (&self->metadata, g_key_file_unref);
  g_clear_pointer (&self->overrides, g_key_file_unref);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_pointer (&self->override_path, g_free);
  g_clear_pointer (&self->rows, g_ptr_array_unref);
  gtk_widget_dispose_template (GTK_WIDGET (self), BZ_TYPE_PERMISSIONS_PAGE);

  G_OBJECT_CLASS (bz_permissions_page_parent_class)->dispose (object);
}

static void
bz_permissions_page_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzPermissionsPage *self = BZ_PERMISSIONS_PAGE (object);

  switch (prop_id)
    {
    case PROP_ENTRY_GROUP:
      g_value_set_object (value, self->entry_group);
      break;
    case PROP_IS_DEFAULT:
      g_value_set_boolean (value, self->is_default);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void populate (BzPermissionsPage *self);

static void
bz_permissions_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzPermissionsPage *self = BZ_PERMISSIONS_PAGE (object);

  switch (prop_id)
    {
    case PROP_ENTRY_GROUP:
      g_set_object (&self->entry_group, g_value_get_object (value));
      populate (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_permissions_page_class_init (BzPermissionsPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_permissions_page_dispose;
  object_class->get_property = bz_permissions_page_get_property;
  object_class->set_property = bz_permissions_page_set_property;

  props[PROP_ENTRY_GROUP] =
      g_param_spec_object ("entry-group", NULL, NULL,
                           BZ_TYPE_ENTRY_GROUP,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_IS_DEFAULT] =
      g_param_spec_boolean ("is-default", NULL, NULL,
                            TRUE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_PERMISSION_TOGGLE_ROW);
  g_type_ensure (BZ_TYPE_PERMISSION_ENTRY_ROW);
  g_type_ensure (BZ_TYPE_PORTAL_PERMISSIONS);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-permissions-page.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, other_files);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, session_bus_talk);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, session_bus_own);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, system_bus_talk);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, system_bus_own);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, environment_vars);
  gtk_widget_class_bind_template_child (widget_class, BzPermissionsPage, banner);

  gtk_widget_class_bind_template_callback (widget_class, reset_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, banner_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, get_show_more_label);
  gtk_widget_class_bind_template_callback (widget_class, run_cb);
}

static void
bz_permissions_page_init (BzPermissionsPage *self)
{
  self->rows = g_ptr_array_new ();

  gtk_widget_init_template (GTK_WIDGET (self));
  collect_permission_rows (GTK_WIDGET (self), self->rows);
}

GtkWidget *
bz_permissions_page_new (BzEntryGroup *entry_group)
{
  return g_object_new (BZ_TYPE_PERMISSIONS_PAGE,
                       "entry-group", entry_group,
                       NULL);
}

static void
populate (BzPermissionsPage *self)
{
  self->bus_entries[0] = (BusPolicyMapping) { self->session_bus_talk, "Session Bus Policy", "talk" };
  self->bus_entries[1] = (BusPolicyMapping) { self->session_bus_own, "Session Bus Policy", "own" };
  self->bus_entries[2] = (BusPolicyMapping) { self->system_bus_talk, "System Bus Policy", "talk" };
  self->bus_entries[3] = (BusPolicyMapping) { self->system_bus_own, "System Bus Policy", "own" };

  if (self->entry_group == NULL)
    return;

  bz_permission_entry_row_set_regex (self->other_files, REGEX_FILESYSTEM);
  bz_permission_entry_row_set_regex (self->environment_vars, REGEX_ENV_VAR);

  for (guint b = 0; b < G_N_ELEMENTS (self->bus_entries); b++)
    bz_permission_entry_row_set_regex (self->bus_entries[b].entry_row, REGEX_DBUS_NAME);

  self->app_id        = g_strdup (bz_entry_group_get_id (self->entry_group));
  self->override_path = get_override_path (self->app_id);
  self->metadata      = load_metadata_for_app (self->app_id);
  self->overrides     = load_key_file_from_path (self->override_path);

  for (guint i = 0; i < self->rows->len; i++)
    {
      BzPermissionToggleRow *row = g_ptr_array_index (self->rows, i);

      load_row_state (self, row);
      g_signal_connect (row, "changed",
                        G_CALLBACK (row_changed_cb), self);
    }

  load_all_entry_row_states (self);

  g_signal_connect (self->other_files, "changed",
                    G_CALLBACK (entry_row_changed_cb), self);
  for (guint b = 0; b < G_N_ELEMENTS (self->bus_entries); b++)
    g_signal_connect (self->bus_entries[b].entry_row, "changed",
                      G_CALLBACK (entry_row_changed_cb), self);
  g_signal_connect (self->environment_vars, "changed",
                    G_CALLBACK (entry_row_changed_cb), self);

  update_is_default (self);

  g_settings_bind (bz_state_info_get_settings (bz_state_info_get_default ()),
                   "show-permission-banner",
                   self->banner, "revealed",
                   G_SETTINGS_BIND_DEFAULT);
}

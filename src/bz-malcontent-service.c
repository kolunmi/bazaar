/* bz-malcontent-service.c
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

#include <appstream.h>
#include <libmalcontent/malcontent.h>
#include <unistd.h>

#include "bz-malcontent-service.h"

struct _BzMalcontentService
{
  GObject          parent_instance;
  MctManager      *manager;
  BzStateInfo     *state;
  GDBusConnection *bus;
  MctAppFilter    *filter;
  gulong           changed_id;
};

G_DEFINE_FINAL_TYPE (BzMalcontentService, bz_malcontent_service, G_TYPE_OBJECT)

static void fetch_blocked_ids (BzMalcontentService *self);
static void apply_filter (BzMalcontentService *self,
                          MctAppFilter        *filter);
static void on_filter_loaded (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data);
static void on_filter_changed (MctManager *manager,
                               guint64     user_id,
                               gpointer    user_data);

static void
bz_malcontent_service_dispose (GObject *object)
{
  BzMalcontentService *self = NULL;

  self = BZ_MALCONTENT_SERVICE (object);

  g_signal_handler_disconnect (self->manager, self->changed_id);
  g_clear_object (&self->manager);
  g_clear_object (&self->state);
  g_clear_object (&self->bus);
  g_clear_pointer (&self->filter, mct_app_filter_unref);

  G_OBJECT_CLASS (bz_malcontent_service_parent_class)->dispose (object);
}

static void
bz_malcontent_service_class_init (BzMalcontentServiceClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = bz_malcontent_service_dispose;
}

static void
bz_malcontent_service_init (BzMalcontentService *self)
{
}

BzMalcontentService *
bz_malcontent_service_new (GDBusConnection *bus,
                           BzStateInfo     *state)
{
  BzMalcontentService *self = NULL;

  self             = g_object_new (BZ_TYPE_MALCONTENT_SERVICE, NULL);
  self->state      = g_object_ref (state);
  self->bus        = g_object_ref (bus);
  self->manager    = mct_manager_new (bus);
  self->changed_id = g_signal_connect (self->manager,
                                       "app-filter-changed",
                                       G_CALLBACK (on_filter_changed),
                                       self);

  return self;
}

void
bz_malcontent_service_start (BzMalcontentService *self)
{
  mct_manager_get_app_filter_async (self->manager,
                                    getuid (),
                                    MCT_MANAGER_GET_VALUE_FLAGS_INTERACTIVE,
                                    NULL,
                                    on_filter_loaded,
                                    self);
}

static void
fetch_blocked_ids (BzMalcontentService *self)
{
  g_autoptr (GError) local_error    = NULL;
  g_autoptr (GVariant) result       = NULL;
  g_autoptr (GVariant) prop         = NULL;
  g_autoptr (GVariant) filter_tuple = NULL;
  g_autoptr (GtkStringList) blocked = NULL;
  g_autofree char *object_path      = NULL;
  gboolean         is_allowlist     = FALSE;
  GVariantIter    *iter             = NULL;
  const char      *ref              = NULL;

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", (int) getuid ());

  result = g_dbus_connection_call_sync (
      self->bus,
      "org.freedesktop.Accounts",
      object_path,
      "org.freedesktop.DBus.Properties",
      "Get",
      g_variant_new ("(ss)",
                     "com.endlessm.ParentalControls.AppFilter",
                     "AppFilter"),
      G_VARIANT_TYPE ("(v)"),
      G_DBUS_CALL_FLAGS_NONE,
      -1,
      NULL,
      &local_error);

  if (result == NULL)
    {
      g_warning ("failed to fetch malcontent AppFilter from D-Bus: %s", local_error->message);
      return;
    }

  prop         = g_variant_get_child_value (result, 0);
  filter_tuple = g_variant_get_variant (prop);

  g_variant_get (filter_tuple, "(bas)", &is_allowlist, &iter);

  blocked = gtk_string_list_new (NULL);
  while (g_variant_iter_next (iter, "s", &ref))
    gtk_string_list_append (blocked, ref);

  g_variant_iter_free (iter);

  bz_state_info_set_parental_blocked_ids (self->state, G_LIST_MODEL (blocked));
}

static void
apply_filter (BzMalcontentService *self,
              MctAppFilter        *filter)
{
  const char *const *oars_sections = NULL;
  int                max_age       = 0;

  g_clear_pointer (&self->filter, mct_app_filter_unref);
  self->filter = mct_app_filter_ref (filter);

  oars_sections = mct_app_filter_get_oars_sections (self->filter);

  for (gsize i = 0; oars_sections[i] != NULL; i++)
    {
      MctAppFilterOarsValue filter_value;
      int                   section_age;

      filter_value = mct_app_filter_get_oars_value (self->filter, oars_sections[i]);

      if (filter_value == MCT_APP_FILTER_OARS_VALUE_UNKNOWN)
        continue;

      section_age = as_content_rating_attribute_to_csm_age (oars_sections[i],
                                                            (AsContentRatingValue) filter_value);

      if (section_age > max_age)
        max_age = section_age;
    }

  bz_state_info_set_parental_age_rating (self->state, max_age);

  fetch_blocked_ids (self);
}

static void
on_filter_loaded (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  BzMalcontentService *self       = NULL;
  g_autoptr (MctAppFilter) filter = NULL;
  g_autoptr (GError) local_error  = NULL;

  self   = user_data;
  filter = mct_manager_get_app_filter_finish (self->manager, result, &local_error);

  if (filter != NULL)
    apply_filter (self, filter);
  else
    g_warning ("Failed to load malcontent app filter: %s", local_error->message);
}

static void
on_filter_changed (MctManager *manager,
                   guint64     user_id,
                   gpointer    user_data)
{
  BzMalcontentService *self = NULL;

  self = user_data;

  if (user_id != (guint64) getuid ())
    return;

  mct_manager_get_app_filter_async (self->manager,
                                    getuid (),
                                    MCT_MANAGER_GET_VALUE_FLAGS_NONE,
                                    NULL,
                                    on_filter_loaded,
                                    self);
}

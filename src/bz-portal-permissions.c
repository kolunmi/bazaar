/* bz-portal-permissions.c
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

#include "bz-portal-permissions.h"

#define PERMISSION_STORE_BUS_NAME    "org.freedesktop.impl.portal.PermissionStore"
#define PERMISSION_STORE_OBJECT_PATH "/org/freedesktop/impl/portal/PermissionStore"
#define PERMISSION_STORE_IFACE       "org.freedesktop.impl.portal.PermissionStore"

typedef struct
{
  const char *table;
  const char *resource_id;
  const char *icon_allowed;
  const char *icon_denied;
  const char *title;
} PortalInfo;

static const PortalInfo portal_infos[] = {
  {    "background",   "background",     "background-app-ghost-symbolic", "background-app-ghost-disabled-symbolic",    N_ ("Background") },
  { "notifications", "notification",            "notifications-symbolic",        "notifications-disabled-symbolic", N_ ("Notifications") },
  {       "devices",     "speakers",           "audio-speakers-symbolic",                     "speaker-0-symbolic",      N_ ("Speakers") },
  {       "devices",       "camera",             "camera-video-symbolic",          "film-camera-disabled-symbolic",        N_ ("Camera") },
  {      "location",     "location", "location-services-active-symbolic",    "location-services-disabled-symbolic",      N_ ("Location") },
  {       "devices",   "microphone",   "audio-input-microphone-symbolic",           "microphone-disabled-symbolic",    N_ ("Microphone") },
};

#define N_PORTALS G_N_ELEMENTS (portal_infos)

typedef struct
{
  GtkWidget *flow_child;
  GtkWidget *card;
  GtkWidget *icon;
  GtkWidget *title_label;
  gboolean   supported;
  gboolean   active;
} PortalTile;

typedef struct
{
  GWeakRef self;
  guint    index;
} AsyncData;

struct _BzPortalPermissions
{
  AdwBin parent_instance;

  char       *app_id;
  GDBusProxy *proxy;
  GtkFlowBox *flow_box;

  PortalTile tiles[N_PORTALS];
};

G_DEFINE_FINAL_TYPE (BzPortalPermissions, bz_portal_permissions, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_APP_ID,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP] = { 0 };

static AsyncData *
async_data_new (BzPortalPermissions *self,
                guint                index)
{
  AsyncData *data = NULL;

  data = g_new0 (AsyncData, 1);
  g_weak_ref_init (&data->self, self);
  data->index = index;

  return data;
}

static void
async_data_free (gpointer ptr)
{
  AsyncData *data = ptr;

  if (data == NULL)
    return;

  g_weak_ref_clear (&data->self);
  g_free (data);
}

static void
swap_css_class (GtkWidget  *widget,
                const char *add,
                const char *remove)
{
  gtk_widget_remove_css_class (widget, remove);
  gtk_widget_add_css_class (widget, add);
}

static void
update_tile_visual (BzPortalPermissions *self,
                    guint                index)
{
  const PortalInfo *info = &portal_infos[index];
  PortalTile       *tile = &self->tiles[index];

  gtk_image_set_from_icon_name (GTK_IMAGE (tile->icon),
                                tile->active ? info->icon_allowed : info->icon_denied);

  if (tile->active)
    {
      swap_css_class (tile->icon, "accent", "dim-label");
      swap_css_class (tile->title_label, "accent", "dim-label");
    }
  else
    {
      swap_css_class (tile->icon, "dim-label", "accent");
      swap_css_class (tile->title_label, "dim-label", "accent");
    }
}

static GtkWidget *
create_tile (BzPortalPermissions *self,
             guint                index)
{
  const PortalInfo *info         = &portal_infos[index];
  PortalTile       *tile         = &self->tiles[index];
  g_autoptr (GtkBuilder) builder = NULL;

  builder = gtk_builder_new_from_resource ("/io/github/kolunmi/Bazaar/bz-portal-tile.ui");

  tile->card        = GTK_WIDGET (gtk_builder_get_object (builder, "card"));
  tile->icon        = GTK_WIDGET (gtk_builder_get_object (builder, "icon"));
  tile->title_label = GTK_WIDGET (gtk_builder_get_object (builder, "title_label"));

  gtk_image_set_from_icon_name (GTK_IMAGE (tile->icon), info->icon_denied);
  gtk_label_set_label (GTK_LABEL (tile->title_label), g_dgettext (NULL, info->title));

  return g_object_ref (tile->card);
}

static void
set_permission_cb (GObject      *source,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  AsyncData *data                          = user_data;
  g_autoptr (BzPortalPermissions) self_ref = NULL;
  g_autoptr (GVariant) ret                 = NULL;
  g_autoptr (GError) error                 = NULL;
  guint index                              = 0;

  index    = data->index;
  self_ref = g_weak_ref_get (&data->self);
  async_data_free (data);

  if (self_ref == NULL)
    return;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);

  if (ret == NULL)
    {
      g_warning ("Failed to set permission for %s/%s: %s",
                 portal_infos[index].table,
                 portal_infos[index].resource_id,
                 error->message);

      self_ref->tiles[index].active = !self_ref->tiles[index].active;
      update_tile_visual (self_ref, index);
    }
}

static void
set_permission (BzPortalPermissions *self,
                guint                index,
                gboolean             allowed)
{
  GVariantBuilder builder;
  const char     *perm_value = NULL;

  g_return_if_fail (self->proxy != NULL);
  g_return_if_fail (self->app_id != NULL);

  perm_value = allowed ? "yes" : "no";

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&builder, "s", perm_value);

  g_dbus_proxy_call (self->proxy,
                     "SetPermission",
                     g_variant_new ("(sbssas)",
                                    portal_infos[index].table,
                                    TRUE,
                                    portal_infos[index].resource_id,
                                    self->app_id,
                                    &builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     set_permission_cb,
                     async_data_new (self, index));

  self->tiles[index].active = allowed;
  update_tile_visual (self, index);
}

static void
lookup_cb (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
  AsyncData *data                          = user_data;
  g_autoptr (BzPortalPermissions) self_ref = NULL;
  g_autoptr (GVariant) ret                 = NULL;
  g_autoptr (GError) error                 = NULL;
  PortalTile *tile                         = NULL;
  guint       index                        = 0;

  index    = data->index;
  self_ref = g_weak_ref_get (&data->self);
  async_data_free (data);

  if (self_ref == NULL)
    return;

  tile = &self_ref->tiles[index];
  ret  = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);

  if (ret == NULL)
    {
      tile->supported = FALSE;
      gtk_widget_set_sensitive (tile->flow_child, FALSE);
      gtk_widget_set_opacity (tile->flow_child, 0.5);
      return;
    }

  tile->supported = TRUE;
  gtk_widget_set_sensitive (tile->flow_child, TRUE);
  gtk_widget_set_opacity (tile->flow_child, 1.0);

  tile->active = FALSE;

  {
    g_autoptr (GVariant) permissions = NULL;
    g_autoptr (GVariant) app_perms   = NULL;

    permissions = g_variant_get_child_value (ret, 0);
    app_perms   = g_variant_lookup_value (permissions, self_ref->app_id, NULL);

    if (app_perms != NULL)
      {
        g_autoptr (GVariant) first = NULL;
        const char *perm_str       = NULL;

        first    = g_variant_get_child_value (app_perms, 0);
        perm_str = g_variant_get_string (first, NULL);

        tile->active = (g_strcmp0 (perm_str, "yes") == 0 ||
                        g_strcmp0 (perm_str, "EXACT") == 0); // For location portal
      }
  }

  update_tile_visual (self_ref, index);
}

static void
load_permissions (BzPortalPermissions *self)
{
  g_return_if_fail (self->proxy != NULL);
  g_return_if_fail (self->app_id != NULL);

  for (guint i = 0; i < N_PORTALS; i++)
    {
      g_dbus_proxy_call (self->proxy,
                         "Lookup",
                         g_variant_new ("(ss)",
                                        portal_infos[i].table,
                                        portal_infos[i].resource_id),
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         lookup_cb,
                         async_data_new (self, i));
    }
}

static void
child_activated_cb (GtkFlowBox      *flow_box,
                    GtkFlowBoxChild *child,
                    gpointer         user_data)
{
  BzPortalPermissions *self  = BZ_PORTAL_PERMISSIONS (user_data);
  int                  index = 0;

  index = gtk_flow_box_child_get_index (child);

  if (index < 0 || index >= (int) N_PORTALS)
    return;

  if (!self->tiles[index].supported)
    return;

  set_permission (self, (guint) index, !self->tiles[index].active);
}

static void
proxy_ready_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  BzPortalPermissions *self = BZ_PORTAL_PERMISSIONS (user_data);
  g_autoptr (GError) error  = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error);

  if (self->proxy == NULL)
    {
      g_warning ("Failed to connect to PermissionStore: %s", error->message);
      return;
    }

  if (self->app_id != NULL)
    load_permissions (self);
}

static void
bz_portal_permissions_dispose (GObject *object)
{
  BzPortalPermissions *self = BZ_PORTAL_PERMISSIONS (object);

  g_clear_pointer (&self->app_id, g_free);
  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (bz_portal_permissions_parent_class)->dispose (object);
}

static void
bz_portal_permissions_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  BzPortalPermissions *self = BZ_PORTAL_PERMISSIONS (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_portal_permissions_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  BzPortalPermissions *self = BZ_PORTAL_PERMISSIONS (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_clear_pointer (&self->app_id, g_free);
      self->app_id = g_value_dup_string (value);
      if (self->proxy != NULL && self->app_id != NULL)
        load_permissions (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_portal_permissions_class_init (BzPortalPermissionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = bz_portal_permissions_dispose;
  object_class->get_property = bz_portal_permissions_get_property;
  object_class->set_property = bz_portal_permissions_set_property;

  props[PROP_APP_ID] =
      g_param_spec_string ("app-id", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
bz_portal_permissions_init (BzPortalPermissions *self)
{
  self->flow_box = GTK_FLOW_BOX (gtk_flow_box_new ());

  gtk_flow_box_set_selection_mode (self->flow_box, GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (self->flow_box, TRUE);
  gtk_flow_box_set_max_children_per_line (self->flow_box, 3);
  gtk_flow_box_set_min_children_per_line (self->flow_box, 2);
  gtk_flow_box_set_row_spacing (self->flow_box, 12);
  gtk_flow_box_set_column_spacing (self->flow_box, 12);
  gtk_widget_add_css_class (GTK_WIDGET (self->flow_box), "portal-flow-box");

  g_signal_connect (self->flow_box, "child-activated",
                    G_CALLBACK (child_activated_cb), self);

  adw_bin_set_child (ADW_BIN (self), GTK_WIDGET (self->flow_box));

  for (guint i = 0; i < N_PORTALS; i++)
    {
      GtkWidget *tile = NULL;

      tile = create_tile (self, i);
      gtk_flow_box_append (self->flow_box, tile);

      self->tiles[i].flow_child = gtk_widget_get_parent (tile);
      gtk_widget_set_sensitive (self->tiles[i].flow_child, FALSE);
    }

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            PERMISSION_STORE_BUS_NAME,
                            PERMISSION_STORE_OBJECT_PATH,
                            PERMISSION_STORE_IFACE,
                            NULL,
                            proxy_ready_cb,
                            self);
}

GtkWidget *
bz_portal_permissions_new (const char *app_id)
{
  return g_object_new (BZ_TYPE_PORTAL_PERMISSIONS,
                       "app-id", app_id,
                       NULL);
}

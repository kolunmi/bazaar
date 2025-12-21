/* bz-installed-tile.c
 *
 * Copyright 2025 Adam Masciola, Alexander Vanhee
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

#include <glib/gi18n.h>

#include "bz-addons-dialog.h"
#include "bz-entry-group-util.h"
#include "bz-entry-group.h"
#include "bz-env.h"
#include "bz-error.h"
#include "bz-installed-page.h"
#include "bz-installed-tile.h"
#include "bz-state-info.h"

struct _BzInstalledTile
{
  BzListTile parent_instance;

  BzEntryGroup *group;

  GtkPicture *icon_picture;
  GtkImage   *fallback_icon;
  GtkLabel   *title_label;
  GtkButton  *support_button;
  GtkButton  *addons_button;
  GtkButton  *remove_button;
};

G_DEFINE_FINAL_TYPE (BzInstalledTile, bz_installed_tile, BZ_TYPE_LIST_TILE)

enum
{
  PROP_0,
  PROP_GROUP,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static gboolean
test_is_support (BzEntry *entry);

static gboolean
test_has_addons (BzEntry *entry);

static void
bz_installed_tile_dispose (GObject *object)
{
  BzInstalledTile *self = BZ_INSTALLED_TILE (object);

  g_clear_object (&self->group);

  G_OBJECT_CLASS (bz_installed_tile_parent_class)->dispose (object);
}

static void
bz_installed_tile_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  BzInstalledTile *self = BZ_INSTALLED_TILE (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, bz_installed_tile_get_group (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_installed_tile_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  BzInstalledTile *self = BZ_INSTALLED_TILE (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      bz_installed_tile_set_group (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
invert_boolean (gpointer object,
                gboolean value)
{
  return !value;
}

static gboolean
is_null (gpointer object,
         GObject *value)
{
  return value == NULL;
}

static gboolean
is_zero (gpointer object,
         int      value)
{
  return value == 0;
}

static char *
format_size (gpointer object, guint64 value)
{
  return g_format_size (value);
}

static void
addon_transact_cb (BzInstalledTile *self,
                   BzEntry         *entry,
                   BzAddonsDialog  *dialog)
{
  BzInstalledPage *page      = NULL;
  gboolean         installed = FALSE;

  page = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (self), BZ_TYPE_INSTALLED_PAGE));
  g_assert (page != NULL);

  g_object_get (entry, "installed", &installed, NULL);

  if (installed)
    g_signal_emit_by_name (page, "remove-addon", entry);
  else
    g_signal_emit_by_name (page, "install-addon", entry);
}

static DexFuture *
support_fiber (BzInstalledTile *tile)
{
  g_autoptr (GError) local_error = NULL;
  GtkWidget *window              = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  const char *url                = NULL;

  window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  entry = bz_entry_group_find_entry (tile->group, test_is_support, window, &local_error);
  if (entry == NULL)
    goto err;

  url = bz_entry_get_donation_url (entry);
  g_app_info_launch_default_for_uri (url, NULL, NULL);

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
support_cb (BzInstalledTile *self,
            GtkButton       *button)
{
  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) support_fiber,
      g_object_ref (self),
      g_object_unref));
}

static DexFuture *
install_addons_fiber (BzInstalledTile *tile)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *page          = NULL;
  BzStateInfo     *state         = NULL;
  GtkWidget       *window        = NULL;
  g_autoptr (BzEntry) entry      = NULL;
  g_autoptr (GListModel) model   = NULL;
  AdwDialog *addons_dialog       = NULL;

  page = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (tile), BZ_TYPE_INSTALLED_PAGE));
  g_assert (page != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  g_object_get (page, "state", &state, NULL);
  g_assert (state != NULL);

  entry = bz_entry_group_find_entry (tile->group, test_has_addons, window, &local_error);
  if (entry == NULL)
    goto err;

  model = bz_application_map_factory_generate (
      bz_state_info_get_entry_factory (state),
      bz_entry_get_addons (entry));

  addons_dialog = bz_addons_dialog_new (entry, model);
  gtk_widget_set_size_request (GTK_WIDGET (addons_dialog), 350, -1);
  g_signal_connect_swapped (addons_dialog, "transact", G_CALLBACK (addon_transact_cb), tile);

  adw_dialog_present (addons_dialog, GTK_WIDGET (tile));

  g_clear_object (&state);
  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  g_clear_object (&state);
  return NULL;
}

static void
install_addons_cb (BzInstalledTile *self,
                   GtkButton       *button)
{
  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) install_addons_fiber,
      g_object_ref (self),
      g_object_unref));
}

static DexFuture *
remove_fiber (BzInstalledTile *tile)
{
  g_autoptr (GError) local_error = NULL;
  BzInstalledPage *page          = NULL;
  GtkWidget       *window        = NULL;
  g_autoptr (BzEntry) entry      = NULL;

  page = BZ_INSTALLED_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (tile), BZ_TYPE_INSTALLED_PAGE));
  g_assert (page != NULL);

  window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GTK_TYPE_WINDOW);
  g_assert (window != NULL);

  entry = bz_entry_group_find_entry (tile->group, NULL, window, &local_error);
  if (entry == NULL)
    goto err;

  g_signal_emit_by_name (page, "remove", entry);

  return NULL;

err:
  if (local_error != NULL)
    bz_show_error_for_widget (window, local_error->message);
  return NULL;
}

static void
remove_cb (BzInstalledTile *self,
           GtkButton       *button)
{
  dex_future_disown (dex_scheduler_spawn (
      dex_scheduler_get_default (),
      bz_get_dex_stack_size (),
      (DexFiberFunc) remove_fiber,
      g_object_ref (self),
      g_object_unref));
}

static void
bz_installed_tile_class_init (BzInstalledTileClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_installed_tile_dispose;
  object_class->get_property = bz_installed_tile_get_property;
  object_class->set_property = bz_installed_tile_set_property;

  props[PROP_GROUP] =
      g_param_spec_object (
          "group",
          NULL, NULL,
          BZ_TYPE_ENTRY_GROUP,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_LIST_TILE);
  g_type_ensure (BZ_TYPE_ENTRY_GROUP);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-installed-tile.ui");
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, icon_picture);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, fallback_icon);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, title_label);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, support_button);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, addons_button);
  gtk_widget_class_bind_template_child (widget_class, BzInstalledTile, remove_button);
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, is_zero);
  gtk_widget_class_bind_template_callback (widget_class, format_size);
  gtk_widget_class_bind_template_callback (widget_class, support_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_addons_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_cb);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_BUTTON);
}

static void
bz_installed_tile_init (BzInstalledTile *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_installed_tile_new (void)
{
  return g_object_new (BZ_TYPE_INSTALLED_TILE, NULL);
}

void
bz_installed_tile_set_group (BzInstalledTile *self,
                             BzEntryGroup    *group)
{
  g_return_if_fail (BZ_IS_INSTALLED_TILE (self));
  g_return_if_fail (group == NULL || BZ_IS_ENTRY_GROUP (group));

  g_clear_object (&self->group);
  if (group != NULL)
    self->group = g_object_ref (group);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

BzEntryGroup *
bz_installed_tile_get_group (BzInstalledTile *self)
{
  g_return_val_if_fail (BZ_IS_INSTALLED_TILE (self), NULL);
  return self->group;
}

static gboolean
test_is_support (BzEntry *entry)
{
  return bz_entry_get_donation_url (entry) != NULL;
}

static gboolean
test_has_addons (BzEntry *entry)
{
  GListModel *model = NULL;

  model = bz_entry_get_addons (entry);
  return model != NULL && g_list_model_get_n_items (model) > 0;
}

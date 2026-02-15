/* bz-updates-card.c
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

#include "bz-list-tile.h"
#include "bz-release.h"
#include "bz-releases-list.h"
#include "bz-template-callbacks.h"
#include "bz-updates-card.h"

struct _BzUpdatesCard
{
  AdwBin parent_instance;

  BzStateInfo *state;

  /* Template widgets */
  GtkRevealer        *revealer;
  GtkImage           *toggle_icon;
  GtkCustomFilter    *apps_filter;
  GtkCustomFilter    *runtimes_filter;
  GtkFilterListModel *runtimes_filter_model;
};

G_DEFINE_FINAL_TYPE (BzUpdatesCard, bz_updates_card, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_STATE,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

enum
{
  SIGNAL_UPDATE,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

static char *
format_update_count (gpointer    object,
                     GListModel *updates);

static char *
format_version_change (gpointer    object,
                       GListModel *version_history,
                       const char *installed_version);

static char *
format_runtime_count (gpointer object,
                      guint    n_items);

static void
tile_activated_cb (BzListTile    *tile,
                   BzUpdatesCard *self);

static void
show_version_history_cb (GtkListItem *template,
                         GtkButton   *button);

static void
update_entry_cb (GtkListItem *template,
                 GtkButton   *button);

static void
update_all_cb (GtkButton     *button,
               BzUpdatesCard *self);

static void
update_runtimes_cb (GtkButton     *button,
                    BzUpdatesCard *self);

static gboolean
filter_apps (BzEntry       *entry,
             BzUpdatesCard *self);

static gboolean
filter_runtimes (BzEntry       *entry,
                 BzUpdatesCard *self);

static void
bz_updates_card_dispose (GObject *object)
{
  BzUpdatesCard *self = BZ_UPDATES_CARD (object);

  g_clear_object (&self->state);

  G_OBJECT_CLASS (bz_updates_card_parent_class)->dispose (object);
}

static void
bz_updates_card_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  BzUpdatesCard *self = BZ_UPDATES_CARD (object);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_object (value, bz_updates_card_get_state (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_updates_card_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BzUpdatesCard *self = BZ_UPDATES_CARD (object);

  switch (prop_id)
    {
    case PROP_STATE:
      bz_updates_card_set_state (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_updates_card_class_init (BzUpdatesCardClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_updates_card_dispose;
  object_class->get_property = bz_updates_card_get_property;
  object_class->set_property = bz_updates_card_set_property;

  props[PROP_STATE] =
      g_param_spec_object (
          "state",
          NULL, NULL,
          BZ_TYPE_STATE_INFO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  signals[SIGNAL_UPDATE] =
      g_signal_new (
          "update",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_FIRST,
          0,
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          G_TYPE_LIST_MODEL);
  g_signal_set_va_marshaller (
      signals[SIGNAL_UPDATE],
      G_TYPE_FROM_CLASS (klass),
      g_cclosure_marshal_VOID__OBJECTv);

  g_type_ensure (BZ_TYPE_LIST_TILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-updates-card.ui");
  bz_widget_class_bind_all_util_callbacks (widget_class);

  gtk_widget_class_bind_template_child (widget_class, BzUpdatesCard, revealer);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatesCard, toggle_icon);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatesCard, apps_filter);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatesCard, runtimes_filter);
  gtk_widget_class_bind_template_child (widget_class, BzUpdatesCard, runtimes_filter_model);
  gtk_widget_class_bind_template_callback (widget_class, format_update_count);
  gtk_widget_class_bind_template_callback (widget_class, format_version_change);
  gtk_widget_class_bind_template_callback (widget_class, format_runtime_count);
  gtk_widget_class_bind_template_callback (widget_class, tile_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_version_history_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_entry_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_all_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_runtimes_cb);
}

static void
bz_updates_card_init (BzUpdatesCard *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_custom_filter_set_filter_func (
      self->apps_filter, (GtkCustomFilterFunc) filter_apps,
      self, NULL);

  gtk_custom_filter_set_filter_func (
      self->runtimes_filter, (GtkCustomFilterFunc) filter_runtimes,
      self, NULL);
}

GtkWidget *
bz_updates_card_new (void)
{
  return g_object_new (BZ_TYPE_UPDATES_CARD, NULL);
}

void
bz_updates_card_set_state (BzUpdatesCard *self,
                           BzStateInfo   *state)
{
  g_return_if_fail (BZ_IS_UPDATES_CARD (self));
  g_return_if_fail (state == NULL || BZ_IS_STATE_INFO (state));

  g_clear_object (&self->state);
  if (state != NULL)
    self->state = g_object_ref (state);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

BzStateInfo *
bz_updates_card_get_state (BzUpdatesCard *self)
{
  g_return_val_if_fail (BZ_IS_UPDATES_CARD (self), NULL);
  return self->state;
}

static char *
format_update_count (gpointer    object,
                     GListModel *updates)
{
  guint n_updates = 0;

  if (updates == NULL)
    return g_strdup ("");

  n_updates = g_list_model_get_n_items (updates);

  return g_strdup_printf (ngettext ("%u Available Update",
                                    "%u Available Updates",
                                    n_updates),
                          n_updates);
}

static char *
format_version_change (gpointer    object,
                       GListModel *version_history,
                       const char *installed_version)
{
  const char *new_version             = NULL; // This will probably the same as the installed version if using cache...
  g_autoptr (BzRelease) first_release = NULL;

  if (installed_version == NULL)
    return g_strdup ("");

  if (version_history == NULL || g_list_model_get_n_items (version_history) == 0)
    return g_strdup ("");

  first_release = g_list_model_get_item (version_history, 0);
  new_version   = bz_release_get_version (first_release);

  if (new_version == NULL)
    return g_strdup ("");

  if (g_strcmp0 (installed_version, new_version) == 0)
    return g_strdup ("");

  return g_strdup_printf ("%s → %s", installed_version, new_version);
}

static char *
format_runtime_count (gpointer object,
                      guint    n_items)
{
  return g_strdup_printf (ngettext ("%u Runtime Update",
                                    "%u Runtime Updates",
                                    n_items),
                          n_items);
}

static void
tile_activated_cb (BzListTile    *tile,
                   BzUpdatesCard *self)
{
  gboolean current_state = FALSE;

  g_assert (BZ_IS_LIST_TILE (tile));
  g_assert (BZ_IS_UPDATES_CARD (self));

  current_state = gtk_revealer_get_reveal_child (self->revealer);
  gtk_revealer_set_reveal_child (self->revealer, !current_state);

  if (!current_state)
    gtk_widget_add_css_class (GTK_WIDGET (self->toggle_icon), "rotated");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self->toggle_icon), "rotated");
}

static void
show_version_history_cb (GtkListItem *template,
                         GtkButton   *button)
{
  GtkWidget  *dialog          = NULL;
  GtkRoot    *root            = NULL;
  GListModel *version_history = NULL;
  BzEntry    *entry           = NULL;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (GTK_IS_LIST_ITEM (template));

  root = gtk_widget_get_root (GTK_WIDGET (button));
  if (root == NULL)
    return;

  entry = gtk_list_item_get_item (template);
  if (entry == NULL || !BZ_IS_ENTRY (entry))
    return;

  g_object_get (entry, "version-history", &version_history, NULL);

  dialog = bz_releases_dialog_new (version_history, NULL);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (root));

  g_clear_object (&version_history);
}

static void
update_entry_cb (GtkListItem *template,
                 GtkButton   *button)
{
  BzUpdatesCard *self          = NULL;
  BzEntry       *entry         = NULL;
  g_autoptr (GListStore) store = NULL;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (GTK_IS_LIST_ITEM (template));

  self = BZ_UPDATES_CARD (gtk_widget_get_ancestor (GTK_WIDGET (button), BZ_TYPE_UPDATES_CARD));
  if (self == NULL)
    return;

  entry = gtk_list_item_get_item (template);
  if (entry == NULL || !BZ_IS_ENTRY (entry))
    return;

  store = g_list_store_new (BZ_TYPE_ENTRY);
  g_list_store_append (store, entry);

  g_signal_emit (self, signals[SIGNAL_UPDATE], 0, store);
}

static void
update_all_cb (GtkButton     *button,
               BzUpdatesCard *self)
{
  GListModel *updates = NULL;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (BZ_IS_UPDATES_CARD (self));

  if (self->state == NULL)
    return;

  updates = bz_state_info_get_available_updates (self->state);
  if (updates == NULL)
    return;

  g_signal_emit (self, signals[SIGNAL_UPDATE], 0, updates);
}

static void
update_runtimes_cb (GtkButton     *button,
                    BzUpdatesCard *self)
{
  GListModel *runtimes         = NULL;
  g_autoptr (GListStore) store = NULL;
  guint n_items                = 0;

  runtimes = G_LIST_MODEL (self->runtimes_filter_model);
  n_items  = g_list_model_get_n_items (runtimes);

  if (n_items == 0)
    return;

  store = g_list_store_new (BZ_TYPE_ENTRY);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (BzEntry) entry = g_list_model_get_item (runtimes, i);
      if (entry != NULL)
        g_list_store_append (store, entry);
    }

  g_signal_emit (self, signals[SIGNAL_UPDATE], 0, store);
}

static gboolean
filter_apps (BzEntry       *entry,
             BzUpdatesCard *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY (entry), FALSE);

  return bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_APPLICATION);
}

static gboolean
filter_runtimes (BzEntry       *entry,
                 BzUpdatesCard *self)
{
  g_return_val_if_fail (BZ_IS_ENTRY (entry), FALSE);

  return bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_RUNTIME) ||
         bz_entry_is_of_kinds (entry, BZ_ENTRY_KIND_ADDON);
}

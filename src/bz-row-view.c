/* bz-row-view.c
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

#include "bz-row-view.h"
#include "bz-curated-view.h"
#include "bz-dynamic-list-view.h"
#include "bz-section-view.h"

struct _BzRowView
{
  AdwBin parent_instance;

  BzCuratedRow *row;

  /* Template widgets */
};

G_DEFINE_FINAL_TYPE (BzRowView, bz_row_view, ADW_TYPE_BIN)

enum
{
  PROP_0,

  PROP_ROW,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_row_view_dispose (GObject *object)
{
  BzRowView *self = BZ_ROW_VIEW (object);

  g_clear_object (&self->row);

  G_OBJECT_CLASS (bz_row_view_parent_class)->dispose (object);
}

static void
bz_row_view_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  BzRowView *self = BZ_ROW_VIEW (object);

  switch (prop_id)
    {
    case PROP_ROW:
      g_value_set_object (value, bz_row_view_get_row (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_row_view_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  BzRowView *self = BZ_ROW_VIEW (object);

  switch (prop_id)
    {
    case PROP_ROW:
      bz_row_view_set_row (self, g_value_get_object (value));
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

static void
group_activated_cb (GtkListItem  *list_item,
                    BzEntryGroup *group,
                    BzRowView    *view)
{
  GtkWidget *self = NULL;

  self = gtk_widget_get_ancestor (GTK_WIDGET (view), BZ_TYPE_CURATED_VIEW);
  g_assert (self != NULL);

  g_signal_emit_by_name (self, "group-selected", group);
}

static void
bind_section_view_cb (GtkListItem       *list_item,
                      BzSectionView     *section_view,
                      BzCuratedSection  *section,
                      BzDynamicListView *view)
{
  g_signal_connect_swapped (section_view, "group-activated",
                            G_CALLBACK (group_activated_cb),
                            list_item);
}

static void
unbind_section_view_cb (GtkListItem       *list_item,
                        BzSectionView     *section_view,
                        BzCuratedSection  *section,
                        BzDynamicListView *view)
{
  g_signal_handlers_disconnect_by_func (section_view, group_activated_cb, list_item);
}

static void
bz_row_view_class_init (BzRowViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = bz_row_view_dispose;
  object_class->get_property = bz_row_view_get_property;
  object_class->set_property = bz_row_view_set_property;

  props[PROP_ROW] =
      g_param_spec_object (
          "row",
          NULL, NULL,
          BZ_TYPE_CURATED_ROW,
          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_type_ensure (BZ_TYPE_DYNAMIC_LIST_VIEW);
  g_type_ensure (BZ_TYPE_SECTION_VIEW);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-row-view.ui");
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, bind_section_view_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_section_view_cb);
}

static void
bz_row_view_init (BzRowView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
bz_row_view_new (BzCuratedRow *row)
{
  return g_object_new (
      BZ_TYPE_ROW_VIEW,
      "row", row,
      NULL);
}

void
bz_row_view_set_row (BzRowView    *self,
                     BzCuratedRow *row)
{
  g_return_if_fail (BZ_IS_ROW_VIEW (self));
  g_return_if_fail (row == NULL || BZ_IS_CURATED_ROW (row));

  g_clear_object (&self->row);
  if (row != NULL)
    self->row = g_object_ref (row);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ROW]);
}

BzCuratedRow *
bz_row_view_get_row (BzRowView *self)
{
  g_return_val_if_fail (BZ_IS_ROW_VIEW (self), NULL);
  return self->row;
}

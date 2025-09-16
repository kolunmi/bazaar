/* bz-transaction-view.c
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

#include <glib/gi18n.h>

#include "bz-transaction-view.h"

struct _BzTransactionView
{
  AdwBin parent_instance;

  BzTransaction *transaction;
};

G_DEFINE_FINAL_TYPE (BzTransactionView, bz_transaction_view, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_TRANSACTION,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
bz_transaction_view_dispose (GObject *object)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  g_clear_pointer (&self->transaction, g_object_unref);

  G_OBJECT_CLASS (bz_transaction_view_parent_class)->dispose (object);
}

static void
bz_transaction_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      g_value_set_object (value, bz_transaction_view_get_transaction (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_transaction_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BzTransactionView *self = BZ_TRANSACTION_VIEW (object);

  switch (prop_id)
    {
    case PROP_TRANSACTION:
      bz_transaction_view_set_transaction (self, g_value_get_object (value));
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

static char *
format_download_size (gpointer object,
                      guint64  value)
{
  g_autofree char *size = NULL;

  return g_format_size (value);
}

static char *
format_installed_size (gpointer object,
                       guint64  value)
{
  g_autofree char *size = NULL;

  return g_format_size (value);
}

static char *
format_bytes_transferred (gpointer object,
                          guint64  value)
{
  g_autofree char *size = NULL;

  size = g_format_size (value);
  return g_strdup_printf (_ ("Transferred %s so far"), size);
}

static void
bz_transaction_view_class_init (BzTransactionViewClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_transaction_view_set_property;
  object_class->get_property = bz_transaction_view_get_property;
  object_class->dispose      = bz_transaction_view_dispose;

  props[PROP_TRANSACTION] =
      g_param_spec_object (
          "transaction",
          NULL, NULL,
          BZ_TYPE_TRANSACTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-transaction-view.ui");
  gtk_widget_class_bind_template_callback (widget_class, invert_boolean);
  gtk_widget_class_bind_template_callback (widget_class, is_null);
  gtk_widget_class_bind_template_callback (widget_class, format_download_size);
  gtk_widget_class_bind_template_callback (widget_class, format_installed_size);
  gtk_widget_class_bind_template_callback (widget_class, format_bytes_transferred);
}

static void
bz_transaction_view_init (BzTransactionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

BzTransactionView *
bz_transaction_view_new (void)
{
  return g_object_new (BZ_TYPE_TRANSACTION_VIEW, NULL);
}

BzTransaction *
bz_transaction_view_get_transaction (BzTransactionView *self)
{
  g_return_val_if_fail (BZ_IS_TRANSACTION_VIEW (self), NULL);
  return self->transaction;
}

void
bz_transaction_view_set_transaction (BzTransactionView *self,
                                     BzTransaction     *transaction)
{
  g_return_if_fail (BZ_IS_TRANSACTION_VIEW (self));

  g_clear_pointer (&self->transaction, g_object_unref);
  if (transaction != NULL)
    self->transaction = g_object_ref (transaction);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TRANSACTION]);
}

/* End of bz-transaction-view.c */

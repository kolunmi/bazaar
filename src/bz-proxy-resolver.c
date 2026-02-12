/* bz-proxy-resolver.c
 *
 * Copyright 2025 Adam Masciola
 * Copyright 2026 libffi <contact@ffi.lol>
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

#include "bz-proxy-resolver.h"

#include "config.h"

#include "gio/gio.h"
#include "glib-object.h"

#include <libproxy/proxy.h>

struct _BzProxyResolver
{
  GSimpleProxyResolver parent_instance;
  pxProxyFactory      *proxy_factory;
  char               **proxies;
};

G_DEFINE_TYPE (BzProxyResolver, bz_proxy_resolver, G_TYPE_SIMPLE_PROXY_RESOLVER)
#define LOG_DOMAIN "BZ::PROXY-RESOLVER"

static void
bz_proxy_resolver_init (BzProxyResolver *self)
{
  g_log (LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "Initialising the proxy resolver instance.");

  self->proxy_factory = px_proxy_factory_new ();

  self->proxies = px_proxy_factory_get_proxies (self->proxy_factory, DONATE_LINK);
  g_simple_proxy_resolver_set_default_proxy (&self->parent_instance, self->proxies[0]);
  g_log (LOG_DOMAIN, G_LOG_LEVEL_INFO, "Resolved %li proxies, using the first one.",
         sizeof (*self->proxies) / sizeof (size_t));
}

static void
bz_proxy_resolver_dispose (GObject *object)
{
  BzProxyResolver *instance = bz_proxy_resolver_get_instance_private (BZ_PROXY_RESOLVER (object));
  g_clear_object (&instance->proxy_factory);
  G_OBJECT_CLASS (bz_proxy_resolver_parent_class)->dispose (object);
}

static void
bz_proxy_resolver_finalize (GObject *object)
{
  BzProxyResolver *instance = bz_proxy_resolver_get_instance_private (BZ_PROXY_RESOLVER (object));
  px_proxy_factory_free_proxies (instance->proxies);
  g_free (instance->proxy_factory);
  G_OBJECT_CLASS (bz_proxy_resolver_parent_class)->finalize (object);
}

static void
bz_proxy_resolver_class_init (BzProxyResolverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose      = bz_proxy_resolver_dispose;
  object_class->finalize     = bz_proxy_resolver_finalize;
}

BzProxyResolver *
bz_proxy_resolver_new (void)
{
  return g_object_new (BZ_TYPE_PROXY_RESOLVER, NULL);
}

// FIXME: This class should **ideally** be a singleton, however as of now
// it's only instantiated in two cases:
// 1. For the login page, every time it is opened/instantiated.
// 2. Once, per application, for the global HTTP(S) client.
// That is NOT critical, however it is a place that could be optimized.

/* End of bz-proxy-resolver.c */

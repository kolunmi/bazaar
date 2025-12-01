/* bz-login-page.c
 *
 * Copyright 2025 Alexander Vanhee
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
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

#include "bz-async-texture.h"
#include "bz-flathub-auth-provider.h"
#include "bz-login-page.h"

struct _BzLoginPage
{
  AdwNavigationPage parent_instance;

  WebKitWebView *webview;
  gboolean       webkit_loaded;

  SoupSession   *session;
  SoupCookieJar *cookie_jar;

  GList                 *providers;
  BzFlathubAuthProvider *current_provider;
  char                  *auth_redirect_url;
  char                  *session_cookie;
  gboolean               oauth_completed;

  /* Template widgets */
  GtkStack          *main_stack;
  AdwStatusPage     *start_status_page;
  AdwPreferencesGroup *provider_preferences_group;
  GtkScrolledWindow *browser_scroll;
  AdwAvatar         *finish_avatar;
  GtkLabel          *welcome_label;
};

G_DEFINE_FINAL_TYPE (BzLoginPage, bz_login_page, ADW_TYPE_NAVIGATION_PAGE)

enum
{
  SIGNAL_LOGIN_COMPLETE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
load_webkit_library (BzLoginPage *self)
{
  if (self->webkit_loaded)
    return;

  self->webview = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  gtk_scrolled_window_set_child (self->browser_scroll, GTK_WIDGET (self->webview));

  self->webkit_loaded = TRUE;
}

static void
on_user_info_loaded (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data);

static void
on_oauth_complete (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data);

static void
complete_oauth (BzLoginPage *self,
                const char  *code,
                const char  *state,
                const char  *error)
{
  g_autoptr (JsonBuilder) builder     = NULL;
  g_autoptr (JsonGenerator) generator = NULL;
  g_autoptr (JsonNode) root           = NULL;
  g_autoptr (GBytes) bytes            = NULL;
  g_autofree char *json_data          = NULL;
  SoupMessage     *msg                = NULL;
  g_autofree char *url                = NULL;

  gtk_stack_set_visible_child_name (self->main_stack, "loading");

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  if (error != NULL)
    {
      json_builder_set_member_name (builder, "state");
      json_builder_add_string_value (builder, state);
      json_builder_set_member_name (builder, "error");
      json_builder_add_string_value (builder, error);
    }
  else
    {
      json_builder_set_member_name (builder, "code");
      json_builder_add_string_value (builder, code);
      json_builder_set_member_name (builder, "state");
      json_builder_add_string_value (builder, state);
    }

  json_builder_end_object (builder);

  root      = json_builder_get_root (builder);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  json_data = json_generator_to_data (generator, NULL);

  url = g_strdup_printf ("https://flathub.org/api/v2/auth/login/%s",
                         bz_flathub_auth_provider_get_method (self->current_provider));

  msg = soup_message_new ("POST", url);
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "accept", "application/json");
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "Content-Type", "application/json");

  bytes = g_bytes_new (json_data, strlen (json_data));
  soup_message_set_request_body_from_bytes (msg, "application/json", bytes);

  soup_session_send_and_read_async (self->session,
                                    msg,
                                    G_PRIORITY_DEFAULT,
                                    NULL,
                                    on_oauth_complete,
                                    self);
}

static gboolean
on_decide_policy (WebKitWebView           *webview,
                  WebKitPolicyDecision    *decision,
                  WebKitPolicyDecisionType decision_type,
                  gpointer                 user_data)
{
  BzLoginPage            *self       = BZ_LOGIN_PAGE (user_data);
  WebKitNavigationAction *nav_action = NULL;
  WebKitURIRequest       *request    = NULL;
  const char             *uri        = NULL;
  g_autoptr (GUri) parsed_uri        = NULL;
  GHashTable *params                 = NULL;
  const char *code                   = NULL;
  const char *state                  = NULL;
  const char *error                  = NULL;

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    return FALSE;

  nav_action = webkit_navigation_policy_decision_get_navigation_action (
      WEBKIT_NAVIGATION_POLICY_DECISION (decision));
  request = webkit_navigation_action_get_request (nav_action);
  uri     = webkit_uri_request_get_uri (request);

  if (uri == NULL || strstr (uri, "flathub.org") == NULL || strstr (uri, "/login/") == NULL)
    return FALSE;

  parsed_uri = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);
  if (parsed_uri == NULL)
    return FALSE;

  params = g_uri_parse_params (g_uri_get_query (parsed_uri), -1, "&",
                               G_URI_PARAMS_NONE, NULL);
  if (params == NULL)
    return FALSE;

  code  = g_hash_table_lookup (params, "code");
  state = g_hash_table_lookup (params, "state");
  error = g_hash_table_lookup (params, "error");

  if ((code != NULL && state != NULL && !self->oauth_completed) ||
      (error != NULL && state != NULL && !self->oauth_completed))
    {
      self->oauth_completed = TRUE;
      webkit_policy_decision_ignore (decision);
      complete_oauth (self, code, state, error);
      g_hash_table_unref (params);
      return TRUE;
    }

  g_hash_table_unref (params);
  return FALSE;
}

static void
get_user_info (BzLoginPage *self)
{
  SoupMessage *msg = NULL;

  msg = soup_message_new ("GET", "https://flathub.org/api/v2/auth/userinfo");
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "accept", "application/json");

  soup_session_send_and_read_async (self->session,
                                    msg,
                                    G_PRIORITY_DEFAULT,
                                    NULL,
                                    on_user_info_loaded,
                                    self);
}

static void
on_oauth_complete (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  BzLoginPage *self             = BZ_LOGIN_PAGE (user_data);
  g_autoptr (GBytes) bytes      = NULL;
  g_autoptr (GError) error      = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode   *root              = NULL;
  JsonObject *obj               = NULL;
  const char *status            = NULL;
  GSList     *cookies           = NULL;
  GSList     *l                 = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object),
                                             res, &error);
  if (error != NULL)
    {
      g_warning ("OAuth complete error: %s", error->message);
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes),
                                   &error))
    {
      g_warning ("Failed to parse OAuth response: %s", error->message);
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  root   = json_parser_get_root (parser);
  obj    = json_node_get_object (root);
  status = json_object_get_string_member (obj, "status");

  cookies = soup_cookie_jar_all_cookies (self->cookie_jar);
  for (l = cookies; l != NULL; l = l->next)
    {
      SoupCookie *cookie = l->data;
      if (g_strcmp0 (soup_cookie_get_name (cookie), "session") == 0)
        {
          g_free (self->session_cookie);
          self->session_cookie = g_strdup (soup_cookie_get_value (cookie));
        }
    }
  g_slist_free_full (cookies, (GDestroyNotify) soup_cookie_free);

  if (g_strcmp0 (status, "ok") == 0 || g_strcmp0 (status, "success") == 0)
    {
      get_user_info (self);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       "Authentication failed");
    }
}

static void
on_user_info_loaded (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  BzLoginPage *self             = BZ_LOGIN_PAGE (user_data);
  g_autoptr (GBytes) bytes      = NULL;
  g_autoptr (GError) error      = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode   *root              = NULL;
  JsonObject *obj               = NULL;
  JsonObject *default_account   = NULL;
  const char *displayname       = NULL;
  const char *avatar_url        = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object),
                                             res, &error);
  if (error != NULL)
    {
      g_warning ("User info load error: %s", error->message);
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes),
                                   &error))
    {
      g_warning ("Failed to parse user info: %s", error->message);
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  root = json_parser_get_root (parser);
  obj  = json_node_get_object (root);

  displayname = json_object_get_string_member (obj, "displayname");
  if (displayname == NULL)
    displayname = "N/A";

  if (json_object_has_member (obj, "default_account"))
    {
      default_account = json_object_get_object_member (obj, "default_account");
      avatar_url      = json_object_get_string_member (default_account, "avatar");
    }

  gtk_label_set_text (self->welcome_label,
                      g_strdup_printf ("Hello, %s!", displayname));
  adw_avatar_set_text (self->finish_avatar, displayname);

  if (avatar_url != NULL && avatar_url[0] != '\0')
    {
      g_autoptr (GFile) avatar_file = NULL;
      BzAsyncTexture *async_texture = NULL;

      avatar_file   = g_file_new_for_uri (avatar_url);
      async_texture = bz_async_texture_new (avatar_file, NULL);

      adw_avatar_set_custom_image (self->finish_avatar,
                                   GDK_PAINTABLE (async_texture));
      g_object_unref (async_texture);
    }

  gtk_stack_set_visible_child_name (self->main_stack, "finish");

  if (self->webview != NULL)
    webkit_web_view_load_uri (self->webview, "about:blank");
}

static void
on_login_response (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  BzLoginPage *self             = BZ_LOGIN_PAGE (user_data);
  g_autoptr (GBytes) bytes      = NULL;
  g_autoptr (GError) error      = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode   *root              = NULL;
  JsonObject *obj               = NULL;
  const char *redirect          = NULL;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object),
                                             res, &error);
  if (error != NULL)
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes),
                                   &error))
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       g_strdup_printf ("Error: %s", error->message));
      return;
    }

  root     = json_parser_get_root (parser);
  obj      = json_node_get_object (root);
  redirect = json_object_get_string_member (obj, "redirect");

  if (redirect != NULL)
    {
      g_free (self->auth_redirect_url);
      self->auth_redirect_url = g_strdup (redirect);

      load_webkit_library (self);

      g_signal_connect (self->webview, "decide-policy",
                        G_CALLBACK (on_decide_policy), self);

      gtk_stack_set_visible_child_name (self->main_stack, "browser");
      webkit_web_view_load_uri (self->webview, self->auth_redirect_url);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (self->start_status_page,
                                       "Error: No redirect URL received");
    }
}

static void
on_provider_row_activated (GtkButton   *button,
                            BzLoginPage *self)
{
  BzFlathubAuthProvider *provider = NULL;
  SoupMessage           *msg      = NULL;
  g_autofree char       *url      = NULL;

  provider = g_object_get_data (G_OBJECT (button), "provider");
  if (provider == NULL)
    return;

  self->current_provider = provider;
  self->oauth_completed  = FALSE;

  gtk_stack_set_visible_child_name (self->main_stack, "loading");

  url = g_strdup_printf ("https://flathub.org/api/v2/auth/login/%s",
                         bz_flathub_auth_provider_get_method (provider));

  msg = soup_message_new ("GET", url);
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "accept", "application/json");

  soup_session_send_and_read_async (self->session,
                                    msg,
                                    G_PRIORITY_DEFAULT,
                                    NULL,
                                    on_login_response,
                                    self);
}

static void
on_providers_loaded (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  BzLoginPage *self             = BZ_LOGIN_PAGE (user_data);
  g_autoptr (GBytes) bytes      = NULL;
  g_autoptr (GError) error      = NULL;
  g_autoptr (JsonParser) parser = NULL;
  JsonNode  *root               = NULL;
  JsonArray *array              = NULL;
  guint      i                  = 0;

  bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object),
                                             res, &error);
  if (error != NULL)
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (
          self->start_status_page,
          g_strdup_printf ("Error loading providers: %s", error->message));
      return;
    }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   g_bytes_get_data (bytes, NULL),
                                   g_bytes_get_size (bytes),
                                   &error))
    {
      gtk_stack_set_visible_child_name (self->main_stack, "start");
      adw_status_page_set_description (
          self->start_status_page,
          g_strdup_printf ("Error parsing providers: %s", error->message));
      return;
    }

  root  = json_parser_get_root (parser);
  array = json_node_get_array (root);

  for (i = 0; i < json_array_get_length (array); i++)
    {
      JsonObject            *provider_obj = NULL;
      BzFlathubAuthProvider *provider     = NULL;
      const char            *method       = NULL;
      const char            *name         = NULL;
      GtkWidget             *row          = NULL;
      GtkWidget             *icon         = NULL;
      g_autofree char       *row_title    = NULL;

      provider_obj = json_array_get_object_element (array, i);
      method       = json_object_get_string_member (provider_obj, "method");
      name         = json_object_get_string_member (provider_obj, "name");

      provider = bz_flathub_auth_provider_new ();
      bz_flathub_auth_provider_set_name (provider, name);
      bz_flathub_auth_provider_set_method (provider, method);

      self->providers = g_list_append (self->providers, provider);

      row_title = g_strdup_printf ("%s", name);
      row       = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), row_title);

      icon = gtk_image_new_from_icon_name ("go-next-symbolic");
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), icon);

      g_object_set_data (G_OBJECT (row), "provider", provider);
      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW(row), true);
      g_signal_connect (row, "activated",
                        G_CALLBACK (on_provider_row_activated), self);

      adw_preferences_group_add (self->provider_preferences_group, row);
    }

  gtk_stack_set_visible_child_name (self->main_stack, "start");
}

static void
load_providers (BzLoginPage *self)
{
  SoupMessage *msg = NULL;

  gtk_stack_set_visible_child_name (self->main_stack, "loading");

  msg = soup_message_new ("GET", "https://flathub.org/api/v2/auth/login");
  soup_message_headers_append (soup_message_get_request_headers (msg),
                               "accept", "application/json");

  soup_session_send_and_read_async (self->session,
                                    msg,
                                    G_PRIORITY_DEFAULT,
                                    NULL,
                                    on_providers_loaded,
                                    self);
}

static void
on_close_clicked (GtkButton   *button,
                  BzLoginPage *self)
{
  g_signal_emit (self, signals[SIGNAL_LOGIN_COMPLETE], 0, self->session_cookie);
}

static void
bz_login_page_dispose (GObject *object)
{
  BzLoginPage *self = BZ_LOGIN_PAGE (object);

  g_clear_object (&self->session);
  g_clear_object (&self->cookie_jar);
  g_clear_pointer (&self->auth_redirect_url, g_free);
  g_clear_pointer (&self->session_cookie, g_free);

  if (self->providers != NULL)
    {
      g_list_free_full (self->providers, (GDestroyNotify) g_free);
      self->providers = NULL;
    }

  G_OBJECT_CLASS (bz_login_page_parent_class)->dispose (object);
}

static void
bz_login_page_class_init (BzLoginPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = bz_login_page_dispose;

  signals[SIGNAL_LOGIN_COMPLETE] =
      g_signal_new ("login-complete",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_STRING);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/io/github/kolunmi/Bazaar/bz-login-page.ui");

  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, main_stack);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, start_status_page);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, provider_preferences_group);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, browser_scroll);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, finish_avatar);
  gtk_widget_class_bind_template_child (widget_class, BzLoginPage, welcome_label);

  gtk_widget_class_bind_template_callback (widget_class, on_close_clicked);
}

static void
bz_login_page_init (BzLoginPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->session    = soup_session_new ();
  self->cookie_jar = soup_cookie_jar_new ();
  soup_cookie_jar_set_accept_policy (self->cookie_jar,
                                     SOUP_COOKIE_JAR_ACCEPT_ALWAYS);
  soup_session_add_feature (self->session, SOUP_SESSION_FEATURE (self->cookie_jar));

  self->webkit_loaded   = FALSE;
  self->oauth_completed = FALSE;

  load_providers (self);
}

AdwNavigationPage *
bz_login_page_new (void)
{
  return g_object_new (BZ_TYPE_LOGIN_PAGE, NULL);
}

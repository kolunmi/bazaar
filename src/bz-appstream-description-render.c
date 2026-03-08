/* bz-appstream-description-render.c
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

#define G_LOG_DOMAIN "BAZAAR::APPSTREAM-DESCRIPTION-RENDER"

#include "config.h"

#include <xmlb.h>

#include "bz-appstream-description-render.h"

enum
{
  NO_ELEMENT,
  PARAGRAPH,
  ORDERED_LIST,
  UNORDERED_LIST,
  LIST_ITEM,
  CODE,
  EMPHASIS,
};

struct _BzAppstreamDescriptionRender
{
  AdwBin parent_instance;

  char *appstream_description;

  /* Template widgets */
  GtkTextView *text_view;
};

G_DEFINE_FINAL_TYPE (BzAppstreamDescriptionRender, bz_appstream_description_render, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_APPSTREAM_DESCRIPTION,

  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { 0 };

static void
setup_text_tags (GtkTextBuffer *buffer);

static void
regenerate (BzAppstreamDescriptionRender *self);

static void
insert (BzAppstreamDescriptionRender *self,
        GtkTextBuffer                *buffer,
        GtkTextIter                  *iter,
        const char                   *text);

static void
compile (BzAppstreamDescriptionRender *self,
         XbNode                       *node,
         GtkTextBuffer                *buffer,
         GtkTextIter                  *iter,
         int                           parent_kind,
         int                           idx,
         gboolean                      is_last_sibling);

static char *
normalize_whitespace (const char *text);

static const char *
get_link_at_coords (GtkTextView *text_view,
                    double       x,
                    double       y);

static void
on_click_released (GtkGestureClick *gesture,
                   int              n_press,
                   double           x,
                   double           y,
                   GtkTextView     *text_view);

static void
on_motion (GtkEventControllerMotion *controller,
           double                    x,
           double                    y,
           GtkTextView              *text_view);

static void
bz_appstream_description_render_dispose (GObject *object)
{
  BzAppstreamDescriptionRender *self = BZ_APPSTREAM_DESCRIPTION_RENDER (object);

  g_clear_pointer (&self->appstream_description, g_free);

  G_OBJECT_CLASS (bz_appstream_description_render_parent_class)->dispose (object);
}

static void
bz_appstream_description_render_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  BzAppstreamDescriptionRender *self = BZ_APPSTREAM_DESCRIPTION_RENDER (object);

  switch (prop_id)
    {
    case PROP_APPSTREAM_DESCRIPTION:
      g_value_set_string (value, bz_appstream_description_render_get_appstream_description (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_appstream_description_render_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  BzAppstreamDescriptionRender *self = BZ_APPSTREAM_DESCRIPTION_RENDER (object);

  switch (prop_id)
    {
    case PROP_APPSTREAM_DESCRIPTION:
      bz_appstream_description_render_set_appstream_description (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bz_appstream_description_render_class_init (BzAppstreamDescriptionRenderClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bz_appstream_description_render_set_property;
  object_class->get_property = bz_appstream_description_render_get_property;
  object_class->dispose      = bz_appstream_description_render_dispose;

  props[PROP_APPSTREAM_DESCRIPTION] =
      g_param_spec_string (
          "appstream-description",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-appstream-description-render.ui");
  gtk_widget_class_bind_template_child (widget_class, BzAppstreamDescriptionRender, text_view);
}

static void
setup_text_tags (GtkTextBuffer *buffer)
{
  gtk_text_buffer_create_tag (buffer, "code",
                              "family", "monospace",
                              NULL);

  gtk_text_buffer_create_tag (buffer, "emphasis",
                              "weight", PANGO_WEIGHT_BOLD,
                              NULL);

  gtk_text_buffer_create_tag (buffer, "paragraph",
                              "pixels-below-lines", 12,
                              NULL);

  gtk_text_buffer_create_tag (buffer, "list-item-ul",
                              "left-margin", 10,
                              "pixels-below-lines", 4,
                              "indent", -12,
                              NULL);

  gtk_text_buffer_create_tag (buffer, "list-item-ol",
                              "left-margin", 10,
                              "pixels-below-lines", 4,
                              "indent", -18,
                              NULL);

  gtk_text_buffer_create_tag (buffer, "list-number",
                              "family", "monospace",
                              "foreground", "gray",
                              NULL);
}

static void
bz_appstream_description_render_init (BzAppstreamDescriptionRender *self)
{
  GtkTextBuffer      *buffer        = NULL;
  GtkGesture         *click         = NULL;
  GtkEventController *motion        = NULL;
  AdwStyleManager    *style_manager = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  buffer = gtk_text_view_get_buffer (self->text_view);
  setup_text_tags (buffer);
  gtk_widget_remove_css_class (GTK_WIDGET (self->text_view), "view");

  click = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect (click, "released", G_CALLBACK (on_click_released), self->text_view);
  gtk_widget_add_controller (GTK_WIDGET (self->text_view), GTK_EVENT_CONTROLLER (click));

  motion = gtk_event_controller_motion_new ();
  g_signal_connect (motion, "motion", G_CALLBACK (on_motion), self->text_view);
  gtk_widget_add_controller (GTK_WIDGET (self->text_view), motion);

  style_manager = adw_style_manager_get_default ();
  g_signal_connect_object (style_manager, "notify::accent-color",
                           G_CALLBACK (regenerate), self, G_CONNECT_SWAPPED);
}

static const char *
get_link_at_coords (GtkTextView *text_view,
                    double       x,
                    double       y)
{
  GtkTextIter iter        = { 0 };
  int         bx          = 0;
  int         by          = 0;
  g_autoptr (GSList) tags = NULL;
  GSList     *l           = NULL;
  const char *href        = NULL;

  gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_WIDGET,
                                         (int) x, (int) y, &bx, &by);
  gtk_text_view_get_iter_at_location (text_view, &iter, bx, by);
  tags = gtk_text_iter_get_tags (&iter);
  for (l = tags; l != NULL; l = l->next)
    {
      href = g_object_get_data (G_OBJECT (l->data), "href");
      if (href != NULL)
        return href;
    }
  return NULL;
}

static void
on_click_released (GtkGestureClick *gesture,
                   int              n_press,
                   double           x,
                   double           y,
                   GtkTextView     *text_view)
{
  const char *href                    = NULL;
  g_autoptr (GtkUriLauncher) launcher = NULL;
  GtkWindow *root                     = NULL;

  href = get_link_at_coords (text_view, x, y);
  if (href == NULL)
    return;

  launcher = gtk_uri_launcher_new (href);
  root     = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (text_view)));
  gtk_uri_launcher_launch (launcher, root, NULL, NULL, NULL);
}

static void
on_motion (GtkEventControllerMotion *controller,
           double                    x,
           double                    y,
           GtkTextView              *text_view)
{
  if (get_link_at_coords (text_view, x, y) != NULL)
    gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "pointer");
  else
    gtk_widget_set_cursor (GTK_WIDGET (text_view), NULL);
}

BzAppstreamDescriptionRender *
bz_appstream_description_render_new (void)
{
  return g_object_new (BZ_TYPE_APPSTREAM_DESCRIPTION_RENDER, NULL);
}

const char *
bz_appstream_description_render_get_appstream_description (BzAppstreamDescriptionRender *self)
{
  g_return_val_if_fail (BZ_IS_APPSTREAM_DESCRIPTION_RENDER (self), NULL);
  return self->appstream_description;
}

void
bz_appstream_description_render_set_appstream_description (BzAppstreamDescriptionRender *self,
                                                           const char                   *appstream_description)
{
  g_return_if_fail (BZ_IS_APPSTREAM_DESCRIPTION_RENDER (self));

  g_clear_pointer (&self->appstream_description, g_free);
  if (appstream_description != NULL)
    self->appstream_description = g_strdup (appstream_description);

  regenerate (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_APPSTREAM_DESCRIPTION]);
}

static void
regenerate (BzAppstreamDescriptionRender *self)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (XbSilo) silo        = NULL;
  g_autoptr (XbNode) root        = NULL;
  GtkTextBuffer *buffer          = NULL;
  GtkTextIter    iter            = { 0 };
  int            node_count      = 0;

  buffer = gtk_text_view_get_buffer (self->text_view);
  gtk_text_buffer_set_text (buffer, "", 0);

  if (self->appstream_description == NULL)
    return;

  silo = xb_silo_new_from_xml (self->appstream_description, &local_error);
  if (silo == NULL)
    {
      g_warning ("Failed to parse appstream description XML: %s", local_error->message);
      return;
    }

  gtk_text_buffer_get_end_iter (buffer, &iter);
  root = xb_silo_get_root (silo);

  for (XbNode *n = g_object_ref (root); n != NULL;)
    {
      XbNode *last = NULL;

      node_count++;

      last = n;
      n    = xb_node_get_next (n);
      g_object_unref (last);
    }

  for (int i = 0; root != NULL; i++)
    {
      g_autoptr (XbNode) next = NULL;
      gboolean is_last        = (i == node_count - 1);

      compile (self, root, buffer, &iter, NO_ELEMENT, i, is_last);

      next = xb_node_get_next (root);
      g_object_unref (root);
      root = g_steal_pointer (&next);
    }
}

static void
insert_url (BzAppstreamDescriptionRender *self,
            GtkTextBuffer                *buffer,
            GtkTextIter                  *iter,
            const char                   *url)
{
  g_autoptr (GdkRGBA) rgba = NULL;
  GtkTextTag  *tag         = NULL;
  GtkTextMark *mark        = NULL;
  GtkTextIter  start       = { 0 };

  rgba = adw_style_manager_get_accent_color_rgba (adw_style_manager_get_default ());
  tag  = gtk_text_buffer_create_tag (buffer, NULL,
                                     "foreground-rgba", rgba,
                                     "underline", PANGO_UNDERLINE_SINGLE,
                                     NULL);
  g_object_set_data_full (G_OBJECT (tag), "href", g_strdup (url), g_free);
  mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
  gtk_text_buffer_insert (buffer, iter, url, -1);
  gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
  gtk_text_buffer_apply_tag (buffer, tag, &start, iter);
  gtk_text_buffer_delete_mark (buffer, mark);
}

static void
insert (BzAppstreamDescriptionRender *self,
        GtkTextBuffer                *buffer,
        GtkTextIter                  *iter,
        const char                   *text)
{
  g_auto (GStrv) parts       = NULL;
  const char      *p         = NULL;
  const char      *http      = NULL;
  const char      *https     = NULL;
  const char      *url_start = NULL;
  const char      *url_end   = NULL;
  const char      *chunk     = NULL;
  GtkTextMark     *m         = NULL;
  GtkTextIter      si        = { 0 };
  g_autofree char *url       = NULL;
  int              j         = 0;

  parts = g_strsplit (text, "**", -1);
  for (j = 0; parts[j] != NULL; j++)
    {
      chunk = parts[j];
      if (j % 2 != 0)
        {
          m = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
          gtk_text_buffer_insert (buffer, iter, chunk, -1);
          gtk_text_buffer_get_iter_at_mark (buffer, &si, m);
          gtk_text_buffer_apply_tag_by_name (buffer, "emphasis", &si, iter);
          gtk_text_buffer_delete_mark (buffer, m);
          continue;
        }

      p = chunk;
      while (*p != '\0')
        {
          http  = strstr (p, "http://");
          https = strstr (p, "https://");

          url_start = (http != NULL && (https == NULL || http < https)) ? http : https;

          if (url_start == NULL)
            {
              gtk_text_buffer_insert (buffer, iter, p, -1);
              break;
            }

          if (url_start > p)
            gtk_text_buffer_insert (buffer, iter, p, (int) (url_start - p));

          url_end = url_start;
          while (*url_end != '\0' && !g_ascii_isspace (*url_end))
            url_end++;

          url = g_strndup (url_start, url_end - url_start);
          insert_url (self, buffer, iter, url);
          p = url_end;
        }
    }
}

static void
compile (BzAppstreamDescriptionRender *self,
         XbNode                       *node,
         GtkTextBuffer                *buffer,
         GtkTextIter                  *iter,
         int                           parent_kind,
         int                           idx,
         gboolean                      is_last_sibling)
{
  const char  *element     = NULL;
  const char  *text        = NULL;
  XbNode      *child       = NULL;
  int          kind        = NO_ELEMENT;
  GtkTextMark *start_mark  = NULL;
  int          child_count = 0;

  element    = xb_node_get_element (node);
  text       = xb_node_get_text (node);
  child      = xb_node_get_child (node);
  kind       = NO_ELEMENT;
  start_mark = NULL;

  if (element != NULL)
    {
      if (g_strcmp0 (element, "p") == 0)
        {
          kind       = PARAGRAPH;
          start_mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
        }
      else if (g_strcmp0 (element, "ol") == 0)
        kind = ORDERED_LIST;
      else if (g_strcmp0 (element, "ul") == 0)
        kind = UNORDERED_LIST;
      else if (g_strcmp0 (element, "li") == 0)
        {
          kind       = LIST_ITEM;
          start_mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);

          if (parent_kind == ORDERED_LIST)
            {
              g_autofree char *prefix = NULL;
              GtkTextMark     *prefix_start_mark;
              GtkTextIter      prefix_start_iter;

              prefix_start_mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
              prefix            = g_strdup_printf ("%d.", idx + 1);
              gtk_text_buffer_insert (buffer, iter, prefix, -1);

              gtk_text_buffer_get_iter_at_mark (buffer, &prefix_start_iter, prefix_start_mark);
              gtk_text_buffer_apply_tag_by_name (buffer, "list-number", &prefix_start_iter, iter);
              gtk_text_buffer_delete_mark (buffer, prefix_start_mark);
            }
          else
            gtk_text_buffer_insert (buffer, iter, "• ", -1);
        }
      else if (g_strcmp0 (element, "code") == 0)
        {
          kind       = CODE;
          start_mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
        }
      else if (g_strcmp0 (element, "em") == 0)
        {
          kind       = EMPHASIS;
          start_mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
        }
    }

  if (text != NULL)
    {
      g_autofree char *normalized = NULL;

      normalized = normalize_whitespace (text);
      if (normalized != NULL && *normalized != '\0')
        insert (self, buffer, iter, normalized);
    }

  for (int i = 0; child != NULL; i++)
    {
      const char *tail = NULL;
      XbNode     *next = NULL;

      next = xb_node_get_next (child);
      compile (self, child, buffer, iter, kind, i, next == NULL);

      tail = xb_node_get_tail (child);
      if (tail != NULL)
        {
          g_autofree char *normalized = NULL;

          normalized = normalize_whitespace (tail);
          if (normalized != NULL && *normalized != '\0')
            insert (self, buffer, iter, normalized);
        }

      g_object_unref (child);
      child = next;
      child_count++;
    }

  if (start_mark != NULL)
    {
      GtkTextIter start_iter = { 0 };

      gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, start_mark);

      if (kind == CODE)
        gtk_text_buffer_apply_tag_by_name (buffer, "code", &start_iter, iter);
      else if (kind == EMPHASIS)
        gtk_text_buffer_apply_tag_by_name (buffer, "emphasis", &start_iter, iter);
      else if (kind == PARAGRAPH)
        gtk_text_buffer_apply_tag_by_name (buffer, "paragraph", &start_iter, iter);
      else if (kind == LIST_ITEM)
        {
          const char *tag_name = NULL;

          tag_name = (parent_kind == ORDERED_LIST) ? "list-item-ol" : "list-item-ul";
          gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &start_iter, iter);
          gtk_text_buffer_insert (buffer, iter, "\n", 1);
        }

      gtk_text_buffer_delete_mark (buffer, start_mark);
    }

  if (kind == PARAGRAPH && !is_last_sibling)
    gtk_text_buffer_insert (buffer, iter, "\n", 1);
  else if ((kind == ORDERED_LIST || kind == UNORDERED_LIST) && !is_last_sibling && child_count > 0)
    gtk_text_buffer_insert (buffer, iter, "\n", 1);
}

static char *
normalize_whitespace (const char *text)
{
  g_autoptr (GString) result = NULL;
  gboolean in_space          = FALSE;
  gboolean at_start          = TRUE;

  if (text == NULL)
    return NULL;

  result = g_string_new (NULL);

  for (const char *p = text;
       p != NULL && *p != '\0';
       p = g_utf8_next_char (p))
    {
      gunichar ch = 0;

      ch = g_utf8_get_char (p);
      if (g_unichar_isspace (ch))
        {
          if (!at_start && !in_space)
            in_space = TRUE;
        }
      else
        {
          if (!at_start && in_space)
            g_string_append_c (result, ' ');
          g_string_append_unichar (result, ch);

          in_space = FALSE;
          at_start = FALSE;
        }
    }

  return g_string_free (g_steal_pointer (&result), FALSE);
}

/* End of bz-appstream-description-render.c */

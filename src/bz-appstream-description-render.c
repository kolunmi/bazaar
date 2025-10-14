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

  char    *appstream_description;
  gboolean selectable;

  GPtrArray *box_children;

  GRegex *split_regex;

  /* Template widgets */
  GtkBox *box;
};

G_DEFINE_FINAL_TYPE (BzAppstreamDescriptionRender, bz_appstream_description_render, ADW_TYPE_BIN);

enum
{
  PROP_0,

  PROP_APPSTREAM_DESCRIPTION,
  PROP_SELECTABLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
regenerate (BzAppstreamDescriptionRender *self);

static void
compile (BzAppstreamDescriptionRender *self,
         XbNode                       *node,
         GString                      *markup,
         int                           parent_kind,
         int                           idx,
         int                           depth);

static void
append_text (BzAppstreamDescriptionRender *self,
             GString                      *markup,
             const char                   *text,
             int                           kind,
             int                           parent_kind,
             int                           idx,
             int                           depth);

static void
bz_appstream_description_render_dispose (GObject *object)
{
  BzAppstreamDescriptionRender *self = BZ_APPSTREAM_DESCRIPTION_RENDER (object);

  g_clear_pointer (&self->appstream_description, g_free);

  g_clear_pointer (&self->box_children, g_ptr_array_unref);

  g_clear_pointer (&self->split_regex, g_regex_unref);

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
    case PROP_SELECTABLE:
      g_value_set_boolean (value, bz_appstream_description_render_get_selectable (self));
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
    case PROP_SELECTABLE:
      bz_appstream_description_render_set_selectable (self, g_value_get_boolean (value));
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

  props[PROP_SELECTABLE] =
      g_param_spec_boolean (
          "selectable",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/io/github/kolunmi/Bazaar/bz-appstream-description-render.ui");
  gtk_widget_class_bind_template_child (widget_class, BzAppstreamDescriptionRender, box);
}

static void
bz_appstream_description_render_init (BzAppstreamDescriptionRender *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->box_children = g_ptr_array_new ();

  self->split_regex = g_regex_new ("\\s+", G_REGEX_DEFAULT, G_REGEX_MATCH_DEFAULT, NULL);
  g_assert (self->split_regex);
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

gboolean
bz_appstream_description_render_get_selectable (BzAppstreamDescriptionRender *self)
{
  g_return_val_if_fail (BZ_IS_APPSTREAM_DESCRIPTION_RENDER (self), FALSE);
  return self->selectable;
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

void
bz_appstream_description_render_set_selectable (BzAppstreamDescriptionRender *self,
                                                gboolean                      selectable)
{
  g_return_if_fail (BZ_IS_APPSTREAM_DESCRIPTION_RENDER (self));

  self->selectable = selectable;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTABLE]);
}

static void
regenerate (BzAppstreamDescriptionRender *self)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (XbSilo) silo        = NULL;
  g_autoptr (XbNode) root        = NULL;

  for (guint i = 0; i < self->box_children->len; i++)
    {
      GtkWidget *child = NULL;

      child = g_ptr_array_index (self->box_children, i);
      gtk_box_remove (self->box, child);
    }
  g_ptr_array_set_size (self->box_children, 0);

  if (self->appstream_description == NULL)
    return;

  silo = xb_silo_new_from_xml (self->appstream_description, &local_error);
  if (silo == NULL)
    {
      g_warning ("Failed to parse appstream description XML: %s", local_error->message);
      return;
    }

  root = xb_silo_get_root (silo);
  for (int i = 0; root != NULL; i++)
    {
      const char *tail        = NULL;
      g_autoptr (XbNode) next = NULL;

      compile (self, root, NULL, NO_ELEMENT, i, 0);

      tail = xb_node_get_tail (root);
      if (tail != NULL)
        append_text (self, NULL, tail, NO_ELEMENT, NO_ELEMENT, 0, 0);

      next = xb_node_get_next (root);
      g_object_unref (root);
      root = g_steal_pointer (&next);
    }
}

static void
compile (BzAppstreamDescriptionRender *self,
         XbNode                       *node,
         GString                      *markup,
         int                           parent_kind,
         int                           idx,
         int                           depth)
{
  XbNode     *child              = NULL;
  const char *element            = NULL;
  const char *text               = NULL;
  int         kind               = NO_ELEMENT;
  g_autoptr (GString) new_markup = NULL;
  GString *cur_markup            = markup;

  child   = xb_node_get_child (node);
  element = xb_node_get_element (node);
  text    = xb_node_get_text (node);

  if (element != NULL)
    {
      if (g_strcmp0 (element, "p") == 0)
        {
          kind       = PARAGRAPH;
          cur_markup = new_markup = g_string_new (NULL);
        }
      else if (g_strcmp0 (element, "ol") == 0)
        kind = ORDERED_LIST;
      else if (g_strcmp0 (element, "ul") == 0)
        kind = UNORDERED_LIST;
      else if (g_strcmp0 (element, "li") == 0)
        {
          kind       = LIST_ITEM;
          cur_markup = new_markup = g_string_new (NULL);
        }
      else if (g_strcmp0 (element, "code") == 0)
        {
          kind = CODE;
          if (cur_markup != NULL)
            g_string_append (cur_markup, "<tt>");
        }
      else if (g_strcmp0 (element, "em") == 0)
        {
          kind = EMPHASIS;
          if (cur_markup != NULL)
            g_string_append (cur_markup, "<b>");
        }
    }

  if (text != NULL)
    append_text (self, cur_markup, text, kind, parent_kind, idx, depth);

  for (int i = 0; child != NULL; i++)
    {
      const char *tail = NULL;
      XbNode     *next = NULL;

      compile (self, child, cur_markup, kind, i, depth + 1);

      tail = xb_node_get_tail (child);
      if (tail != NULL)
        append_text (self, cur_markup, tail, kind, parent_kind, idx, depth);

      next = xb_node_get_next (child);
      g_object_unref (child);
      child = next;
    }

  if (cur_markup != NULL)
    {
      if (kind == EMPHASIS)
        g_string_append (cur_markup, "</b>");
      else if (kind == CODE)
        g_string_append (cur_markup, "</tt>");
    }

  if (new_markup != NULL)
    append_text (self, NULL, new_markup->str, kind, parent_kind, idx, depth);
}

static void
append_text (BzAppstreamDescriptionRender *self,
             GString                      *markup,
             const char                   *text,
             int                           kind,
             int                           parent_kind,
             int                           idx,
             int                           depth)
{
  if (markup != NULL)
    {
      g_autofree char *escaped = NULL;

      escaped = g_markup_escape_text (text, -1);
      g_string_append (markup, escaped);
    }
  else
    {
      g_auto (GStrv) tokens  = NULL;
      g_autofree char *fixed = NULL;
      GtkWidget       *child = NULL;

      tokens = g_regex_split (self->split_regex, text, G_REGEX_MATCH_DEFAULT);
      fixed  = g_strjoinv (" ", tokens);

      child = gtk_label_new (fixed);
      gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
      gtk_label_set_wrap (GTK_LABEL (child), TRUE);
      gtk_label_set_wrap_mode (GTK_LABEL (child), PANGO_WRAP_WORD_CHAR);
      gtk_label_set_xalign (GTK_LABEL (child), 0.0);

      if (kind == LIST_ITEM)
        {
          GtkWidget *box    = NULL;
          GtkWidget *prefix = NULL;

          box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
          if (parent_kind == ORDERED_LIST)
            {
              g_autofree char *prefix_text = NULL;

              prefix_text = g_strdup_printf ("%d", idx);
              prefix      = gtk_label_new (prefix_text);
              gtk_widget_add_css_class (prefix, "caption");
            }
          else
            prefix = gtk_image_new_from_icon_name ("circle-outline-thick-symbolic");

          gtk_box_append (GTK_BOX (box), prefix);
          gtk_box_append (GTK_BOX (box), child);
          gtk_box_append (self->box, box);
          g_ptr_array_add (self->box_children, box);

          child = box;
        }
      else
        {
          gtk_box_append (self->box, child);
          g_ptr_array_add (self->box_children, child);
        }

      gtk_widget_set_margin_start (child, 20 * depth);
    }
}

/* End of bz-appstream-description-render.c */

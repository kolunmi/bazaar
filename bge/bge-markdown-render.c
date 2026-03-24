/* bge-markdown-render.c
 *
 * Copyright 2025 Eva M
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

#define G_LOG_DOMAIN "BGE::MARKDOWN-RENDER"

#include <md4c.h>

#include "bge.h"
#include "util.h"

struct _BgeMarkdownRender
{
  GtkWidget parent_instance;

  char    *markdown;
  gboolean selectable;

  GtkTextView *text_view;
};

G_DEFINE_FINAL_TYPE (BgeMarkdownRender, bge_markdown_render, GTK_TYPE_WIDGET);

enum
{
  PROP_0,

  PROP_MARKDOWN,
  PROP_SELECTABLE,

  LAST_PROP
};
static GParamSpec *props[LAST_PROP] = { 0 };

static void
regenerate (BgeMarkdownRender *self);

BGE_DEFINE_DATA (
    tag,
    Tag,
    {
      GtkTextTag *tag;
      int         start;
      int         end;
    },
    BGE_RELEASE_DATA (tag, g_object_unref));

#define INIT_TAG_DATA_LOCATION(_loc, _buffer, ...)                        \
  G_STMT_START                                                            \
  {                                                                       \
    *(_loc) = tag_data_new ();                                            \
    g_object_get ((_buffer), "cursor-position", &(*(_loc))->start, NULL); \
    (*(_loc))->end = -1;                                                  \
    (*(_loc))->tag = gtk_text_buffer_create_tag (                         \
        (_buffer), NULL,                                                  \
        "accumulative-margin", TRUE,                                      \
        ##__VA_ARGS__, NULL);                                             \
  }                                                                       \
  G_STMT_END

typedef struct
{
  GtkTextBuffer *buffer;
  char          *beginning;
  GArray        *block_stack;
  GPtrArray     *tags;
  int            indent;
  int            list_index;
  MD_CHAR        list_prefix;
} ParseCtx;

static int
enter_block (MD_BLOCKTYPE type,
             void        *detail,
             void        *user_data);

static int
leave_block (MD_BLOCKTYPE type,
             void        *detail,
             void        *user_data);

static int
enter_span (MD_SPANTYPE type,
            void       *detail,
            void       *user_data);

static int
leave_span (MD_SPANTYPE type,
            void       *detail,
            void       *user_data);

static int
text (MD_TEXTTYPE    type,
      const MD_CHAR *buf,
      MD_SIZE        size,
      void          *user_data);

static const MD_PARSER parser = {
  .flags       = MD_FLAG_COLLAPSEWHITESPACE |
                 MD_FLAG_NOHTMLBLOCKS |
                 MD_FLAG_NOHTMLSPANS,
  .enter_block = enter_block,
  .leave_block = leave_block,
  .enter_span  = enter_span,
  .leave_span  = leave_span,
  .text        = text,
};

static void
bge_markdown_render_dispose (GObject *object)
{
  BgeMarkdownRender *self = BGE_MARKDOWN_RENDER (object);

  g_clear_pointer (&self->markdown, g_free);

  gtk_widget_unparent (GTK_WIDGET (self->text_view));

  G_OBJECT_CLASS (bge_markdown_render_parent_class)->dispose (object);
}

static void
bge_markdown_render_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BgeMarkdownRender *self = BGE_MARKDOWN_RENDER (object);

  switch (prop_id)
    {
    case PROP_MARKDOWN:
      g_value_set_string (value, bge_markdown_render_get_markdown (self));
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, bge_markdown_render_get_selectable (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_markdown_render_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BgeMarkdownRender *self = BGE_MARKDOWN_RENDER (object);

  switch (prop_id)
    {
    case PROP_MARKDOWN:
      bge_markdown_render_set_markdown (self, g_value_get_string (value));
      break;
    case PROP_SELECTABLE:
      bge_markdown_render_set_selectable (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bge_markdown_render_measure (GtkWidget     *widget,
                             GtkOrientation orientation,
                             int            for_size,
                             int           *minimum,
                             int           *natural,
                             int           *minimum_baseline,
                             int           *natural_baseline)
{
  BgeMarkdownRender *self = BGE_MARKDOWN_RENDER (widget);

  gtk_widget_measure (
      GTK_WIDGET (self->text_view),
      orientation, for_size,
      minimum, natural,
      minimum_baseline, natural_baseline);
}

static void
bge_markdown_render_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  BgeMarkdownRender *self = BGE_MARKDOWN_RENDER (widget);

  gtk_widget_allocate (GTK_WIDGET (self->text_view), width, height, baseline, NULL);
}

static void
bge_markdown_render_class_init (BgeMarkdownRenderClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = bge_markdown_render_set_property;
  object_class->get_property = bge_markdown_render_get_property;
  object_class->dispose      = bge_markdown_render_dispose;

  props[PROP_MARKDOWN] =
      g_param_spec_string (
          "markdown",
          NULL, NULL, NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SELECTABLE] =
      g_param_spec_boolean (
          "selectable",
          NULL, NULL, FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  widget_class->measure       = bge_markdown_render_measure;
  widget_class->size_allocate = bge_markdown_render_size_allocate;
}

static void
bge_markdown_render_init (BgeMarkdownRender *self)
{
  self->text_view = (GtkTextView *) gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (self->text_view, GTK_WRAP_WORD_CHAR);
  gtk_widget_set_parent (GTK_WIDGET (self->text_view), GTK_WIDGET (self));
}

GtkWidget *
bge_markdown_render_new (void)
{
  return g_object_new (BGE_TYPE_MARKDOWN_RENDER, NULL);
}

const char *
bge_markdown_render_get_markdown (BgeMarkdownRender *self)
{
  g_return_val_if_fail (BGE_IS_MARKDOWN_RENDER (self), NULL);
  return self->markdown;
}

gboolean
bge_markdown_render_get_selectable (BgeMarkdownRender *self)
{
  g_return_val_if_fail (BGE_IS_MARKDOWN_RENDER (self), FALSE);
  return self->selectable;
}

void
bge_markdown_render_set_markdown (BgeMarkdownRender *self,
                                  const char        *markdown)
{
  g_return_if_fail (BGE_IS_MARKDOWN_RENDER (self));

  g_clear_pointer (&self->markdown, g_free);
  if (markdown != NULL)
    self->markdown = g_strdup (markdown);

  regenerate (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MARKDOWN]);
}

void
bge_markdown_render_set_selectable (BgeMarkdownRender *self,
                                    gboolean           selectable)
{
  g_return_if_fail (BGE_IS_MARKDOWN_RENDER (self));

  self->selectable = selectable;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTABLE]);
}

static void
regenerate (BgeMarkdownRender *self)
{
  int      iresult = 0;
  ParseCtx ctx     = { 0 };

  if (self->markdown == NULL)
    return;

  ctx.buffer      = gtk_text_buffer_new (NULL);
  ctx.beginning   = self->markdown;
  ctx.block_stack = g_array_new (FALSE, TRUE, sizeof (int));
  ctx.tags        = g_ptr_array_new_with_free_func (tag_data_unref);
  ctx.indent      = 0;
  ctx.list_index  = 0;
  ctx.list_prefix = '\0';

  iresult = md_parse (
      self->markdown,
      strlen (self->markdown),
      &parser,
      &ctx);
  if (iresult != 0)
    goto error;

  for (guint i = 0; i < ctx.tags->len; i++)
    {
      TagData    *data       = NULL;
      GtkTextIter start_iter = { 0 };
      GtkTextIter end_iter   = { 0 };

      data = g_ptr_array_index (ctx.tags, i);

      gtk_text_buffer_get_iter_at_offset (ctx.buffer, &start_iter, data->start);
      gtk_text_buffer_get_iter_at_offset (ctx.buffer, &end_iter, data->end);
      gtk_text_buffer_apply_tag (ctx.buffer, g_object_ref (data->tag), &start_iter, &end_iter);
    }
  gtk_text_view_set_buffer (self->text_view, ctx.buffer);
  goto done;

error:
  g_warning ("Failed to parse markdown text");

done:
  g_object_unref (ctx.buffer);
  g_ptr_array_unref (ctx.tags);
  g_array_unref (ctx.block_stack);
}

static int
enter_block (MD_BLOCKTYPE type,
             void        *detail,
             void        *user_data)
{
  ParseCtx *ctx           = user_data;
  int       list_parent   = 0;
  g_autoptr (TagData) tag = NULL;

  if (ctx->block_stack->len > 1)
    list_parent = g_array_index (ctx->block_stack, int, ctx->block_stack->len - 2);

  switch (type)
    {
    case MD_BLOCK_DOC:
      break;
    case MD_BLOCK_QUOTE:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer,
                              "indent", 25,
                              "paragraph-background", "darkgray");
      break;
    case MD_BLOCK_UL:
      {
        MD_BLOCK_UL_DETAIL *ul_detail = detail;

        ctx->indent++;
        ctx->list_index  = 0;
        ctx->list_prefix = ul_detail->mark;

        INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "indent", 25);
      }
      break;
    case MD_BLOCK_OL:
      {
        MD_BLOCK_OL_DETAIL *ol_detail = detail;

        ctx->indent++;
        ctx->list_index  = 0;
        ctx->list_prefix = ol_detail->mark_delimiter;

        INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "indent", 25);
      }
      break;
    case MD_BLOCK_LI:
      {
        if (list_parent == MD_BLOCK_OL)
          {
            g_autofree char *prefix_text = NULL;

            prefix_text = g_strdup_printf ("%d%c ", ctx->list_index, ctx->list_prefix);
            gtk_text_buffer_insert_at_cursor (ctx->buffer, prefix_text, -1);
          }
        else
          /* TODO:

            `ctx->list_prefix` is '-', '+', '*'

            maybe handle these?
           */
          gtk_text_buffer_insert_at_cursor (ctx->buffer, "• ", -1);
      }
      break;
    case MD_BLOCK_HR:
      gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n", -1);
      break;
    case MD_BLOCK_H:
      {
        MD_BLOCK_H_DETAIL *h_detail = detail;

        INIT_TAG_DATA_LOCATION (
            &tag, ctx->buffer,
            "size-points", 22.0f - (float) h_detail->level);
      }
      break;
    case MD_BLOCK_CODE:
      INIT_TAG_DATA_LOCATION (
          &tag, ctx->buffer,
          "family", "monospace",
          "paragraph-background", "darkgray");
      break;
    case MD_BLOCK_P:
    case MD_BLOCK_HTML:
    case MD_BLOCK_TABLE:
    case MD_BLOCK_THEAD:
    case MD_BLOCK_TBODY:
    case MD_BLOCK_TR:
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
    default:
      break;
    }

  if (tag != NULL)
    g_ptr_array_add (ctx->tags, g_steal_pointer (&tag));
  g_array_append_val (ctx->block_stack, type);

  return 0;
}

static int
leave_block (MD_BLOCKTYPE type,
             void        *detail,
             void        *user_data)
{
  ParseCtx *ctx = user_data;

#define TERMINATE_TAG_FROM_SET_PROP(_set_prop)           \
  for (guint i = ctx->tags->len; i > 0; i--)             \
    {                                                    \
      TagData *data = NULL;                              \
      gboolean set  = FALSE;                             \
                                                         \
      data = g_ptr_array_index (ctx->tags, i - 1);       \
      if (data->end >= 0)                                \
        continue;                                        \
                                                         \
      g_object_get (data->tag, (_set_prop), &set, NULL); \
      if (set)                                           \
        {                                                \
          g_object_get (                                 \
              ctx->buffer,                               \
              "cursor-position", &data->end,             \
              NULL);                                     \
          break;                                         \
        }                                                \
    }

  g_assert (ctx->block_stack->len > 0);
  if (g_array_index (ctx->block_stack, int, ctx->block_stack->len - 1) >= 0)
    {
      switch (type)
        {
        case MD_BLOCK_DOC:
          break;
        case MD_BLOCK_QUOTE:
          TERMINATE_TAG_FROM_SET_PROP ("paragraph-background-set");
          gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n", -1);
          break;
        case MD_BLOCK_UL:
          {
            // MD_BLOCK_UL_DETAIL *ul_detail = detail;

            TERMINATE_TAG_FROM_SET_PROP ("indent-set");
            ctx->indent--;
          }
          break;
        case MD_BLOCK_OL:
          {
            // MD_BLOCK_OL_DETAIL *ol_detail = detail;

            TERMINATE_TAG_FROM_SET_PROP ("indent-set");
            ctx->indent--;
          }
          break;
        case MD_BLOCK_LI:
          ctx->list_index++;
          break;
        case MD_BLOCK_HR:
          break;
        case MD_BLOCK_H:
          TERMINATE_TAG_FROM_SET_PROP ("size-set");
          gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n", -1);
          break;
        case MD_BLOCK_CODE:
          TERMINATE_TAG_FROM_SET_PROP ("family");
          gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n", -1);
          break;
        case MD_BLOCK_P:
          gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n\n", -1);
          break;
        case MD_BLOCK_HTML:
        case MD_BLOCK_TABLE:
        case MD_BLOCK_THEAD:
        case MD_BLOCK_TBODY:
        case MD_BLOCK_TR:
        case MD_BLOCK_TH:
        case MD_BLOCK_TD:
        default:
          g_warning ("Unsupported markdown event (Did you use html/tables?)");
          return 1;
        }
    }
  g_array_set_size (ctx->block_stack, ctx->block_stack->len - 1);

  return 0;
}

static int
enter_span (MD_SPANTYPE type,
            void       *detail,
            void       *user_data)
{
  ParseCtx *ctx           = user_data;
  g_autoptr (TagData) tag = NULL;

  switch (type)
    {
    case MD_SPAN_EM:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "weight", 600);
      break;
    case MD_SPAN_STRONG:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "weight", 800);
      break;
    case MD_SPAN_A:
      {
        MD_SPAN_A_DETAIL *a_detail = detail;
        g_autofree char  *href     = NULL;
        g_autofree char  *title    = NULL;
        g_autofree char  *xml      = NULL;

        href = g_strndup (a_detail->href.text, a_detail->href.size);
        if (a_detail->title.text != NULL)
          title = g_strndup (a_detail->title.text, a_detail->title.size);

        xml = g_strdup_printf (
            "<a href=\"%s\" title=\"%s\">",
            href,
            title != NULL ? title : href);
        gtk_text_buffer_insert_at_cursor (ctx->buffer, xml, -1);
      }
      break;
    case MD_SPAN_IMG:
      g_warning ("Images aren't implemented yet!");
      break;
    case MD_SPAN_CODE:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "family", "monospace");
      break;
    case MD_SPAN_DEL:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "strikethrough", TRUE);
      break;
    case MD_SPAN_U:
      INIT_TAG_DATA_LOCATION (&tag, ctx->buffer, "underline", TRUE);
      break;
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
    case MD_SPAN_WIKILINK:
    default:
      g_warning ("Unsupported markdown event (Did you use latex/wikilinks?)");
      return 1;
      break;
    }

  if (tag != NULL)
    g_ptr_array_add (ctx->tags, g_steal_pointer (&tag));

  return 0;
}

static int
leave_span (MD_SPANTYPE type,
            void       *detail,
            void       *user_data)
{
  ParseCtx *ctx = user_data;

  switch (type)
    {
    case MD_SPAN_EM:
      TERMINATE_TAG_FROM_SET_PROP ("weight-set");
      break;
    case MD_SPAN_STRONG:
      TERMINATE_TAG_FROM_SET_PROP ("weight-set");
      break;
    case MD_SPAN_A:
      break;
    case MD_SPAN_IMG:
      // g_warning ("Images aren't implemented yet!");
      break;
    case MD_SPAN_CODE:
      TERMINATE_TAG_FROM_SET_PROP ("family-set");
      break;
    case MD_SPAN_DEL:
      TERMINATE_TAG_FROM_SET_PROP ("strikethrough-set");
      break;
    case MD_SPAN_U:
      TERMINATE_TAG_FROM_SET_PROP ("underline-set");
      break;
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
    case MD_SPAN_WIKILINK:
    default:
      g_warning ("Unsupported markdown event (Did you use latex/wikilinks?)");
      return 1;
      break;
    }

  return 0;
}

static int
text (MD_TEXTTYPE    type,
      const MD_CHAR *buf,
      MD_SIZE        size,
      void          *user_data)
{
  ParseCtx        *ctx     = user_data;
  g_autofree char *escaped = NULL;

  if (type == MD_TEXT_SOFTBR)
    /* ctx->markup->len > 0) */
    gtk_text_buffer_insert_at_cursor (ctx->buffer, " ", -1);
  else if (type == MD_TEXT_BR)
    /* ctx->markup->len > 0) */
    gtk_text_buffer_insert_at_cursor (ctx->buffer, "\n", -1);
  else
    gtk_text_buffer_insert_at_cursor (ctx->buffer, buf, size);

  return 0;
}

/* End of bge-markdown-render.c */

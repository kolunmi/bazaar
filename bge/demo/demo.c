/* demo.c
 *
 * Copyright 2026 Eva M
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

#include <bge.h>

static void
on_activate (GtkApplication *app);

int
main (int argc, char **argv)
{
  g_autoptr (GtkApplication) app      = NULL;
  g_autoptr (GtkCssProvider) provider = NULL;

  bge_init ();

  app = gtk_application_new (
      "io.github.kolunmi.BgeDemo",
      G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/io/github/kolunmi/BgeDemo/style.css");
  gtk_style_context_add_provider_for_display (
      gdk_display_get_default (),
      GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  return g_application_run (G_APPLICATION (app), argc, argv);
}

static void
on_activate (GtkApplication *app)
{
  GtkWidget       *window           = NULL;
  GtkWidget       *root             = NULL;
  BgeWdgtRenderer *renderer         = NULL;
  g_autoptr (GtkBuilder) builder    = NULL;
  g_autoptr (GtkBuilderScope) scope = NULL;

  window = gtk_application_window_new (app);

  scope = gtk_builder_cscope_new ();

  builder = gtk_builder_new ();
  gtk_builder_set_scope (builder, scope);
  gtk_builder_add_from_resource (builder, "/io/github/kolunmi/BgeDemo/window.ui", NULL);
  root     = GTK_WIDGET (gtk_builder_get_object (builder, "root"));
  renderer = BGE_WDGT_RENDERER (gtk_builder_get_object (builder, "wdgt"));

  {
    g_autoptr (GError) local_error = NULL;
    g_autoptr (BgeWdgtSpec) spec   = NULL;

    g_type_ensure (GTK_TYPE_LABEL);
    spec = bge_wdgt_spec_new_for_resource ("/io/github/kolunmi/BgeDemo/test.wdgt", &local_error);
    if (spec == NULL)
      g_print ("Error!! %s\n", local_error->message);

    bge_wdgt_renderer_set_spec (renderer, spec);
    bge_wdgt_renderer_set_state (renderer, "default");
  }

  gtk_window_set_child (GTK_WINDOW (window), g_object_ref_sink (root));
  gtk_window_present (GTK_WINDOW (window));
}

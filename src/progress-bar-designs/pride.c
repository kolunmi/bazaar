/* progress-bar-designs/pride.c
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

#include "config.h"

#include "common.h"

static void
append_striped_flag (GtkSnapshot     *snapshot,
                     const GdkRGBA    colors[],
                     const float      offsets[],
                     const float      sizes[],
                     guint            n_stripes,
                     graphene_rect_t *rect);

void
append_pride_flag (GtkSnapshot     *snapshot,
                   graphene_rect_t *bounds,
                   const char      *name)
{
  if (g_strcmp0 (name, "pride-rainbow-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 228.0 / 255.0,   3.0 / 255.0,   3.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 140.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 237.0 / 255.0,   0.0 / 255.0, 1.0 },
        {   0.0 / 255.0, 128.0 / 255.0,  38.0 / 255.0, 1.0 },
        {   0.0 / 255.0,  76.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 115.0 / 255.0,  41.0 / 255.0, 130.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 6.0,
        1.0 / 6.0,
        2.0 / 6.0,
        3.0 / 6.0,
        4.0 / 6.0,
        5.0 / 6.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 6.0,
        1.0 / 6.0,
        1.0 / 6.0,
        1.0 / 6.0,
        1.0 / 6.0,
        1.0 / 6.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "lesbian-pride-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 213.0 / 255.0,  45.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 239.0 / 255.0, 118.0 / 255.0,  39.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 154.0 / 255.0,  86.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 209.0 / 255.0,  98.0 / 255.0, 164.0 / 255.0, 1.0 },
        { 181.0 / 255.0,  86.0 / 255.0, 144.0 / 255.0, 1.0 },
        { 163.0 / 255.0,   2.0 / 255.0,  98.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 7.0,
        1.0 / 7.0,
        2.0 / 7.0,
        3.0 / 7.0,
        4.0 / 7.0,
        5.0 / 7.0,
        6.0 / 7.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "gay-pride-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {   1.0 / 255.0, 142.0 / 255.0, 113.0 / 255.0, 1.0 },
        {  33.0 / 255.0, 207.0 / 172.0, 199.0 / 255.0, 1.0 },
        { 154.0 / 255.0, 233.0 / 255.0, 195.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 124.0 / 255.0, 175.0 / 255.0, 228.0 / 255.0, 1.0 },
        {  79.0 / 255.0,  71.0 / 255.0, 204.0 / 255.0, 1.0 },
        {  60.0 / 255.0,  19.0 / 255.0, 121.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 7.0,
        1.0 / 7.0,
        2.0 / 7.0,
        3.0 / 7.0,
        4.0 / 7.0,
        5.0 / 7.0,
        6.0 / 7.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "transgender-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {  91.0 / 255.0, 206.0 / 255.0, 250.0 / 255.0, 1.0 },
        { 245.0 / 255.0, 169.0 / 255.0, 184.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        5.0 / 5.0,
        3.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "nonbinary-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 252.0 / 255.0, 244.0 / 255.0,  52.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 156.0 / 255.0,  89.0 / 255.0, 209.0 / 255.0, 1.0 },
        {  44.0 / 255.0,  44.0 / 255.0,  44.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 4.0,
        1.0 / 4.0,
        2.0 / 4.0,
        3.0 / 4.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 4.0,
        1.0 / 4.0,
        1.0 / 4.0,
        1.0 / 4.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "bisexual-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 214.0 / 255.0,  2.0 / 255.0, 112.0 / 255.0, 1.0 },
        { 155.0 / 255.0, 79.0 / 255.0, 150.0 / 255.0, 1.0 },
        {   0.0 / 255.0, 56.0 / 255.0, 168.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        2.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "asexual-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {   0.0 / 255.0,   0.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 163.0 / 255.0, 163.0 / 255.0, 163.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 128.0 / 255.0,   0.0 / 255.0, 128.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 4.0,
        1.0 / 4.0,
        2.0 / 4.0,
        3.0 / 4.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 4.0,
        1.0 / 4.0,
        1.0 / 4.0,
        1.0 / 4.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "pansexual-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 255.0 / 255.0,  33.0 / 255.0, 140.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 216.0 / 255.0,   0.0 / 255.0, 1.0 },
        {  33.0 / 255.0, 177.0 / 255.0, 255.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 3.0,
        1.0 / 3.0,
        2.0 / 3.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 3.0,
        1.0 / 3.0,
        1.0 / 3.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "aromantic-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {  61.0 / 255.0, 165.0 / 255.0,  66.0 / 255.0, 1.0 },
        { 167.0 / 255.0, 211.0 / 255.0, 121.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 169.0 / 255.0, 169.0 / 255.0, 169.0 / 255.0, 1.0 },
        {   0.0 / 255.0,   0.0 / 255.0,   0.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
        4.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "genderfluid-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 255.0 / 255.0, 118.0 / 255.0, 164.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 192.0 / 255.0,  17.0 / 255.0, 215.0 / 255.0, 1.0 },
        {   0.0 / 255.0,   0.0 / 255.0,   0.0 / 255.0, 1.0 },
        {  47.0 / 255.0,  60.0 / 255.0, 190.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
        4.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "polysexual-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 247.0 / 255.0,  20.0 / 255.0, 186.0 / 255.0, 1.0 },
        {   1.0 / 255.0, 214.0 / 255.0, 106.0 / 255.0, 1.0 },
        {  21.0 / 255.0, 148.0 / 255.0, 246.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 3.0,
        1.0 / 3.0,
        2.0 / 3.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 3.0,
        1.0 / 3.0,
        1.0 / 3.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "omnisexual-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 254.0 / 255.0, 154.0 / 255.0, 206.0 / 255.0, 1.0 },
        { 255.0 / 255.0,  83.0 / 255.0, 191.0 / 255.0, 1.0 },
        {  32.0 / 255.0,   0.0 / 255.0,  68.0 / 255.0, 1.0 },
        { 103.0 / 255.0,  96.0 / 255.0, 254.0 / 255.0, 1.0 },
        { 142.0 / 255.0, 166.0 / 255.0, 255.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
        4.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "aroace-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 226.0 / 255.0, 140.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 236.0 / 255.0, 205.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        {  98.0 / 255.0, 174.0 / 255.0, 220.0 / 255.0, 1.0 },
        {  32.0 / 255.0,  56.0 / 255.0,  86.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
        4.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "agender-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {   0.0 / 255.0,   0.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 188.0 / 255.0, 196.0 / 255.0, 199.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 183.0 / 255.0, 246.0 / 255.0, 132.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 188.0 / 255.0, 196.0 / 255.0, 199.0 / 255.0, 1.0 },
        {   0.0 / 255.0,   0.0 / 255.0,   0.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 7.0,
        1.0 / 7.0,
        2.0 / 7.0,
        3.0 / 7.0,
        4.0 / 7.0,
        5.0 / 7.0,
        6.0 / 7.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "genderqueer-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 181.0 / 255.0, 126.0 / 255.0, 220.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        {  74.0 / 255.0, 129.0 / 255.0,  35.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 3.0,
        1.0 / 3.0,
        2.0 / 3.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 3.0,
        1.0 / 3.0,
        1.0 / 3.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "intersex-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 255.0 / 255.0, 216.0 / 255.0,   0.0 / 255.0, 1.0 },
        { 121.0 / 255.0,   2.0 / 255.0, 170.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 216.0 / 255.0,   0.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        2.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "demigender-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 127.0 / 255.0, 127.0 / 255.0, 127.0 / 255.0, 1.0 },
        { 195.0 / 255.0, 195.0 / 255.0, 195.0 / 255.0, 1.0 },
        { 251.0 / 255.0, 255.0 / 255.0, 116.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 251.0 / 255.0, 255.0 / 255.0, 116.0 / 255.0, 1.0 },
        { 195.0 / 255.0, 195.0 / 255.0, 195.0 / 255.0, 1.0 },
        { 127.0 / 255.0, 127.0 / 255.0, 127.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 7.0,
        1.0 / 7.0,
        2.0 / 7.0,
        3.0 / 7.0,
        4.0 / 7.0,
        5.0 / 7.0,
        6.0 / 7.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "biromantic-flag") == 0)
    {
      const GdkRGBA colors[] = {
        { 136.0 / 255.0, 105.0 / 255.0, 165.0 / 255.0, 1.0 },
        { 216.0 / 255.0, 167.0 / 255.0, 216.0 / 255.0, 1.0 },
        { 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 1.0 },
        { 253.0 / 255.0, 177.0 / 255.0, 141.0 / 255.0, 1.0 },
        {  21.0 / 255.0,  22.0 / 255.0,  56.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 5.0,
        1.0 / 5.0,
        2.0 / 5.0,
        3.0 / 5.0,
        4.0 / 5.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
        1.0 / 5.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else if (g_strcmp0 (name, "disability-flag") == 0)
    {
      const GdkRGBA colors[] = {
        {  89.0 / 255.0,  89.0 / 255.0,  89.0 / 255.0, 1.0 },
        { 207.0 / 255.0, 114.0 / 255.0, 128.0 / 255.0, 1.0 },
        { 238.0 / 255.0, 222.0 / 255.0, 119.0 / 255.0, 1.0 },
        { 232.0 / 255.0, 232.0 / 255.0, 232.0 / 255.0, 1.0 },
        { 123.0 / 255.0, 194.0 / 255.0, 224.0 / 255.0, 1.0 },
        {  59.0 / 255.0, 176.0 / 255.0, 125.0 / 255.0, 1.0 },
        {  89.0 / 255.0,  89.0 / 255.0,  89.0 / 255.0, 1.0 },
      };
      const float offsets[G_N_ELEMENTS (colors)] = {
        0.0 / 7.0,
        1.0 / 7.0,
        2.0 / 7.0,
        3.0 / 7.0,
        4.0 / 7.0,
        5.0 / 7.0,
        6.0 / 7.0,
      };
      const float sizes[G_N_ELEMENTS (colors)] = {
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
        1.0 / 7.0,
      };
      append_striped_flag (snapshot, colors, offsets, sizes, G_N_ELEMENTS (colors), bounds);
    }
  else
    g_warning ("Invalid pride flag id \"%s\"", name);
}

static void
append_striped_flag (GtkSnapshot     *snapshot,
                     const GdkRGBA    colors[],
                     const float      offsets[],
                     const float      sizes[],
                     guint            n_stripes,
                     graphene_rect_t *rect)
{
  for (guint i = 0; i < n_stripes; i++)
    {
      graphene_rect_t stripe_rect = { 0 };

      stripe_rect = *rect;
      stripe_rect.origin.y += stripe_rect.size.height * offsets[i];
      stripe_rect.size.height *= sizes[i];

      gtk_snapshot_append_color (snapshot, colors + i, &stripe_rect);
    }
}

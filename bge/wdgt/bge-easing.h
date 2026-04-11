/* TAKEN FROM LIBADWAITA */

/*
 * Copyright (C) 2021 Manuel Genovés <manuel.genoves@gmail.com>
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#ifndef BGE_INSIDE
#error "Only <bge.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef enum
{
  BGE_LINEAR,
  BGE_EASE_IN_QUAD,
  BGE_EASE_OUT_QUAD,
  BGE_EASE_IN_OUT_QUAD,
  BGE_EASE_IN_CUBIC,
  BGE_EASE_OUT_CUBIC,
  BGE_EASE_IN_OUT_CUBIC,
  BGE_EASE_IN_QUART,
  BGE_EASE_OUT_QUART,
  BGE_EASE_IN_OUT_QUART,
  BGE_EASE_IN_QUINT,
  BGE_EASE_OUT_QUINT,
  BGE_EASE_IN_OUT_QUINT,
  BGE_EASE_IN_SINE,
  BGE_EASE_OUT_SINE,
  BGE_EASE_IN_OUT_SINE,
  BGE_EASE_IN_EXPO,
  BGE_EASE_OUT_EXPO,
  BGE_EASE_IN_OUT_EXPO,
  BGE_EASE_IN_CIRC,
  BGE_EASE_OUT_CIRC,
  BGE_EASE_IN_OUT_CIRC,
  BGE_EASE_IN_ELASTIC,
  BGE_EASE_OUT_ELASTIC,
  BGE_EASE_IN_OUT_ELASTIC,
  BGE_EASE_IN_BACK,
  BGE_EASE_OUT_BACK,
  BGE_EASE_IN_OUT_BACK,
  BGE_EASE_IN_BOUNCE,
  BGE_EASE_OUT_BOUNCE,
  BGE_EASE_IN_OUT_BOUNCE,
  BGE_EASE,
  BGE_EASE_IN,
  BGE_EASE_OUT,
  BGE_EASE_IN_OUT
} BgeEasing;
GType bge_easing_get_type (void);
#define BGE_TYPE_EASING (bge_easing_get_type ())

BGE_AVAILABLE_IN_ALL
double bge_easing_ease (BgeEasing self,
                        double    value);

G_END_DECLS

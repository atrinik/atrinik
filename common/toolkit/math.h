/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Toolkit math API header file.
 *
 * @author Zoey Rose
 */

#ifndef TOOLKIT_MATH_H
#define TOOLKIT_MATH_H

#include "toolkit.h"

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(math);

/** State for a deterministic, non-cryptographic random number stream. */
typedef struct rng_state {
    uint64_t state;
} rng_state_t;

void rng_seed(rng_state_t *rng, uint64_t seed);
uint64_t rng_u64(rng_state_t *rng);
int rng_range(rng_state_t *rng, int min, int max);
int rng_chance(rng_state_t *rng, uint32_t n);
double rng_real(rng_state_t *rng);
unsigned long isqrt(unsigned long n);
int rndm(int min, int max);
int rndm_chance(uint32_t n);
uint64_t rndm_u64(void);
double rndm_real(void);
void *sort_linked_list(void *p,
                       unsigned index,
                       int (*compare)(void *, void *, void *),
                       void *pointer,
                       unsigned long *pcount,
                       void *end_marker);
size_t nearest_pow_two_exp(size_t n);
bool math_point_in_ellipse(int x, int y, double cx, double cy, int dx, int dy, double angle);
bool math_point_edge_ellipse(int x,
                             int y,
                             double cx,
                             double cy,
                             int dx,
                             int dy,
                             double angle,
                             int *deg);
bool math_base64_decode(const char *str, unsigned char **buf, size_t *buf_len);

#endif

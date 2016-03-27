/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Experience related header file.
 *
 * @author Alex Tokar
 */

#ifndef EXP_H
#define EXP_H

#include <global.h>

/** Level color structure. */
typedef struct level_color {
    /** Green level. */
    int green;

    /** Blue level. */
    int blue;

    /** Yellow level. */
    int yellow;

    /** Orange level. */
    int orange;

    /** Red level. */
    int red;

    /** Purple level. */
    int purple;
} level_color_t;

/* Prototypes */
uint64_t new_levels[MAXLEVEL + 2];
level_color_t level_color[201];

uint64_t
level_exp(int level, double expmul);
int64_t
add_exp(object *op, int64_t exp_gain, int skill_nr, int exact);
int
exp_lvl_adj(object *who, object *op);
float
calc_level_difference(int who_lvl, int op_lvl);

#endif

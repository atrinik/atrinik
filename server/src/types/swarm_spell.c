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
 * Handles code related to @ref SWARM_SPELL "swarm spells".
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/**
 * Cardinal direction adjustments for swarm spells.
 */
static const int cardinal_adjust[9] = {
    -3, -2, -1, 0, 0, 0, 1, 2, 3
};

/**
 * Diagonal direction adjustments for swarm spells.
 */
static const int diagonal_adjust[10] = {
    -3, -2, -2, -1, 0, 0, 1, 2, 2, 3
};

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->stats.hp == 0 || object_owner(op) == NULL) {
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    op->stats.hp--;
    int basedir = op->direction;

    if (basedir == 0) {
        basedir = get_random_dir();
    }

    /* New offset calculation to make swarm element distribution
     * more uniform */
    int adjustdir;
    if (op->stats.hp != 0) {
        if (basedir & 1) {
            adjustdir = cardinal_adjust[rndm(0, 8)];
        } else {
            adjustdir = diagonal_adjust[rndm(0, 9)];
        }
    } else {
        /* Fire the last one from forward. */
        adjustdir = 0;
    }

    int target_x = op->x + freearr_x[absdir(basedir + adjustdir)];
    int target_y = op->y + freearr_y[absdir(basedir + adjustdir)];

    /* Back up one space so we can hit point-blank targets, but this
     * necessitates extra get_map_from_coord check below */
    int x = target_x - freearr_x[basedir];
    int y = target_y - freearr_y[basedir];

    if (get_map_from_coord(op->map, &x, &y) == NULL) {
        return;
    }

    if (wall(op->map, x, y)) {
        return;
    }

    object *caster = object_owner(op);
    if (caster == NULL) {
        caster = op;
    }

    /* Bombs away! */
    fire_arch_from_position(op,
                            caster,
                            x,
                            y,
                            basedir,
                            op->other_arch,
                            op->stats.sp,
                            NULL);
}

/**
 * Initialize the swarm spell type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(swarm_spell)
{
    OBJECT_METHODS(SWARM_SPELL)->process_func = process_func;
}

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Handles code related to @ref SWARM_SPELL "swarm spells". */

#include <global.h>

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    int cardinal_adjust[9] = {-3, -2, -1, 0, 0, 0, 1, 2, 3};
    int diagonal_adjust[10] = {-3, -2, -2, -1, 0, 0, 1, 2, 2, 3};
    int x, y, basedir, adjustdir;
    int target_x, target_y;

    if (op->stats.hp == 0 || !get_owner(op)) {
        object_remove(op, 0);
        return;
    }

    op->stats.hp--;
    basedir = op->direction;

    if (basedir == 0) {
        basedir = get_random_dir();
    }

    /* New offset calculation to make swarm element distribution
     * more uniform */
    if (op->stats.hp) {
        if (basedir & 1) {
            adjustdir = cardinal_adjust[rndm(0, 8)];
        }
        else {
            adjustdir = diagonal_adjust[rndm(0, 9)];
        }
    }
    /* Fire the last one from forward. */
    else {
        adjustdir = 0;
    }

    target_x = op->x + freearr_x[absdir(basedir + adjustdir)];
    target_y = op->y + freearr_y[absdir(basedir + adjustdir)];

    /* Back up one space so we can hit point-blank targets, but this
     * necessitates extra get_map_from_coord check below */
    x = target_x - freearr_x[basedir];
    y = target_y - freearr_y[basedir];

    if (!get_map_from_coord(op->map, &x, &y)) {
        return;
    }

    if (!wall(op->map, x, y)) {
        fire_arch_from_position(op, op, x, y, basedir, op->other_arch, op->stats.sp, NULL);
    }
}

/**
 * Initialize the swarm spell type object methods. */
void object_type_init_swarm_spell(void)
{
    object_type_methods[SWARM_SPELL].process_func = process_func;
}

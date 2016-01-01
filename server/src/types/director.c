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
 * Handles code for @ref DIRECTOR "directors".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (victim->direction == 0 || state == 0) {
        return OBJECT_METHOD_OK;
    }

    int dir = op->direction;
    if (dir == 0) {
        dir = get_random_dir();
    }

    victim->direction = dir;
    update_turn_face(victim);

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    if (op->stats.maxsp == 0) {
        return OBJECT_METHOD_OK;
    }

    op->direction = absdir(op->direction + op->stats.maxsp);
    animate_turning(op);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the director type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(director)
{
    OBJECT_METHODS(DIRECTOR)->move_on_func = move_on_func;
    OBJECT_METHODS(DIRECTOR)->trigger_func = trigger_func;
}

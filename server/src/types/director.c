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
 * Handles code for @ref DIRECTOR "directors".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
    int dir;

    (void) originator;

    if (!victim->direction || !state) {
        return OBJECT_METHOD_OK;
    }

    dir = op->direction;

    if (!dir) {
        dir = get_random_dir();
    }

    victim->direction = dir;
    update_turn_face(victim);

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
    (void) cause;
    (void) state;

    if (!op->stats.maxsp) {
        return OBJECT_METHOD_OK;
    }

    op->direction = absdir(op->direction + op->stats.maxsp);
    animate_turning(op);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the director type object methods. */
void object_type_init_director(void)
{
    object_type_methods[DIRECTOR].move_on_func = move_on_func;
    object_type_methods[DIRECTOR].trigger_func = trigger_func;
}

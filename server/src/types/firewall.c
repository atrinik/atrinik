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
 * Handles code for @ref FIREWALL "firewall".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->last_eat == 0 || op->map == NULL) {
        return;
    }

    cast_spell(op, op, op->direction, op->stats.dam, 1, CAST_NPC, op->race);
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    if (cause->last_eat != 0) {
        op->last_eat = !op->last_eat;
    } else if (op->stats.maxsp != 0) {
        op->direction = absdir(op->direction + op->stats.maxsp);
        animate_turning(op);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the firewall type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(firewall)
{
    OBJECT_METHODS(FIREWALL)->process_func = process_func;
    OBJECT_METHODS(FIREWALL)->trigger_func = trigger_func;
}

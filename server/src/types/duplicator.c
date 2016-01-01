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
 * Handles code related to @ref DUPLICATOR "duplicators".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <object_methods.h>

/**
 * Try matching an object for duplicator.
 *
 * @param op
 * Duplicator.
 * @param tmp
 * The object to try to match.
 */
static void
duplicator_match_obj (object *op, object *tmp)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(tmp != NULL);

    if (op->slaying != tmp->arch->name) {
        return;
    }

    if (op->level <= 0) {
        destruct_ob(tmp);
    } else {
        tmp->nrof = MIN(UINT32_MAX, (uint64_t) tmp->nrof * op->level);
    }
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (state != 0) {
        duplicator_match_obj(op, victim);
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
        duplicator_match_obj(op, tmp);
    } FOR_MAP_FINISH();

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the duplicator type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(duplicator)
{
    OBJECT_METHODS(DUPLICATOR)->move_on_func = move_on_func;
    OBJECT_METHODS(DUPLICATOR)->trigger_func = trigger_func;
}

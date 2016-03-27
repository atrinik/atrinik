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
 * Handles code used for @ref CREATOR "creators".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/**
 * Check whether creator has already created the specified object on its
 * square.
 *
 * @param op
 * The creator.
 * @param check
 * The object to check for.
 * @return
 * True if such object already exists, false otherwise.
 */
static bool
creator_obj_exists (object *op, object *check)
{
    object *tmp;
    FOR_MAP_LAYER_BEGIN(op->map, op->x, op->y, check->layer, -1, tmp) {
        if (tmp->arch == check->arch &&
            tmp->name == check->name &&
            tmp->type == check->type) {
            return true;
        }
    } FOR_MAP_LAYER_END

    return false;
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    if (op->stats.hp <= 0 && !QUERY_FLAG(op, FLAG_LIFESAVE)) {
        return OBJECT_METHOD_OK;
    }

    bool created = false;
    int roll = -1;
    if (QUERY_FLAG(op, FLAG_SPLITTING)) {
        int num_objs = 0;

        FOR_INV_PREPARE(op, tmp) {
            if (tmp->type == EVENT_OBJECT) {
                continue;
            }

            num_objs++;
        } FOR_INV_FINISH();

        roll = rndm(0, num_objs - 1);
    }

    int idx = 0;
    FOR_INV_PREPARE(op, tmp) {
        if (tmp->type == EVENT_OBJECT) {
            continue;
        }

        if (roll != -1 && roll != idx++) {
            continue;
        }

        if (QUERY_FLAG(op, FLAG_ONE_DROP) && creator_obj_exists(op, tmp)) {
            continue;
        }

        object *clone = object_clone(tmp);
        clone->x = op->x;
        clone->y = op->y;
        clone = object_insert_map(clone, op->map, op, 0);
        if (clone != NULL) {
            created = true;
        }
    } FOR_INV_FINISH();

    if (created && !QUERY_FLAG(op, FLAG_LIFESAVE)) {
        op->stats.hp--;
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the creator type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(creator)
{
    OBJECT_METHODS(CREATOR)->trigger_func = trigger_func;
}

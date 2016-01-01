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
 * Handles code for @ref CHECK_INV "inventory checker" objects.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <object_methods.h>
#include <check_inv.h>

/**
 * Inventory checker object tries to find a matching object in creature's
 * inventory.
 *
 * @param op
 * What is doing the searching.
 * @param ob
 * Object of which to search the inventory.
 * @return
 * Object that matches, NULL if none matched.
 */
object *
check_inv (object *op, object *ob)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(ob != NULL);

    FOR_INV_PREPARE(ob, tmp) {
        if (tmp->inv != NULL && !IS_SYS_INVISIBLE(tmp)) {
            object *ret = check_inv(op, tmp);
            if (ret != NULL) {
                return ret;
            }
        } else {
            if (op->stats.hp != 0 && tmp->type != op->stats.hp) {
                continue;
            }

            if (op->slaying != NULL) {
                if (op->stats.sp != 0) {
                    if (tmp->slaying != op->slaying) {
                        continue;
                    }
                } else {
                    if (tmp->name != op->slaying) {
                        continue;
                    }
                }
            }

            if (op->race != NULL && tmp->arch->name != op->race) {
                continue;
            }

            return tmp;
        }
    } FOR_INV_FINISH();

    return NULL;
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (victim->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    object *match = check_inv(op, victim);
    if (match != NULL && op->last_sp != 0) {
        if (op->last_heal != 0) {
            decrease_ob(match);
        }

        connection_trigger(op, state);
    } else if (match == NULL && op->last_sp == 0) {
        connection_trigger(op, state);
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the inventory checker type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(check_inv)
{
    OBJECT_METHODS(CHECK_INV)->move_on_func = move_on_func;
}

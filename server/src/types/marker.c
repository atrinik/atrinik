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
 * Handles code for @ref MARKER "marker" type objects.
 */

#include <global.h>
#include <arch.h>
#include <object_methods.h>

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (victim->type != PLAYER || state == 0) {
        return OBJECT_METHOD_OK;
    }

    if (op->slaying != NULL) {
        FOR_INV_PREPARE(victim, tmp) {
            if (tmp->type == FORCE && tmp->slaying == op->slaying) {
                object_remove(tmp, 0);
                object_destroy(tmp);
                break;
            }
        } FOR_INV_FINISH();
    }

    if (op->race == NULL) {
        return OBJECT_METHOD_OK;
    }

    FOR_INV_PREPARE(victim, tmp) {
        if (tmp->type == FORCE && tmp->slaying == op->race) {
            return OBJECT_METHOD_OK;
        }
    } FOR_INV_FINISH();

    object *force = arch_get("force");
    force->speed = 0;

    if (op->stats.food != 0) {
        force->speed = 0.01;
        force->speed_left = -op->stats.food;
        SET_FLAG(force, FLAG_IS_USED_UP);
    }

    object_update_speed(force);
    FREE_AND_COPY_HASH(force->slaying, op->race);
    object_insert_into(force, victim, 0);

    if (op->msg != NULL) {
        draw_info(COLOR_NAVY, victim, op->msg);
    }

    if (op->stats.hp > 0) {
        op->stats.hp--;

        if (op->stats.hp == 0) {
            object_destruct(op);
            return OBJECT_METHOD_OK;
        }
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the marker type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(marker)
{
    OBJECT_METHODS(MARKER)->move_on_func = move_on_func;
}

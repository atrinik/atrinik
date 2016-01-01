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
 * Handles code for @ref SYMPTOM "symptom".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>
#include <arch.h>

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    object *victim = op->env;
    /* Outside a monster/player, die immediately */
    if (victim == NULL || victim->map == NULL) {
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    victim = HEAD(victim);

    OBJECTS_DESTROYED_BEGIN(op, victim) {
        if (op->stats.dam > 0) {
            attack_hit(victim, op, op->stats.dam);
        } else {
            attack_hit(victim, op,
                       MAX(1.0, -victim->stats.maxhp * op->stats.dam / 100.0));
        }

        if (OBJECTS_DESTROYED_ANY(op, victim)) {
            return;
        }
    } OBJECTS_DESTROYED_END();

    int sp_reduce;
    if (op->stats.maxsp > 0) {
        sp_reduce = op->stats.maxsp;
    } else {
        sp_reduce = MAX(1.0, victim->stats.maxsp * op->stats.maxsp / 100.0);
    }
    victim->stats.sp = MAX(0, victim->stats.sp - sp_reduce);

    /* Create the symptom's "other arch" object and drop it here
     * under every part of the monster. */

    if (op->other_arch != NULL) {
        for (object *tmp = victim; tmp != NULL; tmp = tmp->more) {
            object *new_ob = arch_to_object(op->other_arch);
            new_ob->x = tmp->x;
            new_ob->y = tmp->y;
            new_ob->map = victim->map;
            insert_ob_in_map(new_ob,
                             victim->map,
                             victim,
                             INS_NO_MERGE | INS_NO_WALK_ON);
        }
    }

    if (victim->type == PLAYER) {
        draw_info(COLOR_RED, victim, op->msg);
    }
}

/**
 * Initialize the symptom type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(symptom)
{
    OBJECT_METHODS(SYMPTOM)->process_func = process_func;
}

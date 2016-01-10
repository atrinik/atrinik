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
 * Handles code for @ref RUNE "runes".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>
#include <disease.h>
#include <rune.h>

/**
 * Springs a rune.
 *
 * @param op
 * The rune.
 * @param victim
 * Victim of the rune.
 */
void
rune_spring (object *op, object *victim)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    if (op->stats.hp == 0) {
        return;
    }

    /* Only living objects can trigger runes that do direct damage. */
    if (!IS_LIVE(victim) && op->stats.sp == -1) {
        return;
    }

    if (op->msg != NULL) {
        draw_info(COLOR_WHITE, victim, op->msg);
    }

    object *env = object_get_env(op);
    trap_show(op, env);

    if (victim->type == PLAYER) {
        CONTR(victim)->stat_traps_sprung++;
    }

    /* Direct damage. */
    if (op->stats.sp == -1) {
        OBJECTS_DESTROYED_BEGIN(op, victim) {
            int dam = op->stats.dam * (LEVEL_DAMAGE(op->level) * 0.925f);
            attack_hit(victim, op, dam);

            if (!OBJECTS_DESTROYED(victim)) {
                if (op->randomitems != NULL) {
                    int level = op->level;
                    if (level == 0) {
                        level = env->map->difficulty;
                    }

                    create_treasure(op->randomitems,
                                    op,
                                    0,
                                    level,
                                    T_STYLE_UNSET,
                                    ART_CHANCE_UNSET,
                                    0,
                                    NULL);
                }

                FOR_INV_PREPARE(op, tmp) {
                    if (tmp->type != DISEASE) {
                        continue;
                    }

                    disease_infect(tmp, victim, 1);
                    object_remove(tmp, 0);
                    object_destroy(tmp);
                } FOR_INV_FINISH();
            }

            if (OBJECTS_DESTROYED(op)) {
                return;
            }
        } OBJECTS_DESTROYED_END();
    } else {
        /* Spell. */
        cast_spell(env,
                   op,
                   op->stats.maxsp,
                   op->stats.sp,
                   1,
                   CAST_NORMAL,
                   NULL);
    }

    /* Decrement detonation count and see if it's the last one, but only
     * if the count is not -1 already (infinite). */
    if (op->stats.hp != -1 && --op->stats.hp == 0) {
        /* Make the trap impotent */
        op->type = MISC_OBJECT;
        CLEAR_FLAG(op, FLAG_FLY_ON);
        CLEAR_FLAG(op, FLAG_WALK_ON);
        FREE_AND_CLEAR_HASH2(op->msg);
        /* Make it stick around until its spells are gone */
        op->stats.food = 20;
        SET_FLAG(op, FLAG_IS_USED_UP);
        op->speed = op->speed_left = 1.0;
        object_update_speed(op);
        /* Clear trapped flag. */
        set_trapped_flag(env);
    }
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    rune_spring(op, victim);
    return OBJECT_METHOD_OK;
}

/**
 * Initialize the rune type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(rune)
{
    OBJECT_METHODS(RUNE)->move_on_func = move_on_func;
}

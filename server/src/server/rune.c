/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * All rune related functions.
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <rune.h>
#include <exp.h>

/**
 * Award experience for successfully handling a generated trap.
 *
 * Generated traps store their creator monster's base experience in stats.exp.
 * The normal experience path caps excessive single awards and updates both the
 * skill and, for contributing skills, character experience.
 *
 * @param pl
 * Player receiving experience.
 * @param trap
 * Trap that was found or disarmed.
 * @param skill_nr
 * Skill receiving the experience.
 */
static void trap_award_exp(object *pl, object *trap, int skill_nr) {
    if (trap->stats.exp <= 0 || trap->level <= 0) {
        return;
    }

    int64_t exp = trap->stats.exp;
    if (exp > INT64_MAX / trap->level) {
        exp = INT64_MAX;
    } else {
        exp *= trap->level;
    }

    add_exp(pl, exp, skill_nr, 0);
}

/**
 * Should op see trap?
 * @param op
 * Living that could spot the trap.
 * @param trap
 * Trap that is invisible.
 * @param level
 * Level.
 * @retval 0 Trap wasn't spotted.
 * @retval 1 Trap was spotted.
 */
int trap_see(object *op, object *trap, int level) {
    /* Explicit searching is a capability check, not a rerollable lottery.
     * This prevents repeated skill commands from being strictly better than
     * one deliberate search while still leaving over-level traps hidden. */
    if (trap->stats.Int == 1 || trap->level <= level) {
        draw_info_format(COLOR_WHITE, op, "You spot a %s (lvl %d)!", trap->name, trap->level);

        if (trap->stats.Int != 1) {
            CONTR(op)->stat_traps_found++;
            trap_award_exp(op, trap, SK_FIND_TRAPS);
            /* Mark it as found immediately so repeated checks cannot award
             * experience before trap_show() updates its presentation. */
            trap->stats.Int = 1;
        }

        return 1;
    }

    return 0;
}

/**
 * Handles showing of a trap.
 * @param trap
 * The trap.
 * @param where
 * Where.
 * @return
 * 1 if the trap was shown, 0 otherwise.
 */
int trap_show(object *trap, object *where) {
    object *env;

    if (where == NULL) {
        return 0;
    }

    env = trap->env;

    if (!QUERY_FLAG(trap, FLAG_REMOVED)) {
        /* We must remove and reinsert it so the layer is updated correctly. */
        object_remove(trap, 0);
    }

    CLEAR_FLAG(trap, FLAG_SYS_OBJECT);
    CLEAR_MULTI_FLAG(trap, FLAG_IS_INVISIBLE);
    trap->layer = LAYER_EFFECT;

    /* The trap is not hidden anymore. */
    if (trap->stats.Int > 1) {
        trap->stats.Int = 1;
    }

    if (env && env->type != PLAYER && env->type != MONSTER && env->type != DOOR &&
        !QUERY_FLAG(env, FLAG_NO_PASS)) {
        object_insert_into(trap, env, 0);
        set_trapped_flag(env);
    } else if (where->map != NULL) {
        object_insert_map(trap, where->map, NULL, 0);
    }

    return 1;
}

/**
 * Try to disarm a trap.
 * @param disarmer
 * Player disarming the trap.
 * @param trap
 * Trap to disarm.
 * @return
 * 1 if trap was disarmed, 0 otherwise.
 */
int trap_disarm(object *disarmer, object *trap) {
    object *env = trap->env;
    int disarmer_level = trap_skill_rating(disarmer, SK_REMOVE_TRAPS);

    /* As with explicit detection, disarming is deterministic at a given
     * capability. Repeating the same command cannot reroll a failure. */
    if (trap->level <= disarmer_level) {
        draw_info_format(COLOR_WHITE,
                         disarmer,
                         "You successfully remove the %s (lvl %d)!",
                         trap->name,
                         trap->level);
        object_remove(trap, 0);
        set_trapped_flag(env);
        CONTR(disarmer)->stat_traps_disarmed++;
        trap_award_exp(disarmer, trap, SK_REMOVE_TRAPS);
        return 1;
    } else {
        draw_info_format(COLOR_WHITE,
                         disarmer,
                         "You fail to remove the %s (lvl %d).",
                         trap->name,
                         trap->level);

        if (trap->level > disarmer_level * 1.4f || rndm(0, 2)) {
            if (!(rndm(0,
                       (MAX(2, disarmer_level - trap->level + disarmer->stats.Dex / 2 - 6)) - 1))) {
                draw_info(COLOR_WHITE, disarmer, "In fact, you set it off!");
                rune_spring(trap, disarmer);
            }
        }

        return 0;
    }
}

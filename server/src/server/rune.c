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

    object *skill = CONTR(pl)->skill_ptr[skill_nr];
    HARD_ASSERT(skill != NULL);
    int64_t exp = calc_skill_exp(pl, trap, skill->level);
    add_exp(pl, exp, skill_nr, 0);
}

/** Stable per-player/per-trap roll, preventing command-spam rerolls. */
static int trap_skill_roll(const object *pl, const object *trap, int skill_nr, uint64_t salt) {
    uint64_t value = (uint64_t)pl->count * UINT64_C(0x9e3779b185ebca87) ^
                     (uint64_t)trap->count * UINT64_C(0xc2b2ae3d27d4eb4f) ^
                     (uint64_t)skill_nr * UINT64_C(0x165667b19e3779f9) ^ salt;
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33;
    value *= UINT64_C(0xc4ceb9fe1a85ec53);
    value ^= value >> 33;
    return (int)(value % 100) + 1;
}

/** Chance to find a still-hidden trap. */
static int trap_find_chance(const object *trap, int rating) {
    int difficulty = MAX(trap->level, trap->stats.Int);
    return MAX(15, MIN(95, 70 + (rating - difficulty) * 4));
}

/** Chance to disarm a discovered trap without triggering failure handling. */
static int trap_disarm_chance(const object *trap, int rating) {
    return MAX(10, MIN(90, 65 + (rating - trap->level) * 5));
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
    /* A stable roll makes individual traps uncertain without making repeated
     * skill-command spam better than one deliberate attempt. */
    if (trap->stats.Int == 1 || trap_skill_roll(op, trap, SK_FIND_TRAPS, UINT64_C(0x21f0aaad)) <=
                                    trap_find_chance(trap, level)) {
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
 * One of @ref trap_disarm_result_t.
 */
int trap_disarm(object *disarmer, object *trap) {
    object *env = trap->env;
    int disarmer_level = trap_skill_rating(disarmer, SK_REMOVE_TRAPS);

    if (trap_skill_roll(disarmer, trap, SK_REMOVE_TRAPS, UINT64_C(0x8b8b8b8b)) <=
        trap_disarm_chance(trap, disarmer_level)) {
        draw_info_format(COLOR_WHITE,
                         disarmer,
                         "You successfully remove the %s (lvl %d)!",
                         trap->name,
                         trap->level);
        object_remove(trap, 0);
        set_trapped_flag(env);
        CONTR(disarmer)->stat_traps_disarmed++;
        trap_award_exp(disarmer, trap, SK_REMOVE_TRAPS);
        return TRAP_DISARM_SUCCESS;
    } else {
        draw_info_format(COLOR_WHITE,
                         disarmer,
                         "You fail to remove the %s (lvl %d).",
                         trap->name,
                         trap->level);

        int trip_chance = MAX(50, MIN(90, 55 + (trap->level - disarmer_level) * 5));
        if (trap_skill_roll(disarmer, trap, SK_REMOVE_TRAPS, UINT64_C(0xf00dcafe)) <= trip_chance) {
            draw_info(COLOR_WHITE, disarmer, "In fact, you set it off!");
            rune_spring(trap, disarmer);
            return TRAP_DISARM_TRIPPED;
        }

        return TRAP_DISARM_FAILED;
    }
}

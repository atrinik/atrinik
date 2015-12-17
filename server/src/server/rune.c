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
 * All rune related functions. */

#include <global.h>

/**
 * Should op see trap?
 * @param op Living that could spot the trap.
 * @param trap Trap that is invisible.
 * @param level Level.
 * @retval 0 Trap wasn't spotted.
 * @retval 1 Trap was spotted. */
int trap_see(object *op, object *trap, int level)
{
    int chance = rndm(0, 99);

    /* Decide if we can see the rune or not */
    if ((trap->level <= level && rndm_chance(10)) || trap->stats.Int == 1 || (chance > MIN(95, MAX(5, ((int) ((float) (op->map->difficulty + trap->level + trap->stats.Int - op->level) / 10.0 * 50.0)))))) {
        draw_info_format(COLOR_WHITE, op, "You spot a %s (lvl %d)!", trap->name, trap->level);

        if (trap->stats.Int != 1) {
            CONTR(op)->stat_traps_found++;
        }

        return 1;
    }

    return 0;
}

/**
 * Handles showing of a trap.
 * @param trap The trap.
 * @param where Where.
 * @return 1 if the trap was shown, 0 otherwise. */
int trap_show(object *trap, object *where)
{
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

    if (env && env->type != PLAYER && env->type != MONSTER &&
            env->type != DOOR && !QUERY_FLAG(env, FLAG_NO_PASS)) {
        insert_ob_in_ob(trap, env);
        set_trapped_flag(env);
    } else if (where->map != NULL) {
        insert_ob_in_map(trap, where->map, NULL, 0);
    }

    return 1;
}

/**
 * Try to disarm a trap.
 * @param disarmer Player disarming the trap.
 * @param trap Trap to disarm.
 * @return 1 if trap was disarmed, 0 otherwise. */
int trap_disarm(object *disarmer, object *trap)
{
    object *env = trap->env;
    int disarmer_level = disarmer->level;

    if ((trap->level <= disarmer_level && rndm_chance(10)) || !(rndm(0, (MAX(2, MIN(20, trap->level - disarmer_level + 5 - disarmer->stats.Dex / 2)) - 1)))) {
        draw_info_format(COLOR_WHITE, disarmer, "You successfully remove the %s (lvl %d)!", trap->name, trap->level);
        object_remove(trap, 0);
        set_trapped_flag(env);
        CONTR(disarmer)->stat_traps_disarmed++;
        return 1;
    } else {
        draw_info_format(COLOR_WHITE, disarmer, "You fail to remove the %s (lvl %d).", trap->name, trap->level);

        if (trap->level > disarmer_level * 1.4f || rndm(0, 2)) {
            if (!(rndm(0, (MAX(2, disarmer_level - trap->level + disarmer->stats.Dex / 2 - 6)) - 1))) {
                draw_info(COLOR_WHITE, disarmer, "In fact, you set it off!");
                rune_spring(trap, disarmer);
            }
        }

        return 0;
    }
}

/**
 * Adjust trap difficulty to the map. The default traps are too strong
 * for wimpy level 1 players, and unthreatening to anyone of high level.
 * @param trap Trap to adjust.
 * @param difficulty Map difficulty. */
void trap_adjust(object *trap, int difficulty)
{
    int off, level, hide;

    if (difficulty < 1) {
        difficulty = 1;
    }

    off = (int) ((float) difficulty * 0.2f);
    level = rndm(difficulty - off, difficulty + off);
    level = MAX(1, MIN(level, MAXLEVEL));
    hide = rndm(0, 19) + rndm(difficulty - off, difficulty + off);
    hide = MAX(1, MIN(hide, INT8_MAX));

    trap->level = level;
    trap->stats.Int = hide;
}

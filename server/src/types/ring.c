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
 * Handles code for @ref RING "rings".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object_methods.h>
#include <arch.h>
#include <artifact.h>

#include "common/process_treasure.h"

/**
 * Chance for the ring to become cursed - 1/x.
 */
#define RING_CHANCE_CURSED 5
/**
 * Chance for the ring to become damned, but only if it's already been
 * cursed - 1/x.
 */
#define RING_CHANCE_DAMNED 10
/**
 * Chance to roll from the ring table instead of the jewelry one - 1/x.
 */
#define RING_CHANCE_TABLE 3
/**
 * Chance to roll another bonus on the ring - 1/x.
 */
#define RING_CHANCE_EXTRA 4

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_speed (object              *op,
                      int                  difficulty,
                      treasure_affinity_t *affinity,
                      double              *item_power,
                      int                  bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    op->stats.exp += bonus;
    op->value *= 1.0 + (0.15 * FABS(bonus));

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_regen_hp (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        tmp++;
        op->value *= 1.3;
    }

    op->stats.hp += tmp;
    op->value *= 1.0 + (0.2 * FABS(bonus));

    if (tmp > 0) {
        *item_power += tmp;
    }

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_regen_sp (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double               *item_power,
                         int                   bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        tmp++;
        op->value *= 1.3;
    }

    op->stats.sp += tmp;
    op->value *= 1.0 + (0.2 * FABS(bonus));

    if (tmp > 0) {
        *item_power += tmp;
    }

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_damage (object              *op,
                       int                  difficulty,
                       treasure_affinity_t *affinity,
                       double              *item_power,
                       int                  bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        tmp++;
        op->value *= 1.5;
    }

    op->stats.dam += tmp;
    op->value *= 1.0 + (0.4 * FABS(bonus));

    if (tmp > 0) {
        *item_power += tmp;
    }

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_sustenance (object              *op,
                           int                  difficulty,
                           treasure_affinity_t *affinity,
                           double              *item_power,
                           int                  bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        tmp++;
        op->value *= 1.1;
    }

    op->stats.food += tmp;
    op->value *= 1.0 + (0.1 * FABS(bonus));

    return true;
}

/**
 * Generic function for setting bonus stats, such as strength, dexterity, etc.
 *
 * @param op
 * Object.
 * @param difficulty
 * Difficulty level.
 * @param affinity
 * Treasure affinity.
 * @param[out] item_power
 * Item power adjustment.
 * @param bonus
 * Bonus value.
 * @param stat
 * Stat ID.
 * @return
 * True on success, false on failure.
 */
static bool
ring_set_bonus_stat (object              *op,
                     int                  difficulty,
                     treasure_affinity_t *affinity,
                     double              *item_power,
                     int                  bonus,
                     int                  stat)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int value = get_attr_value(&op->stats, stat) + bonus;
    if (value > MAX_STAT || value < -MAX_STAT) {
        /* Extremely unlikely to happen (if it's even possible), but guard
         * against it anyway. Returning false will make the main processing
         * function roll something else (hopefully). */
        LOG(DEVEL,
            "Stat value reached minimum/maximum %d: %s",
            value,
            object_get_str(op));
        return false;
    }

    set_attr_value(&op->stats, stat, value);
    op->value *= 1.0 + (0.25 * FABS(bonus));

    if (bonus > 0) {
        *item_power += bonus;
    }

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_stat_str (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    return ring_set_bonus_stat(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus,
                               STR);
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_stat_dex (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    return ring_set_bonus_stat(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus,
                               DEX);
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_stat_con (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    return ring_set_bonus_stat(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus,
                               CON);
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_stat_int (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    return ring_set_bonus_stat(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus,
                               INT);
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
ring_set_bonus_stat_pow (object              *op,
                         int                  difficulty,
                         treasure_affinity_t *affinity,
                         double              *item_power,
                         int                  bonus)
{
    return ring_set_bonus_stat(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus,
                               POW);
}

/**
 * The ring treasure table.
 */
static const process_treasure_table_t ring_treasure_table[] = {
    {40, ring_set_bonus_speed},
    {30, ring_set_bonus_regen_hp},
    {30, ring_set_bonus_regen_sp},
    {25, ring_set_bonus_damage},
    {40, ring_set_bonus_sustenance},
    {35, ring_set_bonus_stat_str},
    {35, ring_set_bonus_stat_dex},
    {35, ring_set_bonus_stat_con},
    {35, ring_set_bonus_stat_int},
    {35, ring_set_bonus_stat_pow},
};

/** @copydoc object_methods_t::process_treasure_func */
static int
process_treasure_func (object              *op,
                       object             **ret,
                       int                  difficulty,
                       treasure_affinity_t *affinity,
                       int                  flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 0);

    /* Avoid processing if the item is already special. */
    if (process_treasure_is_special(op)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* We only process ring_generic objects. */
    if (op->arch != arches[ARCH_RING_GENERIC]) {
        return OBJECT_METHOD_UNHANDLED;
    }

    if (!QUERY_FLAG(op, FLAG_REMOVED)) {
        object_remove(op, 0);
    }

    /* Destroy the original and create a ring_normal archetype,
     * which will be turned into an artifact. */
    object_destroy(op);
    *ret = op = arch_to_object(arches[ARCH_RING_NORMAL]);

    if (!artifact_generate(op, difficulty, affinity)) {
        log_error("Failed to generate artifact: %s", object_get_str(op));
        object_destroy(op);
        return OBJECT_METHOD_ERROR;
    }

    if (!(flags & GT_ONLY_GOOD)) {
        if (rndm_chance(RING_CHANCE_CURSED)) {
            if (rndm_chance(RING_CHANCE_DAMNED)) {
                SET_FLAG(op, FLAG_DAMNED);
            } else {
                SET_FLAG(op, FLAG_CURSED);
            }
        }
    }

    double item_power = 0.0;

    for (int i = 0; i < 5; i++) {
        /* If it's not the first bonus, roll for a chance of another bonus.
         *
         * The first unsuccessful roll is a combo-breaker. */
        if (i != 0 && !rndm_chance(RING_CHANCE_EXTRA)) {
            break;
        }

        /* Decide which table to roll from. */
        if (rndm_chance(RING_CHANCE_TABLE)) {
            static uint32_t total_chance = 0;
            if (total_chance == 0) {
                total_chance =
                    PROCESS_TREASURE_TABLE_TOTAL_CHANCE(ring_treasure_table);
            }

            if (!process_treasure_table(ring_treasure_table,
                                        arraysize(ring_treasure_table),
                                        total_chance,
                                        op,
                                        difficulty,
                                        affinity,
                                        &item_power)) {
                object_destroy(op);
                return OBJECT_METHOD_ERROR;
            }
        } else {
            if (!process_treasure_table_jewelry(op,
                                                difficulty,
                                                affinity,
                                                &item_power)) {
                object_destroy(op);
                return OBJECT_METHOD_ERROR;
            }
        }
    }

    process_treasure_set_item_power(op, item_power);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the ring type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(ring)
{
    OBJECT_METHODS(RING)->apply_func = object_apply_item;
    OBJECT_METHODS(RING)->process_treasure_func = process_treasure_func;
}

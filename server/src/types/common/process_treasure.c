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
 * Common treasure processing functions.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>

#include "process_treasure.h"

/**
 * Maximum level for jewelry items.
 */
#define JEWELRY_MAX_LEVEL 100

/**
 * Set bonus max HP/SP.
 *
 * @param op
 * Object.
 * @param difficulty
 * Difficulty level.
 * @param affinity
 * Treasure affinity.
 * @param[out] item_power
 * Item power.
 * @param bonus
 * Bonus.
 * @param diff_scale
 * Difficulty scaling.
 * @return
 * Max HP/SP to set on the object.
 */
static int
jewelry_set_bonus_max_hpsp (object *op,
                            int     difficulty,
                            int     affinity,
                            double *item_power,
                            int     bonus,
                            double  diff_scale)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = 5;
    tmp += difficulty * (diff_scale * FABS(bonus));
    tmp *= 1.0 + (0.15 * FABS(bonus));
    tmp += rndm(0, difficulty * (diff_scale * FABS(bonus)) + 0.5);

    if (bonus > 0) {
        /* Adjust the item's level requirement. */
        uint32_t level = op->item_level;
        level += (double) difficulty * (0.3 + (rndm(0, 40) / 100.0));
        op->item_level = MIN(level, JEWELRY_MAX_LEVEL);

        op->value *= 2.0 + (0.25 * bonus);

        *item_power += tmp / 50.0;
    } else {
        tmp = -tmp;
    }

    return tmp;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
jewelry_set_bonus_maxhp (object *op,
                         int     difficulty,
                         int     affinity,
                         double *item_power,
                         int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    op->stats.maxhp += jewelry_set_bonus_max_hpsp(op,
                                                  difficulty,
                                                  affinity,
                                                  item_power,
                                                  bonus,
                                                  0.65);
    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
jewelry_set_bonus_maxsp (object *op,
                         int     difficulty,
                         int     affinity,
                         double *item_power,
                         int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    op->stats.maxsp += jewelry_set_bonus_max_hpsp(op,
                                                  difficulty,
                                                  affinity,
                                                  item_power,
                                                  bonus,
                                                  0.35);
    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
jewelry_set_bonus_ac (object *op,
                      int     difficulty,
                      int     affinity,
                      double *item_power,
                      int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        op->value *= 1.3;
        tmp++;
    }

    op->stats.ac += tmp;
    *item_power += tmp;

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
jewelry_set_bonus_wc (object *op,
                      int     difficulty,
                      int     affinity,
                      double *item_power,
                      int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int tmp = bonus;

    if (bonus > 0 && rndm_chance(5)) {
        op->value *= 1.3;
        tmp++;
    }

    op->stats.wc += tmp;
    *item_power += tmp;

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
jewelry_set_bonus_protect (object *op,
                           int     difficulty,
                           int     affinity,
                           double *item_power,
                           int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int roll = difficulty / 20.0 + FABS(bonus);
    int tmp = 10;

    for (int i = 0; i < 5; i++) {
        tmp += rndm(0, roll);
    }

    /* Cursed items need to have higher negative values to equal
     * out with positive values for how protections work out. Put
     * another little random element in since that they don't
     * always end up with even values. */
    if (bonus < 0) {
        tmp = 2 * -tmp - rndm(0, roll);
    }

    /* Upper limit */
    if (tmp > 40) {
        tmp = 40;
    }

    /* Attempt to find a free protection. */
    for (int i = 0; i < 5; i++) {
        int idx = rndm(0, LAST_PROTECTION - 1);
        if (op->protection[idx] != 0) {
            continue;
        }

        op->protection[idx] = tmp;

        if (tmp > 0) {
            *item_power += (tmp + 10.0) / 20.0;
        }

        return true;
    }

    return false;
}

/**
 * The common jewelry treasure table.
 */
static const process_treasure_table_t jewelry_treasure_table[] = {
    {10, jewelry_set_bonus_maxhp},
    {10, jewelry_set_bonus_maxsp},
    {10, jewelry_set_bonus_ac},
    {10, jewelry_set_bonus_wc},
    {40, jewelry_set_bonus_protect},
};

/**
 * Checks if the specified treasure is already a special item.
 *
 * @param op
 * Object to check.
 * @return
 * True if the object is a special item, false otherwise.
 */
bool
process_treasure_is_special (object *op)
{
    HARD_ASSERT(op != NULL);

    /* If it has a title, it's already a special treasure item. */
    if (op->title != NULL) {
        return true;
    }

    return false;
}

/**
 * Calculate the total chance of the specified treasure table.
 *
 * @param table
 * Table.
 * @param table_size
 * Number of elements in the table.
 * @return
 * Total table chance.
 */
uint32_t
process_treasure_table_total_chance (const process_treasure_table_t *table,
                                     size_t                          table_size)
{
    HARD_ASSERT(table != NULL);
    HARD_ASSERT(table_size != 0);

    uint32_t total_chance = 0;
    for (size_t i = 0; i < table_size; i++) {
        total_chance += table[i].chance;
    }

    /* We need to have *some* total chance. */
    HARD_ASSERT(total_chance != 0);

    return total_chance;
}

/**
 * Get a jewelry bonus depending on the specified difficulty. The higher
 * the difficulty, the higher the chance to get a high bonus.
 *
 * @param difficulty
 * Difficulty level.
 * @return
 * Bonus.
 */
static int
process_treasure_get_jewelry_bonus (int difficulty)
{
#define DIFFICULTY_BONUS_ROLLS      40
#define DIFFICULTY_BONUS_CHANCE     5

    int bonus = 1;
    if (difficulty < DIFFICULTY_BONUS_ROLLS) {
        return bonus;
    }

    /* Essentially, for every X difficulty levels we add a chance for some
     * amount of bonus rolls. */
    int bonus_rolls = rndm(0, difficulty / DIFFICULTY_BONUS_ROLLS);
    for (int i = 0; i < bonus_rolls; i++) {
        /* Only apply the bonus if we're lucky enough. */
        if (rndm_chance(DIFFICULTY_BONUS_CHANCE)) {
            bonus++;
        }
    }

    return bonus;

#undef DIFFICULTY_BONUS_ROLLS
#undef DIFFICULTY_BONUS_CHANCE
}

/**
 * Process a treasure table bonus and set it on the specified item.
 *
 * @param entry
 * Table bonus entry to process.
 * @param op
 * Object to set the bonus on.
 * @param difficulty
 * Difficulty level.
 * @param affinity
 * Treasure affinity.
 * @param[out] item_power
 * Item power adjustment.
 * @param bonus
 * Bonus value to apply.
 * @return
 * Whether a bonus was applied.
 */
static bool
process_treasure_table_set_bonus (const process_treasure_table_t *entry,
                                  object                         *op,
                                  int                             difficulty,
                                  int                             affinity,
                                  double                         *item_power,
                                  int                             bonus)
{
    HARD_ASSERT(entry != NULL);
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    if (!entry->set_bonus_func(op,
                               difficulty,
                               affinity,
                               item_power,
                               bonus)) {
        return false;
    }

    if (bonus > 0) {
        op->value = (int64_t) ((double) op->value * 2.0 * (double) bonus);
    } else {
        op->value = -(op->value * 2.0 * bonus) / 2.0;
    }

    if (op->value < 0) {
        op->value = 0;
    }

    return true;
}

/**
 * Process a specific treasure table and apply a single bonus from it
 * to the specified item.
 *
 * @param table
 * Table to process.
 * @param table_size
 * Size of the table.
 * @param total_chance
 * Total chance of the specified table.
 * @param op
 * Object to apply the bonus to.
 * @param difficulty
 * Difficulty level.
 * @param affinity
 * Treasure affinity.
 * @param[out] item_power
 * Item power adjustment. Make sure this value is initialized to something
 * sane.
 * @return
 * True if a bonus was applied, false otherwise.
 */
bool
process_treasure_table (const process_treasure_table_t *table,
                        size_t                          table_size,
                        uint32_t                        total_chance,
                        object                         *op,
                        int                             difficulty,
                        int                             affinity,
                        double                         *item_power)
{
#define MAX_BONUS_TRIES 100

    HARD_ASSERT(table != NULL);
    HARD_ASSERT(table_size > 1);
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    int bonus;
    if (op->type == AMULET || op->type == RING) {
        bonus = process_treasure_get_jewelry_bonus(difficulty);
    } else {
        log_error("Cannot get item type bonus for type: %u",
                  op->type);
        return false;
    }

    SET_FLAG(op, FLAG_IS_MAGICAL);

    if (QUERY_FLAG(op, FLAG_CURSED)) {
        bonus = -bonus;
    } else if (QUERY_FLAG(op, FLAG_DAMNED)) {
        bonus = -(bonus * 2);
    }

    for (int attempt = 0; attempt < MAX_BONUS_TRIES; attempt++) {
        int roll = rndm(0, total_chance - 1);

        for (size_t i = 0; i < table_size; i++) {
            roll -= table[i].chance;
            if (roll < 0) {
                if (process_treasure_table_set_bonus(&table[i],
                                                     op,
                                                     difficulty,
                                                     affinity,
                                                     item_power,
                                                     bonus)) {
                    return true;
                }

                /* Failed to set a bonus, so attempt to roll hopefully a
                 * different one. */
                break;
            }
        }

        if (roll >= 0) {
            log_error("Roll has %d remaining, total chance: %d",
                      roll,
                      total_chance);
            return false;
        }
    }

    log_error("Failed to set bonus for treasure: %s",
              object_get_str(op));
    return false;

#undef MAX_BONUS_TRIES
}

/**
 * Process the jewelry treasure table and apply a single bonus from it
 * to the specified item.
 *
 * @param op
 * Object to apply the bonus to.
 * @param difficulty
 * Difficulty level.
 * @param affinity
 * Treasure affinity.
 * @param[out] item_power
 * Item power adjustment. Make sure this value is initialized to something
 * sane.
 * @return
 * True if a bonus was applied, false otherwise.
 */
bool
process_treasure_table_jewelry (object *op,
                                int     difficulty,
                                int     affinity,
                                double *item_power)
{
    static uint32_t total_chance = 0;
    if (total_chance == 0) {
        total_chance =
            PROCESS_TREASURE_TABLE_TOTAL_CHANCE(jewelry_treasure_table);
    }

    return process_treasure_table(jewelry_treasure_table,
                                  arraysize(jewelry_treasure_table),
                                  total_chance,
                                  op,
                                  difficulty,
                                  affinity,
                                  item_power);
}

/**
 * Set calculated item power on the specified object.
 *
 * @param op
 * Object.
 * @param item_power
 * Item power to set.
 */
void
process_treasure_set_item_power (object *op,
                                 double  item_power)
{
    HARD_ASSERT(op != NULL);

    int tmp = (int) item_power;
    if (tmp < INT8_MIN) {
        tmp = INT8_MIN;
    } else if (tmp > INT8_MAX) {
        tmp = INT8_MAX;
    }

    op->item_power += tmp;
}


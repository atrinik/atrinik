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
 * Common treasure processing file.
 */

#ifndef COMMON_PROCESS_TREASURE_H
#define COMMON_PROCESS_TREASURE_H

#include <toolkit.h>

/**
 * Holds data about a single bonus type.
 */
typedef struct process_treasure_table {
    /** Chance for the bonus to roll. */
    uint32_t chance;

    /**
     * Function for setting a bonus for the specified bonus type.
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
     * @return
     * Whether a bonus was applied.
     */
    bool (*set_bonus_func)(object *op,
                           int     difficulty,
                           int     affinity,
                           double *item_power,
                           int     bonus);
} process_treasure_table_t;

/**
 * Compute the total chance of the specified treasure table.
 *
 * @param table
 * Treasure table.
 * @return
 * Total chance of the treasure table.
 */
#define PROCESS_TREASURE_TABLE_TOTAL_CHANCE(table) \
    process_treasure_table_total_chance(table, arraysize(table))

/* Prototypes */

bool
process_treasure_is_special(object *op);
uint32_t
process_treasure_table_total_chance(const process_treasure_table_t *table,
                                    size_t                          table_size);
bool
process_treasure_table(const process_treasure_table_t *table,
                       size_t                          table_size,
                       uint32_t                        total_chance,
                       object                         *op,
                       int                             difficulty,
                       int                             affinity,
                       double                         *item_power);
bool
process_treasure_table_jewelry(object *op,
                               int     difficulty,
                               int     affinity,
                               double *item_power);
void
process_treasure_set_item_power(object *op,
                                double  item_power);

#endif

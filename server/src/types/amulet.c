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
 * Handles code for @ref AMULET "amulets".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object_methods.h>
#include <arch.h>
#include <artifact.h>

#include "common/process_treasure.h"

/**
 * Chance for the amulet to become cursed - 1/x.
 */
#define AMULET_CHANCE_CURSED 5
/**
 * Chance for the amulet to become damned, but only if it's already been
 * cursed - 1/x.
 */
#define AMULET_CHANCE_DAMNED 10
/**
 * Chance to roll from the amulet table instead of the jewelry one - 1/x.
 */
#define AMULET_CHANCE_TABLE 20

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
amulet_set_bonus_reflect_missiles (object *op,
                                   int     difficulty,
                                   int     affinity,
                                   double *item_power,
                                   int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    SET_FLAG(op, FLAG_REFL_MISSILE);
    op->value *= 9;

    return true;
}

/** @copydoc process_treasure_table_t::set_bonus_func */
static bool
amulet_set_bonus_reflect_spells (object *op,
                                 int     difficulty,
                                 int     affinity,
                                 double *item_power,
                                 int     bonus)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(item_power != NULL);

    SET_FLAG(op, FLAG_REFL_SPELL);
    op->value *= 11;

    return true;
}

/**
 * The amulet treasure table.
 */
static const process_treasure_table_t amulet_treasure_table[] = {
    {0,  NULL}, /* Total chance */
    {50, amulet_set_bonus_reflect_missiles},
    {60, amulet_set_bonus_reflect_spells},
};

/** @copydoc object_methods_t::process_treasure_func */
static int
process_treasure_func (object  *op,
                       object **ret,
                       int      difficulty,
                       int      affinity,
                       int      flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 1);

    /* Avoid processing if the item is already special. */
    if (process_treasure_is_special(op)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* We only process amulet_generic objects. */
    if (op->arch != arches[ARCH_AMULET_GENERIC]) {
        return OBJECT_METHOD_UNHANDLED;
    }

    if (!QUERY_FLAG(op, FLAG_REMOVED)) {
        object_remove(op, 0);
    }

    /* Destroy the original and create an amulet_normal archetype,
     * which will be turned into an artifact. */
    object_destroy(op);
    *ret = op = arch_to_object(arches[ARCH_AMULET_NORMAL]);

    if (!artifact_generate(op, difficulty, affinity)) {
        log_error("Failed to generate artifact: %s", object_get_str(op));
        object_destroy(op);
        return OBJECT_METHOD_ERROR;
    }

    if (!(flags & GT_ONLY_GOOD)) {
        if (rndm_chance(AMULET_CHANCE_CURSED)) {
            if (rndm_chance(AMULET_CHANCE_DAMNED)) {
                SET_FLAG(op, FLAG_DAMNED);
            } else {
                SET_FLAG(op, FLAG_CURSED);
            }
        }
    }

    double item_power = 0.0;

    /* Decide which table to roll from. */
    if (rndm_chance(AMULET_CHANCE_TABLE)) {
        static uint32_t total_chance = 0;
        if (total_chance == 0) {
            total_chance =
                PROCESS_TREASURE_TABLE_TOTAL_CHANCE(amulet_treasure_table);
        }

        if (!process_treasure_table(amulet_treasure_table,
                                    arraysize(amulet_treasure_table),
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

    process_treasure_set_item_power(op, item_power);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the amulet type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(amulet)
{
    OBJECT_METHODS(AMULET)->apply_func = object_apply_item;
    OBJECT_METHODS(AMULET)->process_treasure_func = process_treasure_func;
}

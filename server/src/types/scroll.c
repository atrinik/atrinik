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
 * Handles code for @ref SCROLL "scrolls".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>
#include <artifact.h>

#include "common/process_treasure.h"

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (QUERY_FLAG(applier, FLAG_BLIND)) {
        draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
        return OBJECT_METHOD_OK;
    }

    if (!QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        identify(op);
    }

    if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS) {
        draw_info(COLOR_WHITE, applier,
                  "The scroll just doesn't make sense!");
        return OBJECT_METHOD_OK;
    }

    if (applier->type == PLAYER) {
        /* Players need a literacy skill to read scrolls. */
        if (!change_skill(applier, SK_LITERACY)) {
            draw_info(COLOR_WHITE, applier,
                      "You are unable to decipher the strange symbols.");
            return OBJECT_METHOD_OK;
        }

        /* Also need the appropriate skill for the scroll's spell. */
        if (!change_skill(applier, SK_WIZARDRY_SPELLS)) {
            draw_info(COLOR_WHITE, applier,
                      "You can read the scroll but you don't understand it.");
            return OBJECT_METHOD_OK;
        }

        CONTR(applier)->stat_scrolls_used++;
    }

    draw_info_format(COLOR_WHITE, applier,
                     "The scroll of %s turns to dust.",
                     spells[op->stats.sp].name);

    int direction = applier->direction ? applier->direction : SOUTHEAST;
    cast_spell(applier,
               op,
               direction,
               op->stats.sp,
               0,
               CAST_SCROLL,
               NULL);
    decrease_ob(op);

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::process_treasure_func */
static int
process_treasure_func (object  *op,
                       object **ret,
                       int      difficulty,
                       int      affinity,
                       int      flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 0);

    /* Avoid processing if the item is already special. */
    if (process_treasure_is_special(op)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* Attempt to generate an artifact. */
    for (int tries = 0; op->stats.sp == SP_NO_SPELL; tries++) {
        /* Give it a few tries. */
        if (tries >= 5) {
            log_error("Failed to generate a spell for scroll: %s",
                      object_get_str(op));
            object_remove(op, 0);
            object_destroy(op);
            return OBJECT_METHOD_ERROR;
        }

        artifact_generate(op, difficulty, affinity);
    }

    SET_FLAG(op, FLAG_IS_MAGICAL);

    /* Calculate the level. Essentially -20 below the difficulty at worst, or
     * +15 above the difficulty at best. */
    int level = difficulty - 20 + rndm(0, 35);
    level = MIN(MAX(level, 1), MAXLEVEL);
    op->level = level;

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the scroll type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(scroll)
{
    OBJECT_METHODS(SCROLL)->apply_func = apply_func;
    OBJECT_METHODS(SCROLL)->process_treasure_func = process_treasure_func;
    OBJECT_METHODS(SCROLL)->override_treasure_processing = true;
}

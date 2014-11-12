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
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    (void) aflags;

    if (QUERY_FLAG(applier, FLAG_BLIND)) {
        draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
        return OBJECT_METHOD_OK;
    }

    if (!QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        identify(op);
    }

    if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS) {
        draw_info(COLOR_WHITE, applier, "The scroll just doesn't make sense!");
        return OBJECT_METHOD_OK;
    }

    if (applier->type == PLAYER) {
        /* Players need a literacy skill to read scrolls. */
        if (!change_skill(applier, SK_LITERACY)) {
            draw_info(COLOR_WHITE, applier, "You are unable to decipher the strange symbols.");
            return OBJECT_METHOD_OK;
        }

        /* Also need the appropriate skill for the scroll's spell. */
        if (!change_skill(applier, SK_WIZARDRY_SPELLS)) {
            draw_info(COLOR_WHITE, applier, "You can read the scroll but you don't understand it.");
            return OBJECT_METHOD_OK;
        }

        CONTR(applier)->stat_scrolls_used++;
    }

    draw_info_format(COLOR_WHITE, applier, "The scroll of %s turns to dust.", spells[op->stats.sp].name);

    cast_spell(applier, op, applier->direction ? applier->direction : SOUTHEAST, op->stats.sp, 0, CAST_SCROLL, NULL);
    decrease_ob(op);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the scroll type object methods. */
void object_type_init_scroll(void)
{
    object_type_methods[SCROLL].apply_func = apply_func;
}

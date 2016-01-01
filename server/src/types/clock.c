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
 * Handles code related to @ref CLOCK "clocks".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object.h>
#include <object_methods.h>

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->map == NULL) {
        return;
    }

    if (op->last_heal > 0) {
        play_sound_map(op->map,
                       CMD_SOUND_EFFECT,
                       "clock.ogg",
                       op->x,
                       op->y,
                       0,
                       0);
        op->last_heal--;
        return;
    }

    timeofday_t tod;
    get_tod(&tod);

    if (tod.hour == op->last_sp) {
        return;
    }

    op->last_sp = tod.hour;
    if ((tod.hour % (HOURS_PER_DAY / 2)) == 0) {
        op->last_heal = HOURS_PER_DAY / 2;
    } else {
        op->last_heal = tod.hour % (HOURS_PER_DAY / 2);
    }
}

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_UNHANDLED;
    }

    timeofday_t tod;
    get_tod(&tod);
    int hour;
    if ((tod.hour % (HOURS_PER_DAY / 2)) == 0) {
        hour = HOURS_PER_DAY / 2;
    } else {
        hour =  tod.hour % (HOURS_PER_DAY / 2);
    }

    draw_info_format(COLOR_WHITE, applier,
                     "It is %d minute%s past %d o'clock %s.",
                     tod.minute,
                     tod.minute == 1 ? "" : "s",
                     hour,
                     tod.hour >= HOURS_PER_DAY / 2 ? "pm" : "am");

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::insert_map_func */
static void
insert_map_func (object *op)
{
    timeofday_t tod;
    get_tod(&tod);
    op->last_sp = tod.hour;
}

/**
 * Initialize the clock type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(clock)
{
    OBJECT_METHODS(CLOCK)->process_func = process_func;
    OBJECT_METHODS(CLOCK)->apply_func = apply_func;
    OBJECT_METHODS(CLOCK)->insert_map_func = insert_map_func;
}

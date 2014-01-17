/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Handles code for @ref SPELL "spells".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::ranged_fire_func */
static int ranged_fire_func(object *op, object *shooter, int dir, double *delay)
{
    int cost;

    cost = cast_spell(shooter, shooter, dir, op->stats.sp, 0, CAST_NORMAL, NULL);

    if (cost) {
        shooter->stats.sp -= cost;

        if (delay) {
            *delay = spells[op->stats.sp].time;
        }

        return OBJECT_METHOD_OK;
    }

    return OBJECT_METHOD_UNHANDLED;
}

/**
 * Initialize the spell type object methods. */
void object_type_init_spell(void)
{
    object_type_methods[SPELL].apply_func = object_apply_item;
    object_type_methods[SPELL].ranged_fire_func = ranged_fire_func;
}

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
 * Handles code for @ref PEDESTAL "pedestals".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Check whether pedestal matches the specified object.
 * @param op Pedestal.
 * @param tmp Object to check.
 * @return 1 if the object matches, 0 otherwise. */
int pedestal_matches_obj(object *op, object *tmp)
{
    /* Check type. */
    if (op->stats.hp && tmp->type != op->stats.hp) {
        return 0;
    }

    /* Check name. */
    if (op->slaying && tmp->name != op->slaying) {
        return 0;
    }

    /* Check archname. */
    if (op->race && tmp->arch->name != op->race) {
        return 0;
    }

    return 1;
}

/** @copydoc object_methods::trigger_button_func */
static int trigger_button_func(object *op, object *cause, int state)
{
    object *tmp, *head;

    (void) cause;
    (void) state;

    op->value = 0;

    for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = tmp->above) {
        head = HEAD(tmp);

        if (head != op && (QUERY_FLAG(head, FLAG_FLYING) ? QUERY_FLAG(op, FLAG_FLY_ON) : QUERY_FLAG(op, FLAG_WALK_ON)) && pedestal_matches_obj(op, head)) {
            op->value = 1;
        }
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the pedestal type object methods. */
void object_type_init_pedestal(void)
{
    object_type_methods[PEDESTAL].trigger_button_func = trigger_button_func;
    object_type_methods[PEDESTAL].fallback = &object_type_methods[BUTTON];
}

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
 * Implements the /push command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_push(object *op, const char *command, char *params)
{
    object *tmp;
    mapstruct *m;
    int x, y, dir;

    dir = op->facing;

    /* We check for all conditions where the player can't push anything. */
    if (dir <= 0 || QUERY_FLAG(op, FLAG_PARALYZED)) {
        draw_info(COLOR_WHITE, op, "You are unable to push anything.");
        return;
    }

    x = op->x + freearr_x[dir];
    y = op->y + freearr_y[dir];
    m = get_map_from_coord(op->map, &x, &y);

    if (!m) {
        return;
    }

    for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above) {
        if (QUERY_FLAG(tmp, FLAG_CAN_ROLL)) {
            break;
        }
    }

    if (!tmp) {
        draw_info(COLOR_WHITE, op, "You fail to push anything.");
        return;
    }

    tmp->direction = dir;

    /* Try to push the object. */
    if (!push_ob(tmp, dir, op)) {
        draw_info_format(COLOR_WHITE, op, "You fail to push the %s.", query_name(tmp, NULL));
        return;
    }

    /* Now we move the player who was pushing the object. */
    move_ob(op, dir, op);
    draw_info_format(COLOR_WHITE, op, "You push the %s.", query_name(tmp, NULL));
}

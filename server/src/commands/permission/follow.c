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
 * Implements the /follow command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>

/** @copydoc command_func */
void command_follow(object *op, const char *command, char *params)
{
    player *pl;

    if (!params) {
        if (*CONTR(op)->followed_player != '\0') {
            draw_info_format(COLOR_WHITE, op, "You stop following %s.", CONTR(op)->followed_player);
            CONTR(op)->followed_player[0] = '\0';
        }

        return;
    }

    pl = find_player(params);

    if (!pl) {
        draw_info(COLOR_WHITE, op, "No such player.");
        return;
    }

    strncpy(CONTR(op)->followed_player, params, sizeof(CONTR(op)->followed_player) - 1);
    CONTR(op)->followed_player[sizeof(CONTR(op)->followed_player) - 1] = '\0';

    draw_info_format(COLOR_GREEN, op, "Following %s.", params);
}

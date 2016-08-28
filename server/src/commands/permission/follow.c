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
void
command_follow (object *op, const char *command, char *params)
{
    player *pl = CONTR(op);

    if (params == NULL) {
        if (pl->followed_player == NULL) {
            /* Not following anyone. */
            draw_info(COLOR_WHITE, op, "Usage: /follow <player>");
            return;
        }

        player *followed = find_player_sh(pl->followed_player);
        if (!IS_INVISIBLE(op, followed->ob)) {
            draw_info_format(COLOR_WHITE, followed->ob,
                             "%s is no longer following you.",
                             op->name);
        }

        draw_info_format(COLOR_WHITE, op,
                         "You stop following %s.",
                         pl->followed_player);
        FREE_AND_CLEAR_HASH(pl->followed_player);

        return;
    }

    player *followed = find_player(params);
    if (followed == NULL) {
        draw_info(COLOR_WHITE, op, "No such player.");
        return;
    }

    if (!IS_INVISIBLE(op, followed->ob)) {
        draw_info_format(COLOR_GREEN, followed->ob,
                         "%s is now following you.",
                         op->name);
    }

    pl->followed_player = add_string(params);
    draw_info_format(COLOR_GREEN, op,
                     "You are now following %s.",
                     pl->followed_player);
}

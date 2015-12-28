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
 * Implements the /who command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>

/** @copydoc command_func */
void command_who(object *op, const char *command, char *params)
{
    player *pl;
    int ip = 0;
    char buf[MAX_BUF];

    draw_info(COLOR_WHITE, op, " ");

    for (pl = first_player; pl; pl = pl->next) {
        ip++;

        snprintf(buf, sizeof(buf), "%s the %s %s (lvl %d)", pl->ob->name, gender_noun[object_get_gender(pl->ob)], pl->ob->race, pl->ob->level);

        if (pl->afk) {
            strncat(buf, " [AFK]", sizeof(buf) - strlen(buf) - 1);
        }

        if (pl->socket.is_bot) {
            strncat(buf, " [BOT]", sizeof(buf) - strlen(buf) - 1);
        }

        draw_info(COLOR_WHITE, op, buf);
    }

    draw_info_format(COLOR_WHITE, op, "There %s %d player%s online.", ip > 1 ? "are" : "is", ip, ip > 1 ? "s" : "");
}

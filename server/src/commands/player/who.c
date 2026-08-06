/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * @author Zoey Rose
 */

#include <global.h>
#include <toolkit/string.h>
#include <player.h>
#include <object.h>

/** @copydoc command_func */
void command_who(object *op, const char *command, char *params) {
    player *pl;
    int ip = 0;
    char buf[MAX_BUF];
    bool show_connection = commands_check_permission(CONTR(op), "stats");

    draw_info(COLOR_WHITE, op, " ");

    for (pl = first_player; pl; pl = pl->next) {
        ip++;

        snprintf(buf,
                 sizeof(buf),
                 "%s the %s %s (lvl %d)",
                 pl->ob->name,
                 gender_noun[object_get_gender(pl->ob)],
                 pl->ob->race,
                 pl->ob->level);

        if (pl->afk) {
            snprintfcat(buf, sizeof(buf), " [AFK]");
        }

        if (pl->cs->is_bot) {
            snprintfcat(buf, sizeof(buf), " &lsqb;BOT&rsqb;");
        }

        if (show_connection) {
            snprintfcat(buf,
                        sizeof(buf),
                        " (route: %s; connection: %s)",
                        socket_connection_mode_name(pl->cs->connection_mode),
                        socket_get_id(pl->cs->sc));
        }

        draw_info(COLOR_WHITE, op, buf);
    }

    draw_info_format(COLOR_WHITE,
                     op,
                     "There %s %d player%s online.",
                     ip > 1 ? "are" : "is",
                     ip,
                     ip > 1 ? "s" : "");
}

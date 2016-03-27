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
 * Implements the /mod_chat command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>

/** @copydoc command_func */
void command_mod_chat(object *op, const char *command, char *params)
{
    player *pl;
    char name[MAX_BUF];

    params = player_sanitize_input(params);

    if (!params) {
        return;
    }

    LOG(CHAT, "[MOD CHAT] [%s] %s", op->name, params);

    for (pl = first_player; pl; pl = pl->next) {
        if (commands_check_permission(pl, command)) {
            snprintf(name, sizeof(name), "[Moderator] ([a=#charname]%s[/a])", op->name);
        } else {
            strncpy(name, "[Moderator]", sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }

        draw_info_type(CHAT_TYPE_CHAT, NULL, COLOR_BRIGHT_PURPLE, pl->ob, params);
    }
}

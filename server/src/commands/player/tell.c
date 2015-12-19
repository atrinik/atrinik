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
 * Implements the /tell command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

/** @copydoc command_func */
void command_tell(object *op, const char *command, char *params)
{
    char name[MAX_BUF], *msg, buf[HUGE_BUF];
    size_t pos;
    player *pl;

    pos = 0;

    if (!string_get_word(params, &pos, ' ', name, sizeof(name), '"')) {
        draw_info(COLOR_WHITE, op, "Tell whom what?");
        return;
    }

    msg = player_sanitize_input(params + pos);

    if (!msg) {
        draw_info_format(COLOR_WHITE, op, "Tell %s what?", name);
        return;
    }

    pl = find_player(name);

    if (!pl) {
        draw_info(COLOR_WHITE, op, "No such player.");
        return;
    }

    /* Send to yourself? Intelligent... */
    if (pl == CONTR(op)) {
        draw_info(COLOR_WHITE, op, "You tell yourself the news. Very smart.");
        return;
    }

    strncpy(pl->player_reply, op->name, sizeof(pl->player_reply) - 1);
    pl->player_reply[sizeof(pl->player_reply) - 1] = '\0';

    LOG(CHAT, "[TELL] [%s] [%s] %s", op->name, name, msg);

    snprintf(buf, sizeof(buf), "[a=#charname]%s[/a] tells you: %s", op->name, msg);
    draw_info_type(CHAT_TYPE_PRIVATE, NULL, COLOR_NAVY, pl->ob, buf);

    snprintf(buf, sizeof(buf), "You tell [a=#charname]%s[/a]: %s", pl->ob->name, msg);
    draw_info_type(CHAT_TYPE_PRIVATE, NULL, COLOR_NAVY, op, buf);

    /* Auto-reply message available */
    if (CONTR(pl->ob)->afk_auto_reply[0] != '\0') {
        snprintf(buf, sizeof(buf), "[a=#charname]%s[/a] auto-replies: %s", pl->ob->name, CONTR(pl->ob)->afk_auto_reply);
        draw_info_type(CHAT_TYPE_PRIVATE, NULL, COLOR_NAVY, op, buf);
    } else if (CONTR(pl->ob)->afk) {
        /* Player is AFK */
        snprintf(buf, sizeof(buf), "[a=#charname]%s[/a] is currently [AFK].", pl->ob->name);
        draw_info_type(CHAT_TYPE_PRIVATE, NULL, COLOR_NAVY, op, buf);
    }
}

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
 * Implements the /afk command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_afk(object *op, const char *command, char *params)
{
    params = player_sanitize_input(params);

    /* No auto-reply message given*/
    if (!params) {
        CONTR(op)->afk_auto_reply[0] = '\0';

        /* Currently afk */
        if (CONTR(op)->afk) {
            CONTR(op)->afk = 0;
            draw_info(COLOR_WHITE, op, "You are no longer AFK.");
        } else {
            /* Currently not afk */
            CONTR(op)->afk = 1;
            CONTR(op)->stat_afk_used++;
            draw_info(COLOR_WHITE, op, "You are now AFK.");
        }
    } else {
        /* Auto-reply message given */

        CONTR(op)->afk = 1;
        CONTR(op)->stat_afk_used++;
        draw_info_format(COLOR_WHITE, op, "You are now AFK. Auto-reply: %s", params);

        LOG(CHAT, "[AFK] [%s] %s", op->name, params);
        strncpy(CONTR(op)->afk_auto_reply, params, sizeof(CONTR(op)->afk_auto_reply) - 1);
        CONTR(op)->afk_auto_reply[sizeof(CONTR(op)->afk_auto_reply) - 1] = '\0';
    }

    CONTR(op)->socket.ext_title_flag = 1;
}

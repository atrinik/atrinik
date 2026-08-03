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
 * Implements the /ban command.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <toolkit/string.h>
#include <ban.h>
#include <player.h>
#include <object.h>

/** @copydoc command_func */
void command_ban(object *op, const char *command, char *params)
{
    size_t pos = 0;
    char word[MAX_BUF];
    if (!string_get_word(params, &pos, ' ', VS(word), '"')) {
        draw_info(COLOR_WHITE,
                  op,
                  "Usage: /ban add|remove <player|*> [account|*], "
                  "/ban list, or /ban kick");
        return;
    }

    params = player_sanitize_input(params + pos);

    if (strcmp(word, "add") == 0) {
        if (params == NULL) {
            draw_info(COLOR_WHITE,
                      op,
                      "Usage: /ban add <player|*> [account|*]");
            return;
        }

        ban_error_t rc = ban_add(params);
        if (rc == BAN_OK) {
            draw_info(COLOR_GREEN, op, "Added new ban successfully.");
        } else {
            draw_info_format(COLOR_RED, op, "Failed to add new ban: %s",
                    ban_strerror(rc));
        }
    } else if (strcmp(word, "remove") == 0) {
        if (params == NULL) {
            draw_info(COLOR_WHITE,
                      op,
                      "Usage: /ban remove <player|#ID|*> [account|*]");
            return;
        }

        ban_error_t rc = ban_remove(params);
        if (rc == BAN_OK) {
            draw_info(COLOR_GREEN, op, "Removed ban successfully.");
        } else {
            draw_info_format(COLOR_RED, op, "Failed to remove ban: %s",
                    ban_strerror(rc));
        }
    } else if (strcmp(word, "list") == 0) {
        ban_list(op);
    } else if (strcmp(word, "kick") == 0) {
        for (player *pl = first_player; pl != NULL; pl = pl->next) {
            if (ban_check(pl->ob->name, pl->cs->account)) {
                LOG(SYSTEM,
                        "Connection %s: kicking player %s due to a ban",
                        socket_get_id(pl->cs->sc), pl->ob->name);
                draw_info_type(CHAT_TYPE_GAME, NULL, COLOR_RED, pl->ob,
                        "You have been banned.");
                pl->cs->state = ST_ZOMBIE;
            }
        }
    } else {
        draw_info(COLOR_WHITE,
                  op,
                  "Usage: /ban add|remove <player|*> [account|*], "
                  "/ban list, or /ban kick");
    }
}

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
 * Implements the /shutdown command.
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

/** @copydoc command_func */
void command_shutdown(object *op, const char *command, char *params)
{
    char when[MAX_BUF];
    int mins, secs;
    size_t pos;

    pos = 0;

    if (!string_get_word(params, &pos, ' ', when, sizeof(when), 0)) {
        return;
    }

    if (strcasecmp(when, "stop") == 0) {
        shutdown_timer_stop();
        draw_info_type(CHAT_TYPE_CHAT, NULL, COLOR_GREEN, NULL, "[Server]: Server shut down stopped.");
    } else if (sscanf(when, "%d:%d", &mins, &secs) == 2) {
        char *reason;

        secs = MAX(30, MAX(0, MIN(60, secs)) + MAX(0, mins) * 60);
        reason = player_sanitize_input(params + pos);

        shutdown_timer_start(secs);
        draw_info_type_format(CHAT_TYPE_CHAT, NULL, COLOR_GREEN, NULL, "[Server]: Server shut down started; will shut down in %02d:%02d minutes.", secs / 60, secs % 60);

        if (reason) {
            draw_info_type_format(CHAT_TYPE_CHAT, NULL, COLOR_GREEN, NULL, "[Server]: %s", reason);
        }
    }
}

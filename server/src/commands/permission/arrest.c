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
 * Implements the /arrest command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_arrest(object *op, const char *command, char *params)
{
    object *dummy;
    player *pl;

    if (!params) {
        draw_info(COLOR_WHITE, op, "Usage: /arrest <player>");
        return;
    }

    pl = find_player(params);

    if (!pl) {
        draw_info(COLOR_WHITE, op, "No such player.");
        return;
    }

    if (!region_enter_jail(pl->ob)) {
        /* We have nowhere to send the prisoner....*/
        draw_info(COLOR_RED, op, "Can't jail player, there is no map to hold them.");
        return;
    }

    draw_info_format(COLOR_GREEN, op, "Jailed %s.", pl->ob->name);
    logger_print(LOG(CHAT), "[ARREST] Player %s arrested by %s.", pl->ob->name, op->name);
}

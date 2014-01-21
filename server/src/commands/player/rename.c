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
 * Implements the /rename command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_rename(object *op, const char *command, char *params)
{
    object *tmp;

    tmp = find_marked_object(op);

    if (!tmp) {
        draw_info(COLOR_WHITE, op, "No marked item to rename.");
        return;
    }

    params = player_sanitize_input(params);

    /* Clear custom name. */
    if (!params) {
        if (!tmp->custom_name) {
            draw_info(COLOR_WHITE, op, "This item has no custom name.");
            return;
        }

        FREE_AND_CLEAR_HASH(tmp->custom_name);
        draw_info_format(COLOR_WHITE, op, "You stop calling your %s with weird names.", query_base_name(tmp, NULL));
    }
    else {
        if (tmp->type == MONEY) {
            draw_info(COLOR_WHITE, op, "You cannot rename that item.");
            return;
        }

        if (strlen(params) > 127) {
            draw_info(COLOR_WHITE, op, "New name is too long, maximum is 127 characters.");
            return;
        }

        if (tmp->custom_name && strcmp(tmp->custom_name, params) == 0) {
            draw_info_format(COLOR_WHITE, op, "You keep calling your %s %s.", query_base_name(tmp, NULL), tmp->custom_name);
            return;
        }

        /* Set custom name. */
        FREE_AND_COPY_HASH(tmp->custom_name, params);
        draw_info_format(COLOR_WHITE, op, "Your %s will now be called %s.", query_base_name(tmp, NULL), tmp->custom_name);
        CONTR(op)->stat_renamed_items++;
    }

    esrv_update_item(UPD_NAME, tmp);
    object_merge(tmp);
}

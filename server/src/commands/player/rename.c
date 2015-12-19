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
 * Implements the /rename command.
 *
 * @author Alex Tokar
 */

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
    if (params == NULL) {
        if (tmp->custom_name == NULL) {
            draw_info(COLOR_WHITE, op, "This item has no custom name.");
            return;
        }

        FREE_AND_CLEAR_HASH(tmp->custom_name);
        char *name = object_get_base_name_s(tmp, op);
        draw_info_format(COLOR_WHITE, op, "You stop calling your %s with weird "
                "names.", name);
        efree(name);
    } else {
        if (tmp->type == MONEY) {
            draw_info(COLOR_WHITE, op, "You cannot rename that item.");
            return;
        }

        if (strlen(params) > 127) {
            draw_info(COLOR_WHITE, op, "New name is too long, maximum is 127 characters.");
            return;
        }

        char *name = object_get_base_name_s(tmp, op);

        if (tmp->custom_name != NULL && strcmp(tmp->custom_name, params) == 0) {
            draw_info_format(COLOR_WHITE, op, "You keep calling your %s %s.",
                    name, tmp->custom_name);
            efree(name);
            return;
        }

        /* Set custom name. */
        FREE_AND_COPY_HASH(tmp->custom_name, params);
        draw_info_format(COLOR_WHITE, op, "Your %s will now be called %s.",
                name, tmp->custom_name);
        CONTR(op)->stat_renamed_items++;
        efree(name);
    }

    esrv_update_item(UPD_NAME, tmp);
    object_merge(tmp);
}

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Implements the /cmd_permission command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_cmd_permission(object *op, const char *command, char *params)
{
	char word[MAX_BUF];
	size_t pos;
	player *pl;
	int i;

	pos = 0;

	if (!string_get_word(params, &pos, word, sizeof(word)))
	{
		draw_info(COLOR_RED, op, "Usage: /cmd_permission <player> <add|remove|list> [command]");
		return;
	}

	pl = find_player(word);

	if (!pl)
	{
		draw_info(COLOR_WHITE, op, "No such player.");
		return;
	}

	string_get_word(params, &pos, word, sizeof(word));
	params = player_sanitize_input(params + pos);

	if (strcmp(word, "add") == 0)
	{
		if (!params)
		{
			return;
		}

		for (i = 0; i < pl->num_cmd_permissions; i++)
		{
			if (pl->cmd_permissions[i] && !strcmp(pl->cmd_permissions[i], params))
			{
				draw_info_format(COLOR_RED, op, "%s already has permission for /%s.", pl->ob->name, params);
				return;
			}
		}

		pl->num_cmd_permissions++;
		pl->cmd_permissions = realloc(pl->cmd_permissions, sizeof(char *) * pl->num_cmd_permissions);
		pl->cmd_permissions[pl->num_cmd_permissions - 1] = strdup(params);
		draw_info_format(COLOR_GREEN, op, "%s has been granted permission for /%s.", pl->ob->name, params);
	}
	else if (strcmp(word, "remove") == 0)
	{
		if (!params)
		{
			return;
		}

		for (i = 0; i < pl->num_cmd_permissions; i++)
		{
			if (pl->cmd_permissions[i] && !strcmp(pl->cmd_permissions[i], params))
			{
				FREE_AND_NULL_PTR(pl->cmd_permissions[i]);
				draw_info_format(COLOR_GREEN, op, "%s has had permission for /%s command removed.", pl->ob->name, params);
				return;
			}
		}

		draw_info_format(COLOR_RED, op, "%s does not have permission for /%s.", pl->ob->name, params);
	}
	else if (strcmp(word, "list") == 0)
	{
		if (pl->cmd_permissions)
		{
			draw_info_format(COLOR_WHITE, op, "%s has permissions for the following commands:\n", pl->ob->name);

			for (i = 0; i < pl->num_cmd_permissions; i++)
			{
				if (pl->cmd_permissions[i])
				{
					draw_info_format(COLOR_WHITE, op, "/%s", pl->cmd_permissions[i]);
				}
			}
		}
		else
		{
			draw_info_format(COLOR_WHITE, op, "%s has no command permissions.", pl->ob->name);
		}
	}
}

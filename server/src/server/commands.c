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
 * Commands API.
 *
 * @author Alex Tokar */

#include <global.h>

static command_struct *commands;

/**
 * Initialize the commands API.
 * @internal */
void toolkit_commands_init(void)
{
	TOOLKIT_INIT_FUNC_START(commands)
	{
		commands = NULL;

		/* [operator] */
		commands_add(COMMAND(arrest), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(ban), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(cmd_permission), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(follow), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(freeze), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(goto), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(kick), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(mod_shout), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(no_shout), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(opsay), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(resetmap), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(server_shout), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(setpassword), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(settime), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(shutdown), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(summon), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tcl), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(teleport), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tgm), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tli), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tls), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tsi), 0.0, COMMAND_PERMISSION);

		/* [player] */
		commands_add(COMMAND(afk), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(apply), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(cast), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(drop), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(gsay), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(hiscore), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(left), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(motd), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(party), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(pray), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(push), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(ready_skill), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(region_map), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(rename), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(reply), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(right), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(say), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(shout), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(statistics), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(sys_tell), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(t_tell), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(take), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(tell), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(time), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(use_skill), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(version), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(whereami), 0.0, COMMAND_PERMISSION);
		commands_add(COMMAND(who), 0.0, COMMAND_PERMISSION);
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the commands API.
 * @internal */
void toolkit_commands_deinit(void)
{
	command_struct *curr, *tmp;

	HASH_ITER(hh, commands, curr, tmp)
	{
		HASH_DEL(commands, curr);
		free(curr->name);
		free(curr);
	}
}

void commands_add(const char *name, command_func handle_func, double delay, uint64 flags)
{
	command_struct *command;

	command = malloc(sizeof(*command));
	command->name = strdup(name);
	command->handle_func = handle_func;
	command->delay = delay;
	command->flags = flags;

	HASH_ADD_KEYPTR(hh, commands, command->name, strlen(command->name), command);
}

int commands_check_permission(player *pl, const char *command)
{
	int i;

	for (i = 0; i < pl->num_cmd_permissions; i++)
	{
		if (strcmp(pl->cmd_permissions[i], command) == 0)
		{
			return 1;
		}
	}

	return 0;
}

void commands_handle(object *op, char *cmd)
{
	if (cmd[0] == '/' && cmd[1] != '\0')
	{
		char *cp, *params;
		command_struct *command;

		cmd++;
		cp = strchr(cmd, ' ');

		if (cp)
		{
			cmd[cp - cmd] = '\0';
			params = cp + 1;

			if (*params == '\0')
			{
				params = NULL;
			}
		}
		else
		{
			params = NULL;
		}

		HASH_FIND(hh, commands, cmd, strlen(cmd), command);

		if (command)
		{
			op->speed_left -= command->delay;
			command->handle_func(op, cmd, params);
			return;
		}
	}

	draw_info_format(COLOR_WHITE, op, "'/%s' is not a valid command.", cmd);
}

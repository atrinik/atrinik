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
 * Interactive console API.
 *
 * In order to add a new command to the array of console's available
 * commands, use console_command_add().
 *
 * The API automatically creates the 'help' command at initialization
 * time.
 *
 * In order to actually allow usage of the console through terminal's
 * standard input, you will need to call console_command_handle() in your
 * program's main loop every iteration.
 *
 * @author Alex Tokar */

#include <global.h>

#ifndef WIN32
#	include <poll.h>
/**
 * stdin information for poll(). */
static struct pollfd stdin_fd[1];
#else
/**
 * stdin handle. */
static HANDLE stdin_handle;
#endif

/**
 * Dynamic array of all the possible console commands. */
static console_command_struct *console_commands;

/**
 * Number of ::console_commands. */
size_t console_commands_num;

/**
 * The help command of the console.
 * @param params Command parameters. */
static void console_command_help(const char *params)
{
	size_t i;

	/* Parameters provided, so give out info about the command. */
	if (params)
	{
		for (i = 0; i < console_commands_num; i++)
		{
			if (strcmp(console_commands[i].command, params) == 0)
			{
				char *curr, *next, *cp;

				LOG(llevInfo, "##### Command: %s #####\n", console_commands[i].command);
				LOG(llevInfo, "\n");

				for (curr = console_commands[i].desc; (curr && (next = strchr(curr, '\n'))) || curr; curr = next ? next + 1 : NULL)
				{
					cp = strndup(curr, next - curr);
					LOG(llevInfo, "%s\n", cp);
					free(cp);
				}

				return;
			}
		}

		LOG(llevInfo, "No such command '%s'.\n", params);
	}
	/* Otherwise brief information about all available commands. */
	else
	{
		LOG(llevInfo, "List of available commands:\n");
		LOG(llevInfo, "\n");

		for (i = 0; i < console_commands_num; i++)
		{
			LOG(llevInfo, "\t- %s: %s\n", console_commands[i].command, console_commands[i].desc_brief);
		}

		LOG(llevInfo, "\n");
		LOG(llevInfo, "Use 'help <command>' to learn more about the specific command.\n");
	}
}

/**
 * Initialize the console API.
 * @internal */
void toolkit_console_init(void)
{
	TOOLKIT_INIT_FUNC_START(console)
	{
		toolkit_import(memory);
		console_commands = NULL;
		console_commands_num = 0;

#ifndef WIN32
		stdin_fd[0].fd = fileno(stdin);
		stdin_fd[0].events = POLLIN;
#else
		stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
#endif

		/* Add the 'help' command. */
		console_command_add(
			"help",
			console_command_help,
			"Displays this help.",
			"Displays the help, listing available console commands, etc.\n\n"
			"'help <command>' can be used to get more detailed help about the specified command."
		);
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the console API.
 * @internal */
void toolkit_console_deinit(void)
{
	size_t i;

	for (i = 0; i < console_commands_num; i++)
	{
		free(console_commands[i].command);
		free(console_commands[i].desc_brief);
		free(console_commands[i].desc);
	}

	if (console_commands)
	{
		free(console_commands);
		console_commands = NULL;
	}

	console_commands_num = 0;
}

/**
 * Add a possible command to the console.
 * @param command Command name, must be unique.
 * @param handle_func Function that will handle the command.
 * @param desc_brief Brief, one-line description of the command.
 * @param desc More detailed description of the command. */
void console_command_add(const char *command, console_command_func handle_func, const char *desc_brief, const char *desc)
{
	size_t i;

	/* Make sure the command doesn't exist yet. */
	for (i = 0; i < console_commands_num; i++)
	{
		if (strcmp(console_commands[i].command, command) == 0)
		{
			LOG(llevBug, "console_command_add(): Tried to add duplicate entry for command '%s'.\n", command);
			return;
		}
	}

	/* Add it to the commands array. */
	console_commands = memory_reallocz(console_commands, sizeof(*console_commands) * console_commands_num, sizeof(*console_commands) * (console_commands_num + 1));
	console_commands[console_commands_num].command = strdup(command);
	console_commands[console_commands_num].handle_func = handle_func;
	console_commands[console_commands_num].desc_brief = strdup(desc_brief);
	console_commands[console_commands_num].desc = strdup(desc);
	console_commands_num++;
}

/**
 * Process the console API. This should usually be part of the program's
 * main loop. */
void console_command_handle(void)
{
	ssize_t numread;
	char *line, *cp;
	size_t len, i;

	/* If stdin is not connected to a terminal, no point in going on. */
	if (!isatty(fileno(stdin)))
	{
		return;
	}

	/* If no input is ready yet, quit. */
#ifndef WIN32
	if (poll(stdin_fd, 1, 0) == 0)
#else
	if (WaitForSingleObject(stdin_handle, 0) != WAIT_OBJECT_0)
#endif
	{
		return;
	}

	line = NULL;
	numread = getline(&line, &len, stdin);

	if (numread <= 0)
	{
		return;
	}

	/* Remove the newline. */
	cp = strchr(line, '\n');

	if (cp)
	{
		*cp = '\0';
	}

	/* Remove the command from the parameters. */
	cp = strchr(line, ' ');

	if (cp)
	{
		*(cp++) = '\0';

		if (cp && *cp == '\0')
		{
			cp = NULL;
		}
	}

	for (i = 0; i < console_commands_num; i++)
	{
		if (strcmp(console_commands[i].command, line) == 0)
		{
			console_commands[i].handle_func(cp);
			break;
		}
	}

	free(line);
}

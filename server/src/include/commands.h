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
 * Commands header file.
 *
 * @author Alex Tokar */

#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * Format for a command handler function.
 * @param op The player.
 * @param command The command's name.
 * @param params Optional arguments for the command. */
typedef void (*command_func)(object *op, const char *command, char *params);

/**
 * A single command. */
typedef struct command_struct
{
	/**
	 * Name of the command. */
	char *name;

	/**
	 * Handler function. */
	command_func handle_func;

	/**
	 * Time the player must wait before doing another command. */
	double delay;

	/**
	 * A combination of @ref COMMAND_xxx. */
	uint64 flags;

	/**
	 * Hash handle. */
	UT_hash_handle hh;
} command_struct;

/**
 * A single permission group. */
typedef struct permission_group_struct
{
	/**
	 * Name, eg, '[OP]'. */
	char *name;

	/**
	 * The command permissions for this group. */
	char **cmd_permissions;

	/**
	 * Number of command permissions. */
	size_t cmd_permissions_num;

	/**
	 * Hash handle. */
	UT_hash_handle hh;
} permission_group_struct;

/**
 * @defgroup COMMAND_xxx Command flags
 * Command flags.
 *@{*/
/**
 * The command requires a permission. */
#define COMMAND_PERMISSION 1
/*@}*/

/**
 * Shortcut macro for commands_add(). */
#define COMMAND(__name) #__name, command_##__name

/**
 * Execute the specific command. */
#define COMMAND_EXECUTE(__op, __command, __params) command_##__command((__op), #__command, (__params))

#endif

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
 * Console API header file.
 *
 * @author Alex Tokar */

#ifndef CONSOLE_H
#define CONSOLE_H

/**
 * Console command handler function. */
typedef void (*console_command_func)(const char *params);

/**
 * One console command. */
typedef struct console_command_struct
{
	/** Name of the command. */
	char *command;

	/** The function that will handle the command. */
	console_command_func handle_func;

	/** Brief, one-line description of the command. */
	char *desc_brief;

	/** Detailed description of the command. */
	char *desc;
} console_command_struct;

#endif

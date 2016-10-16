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
 * Console API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_CONSOLE_H
#define TOOLKIT_CONSOLE_H

#include "toolkit.h"

/**
 * Console command handler function.
 */
typedef void (*console_command_func)(const char *params);

/**
 * One console command.
 */
typedef struct console_command_struct {
    /** Name of the command. */
    char *command;

    /** The function that will handle the command. */
    console_command_func handle_func;

    /** Brief, one-line description of the command. */
    char *desc_brief;

    /** Detailed description of the command. */
    char *desc;
} console_command_struct;

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(console);

/**
 * Start the console stdin-reading thread.
 *
 * @return
 * 1 on success, 0 on failure.
 */
extern int
console_start_thread(void);

/**
 * Add a possible command to the console.
 *
 * @param command
 * Command name, must be unique.
 * @param handle_func
 * Function that will handle the command.
 * @param desc_brief
 * Brief, one-line description of the command.
 * @param desc
 * More detailed description of the command.
 */
extern void
console_command_add(const char          *command,
                    console_command_func handle_func,
                    const char          *desc_brief,
                    const char          *desc);

/**
 * Process the console API. This should usually be part of the program's
 * main loop.
 */
extern void
console_command_handle(void);

#endif

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
 * Command-line options API header file.
 *
 * @author Alex Tokar
 */

#ifndef CLIOPTIONS_H
#define CLIOPTIONS_H

/**
 * Command line option handler function.
 * @param arg
 * Argument, if any.
 */
typedef void (*clioptions_handler_func)(const char *arg);

/**
 * A single command line option.
 */
typedef struct clioptions_struct {
    /**
     * Long option name, eg, 'verbose'.
 */
    char *longname;

    /**
     * Short option name, eg, 'v'.
 */
    char *shortname;

    /**
     * Handler function for the option.
 */
    clioptions_handler_func handle_func;

    /**
     * Whether this option accepts an argument.
 */
    uint8_t argument;

    /**
     * Brief description.
 */
    char *desc_brief;

    /**
     * More detailed description.
 */
    char *desc;
} clioptions_struct;

#endif

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * This is the config file for the client. */

/** Socket version. */
#define SOCKET_VERSION 1030

/** File with all the key bindings. */
#define KEYBIND_FILE "keys.dat"
/** File with the options. */
#define OPTION_FILE "options.dat"
/** File the the arch definitions. */
#define ARCHDEF_FILE "archdef.dat"
/** File with the widgets' positions. */
#define INTERFACE_FILE "interface.gui"
/** What scripts to autoload on client startup. */
#define SCRIPTS_AUTOLOAD "scripts_autoload"
/** Log file. */
#define LOG_FILE "client.log"
/** Name of the icon. */
#define CLIENT_ICON_NAME "icon.png"

/* Experimental feature of widget snapping */
/*#define WIDGET_SNAP*/

/** Maximum number of faces. */
#define MAX_FACE_TILES 30000

/** Maximum number of animations. */
#define MAXANIM 10000

/** Maximum map size. */
#define MAP_MAX_SIZE 17

/** Max string length in input string. */
#define MAX_INPUT_STRING 256
/** Max input history lines. */
#define MAX_HISTORY_LINES 20

/**
 * Maximum size of any packet we expect. Using this makes it so we don't
 * need to allocate and deallocate the same buffer over and over again
 * at the price of using a bit of extra memory. It also makes the code
 * simpler. */
#define MAXSOCKBUF (256 * 1024)

/** Should be the same as server's MAX_TIME. */
#define MAX_TIME 125000

/**
 * The number of our dark levels.
 *
 * For each level we store an own bitmap copy. */
#define DARK_LEVELS 7

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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

/* This is a config file for the client. */

#define VERSION_CS 991017
#define VERSION_SC 991017

#define KEYBIND_FILE 	"keys.dat"
#define OPTION_FILE  	"options.dat"
#define ARCHDEF_FILE 	"archdef.dat"
#define INTERFACE_FILE 	"interface.gui"

#define LOG_FILE		"client.log"

#define CLIENT_ICON_NAME "icon.png"

/* Socket timeout value */
#define MAX_TIME 0

/* Maximum level you can reach */
#define MAX_LEVEL 110

/* Experimental feature of widget snapping */
/*#define WIDGET_SNAP*/

/* Increase when we got MANY new face... Hopefully, we need to increase this
 * in the future... */
#define MAX_FACE_TILES 30000

#define MAXANIM 10000

#define MAP_MAX_SIZE 17

/* Careful when changing this, should be no need */

/* Max string len in input string */
#define MAX_INPUT_STRING 256
/* Max input history lines */
#define MAX_HISTORY_LINES 20

/* Maximum size of any packet we expect.  Using this makes it so we don't need to
 * allocated and deallocated the same buffer over and over again and the price
 * of using a bit of extra memory. IT also makes the code simpler. */
#define MAXSOCKBUF (256 * 4096)

/* The numbers of our dark levels.
 * For each level - 1 we store an own bitmap copy, so be careful */
#define DARK_LEVELS 7

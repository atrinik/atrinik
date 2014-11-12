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
 * This is the config file for the client. */

#ifndef CONFIG_H
#define CONFIG_H

/** Socket version. */
#define SOCKET_VERSION 1058

/** File the the arch definitions. */
#define ARCHDEF_FILE "data/archdef.dat"
/** File with the widgets' positions. */
#define INTERFACE_FILE "settings/interface.gui"
/** What scripts to autoload on client startup. */
#define SCRIPTS_AUTOLOAD "settings/scripts_autoload"
/** Log file. */
#define LOG_FILE "client.log"
/** Name of the icon. */
#define CLIENT_ICON_NAME "icon.png"
/** File that contains the default settings. */
#define FILE_SETTINGS_TXT "data/settings.txt"
/** File that contains the user settings. */
#define FILE_SETTINGS_DAT "settings/settings.dat"

/** Maximum number of faces. */
#define MAX_FACE_TILES 32767

/** Maximum map size. */
#define MAP_MAX_SIZE 17

/**
 * Size of the Fog of War cache. Setting this to 1 will decrease memory usage
 * somewhat, but make Fog of War less useful when moving across maps.
 *
 * Basically this value represents how big grid of maps to keep in memory. For
 * example, if the value is 3, the grid will be 3x3, which means 9 "maps", and
 * the middle one is the currently displayed one.
 */
#define MAP_FOW_SIZE 3

/**
 * The number of our dark levels.
 *
 * For each level we store an own bitmap copy. */
#define DARK_LEVELS 7

#define DIRECTORY_SFX "sound/effects"
#define DIRECTORY_CACHE "cache"
#define DIRECTORY_GFX_USER "gfx_user"
#define DIRECTORY_MEDIA "sound/background"

/**
 * If 1, used memory is freed on shutdown, allowing easier memory leak
 * checking with tools like Valgrind. */
#define MEMORY_DEBUG 0

#endif

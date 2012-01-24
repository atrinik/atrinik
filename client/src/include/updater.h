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
 * Header file for the updater GUI. */

#ifndef UPDATER_H
#define UPDATER_H

/**
 * URL where the updater will check for updates. "&version=VERSION" will
 * be appended to this URL, replacing VERSION with the current client's
 * version number. */
#define UPDATER_CHECK_URL "http://www.atrinik.org/page/client_update"
/**
 * Base directory of all the updates. This is where the updates will be
 * downloaded from, as the updater server will only tell us the
 * filenames. */
#define UPDATER_PATH_URL "http://www.atrinik.org/cms/uploads"

/**
 * A single update file that is to be downloaded. */
typedef struct update_file_struct
{
	/** File name to download. */
	char *filename;

	/** SHA-1. */
	char *sha1;
} update_file_struct;

#endif

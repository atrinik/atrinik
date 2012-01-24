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
 * Header file for servers files declarations. */

#ifndef SERVER_FILES_H
#define SERVER_FILES_H

/**
 * @anchor SERVER_FILE_xxx
 * Server file IDs. */
enum
{
	/** @deprecated */
	SERVER_FILE_UNUSED4,
	/** @deprecated */
	SERVER_FILE_UNUSED1,
	/** @deprecated */
	SERVER_FILE_UNUSED2,
	/** @deprecated */
	SERVER_FILE_UNUSED3,
	SERVER_FILE_BMAPS,
	SERVER_FILE_UNUSED5,
	SERVER_FILE_UPDATES,
	SERVER_FILE_SPELLS,
	SERVER_FILE_SETTINGS,
	SERVER_FILE_ANIMS,
	SERVER_FILE_EFFECTS,
	SERVER_FILE_SKILLS,
	SERVER_FILE_HFILES,

	/** Last index. */
	SERVER_FILES_MAX
};

/** One server file. */
typedef struct server_files_struct
{
	/** If 0, will be (re-)loaded. */
	uint8 loaded;

	/**
	 * Update status of this file:
	 *
	 * - 0: Not being updated, or just finished updating.
	 * - 1: Start updating the file the next time server_files_updating()
	 *      is called.
	 * - -1: The file is being updated. */
	sint8 update;

	/** Size of the file. */
	size_t size;

	/** Calculate checksum. */
	unsigned long crc32;
} server_files_struct;

#endif

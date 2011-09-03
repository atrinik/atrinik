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
 * Handles scripts structures and function prototype declarations. */

#include <global.h>

#ifndef SCRIPTS_H
#define SCRIPTS_H

/** Command format. */
enum CmdFormat
{
	/** Regular ASCII string. */
	ASCII,
	/** Array of shorts. */
	SHORT_ARRAY,
	/** Integer array. */
	INT_ARRAY,
	/** Short and integer. */
	SHORT_INT,
	/** Mixed data. */
	MIXED,
	/** The stats command. */
	STATS,
	/** No data. */
	NODATA
};

/** Script structure. */
struct script
{
	/** The script name. */
	char *name;

	/** The script parameters, if any. */
	char *params;

	/** Command from the script. */
	char cmd[HUGE_BUF];

#ifndef WIN32
	/** The file descriptor to which the client writes to the script. */
	int out_fd;

	/** The file descriptor from which we read commands from the script. */
	int in_fd;
#else
	HANDLE out_fd;
	HANDLE in_fd;
#endif

	/** Bytes already read in. */
	int cmd_count;

#ifndef WIN32
	/** Process ID. */
	pid_t pid;
#else
	DWORD pid;

	/** Process handle for win32 */
	HANDLE process;
#endif

	/** All the events this script has registered. */
	char **events;

	/** Number of events this event has registered so far. */
	int events_count;
};

#endif

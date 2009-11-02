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

/**
 * @file
 * Handles scripts structures and function prototype declarations. */

#include <include.h>

#ifndef SCRIPTS_H
#define SCRIPTS_H

/**
 * @defgroup script_event_flags Script event flags
 * Event flags a script can register to get updates when they happen.
 *@{*/

/** No event flags. */
#define SCRIPT_EVENT_NONE 0
/** Get update when the player's stats change. */
#define SCRIPT_EVENT_STATS 1
/*@}*/

/** Number of script events. */
#define SCRIPT_EVENTS 1

/** Script structure. */
struct script {
	/** The script name */
	char *name;

	/** The script parameters, if any */
	char *params;

	/** Command from the script */
	char cmd[HUGE_BUF];

#ifndef WIN32
	/** The file descriptor to which the client writes to the script */
	int out_fd;

	/** The file descriptor from which we read commands from the script */
	int in_fd;
#else
	/** The file descriptor to which the client writes to the script */
	HANDLE out_fd;

	/** The file descriptor from which we read commands from the script */
	HANDLE in_fd;
#endif

	/** Bytes already read in */
	int cmd_count;

#ifndef WIN32
	/** Process ID */
	pid_t pid;
#else
	/** Process ID */
	DWORD pid;

	/** Process handle for win32 */
	HANDLE process;
#endif

	/** All the events this script has registered. */
	char **events;

	/** Number of events this event has registered so far. */
	int events_count;
};

void script_load(const char *cparams);
void script_list();
void script_fdset(int *maxfd, fd_set *set);
void script_process(fd_set *set);
int script_trigger_event(int event_id, void *void_data, int data_len);
void script_send(char *params);
void script_killall();
void script_autoload();
void script_unload(const char *params);

#endif

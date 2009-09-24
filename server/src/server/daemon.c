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
 * Daemon related code. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * Make the Atrinik server become a daemon.
 * @param filename Log file name to use */
void become_daemon(char *filename)
{
#ifndef WIN32
	pid_t pid, sid;
	time_t now = time(NULL);

	/* Fork off the parent process */
	pid = fork();

	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}

	/* If we got a good PID, then
	 * we can exit the parent process. */
	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	/* Change the file mode mask */
	umask(0);

	logfile = fopen(filename, "a");

	LOG(llevInfo, "\n******************************************************\n");
	LOG(llevInfo, "* New server session initialized at %.16s *\n", ctime(&now));
	LOG(llevInfo, "******************************************************\n\n");

	/* Create a new SID for the child process */
	sid = setsid();

	if (sid < 0)
	{
		/* Log the failure */
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
#else
	(void) filename;
#endif
}

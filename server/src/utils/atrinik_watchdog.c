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
 * @defgroup atrinik_watchdog Atrinik Watchdog
 * Simple C application to watch over the Atrinik server that is running
 * with -watchdog enabled and kill it if it hangs.
 *
 * Supports command line option "-detach" to become a daemon.
 * @author Alex Tokar
 * @{ */

/**
 * @file
 * This file handles the Atrinik Watchdog functions.
 * @see atrinik_watchdog */

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

/** Port number to listen on */
#define PORT 13325

/** Maximum buffer size */
#define MAX_BUF 256

/**
 * When amount of seconds since last server's datagram message
 * reaches this or more, the server will be killed. */
#define MAX_TIMEOUT 120

/**
 * The main function.
 * @param argc Number of arguments
 * @param argv[] Arguments
 * @return Always returns 0. */
int main(int argc, char *argv[])
{
	int sock, length, n, server_running = 1, daemonmode = 0;
	struct sockaddr_in server;
	struct sockaddr_in from;
	char buf[MAX_BUF];
	time_t last_time = time(NULL);
	unsigned int fromlen = sizeof(from);

	/* Parse options */
	while (argc > 1)
	{
		--argc;

		/* The -detach option */
		if (strcmp(argv[argc], "-detach") == 0)
		{
			daemonmode = 1;
		}
	}

	/* If we are in daemon mode */
	if (daemonmode)
	{
		pid_t pid, sid;

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
	}

	/* Print out when watchdog started */
	printf("Started Atrinik server watchdog at %.19s\n", ctime(&last_time));

	/* Create a new socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	/* If we failed */
	if (sock < 0)
	{
		printf("ERROR: Opening socket failed.\n");
		exit(1);
	}

	/* Set a non blocking state for the socket */
	if (fcntl(sock, F_SETFL, 1 | O_NONBLOCK) < 0)
	{
		printf("Error setting non blocking socket.\n");
		exit(1);
	}

	length = sizeof(server);
	bzero(&server, length);

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	/* Try to bind it */
	if (bind(sock, (struct sockaddr *) &server, length) < 0)
	{
		printf("ERROR: Binding failed.\n");
		exit(1);
	}

	fromlen = sizeof(struct sockaddr_in);

	/* The main loop */
	while (1)
	{
		/* Use recvfrom to receive a datagram */
		n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &from, &fromlen);

		/* If we got a datagram */
		if (n > 0)
		{
			/* Server is now (or still) running */
			server_running = 1;

			/* Update the last time timestamp */
			last_time = time(NULL);
		}
		/* Otherwise no datagram, and the server is still running */
		else if (server_running)
		{
			time_t now = time(NULL);

			/* If no datagram received in specified number of seconds */
			if (last_time <= now - MAX_TIMEOUT)
			{
				printf("Server hang at: %.19s\n", ctime(&now));

				/* Try to kill the Atrinik server process */
				if (system("pkill atrinik_server") == 0)
				{
					/* If we succeeded, the server is no longer running */
					server_running = 0;
				}
			}
		}

		/* Sleep for a second */
		sleep(1);
	}

	return 0;
}

/*@}*/

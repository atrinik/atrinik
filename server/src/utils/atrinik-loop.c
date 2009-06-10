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
* the Free Software Foundation; either version 3 of the License, or     *
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

/* This is a watchdog pgrogram from Christian Stieber.  It is not an
 * officially supported piece of crossfire (I hope it works, but I am not
 * going to be spending time debugging problems in this piece of code).  The
 * idea is that it periodically sends/gets udp messages to the server - if
 * the server isn't responding, it kills it off and starts a new one.  There
 * is a bit more logic to it - From Christian:

The wrapper is just a hack. I'm using it on a Solaris machine, and it
seems to work fine. Notable features:
- uses the watchdog interface
- if the server crashes more than 10 times, with the time between
  successive crashes being less than 30 seconds, the wrapper terminates
  itself (to prevent bringing down the machine in case of startup problems)
- the server runs at nice 10

 * Note that the main advantage the watchdog has over just the simple
 * crossloop scripts is in the case of infinite loops.  For simple crashes,
 * the crossloop programs do a fine job. */

/* Version 1 */

/************************************************************************/
/*
 * Configuration options */

/* server executable */
#define CROSSFIRE_SERVER	"/home/alex/Desktop/daimonin/server/daimonin_server"

/* directory to cd to before starting the server */
#define CROSSFIRE_TMPDIR	"/home/alex/Desktop/daimonin/server/"

/* if the server crashes more than CRASH_COUNT times, with less than
 * CRASH_INTERVAL seconds between two successive crashes, the loop
 * program is terminated. */
#define CRASH_COUNT		10
#define CRASH_INTERVAL		30

#define USE_WATCHDOG
#define ERROR_SLEEP		30

/************************************************************************/

#include <sys/unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/************************************************************************/

#ifdef USE_WATCHDOG
int Pipe[2];
#endif

/************************************************************************/

void SignalHandler(int Unused)
{
	if (write(Pipe[1], "", 1) != 1)
	{
		perror("Pipe");
		exit(EXIT_FAILURE);
	}
}

/************************************************************************/

int main(void)
{
	int CrashCount;
#ifdef USE_WATCHDOG
	struct protoent *protoent;
	struct sockaddr_in insock;
	int fd;

	memset(&insock, 0, sizeof(insock));

	if ((protoent = getprotobyname("udp")) == NULL)
	{
		perror("Can't get protobyname");
		return EXIT_FAILURE;
	}

	if ((fd = socket(PF_INET, SOCK_DGRAM, protoent->p_proto)) == -1)
	{
		perror("Can't create socket");
		return EXIT_FAILURE;
	}

	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short)13325);

	if (bind(fd, (struct sockaddr *)&insock, sizeof(insock)) == -1)
	{
		perror("Error on bind");
		return EXIT_FAILURE;
	}
#endif

	CrashCount = 0;
	if (!nice(10 - nice(0)))
	{
		perror("Error on setting nice value");
		return EXIT_FAILURE;
	}

	while (CrashCount < CRASH_COUNT)
	{
		time_t StartTime;
		time_t EndTime;
		pid_t Server;

		if (chdir(CROSSFIRE_TMPDIR))
		{
			perror("Error on chdir to CROSSFIRE_TMPDIR");
			return EXIT_FAILURE;
		}
		time(&StartTime);
#ifdef USE_WATCHDOG
		if (pipe(Pipe) == 0)
		{
			void (*OldHandler)(int);

			OldHandler = signal(SIGCHLD, SignalHandler);
#endif
			switch (Server = fork())
			{
				case 0:
					execl(CROSSFIRE_SERVER, CROSSFIRE_SERVER, "-server", NULL);
					return EXIT_FAILURE;

				case -1:
					sleep(ERROR_SLEEP);
					break;

				default:
#ifdef USE_WATCHDOG
				while (1)
				{
					fd_set Files;
					struct timeval Timeout;
					int Max;

					FD_ZERO(&Files);
					FD_SET(Pipe[0], &Files);
					FD_SET(fd, &Files);
					Timeout.tv_sec = 5 * 60;
					Timeout.tv_usec = 0;
					if (fd > Pipe[0])
					{
						Max=fd + 1;
					}
					else
					{
						Max = Pipe[0] + 1;
					}

					while (select(Max, &Files, NULL, NULL, &Timeout) == -1)
					{
						if (errno != EINTR)
						{
							perror("Error on select");
							return EXIT_FAILURE;
						}
					}

					if (FD_ISSET(Pipe[0], &Files))
					{
						/* crash */
						unlink("core");
						waitpid(Server, NULL, 0);
						printf("Server crash!\n");
						break;
					}
					else if (FD_ISSET(fd, &Files))
					{
						/* watchdog */
						char t;

						recv(fd, &t, 1, 0);
					}
					else
					{
						/* timeout */
						printf("Watchdog timeout!\n");
						if (kill(Server, SIGKILL) != 0)
						{
							perror("Error on kill");
							return EXIT_FAILURE;
						}
					}
				}
#else
				waitpid(Server, NULL, 0);
#endif

#ifdef USE_WATCHDOG
				signal(SIGCHLD, OldHandler);
#endif
				time(&EndTime);
				if (EndTime-StartTime < CRASH_INTERVAL)
				{
					CrashCount++;
				}
				else
				{
					CrashCount=0;
				}

				break;
			}
#ifdef USE_WATCHDOG
			close(Pipe[0]);
			close(Pipe[1]);
		}
		else
		{
			sleep(ERROR_SLEEP);
		}
#endif
		}

#ifdef USE_WATCHDOG
	close(fd);
#endif
	return 0;
}

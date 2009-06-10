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

#include <global.h>

#ifndef WIN32 /* ---win32 exclude unix header files */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <version.h>
#endif /* end win32 */

static int metafd = -1;
static struct sockaddr_in sock;

/* metaserver_init sets up the connection.  Its only called once.  If we are not
 * trying to contact the metaserver of the connection attempt fails, metafd will be
 * set to -1.  We use this instead of messing with the settings.meta_on so that
 * that can be examined to at least see what the user was trying to do. */
void metaserver_init()
{
#ifdef WIN32
	struct hostent *hostbn;
	int temp = 1;
#endif

    if (!settings.meta_on)
	{
		metafd = -1;
		return;
    }

    if (isdigit(settings.meta_server[0]))
		sock.sin_addr.s_addr = inet_addr(settings.meta_server);
    else
	{
        struct hostent *hostbn = gethostbyname(settings.meta_server);
		if (hostbn == NULL)
		{
	    	LOG(llevDebug, "metaserver_init: Unable to resolve hostname %s\n", settings.meta_server);
	    	return;
		}

		memcpy(&sock.sin_addr, hostbn->h_addr, hostbn->h_length);
    }

#ifdef WIN32
	ioctlsocket(metafd, FIONBIO, &temp);
#else
    fcntl(metafd, F_SETFL, O_NONBLOCK);
#endif

    if ((metafd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		LOG(llevDebug, "metaserver_init: Unable to create socket, err %d\n", errno);
		return;
    }

    sock.sin_family = AF_INET;
    sock.sin_port = htons(settings.meta_port);
#if 0
    /* Freebsd seems to have a problem in that the sendto in metaserver_update will
     * fail on a connected socket.  So don't connect the socket.
     * Solaris has the same problem, so for now just disable this. */
    if (connect(metafd, &sock, sizeof(sock)) < 0)
	{
		LOG(llevDebug, "metaserver_init: Unable to connect to metaserver, err %d\n", errno);
    }
#endif

    /* No hostname specified, so lets try to figure one out */
    if (settings.meta_host[0] == 0)
	{
		char hostname[MAX_BUF], domain[MAX_BUF];
		if (gethostname(hostname, MAX_BUF - 1))
		{
	    	LOG(llevDebug, "metaserver_init: gethostname failed - will not report hostname\n");
	    	return;
		}

#ifdef WIN32
		hostbn = gethostbyname(hostname);

		/* quick hack */
		if (hostbn != (struct hostent *) NULL)
			memcpy(domain, hostbn->h_addr, hostbn->h_length);

		if (hostbn == (struct hostent *) NULL)
		{
#else
		if (getdomainname(domain, MAX_BUF - 1))
		{
#endif
	    	LOG(llevDebug, "metaserver_init: getdomainname failed - will not report hostname\n");
	    	return;
		}

		/* Potential overrun here but unlikely to occur */
		sprintf(settings.meta_host, "%s.%s", hostname, domain);
    }

    if (metafd != -1)
		LOG(llevInfo, "metaserver_init: Connected to %s. Done.\n", settings.meta_server);

	metaserver_update();
}

void metaserver_update()
{
    char data[MAX_BUF], num_players = 0;
    player *pl;

	/* No valid connection */
    if (metafd == -1)
		return;

    /* We could use socket_info.nconns, but that is not quite as accurate,
     * as connections in the progress of being established, are listening
     * but don't have a player, etc.  This operation below should not be that
     * costly. */
    for (pl = first_player; pl != NULL; pl = pl->next)
		num_players++;

#ifdef CS_LOG_STATS
    sprintf(data, "%s|%d|%s|%s|%d|%d|%ld", settings.meta_host, num_players, VERSION, settings.meta_comment, cst_tot.ibytes, cst_tot.obytes, (long)time(NULL) - cst_tot.time_start);
#else
	sprintf(data, "%s|%d|%s|%s", settings.meta_host, num_players, VERSION, settings.meta_comment);
#endif

    if (sendto(metafd, data, strlen(data), 0, (struct sockaddr *)&sock, sizeof(sock)) < 0)
		LOG(llevDebug, "metaserver_update: sendto failed, err = %d\n", errno);

	LOG(llevInfo, "metaserver_update: Send data.\n");
}


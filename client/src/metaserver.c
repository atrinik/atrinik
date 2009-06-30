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

#include <include.h>
#include <curl/curl.h>

/**
 * @file
 * This file handles connection to the metaserver and receiving data from it.
 * @todo There seems to be some kind of a bug, which rarely happens when
 * receiving data from the metaserver. The data gets split to several lines,
 * while it should be only one line. */

/**
 * Parse data returned from HTTP metaserver.
 * @param info The data */
static void parse_metaserver_data(char *info)
{
    char server_ip[MAX_BUF], port[MAX_BUF], server[MAX_BUF], num_players[MAX_BUF], version[MAX_BUF], desc[HUGE_BUF];

	server[0] = server_ip[0] = port[0] = num_players[0] = version[0] = desc[0] = '\0';

	if (info == NULL || !sscanf(info, "%64[^:]:%32[^:]:%128[^:]:%64[^:]:%64[^:]:%512[^\n]", server_ip, port, server, num_players, version, desc))
		return;

	if (server[0] == '\0' || server_ip[0] == '\0' || port[0] == '\0' || num_players[0] == '\0' || version[0] == '\0' || desc[0] == '\0')
		return;

	add_metaserver_data(server, atoi(port), atoi(num_players), version, desc);
}


/**
 * Function to call when receiving data from the metaserver.
 * @param ptr Pointer to the data
 * @param size Size of the data
 * @param nmemb Number of items
 * @param data Data
 * @return Returns the realsize of the data returned (size * nmemb). */
static size_t metaserver_reader(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;
	char *p, *buf;

	/* So that we don't get unused parameter warning */
	(void) data;

	buf = (char *)malloc(realsize);

	snprintf(buf, realsize, "%s", (char *) ptr);

	p = strtok(buf, "\n");

	/* Loop through all the lines returned */
	while (p)
	{
		/* Store it in a temporary buf, and parse it */
    	parse_metaserver_data(p);

		p = strtok(NULL, "\n");
	}

	free(buf);

    return realsize;
}


/**
 * Connect to a metaserver.
 * If we fail, log the error and show information that metaserver failed. */
void metaserver_connect()
{
	CURL *handle;
	CURLcode res;

	/* Init "easy" cURL */
	handle = curl_easy_init();

	if (handle)
	{
		/* Set connection timeout value in case metaserver is down or something */
		curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, METASERVER_TIMEOUT);

		/* URL */
		curl_easy_setopt(handle, CURLOPT_URL, "http://meta.atrinik.org/");

		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, metaserver_reader);
		res = curl_easy_perform(handle);

		if (res)
		{
			LOG(LOG_DEBUG, "DEBUG: metaserver_connect(): curl_easy_perform got error %d (%s).\n", res, curl_easy_strerror(res));
			draw_info("Metaserver failed!", COLOR_RED);
		}

		/* Always cleanup */
		curl_easy_cleanup(handle);
	}
}

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

#include <include.h>

/**
 * @file
 * This file handles connection to the metaserver and receiving data from it. */

/**
 * There might be a realloc() out there that doesn't like reallocing
 * NULL pointers, so we take care of it with this function.
 * @param ptr Pointer to reallocate
 * @param size Size to reallocate
 * @return The pointer to the memory */
static void *metaserver_realloc(void *ptr, size_t size)
{
	if (ptr)
	{
		return realloc(ptr, size);
	}
	else
	{
		return malloc(size);
	}
}

/**
 * Function to call when receiving data from the metaserver.
 * @param ptr Pointer to the data
 * @param size Size of the data
 * @param nmemb Number of items
 * @param data Data
 * @return Returns the realsize of the data returned (size * nmemb). */
static size_t metaserver_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	metaserver_struct *mem = (metaserver_struct *) data;

	mem->memory = metaserver_realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory)
	{
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}

	return realsize;
}

/**
 * Parse data returned from HTTP metaserver.
 * @param info The data */
static void parse_metaserver_data(char *info)
{
    char server_ip[MAX_BUF], port[MAX_BUF], server[MAX_BUF], num_players[MAX_BUF], version[MAX_BUF], desc[HUGE_BUF];

	server[0] = server_ip[0] = port[0] = num_players[0] = version[0] = desc[0] = '\0';

	if (info == NULL || !sscanf(info, "%64[^:]:%32[^:]:%128[^:]:%64[^:]:%64[^:]:%512[^\n]", server_ip, port, server, num_players, version, desc))
	{
		return;
	}

	if (server[0] == '\0' || server_ip[0] == '\0' || port[0] == '\0' || num_players[0] == '\0' || version[0] == '\0' || desc[0] == '\0')
	{
		return;
	}

	add_metaserver_data(server, atoi(port), atoi(num_players), version, desc);
}

/**
 * Connect to a metaserver. This function takes care of parsing the returned
 * data. The data is temporarily put into memory so we can parse it.
 * If we fail, log the error and show information that metaserver failed. */
void metaserver_connect()
{
	CURL *handle;
	CURLcode res;
	metaserver_struct *chunk = (metaserver_struct *) malloc(sizeof(metaserver_struct));
	char user_agent[MAX_BUF];

	/* Store user agent for cURL, including if this is Linux build of client
	 * or Windows one. Could be used for statistics or something. */
#ifdef __LINUX
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Linux)/%s", PACKAGE_VERSION);
#elif __WIN_32
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Win32)/%s", PACKAGE_VERSION);
#else
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Unknown)/%s", PACKAGE_VERSION);
#endif

	/* We expect realloc(NULL, size) to work */
	chunk->memory = NULL;

	/* No data at this point */
	chunk->size = 0;

	/* Init "easy" cURL */
	handle = curl_easy_init();

	if (handle)
	{
		/* Set connection timeout value in case metaserver is down or something */
		curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, METASERVER_TIMEOUT);

		/* URL */
		curl_easy_setopt(handle, CURLOPT_URL, "http://meta.atrinik.org/");

		/* Send all data to this function */
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, metaserver_callback);

		/* We pass our 'chunk' struct to the callback function */
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) chunk);

		/* Specify user agent for the metaserver. */
		curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent);

		/* Get the data */
		res = curl_easy_perform(handle);

		/* If it failed, log it and show information about to the client */
		if (res)
		{
			LOG(LOG_DEBUG, "DEBUG: metaserver_connect(): curl_easy_perform got error %d (%s).\n", res, curl_easy_strerror(res));
			draw_info("Metaserver failed!", COLOR_RED);
		}

		/* Always cleanup */
		curl_easy_cleanup(handle);

		/* If we go the data, might as well do something with it. */
		if (chunk->memory)
		{
			char *buf = (char *) malloc(chunk->size), *p;

			/* Store the data in a temporary buffer */
			snprintf(buf, chunk->size, "%s", chunk->memory);

			p = strtok(buf, "\n");

			/* Loop through all the lines returned */
			while (p)
			{
				/* Parse it */
				parse_metaserver_data(p);

				p = strtok(NULL, "\n");
			}

			/* Free the temporary buffer */
			free(buf);

			/* Free the memory */
			free(chunk->memory);
		}

		/* Free the chunk */
		if (chunk)
		{
			free(chunk);
		}
	}
}

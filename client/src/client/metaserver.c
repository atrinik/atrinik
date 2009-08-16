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
#include <pthread.h>

/**
 * @file
 * This file handles connection to the metaserver and receiving data from it. */

/** List of metaservers. Will loop until these until we successfully connect to one. */
static char *metaservers[] = {"http://meta.atrinik.org/", "http://atokar.is-a-geek.net/"};

/** Are we connecting to the metaserver? */
int metaserver_connecting = 1;

/**
 * Parse data returned from HTTP metaserver and add it to the list of servers.
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

	metaserver_add(server, atoi(port), atoi(num_players), version, desc);
}

/**
 * Get metaserver data, IP/hostname and port.
 * @param num ID of the server
 * @param server Char pointer where to copy the server IP/hostname
 * @param port Int pointer where to copy the server port */
void metaserver_get_data(int num, char *server, int *port)
{
	_server *node = start_server;
	int i;

	for (i = 0; node; i++)
	{
		if (i == num)
		{
			strcpy(server, node->nameip);
			*port = node->port;

			return;
		}

		node = node->next;
	}
}

/**
 * Clear all data in the linked list of servers reported by metaserver. */
void metaserver_clear_data()
{
	_server *node, *tmp;
	void *tmp_free;

	node = start_server;

	for (; node ;)
	{
		tmp_free = &node->nameip;
		FreeMemory(tmp_free);

		tmp_free = &node->version;
		FreeMemory(tmp_free);

		tmp_free = &node->desc;
		FreeMemory(tmp_free);

		tmp = node->next;
		tmp_free = &node;
		FreeMemory(tmp_free);
		node = tmp;
	}

	start_server = NULL;
	metaserver_start = 0;
	metaserver_sel = 0;
	metaserver_count = 0;
}

/**
 * Add a server entry to the linked list of available servers
 * reported by metaserver.
 * @param server The server IP/hostname
 * @param port Server port
 * @param player Number of players
 * @param ver Server version
 * @param desc Description of the server */
void metaserver_add(char *server, int port, int player, char *ver, char *desc)
{
	_server *node;

	node = (_server*) _malloc(sizeof(_server), "add_metaserver_data(): add server struct");
	memset(node, 0, sizeof(_server));

	node->next = start_server;
	start_server = node;

	node->player = player;
	node->port = port;
	node->nameip = _malloc(strlen(server) + 1, "add_metaserver_data(): nameip string");
	strcpy(node->nameip, server);
	node->version = _malloc(strlen(ver) + 1, "add_metaserver_data(): version string");
	strcpy(node->version, ver);
	node->desc = _malloc(strlen(desc) + 1, "add_metaserver_data(): desc string");
	strcpy(node->desc, desc);

	metaserver_count++;
}

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
 * Connect to a metaserver. This function takes care of parsing the returned
 * data. The data is temporarily put into memory so we can parse it.
 * If we fail, log the error and show information that metaserver failed. */
int metaserver_connect(char *metaserver_url)
{
	CURL *handle;
	CURLcode res;
	metaserver_struct *chunk = (metaserver_struct *) malloc(sizeof(metaserver_struct));
	char user_agent[MAX_BUF];
	int success = 0;

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
		curl_easy_setopt(handle, CURLOPT_URL, metaserver_url);

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
		}

		/* Always cleanup */
		curl_easy_cleanup(handle);

		/* If we go the data, might as well do something with it. */
		if (chunk->memory)
		{
			char *buf = (char *) malloc(chunk->size), *p;

			/* No need to connect to other mirror metaservers */
			success = 1;

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

	return success;
}

/**
 * Threaded functionto connect to metaserver.
 *
 * Goes through the list of metaservers and calls metaserver_connect()
 * until it gets a return value of 1. If if goes through all the
 * metaservers and still fails, show an info to the user.
 * @param dummy Unused.
 * @return Always returns 0. */
int metaserver_thread(void *dummy)
{
	int metaserver_id, metaserver_failed = 1;

	(void) dummy;

	/* Go through all the metaservers in the list */
	for (metaserver_id = 0; metaserver_id < (int) (sizeof(metaservers) / sizeof(char *)); metaserver_id++)
	{
		/* If the connection succeeded, break out */
		if (metaserver_connect(metaservers[metaserver_id]))
		{
			metaserver_failed = 0;

			break;
		}
	}

	/* If we couldn't get data out of any of the metaservers */
	if (metaserver_failed)
	{
		draw_info("Metaserver failed! Using default list.", COLOR_RED);
	}

	/* We're not connecting anymore */
	metaserver_connecting = 0;

	return 0;
}

/**
 * Connect to metaserver and get the available servers.
 *
 * Uses SDL CreateThread function and calls metaserver_thread(). */
void metaserver_get_servers()
{
	SDL_Thread *thread;

	thread = SDL_CreateThread(metaserver_thread, NULL);

	if (!thread)
	{
		LOG(LOG_ERROR, "ERROR: Thread creation failed.\n");
	}
}

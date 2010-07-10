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
 * @file
 * Handles connection to the metaserver and receiving data from it. */

#include <include.h>

/** List of metaservers. Will loop these until we successfully connect to one. */
static const char *const metaservers[] = {"http://meta.atrinik.org/", "http://atokar.is-a-geek.net/", "http://www.wordowl.com/misc/atrinik/"};
/** The number of ::metaservers. */
#define NUM_METASERVERS (sizeof(metaservers) / sizeof(metaservers[0]))

/** Are we connecting to the metaserver? */
static int metaserver_connecting;
/** Mutex to protect ::metaserver_connecting. */
static SDL_mutex *metaserver_connecting_mutex;
/** The list of the servers. */
static server_struct *start_server;
/** Number of the servers. */
static size_t server_count;
/** Mutex to protect ::start_server and ::server_count. */
static SDL_mutex *start_server_mutex;
/** Selected server. */
int server_sel;

/**
 * Initialize the metaserver data. */
void metaserver_init()
{
	/* Initialize the data. */
	start_server = NULL;
	server_sel = 0;
	server_count = 0;
	metaserver_connecting = 1;

	/* Initialize mutexes. */
	metaserver_connecting_mutex = SDL_CreateMutex();
	start_server_mutex = SDL_CreateMutex();
}

/**
 * Parse data returned from HTTP metaserver and add it to the list of servers.
 * @param info The data to parse. */
static void parse_metaserver_data(char *info)
{
	char *tmp[6];

	if (!info || split_string(info, tmp, sizeof(tmp) / sizeof(*tmp), ':') != 6)
	{
		return;
	}

	metaserver_add(tmp[0], atoi(tmp[1]), tmp[2], atoi(tmp[3]), tmp[4], tmp[5]);
}

/**
 * Get server from the servers list by its ID.
 * @param num ID of the server to find.
 * @return The server if found, NULL otherwise. */
server_struct *server_get_id(size_t num)
{
	server_struct *node;
	size_t i = 0;

	SDL_LockMutex(start_server_mutex);
	node = start_server;

	for (i = 0; node; i++, node = node->next)
	{
		if (i == num)
		{
			break;
		}
	}

	SDL_UnlockMutex(start_server_mutex);
	return node;
}

/**
 * Get number of the servers in the list.
 * @return The number. */
size_t server_get_count()
{
	size_t count;

	SDL_LockMutex(start_server_mutex);
	count = server_count;
	SDL_UnlockMutex(start_server_mutex);
	return count;
}

/**
 * Check if we're connecting to the metaserver.
 * @param val If not -1, set the metaserver connecting value to this.
 * @return 1 if we're connecting to the metaserver, 0 otherwise. */
int ms_connecting(int val)
{
	int connecting;

	SDL_LockMutex(metaserver_connecting_mutex);
	connecting = metaserver_connecting;

	/* More useful to return the old value than the one we're setting. */
	if (val != -1)
	{
		metaserver_connecting = val;
	}

	SDL_UnlockMutex(metaserver_connecting_mutex);
	return connecting;
}

/**
 * Clear all data in the linked list of servers reported by metaserver. */
void metaserver_clear_data()
{
	server_struct *node, *tmp;

	SDL_LockMutex(start_server_mutex);
	node = start_server;

	while (node)
	{
		tmp = node->next;

		free(node->ip);
		free(node->name);
		free(node->version);
		free(node->desc);
		free(node);

		node = tmp;
	}

	start_server = NULL;
	server_sel = 0;
	server_count = 0;
	SDL_UnlockMutex(start_server_mutex);
}

/**
 * Add a server entry to the linked list of available servers reported by
 * metaserver.
 * @param ip The server IP.
 * @param port Server port.
 * @param name Server's name.
 * @param player Number of players.
 * @param version Server version.
 * @param desc Description of the server. */
void metaserver_add(const char *ip, int port, const char *name, int player, const char *version, const char *desc)
{
	server_struct *node = (server_struct *) malloc(sizeof(server_struct));

	memset(node, 0, sizeof(server_struct));
	node->player = player;
	node->port = port;
	node->ip = strdup(ip);
	node->name = strdup(name);
	node->version = strdup(version);
	node->desc = strdup(desc);

	SDL_LockMutex(start_server_mutex);
	node->next = start_server;
	start_server = node;
	server_count++;
	SDL_UnlockMutex(start_server_mutex);
}

/**
 * Function to call when receiving data from the metaserver.
 * @param ptr Pointer to data to process.
 * @param size The size of each piece of data.
 * @param nmemb Number of data elements.
 * @param data User supplied data pointer - points to @ref metaserver_struct "metaserver structure"
 * that holds the data returned from the metaserver.
 * @return Number of bytes processed. */
static size_t metaserver_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	metaserver_struct *mem = (metaserver_struct *) data;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory)
	{
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = '\0';
	}

	return realsize;
}

/**
 * Connect to a metaserver using cURL and get data about metaservers.
 * @param metaserver_url URL of the metaserver to connect to.
 * @return 1 on success, 0 on failure. */
static int metaserver_connect(const char *metaserver_url)
{
	CURL *handle;
	CURLcode res;
	metaserver_struct *chunk = (metaserver_struct *) malloc(sizeof(metaserver_struct));
	char user_agent[MAX_BUF];
	int success = 0;

	/* Store user agent for cURL, including if this is GNU/Linux build of client
	 * or Windows one. Could be used for statistics or something. */
#if defined(__LINUX)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (GNU/Linux)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#elif defined(WIN32)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Win32)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#else
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Unknown)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#endif

	chunk->memory = NULL;
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

		if (res)
		{
			LOG(llevBug, "metaserver_connect(): curl_easy_perform got error %d (%s).\n", res, curl_easy_strerror(res));
		}

		/* Always cleanup */
		curl_easy_cleanup(handle);

		/* If we go the data, might as well do something with it. */
		if (chunk->memory)
		{
			char *buf = strdup(chunk->memory), *cp, *saveptr = NULL;

			/* No need to connect to other mirror metaservers */
			success = 1;

			cp = strtok_r(buf, "\n", &saveptr);

			/* Loop through all the lines returned */
			while (cp)
			{
				parse_metaserver_data(cp);
				cp = strtok_r(NULL, "\n", &saveptr);
			}

			free(buf);
			free(chunk->memory);
		}

		if (chunk)
		{
			free(chunk);
		}
	}

	return success;
}

/**
 * Threaded function to connect to metaserver.
 *
 * Goes through the list of metaservers and calls metaserver_connect()
 * until it gets a return value of 1. If if goes through all the
 * metaservers and still fails, show an info to the user.
 * @param dummy Unused.
 * @return Always returns 0. */
int metaserver_thread(void *dummy)
{
	size_t metaserver_id;
	int metaserver_failed = 1;

	(void) dummy;

	/* Go through all the metaservers in the list */
	for (metaserver_id = 0; metaserver_id < NUM_METASERVERS; metaserver_id++)
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

	SDL_LockMutex(metaserver_connecting_mutex);
	/* We're not connecting anymore */
	metaserver_connecting = 0;
	SDL_UnlockMutex(metaserver_connecting_mutex);
	return 0;
}

/**
 * Connect to metaserver and get the available servers.
 *
 * Works in a thread using SDL_CreateThread(). */
void metaserver_get_servers()
{
	SDL_Thread *thread = SDL_CreateThread(metaserver_thread, NULL);

	if (!thread)
	{
		LOG(llevError, "metaserver_get_servers(): Thread creation failed.\n");
	}
}

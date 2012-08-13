/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Handles connection to the metaserver and receiving data from it.
 *
 * @author Alex Tokar */

#include <global.h>

/** Are we connecting to the metaserver? */
static int metaserver_connecting = 1;
/** Mutex to protect ::metaserver_connecting. */
static SDL_mutex *metaserver_connecting_mutex;
/** The list of the servers. */
static server_struct *server_head;
/** Number of the servers. */
static size_t server_count;
/** Mutex to protect ::server_head and ::server_count. */
static SDL_mutex *server_head_mutex;
/** Is metaserver enabled? */
static uint8 enabled = 1;

/**
 * Initialize the metaserver data. */
void metaserver_init(void)
{
	/* Initialize the data. */
	server_head = NULL;
	server_count = 0;

	/* Initialize mutexes. */
	metaserver_connecting_mutex = SDL_CreateMutex();
	server_head_mutex = SDL_CreateMutex();
}

/**
 * Disable the metaserver. */
void metaserver_disable(void)
{
	enabled = 0;
	metaserver_connecting = 0;
}

/**
 * Parse data returned from HTTP metaserver and add it to the list of servers.
 * @param info The data to parse. */
static void parse_metaserver_data(char *info)
{
	char *tmp[6];

	if (!info || string_split(info, tmp, arraysize(tmp), ':') != 6)
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
	size_t i;

	SDL_LockMutex(server_head_mutex);

	for (node = server_head, i = 0; node; node = node->next, i++)
	{
		if (i == num)
		{
			break;
		}
	}

	SDL_UnlockMutex(server_head_mutex);
	return node;
}

/**
 * Get number of the servers in the list.
 * @return The number. */
size_t server_get_count(void)
{
	size_t count;

	SDL_LockMutex(server_head_mutex);
	count = server_count;
	SDL_UnlockMutex(server_head_mutex);
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
void metaserver_clear_data(void)
{
	server_struct *node, *tmp;

	SDL_LockMutex(server_head_mutex);

	DL_FOREACH_SAFE(server_head, node, tmp)
	{
		DL_DELETE(server_head, node);
		free(node->ip);
		free(node->name);
		free(node->version);
		free(node->desc);
		free(node);
	}

	server_count = 0;
	SDL_UnlockMutex(server_head_mutex);
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
	server_struct *node;

	node = calloc(1, sizeof(*node));
	node->player = player;
	node->port = port;
	node->ip = strdup(ip);
	node->name = strdup(name);
	node->version = strdup(version);
	node->desc = strdup(desc);

	SDL_LockMutex(server_head_mutex);
	DL_PREPEND(server_head, node);
	server_count++;
	SDL_UnlockMutex(server_head_mutex);
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
	size_t i;
	curl_data *data;

	/* Go through all the metaservers in the list */
	for (i = clioption_settings.metaservers_num; i > 0; i--)
	{
		data = curl_data_new(clioption_settings.metaservers[i - 1]);

		/* If the connection succeeded, break out. */
		if (curl_connect(data) == 1 && data->memory)
		{
			char word[HUGE_BUF];
			size_t pos;

			pos = 0;

			while (string_get_word(data->memory, &pos, '\n', word, sizeof(word), 0))
			{
				parse_metaserver_data(word);
			}

			curl_data_free(data);
			break;
		}

		curl_data_free(data);
	}

	SDL_LockMutex(metaserver_connecting_mutex);
	/* We're not connecting anymore. */
	metaserver_connecting = 0;
	SDL_UnlockMutex(metaserver_connecting_mutex);
	return 0;
}

/**
 * Connect to metaserver and get the available servers.
 *
 * Works in a thread using SDL_CreateThread(). */
void metaserver_get_servers(void)
{
	SDL_Thread *thread;

	if (!enabled)
	{
		return;
	}

	SDL_LockMutex(metaserver_connecting_mutex);
	metaserver_connecting = 1;
	SDL_UnlockMutex(metaserver_connecting_mutex);

	thread = SDL_CreateThread(metaserver_thread, NULL);

	if (!thread)
	{
		logger_print(LOG(ERROR), "Thread creation failed.");
	}
}

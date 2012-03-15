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
 * Header file for the cURL module. */

#ifndef CURL_H
#define CURL_H

/** cURL data. */
typedef struct curl_data
{
	/** The data. Can be NULL in case we got no data from the url. */
	char *memory;

	/** Size of the data. */
	size_t size;

	/** URL used. */
	char *url;

	/**
	 * Mutex to protect the data in this structure when accessed across
	 * threads. */
	SDL_mutex *mutex;

	/** The thread. */
	SDL_Thread *thread;

	/**
	 * State of the data:
	 * - 0: still trying to get data (connecting to server, getting data,
	 *      etc). While this is the state, no members of this structure
	 *      should be accessed from the outside (at the very least not
	 *      without mutex locking).
	 * - -1: An error occurred trying to get the data.
	 * - 1: cURL thread finished and the data is ready to be used. */
	sint8 status;

	/**
	 * Will contain HTTP code. */
	int http_code;

	/**
	 * cURL handle being used. */
	CURL *handle;
} curl_data;

#define CURL_TIMEOUT 15

#endif

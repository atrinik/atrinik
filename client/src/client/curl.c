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
 * cURL module for downloading data from URLs. */

#include <include.h>

/** User agent. Initialized in init(). */
static char user_agent[MAX_BUF];

/** Whether we have called init() or not. */
static int did_init = 0;

/**
 * Initialize the module. */
static void init()
{
	did_init = 1;

	/* Store user agent for cURL, including if this is GNU/Linux build of client
	 * or Windows one. */
#if defined(__LINUX)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (GNU/Linux)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#elif defined(WIN32)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Win32)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#else
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Unknown)/%s (%d)", PACKAGE_VERSION, SOCKET_VERSION);
#endif
}

/**
 * Function to call when receiving data from cURL.
 * @param ptr Pointer to data to process.
 * @param size The size of each piece of data.
 * @param nmemb Number of data elements.
 * @param data User supplied data pointer - points to ::curl_data that
 * holds the data returned from the url.
 * @return Number of bytes processed. */
static size_t curl_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	curl_data *mem = (curl_data *) data;

	SDL_LockMutex(mem->mutex);
	mem->memory = realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory)
	{
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = '\0';
	}

	SDL_UnlockMutex(mem->mutex);

	return realsize;
}

/**
 * Use cURL to download the url we specified in curl_data structure
 * (c_data).
 * @param c_data ::curl_data structure that will receive the data (and
 * has url of what to download).
 * @return -1 on failure, 1 on success. */
int curl_connect(void *c_data)
{
	curl_data *data = (curl_data *) c_data;
	CURL *handle;
	CURLcode res;

	/* Init "easy" cURL */
	handle = curl_easy_init();

	if (!handle)
	{
		SDL_LockMutex(data->mutex);
		data->status = -1;
		SDL_UnlockMutex(data->mutex);
		return -1;
	}

	/* Set connection timeout. */
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);

	SDL_LockMutex(data->mutex);
	curl_easy_setopt(handle, CURLOPT_URL, data->url);
	SDL_UnlockMutex(data->mutex);

	/* The callback function. */
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) data);

	/* Set user agent. */
	curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent);

	/* Get the data. */
	res = curl_easy_perform(handle);

	SDL_LockMutex(data->mutex);
	data->status = 1;
	SDL_UnlockMutex(data->mutex);

	if (res)
	{
		LOG(llevBug, "curl_thread(): curl_easy_perform() got error %d (%s).\n", res, curl_easy_strerror(res));
		curl_easy_cleanup(handle);
		SDL_LockMutex(data->mutex);
		data->status = -1;
		SDL_UnlockMutex(data->mutex);
		return -1;
	}

	curl_easy_cleanup(handle);

	return 1;
}

/**
 * Initialize new ::curl_data structure.
 * @param url Url to connect to.
 * @return The new structure. */
curl_data *curl_data_new(const char *url)
{
	curl_data *data = malloc(sizeof(curl_data));

	/* Store the url. */
	data->url = strdup(url);
	data->memory = NULL;
	data->size = 0;
	data->status = 0;
	/* Create a mutex to protect the structure. */
	data->mutex = SDL_CreateMutex();

	/* Do we need to initialize because we haven't done so yet? */
	if (!did_init)
	{
		init();
	}

	return data;
}

/**
 * Start downloading an url.
 * @param url What to download.
 * @return cURL data structure. You should store this somehow, and the
 * next tick see if it has finished by using curl_download_finished(). If
 * so, you can access its members such as curl_data::memory. Do not
 * forget to clean up with curl_data_free() at some point (even if an
 * error occurred). */
curl_data *curl_download_start(const char *url)
{
	SDL_Thread *thread;
	curl_data *data = curl_data_new(url);

	/* Create a new thread. */
	thread = SDL_CreateThread(curl_connect, data);

	if (!thread)
	{
		LOG(llevError, "curl_download_start(): Thread creation failed.\n");
	}

	return data;
}

/**
 * Check if cURL has finished downloading the previously supplied url.
 * @param data cURL data structure that was returned by a previous
 * curl_download_start() call.
 * @return @copydoc curl_data::status */
sint8 curl_download_finished(curl_data *data)
{
	sint8 status;

	SDL_LockMutex(data->mutex);
	status = data->status;
	SDL_UnlockMutex(data->mutex);

	return status;
}

/**
 * Frees previously created ::curl_data structure.
 * @param data What to free. */
void curl_data_free(curl_data *data)
{
	if (data->memory)
	{
		free(data->memory);
	}

	SDL_DestroyMutex(data->mutex);
	free(data->url);
}

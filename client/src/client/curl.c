/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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

#include <global.h>

/** Shared handle. */
static CURLSH *handle_share = NULL;
/** Mutex to protect the shared handle. */
static SDL_mutex *handle_share_mutex = NULL;

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
	char user_agent[MAX_BUF], version[MAX_BUF];
	CURL *handle;
	CURLcode res;
	long http_code;

	package_get_version_full(version, sizeof(version));

	/* Store user agent for cURL, including if this is GNU/Linux build of client
	 * or Windows one. */
#if defined(LINUX)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (GNU/Linux)/%s (%d)", version, SOCKET_VERSION);
#elif defined(WIN32)
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Win32)/%s (%d)", version, SOCKET_VERSION);
#else
	snprintf(user_agent, sizeof(user_agent), "Atrinik Client (Unknown)/%s (%d)", version, SOCKET_VERSION);
#endif

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
	/* Disable signals since we are in a thread. See
	 * http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTNOSIGNAL
	 * for details. */
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);

	SDL_LockMutex(data->mutex);
	curl_easy_setopt(handle, CURLOPT_URL, data->url);
	curl_easy_setopt(handle, CURLOPT_REFERER, data->url);
	curl_easy_setopt(handle, CURLOPT_SHARE, handle_share);
	SDL_UnlockMutex(data->mutex);

	/* The callback function. */
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *) data);

	/* Set user agent. */
	curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent);

	/* Get the data. */
	res = curl_easy_perform(handle);

	if (res)
	{
		LOG(llevBug, "curl_thread(): curl_easy_perform() got error %d (%s).\n", res, curl_easy_strerror(res));
		curl_easy_cleanup(handle);
		SDL_LockMutex(data->mutex);
		data->status = -1;
		SDL_UnlockMutex(data->mutex);
		return -1;
	}

	curl_easy_getinfo(handle, CURLINFO_HTTP_CODE, &http_code);

	if (http_code != 200)
	{
		SDL_LockMutex(data->mutex);
		data->status = -1;
		SDL_UnlockMutex(data->mutex);
		return -1;
	}

	curl_easy_cleanup(handle);

	SDL_LockMutex(data->mutex);
	data->status = 1;
	SDL_UnlockMutex(data->mutex);

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
	curl_data *data = curl_data_new(url);

	/* Create a new thread. */
	data->thread = SDL_CreateThread(curl_connect, data);

	if (!data->thread)
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
	/* Still downloading? Kill the thread. */
	if (curl_download_finished(data) == 0)
	{
		SDL_KillThread(data->thread);
	}

	if (data->memory)
	{
		free(data->memory);
	}

	SDL_DestroyMutex(data->mutex);
	free(data->url);
	free(data);
}

/**
 * Lock the share handle. */
static void curl_share_lock(CURL *handle, curl_lock_data data, curl_lock_access lock_access, void *userptr)
{
	(void) handle;
	(void) data;
	(void) lock_access;
	SDL_LockMutex(userptr);
}

/**
 * Unlock the share handle. */
static void curl_share_unlock(CURL *handle, curl_lock_data data, void *userptr)
{
	(void) handle;
	(void) data;
	SDL_UnlockMutex(userptr);
}

/**
 * Initialize cURL module. */
void curl_init()
{
	curl_global_init(CURL_GLOBAL_ALL);
	handle_share_mutex = SDL_CreateMutex();

	handle_share = curl_share_init();
	curl_share_setopt(handle_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(handle_share, CURLSHOPT_USERDATA, handle_share_mutex);
	curl_share_setopt(handle_share, CURLSHOPT_LOCKFUNC, curl_share_lock);
	curl_share_setopt(handle_share, CURLSHOPT_UNLOCKFUNC, curl_share_unlock);
}

/**
 * Deinitialize cURL module. */
void curl_deinit()
{
	curl_share_cleanup(handle_share);
	SDL_DestroyMutex(handle_share_mutex);
	curl_global_cleanup();
}

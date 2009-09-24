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

/**
 * @file
 * Metaserver updating related code. */

#include <global.h>
#include <curl/curl.h>
#ifndef WIN32
#include <pthread.h>
#endif

/** The time in seconds for timeout upon connecting */
#define METASERVER_TIMEOUT 3

/**
 * Init metaserver. */
void metaserver_init()
{
	if (!settings.meta_on)
	{
		return;
	}

	/* Init global cURL */
	curl_global_init(CURL_GLOBAL_ALL);
}

/**
 * Function to call when receiving data from the metaserver.
 * @param ptr Pointer to the actual data
 * @param size Size of the data
 * @param nmemb
 * @param data Unused
 * @return The real size of the data */
static size_t metaserver_writer(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;

	(void) data;

	LOG(llevDebug, "DEBUG: metaserver_writer(): Returned data:\n%s\n", (const char *) ptr);

	return realsize;
}

/**
 * The actual function doing the metaserver updating, called by
 * pthread_create() in metaserver_update().
 * @return Always returns NULL. */
void *metaserver_thread(void *dummy)
{
	char buf[MAX_BUF], num_players = 0;
	player *pl;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	CURL *curl;
	CURLcode res = 0;
	time_t now = time(NULL);

	(void) dummy;

	/* We could use socket_info.nconns, but that is not quite as accurate,
	 * as connections in the progress of being established, are listening
	 * but don't have a player, etc.  This operation below should not be that
	 * costly. */
	for (pl = first_player; pl != NULL; pl = pl->next)
	{
		num_players++;
	}

	/* Hostname */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "hostname", CURLFORM_COPYCONTENTS, settings.meta_host, CURLFORM_END);

	/* Server version */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, VERSION, CURLFORM_END);

	/* Server comment */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "text_comment", CURLFORM_COPYCONTENTS, settings.meta_comment, CURLFORM_END);

	/* Number of players */
	snprintf(buf, MAX_BUF - 1, "%d", num_players);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "num_players", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	/* Port number */
	snprintf(buf, MAX_BUF - 1, "%d", settings.csport);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "port", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	/* Init "easy" cURL */
	curl = curl_easy_init();

	if (curl)
	{
		/* Set connection timeout value in case metaserver is down or something */
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, METASERVER_TIMEOUT);

		/* What URL that receives this POST */
		curl_easy_setopt(curl, CURLOPT_URL, settings.meta_server);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		/* Almost always, we will get HTTP data returned
		 * to us - instead of it going to stderr,
		 * we want to take care of it ourselves. */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, metaserver_writer);
		res = curl_easy_perform(curl);

		if (res)
		{
			LOG(llevDebug, "DEBUG: metaserver_update(): easy_perform got error %d (%s).\n", res, curl_easy_strerror(res));
		}

		/* Always cleanup */
		curl_easy_cleanup(curl);
	}

	/* Free the form */
	curl_formfree(formpost);

	/* Output info that the data was updated. */
	if (!res)
	{
		LOG(llevInfo, "INFO: metaserver_update(): Sent data at %.16s.\n", ctime(&now));
	}

	return NULL;
}

/**
 * Update the metaserver info about this server. */
void metaserver_update()
{
#ifndef WIN32
	pthread_t thread_id;
	int ret;
#endif

	/* If the setting is off, just return */
	if (!settings.meta_on)
	{
		return;
	}

#ifndef WIN32
	/* Create a thread to update the data */
	ret = pthread_create(&thread_id, NULL, metaserver_thread, NULL);

	if (ret)
	{
		LOG(llevBug, "BUG: metaserver_update(): Failed to create thread.\n");
	}

	pthread_detach(thread_id);
#else
	metaserver_thread(NULL);
#endif
}

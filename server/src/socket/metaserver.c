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
 * Metaserver updating related code. */

#include <pthread.h>
#include <global.h>
#include <curl/curl.h>

static void *metaserver_thread(void *junk);

/**
 * Init metaserver. */
void metaserver_init()
{
	int ret;
	pthread_t thread_id;

	if (!settings.meta_on)
	{
		return;
	}

	/* Init global cURL */
	curl_global_init(CURL_GLOBAL_ALL);

	ret = pthread_create(&thread_id, NULL, metaserver_thread, NULL);

	if (ret)
	{
		LOG(llevBug, "BUG: metaserver_init(): Failed to create thread.\n");
	}
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
 * Do the metaserver updating. */
static void metaserver_update()
{
	char buf[MAX_BUF], num_players = 0;
	player *pl;
	struct curl_httppost *formpost = NULL, *lastptr = NULL;
	CURL *curl;
	CURLcode res = 0;
	StringBuffer *sb = stringbuffer_new();
	char *cp;

	for (pl = first_player; pl; pl = pl->next)
	{
		if (pl->state == ST_PLAYING && !pl->ms_privacy && !pl->dm_stealth)
		{
			if (sb->pos)
			{
				stringbuffer_append_string(sb, ":");
			}

			stringbuffer_append_string(sb, pl->quick_name);
		}

		num_players++;
	}

	/* Hostname */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "hostname", CURLFORM_COPYCONTENTS, settings.meta_host, CURLFORM_END);

	/* Server version */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, VERSION, CURLFORM_END);

	/* Server comment */
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "text_comment", CURLFORM_COPYCONTENTS, settings.meta_comment, CURLFORM_END);

	/* Number of players */
	snprintf(buf, sizeof(buf), "%d", num_players);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "num_players", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	/* Port number */
	snprintf(buf, sizeof(buf), "%d", settings.csport);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "port", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	cp = stringbuffer_finish(sb);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "players", CURLFORM_COPYCONTENTS, cp, CURLFORM_END);
	free(cp);

	/* Init "easy" cURL */
	curl = curl_easy_init();

	if (curl)
	{
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
		time_t now = time(NULL);

		LOG(llevInfo, "INFO: metaserver_update(): Sent data at %.19s.\n", ctime(&now));
	}
}

/**
 * Send metaserver updates in a thread.
 * @return NULL. */
static void *metaserver_thread(void *junk)
{
	while (1)
	{
		metaserver_update();
		sleep(600);
	}

	(void) junk;
	return NULL;
}

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

#include <global.h>
#include <curl/curl.h>

void metaserver_init()
{
	if (!settings.meta_on)
		return;

	curl_global_init(CURL_GLOBAL_ALL);

	metaserver_update();
}

static size_t metaserver_writer(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;

    LOG(llevDebug, "DEBUG: metaserver_writer(): Start of text:\n%s\n", (const char*)ptr);
    LOG(llevDebug, "DEBUG: metaserver_writer(): End of text.\n");

    return realsize;
}

void metaserver_update()
{
    char buf[MAX_BUF], num_players = 0;
    player *pl;
	struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
	CURL *curl;
	CURLcode res;

	if (!settings.meta_on)
		return;

	/* We could use socket_info.nconns, but that is not quite as accurate,
     * as connections in the progress of being established, are listening
     * but don't have a player, etc.  This operation below should not be that
     * costly. */
    for (pl = first_player; pl != NULL; pl = pl->next)
		num_players++;

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "hostname", CURLFORM_COPYCONTENTS, settings.meta_host, CURLFORM_END);

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, VERSION, CURLFORM_END);

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "text_comment", CURLFORM_COPYCONTENTS, settings.meta_comment, CURLFORM_END);

	snprintf(buf, MAX_BUF - 1, "%d", num_players);
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "num_players", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	curl = curl_easy_init();
	if (curl)
	{
		/* what URL that receives this POST */
		curl_easy_setopt(curl, CURLOPT_URL, settings.meta_server);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		/* Almost always, we will get HTTP data returned
		 * to us - instead of it going to stderr,
		 * we want to take care of it ourselves. */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, metaserver_writer);
		res = curl_easy_perform(curl);

		if (res)
			LOG(llevDebug, "DEBUG: metaserver_update(): easy_perform got error %d\n", res);

		/* always cleanup */
		curl_easy_cleanup(curl);
	}

	curl_formfree(formpost);

	LOG(llevInfo, "INFO: metaserver_update(): Sent data.\n");
}


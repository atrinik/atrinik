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

#include <include.h>

static size_t write_callback(char *buffer, size_t size, size_t nitems, void *userp)
{
	char *newbuff;
	int rembuff;

	CURL_FILE *url = (CURL_FILE *)userp;
	size *= nitems;

	rembuff = url->buffer_len - url->buffer_pos;

	if (size > rembuff)
	{
		newbuff = realloc(url->buffer, url->buffer_len + (size - rembuff));

		if (newbuff == NULL)
		{
			LOG(LOG_ERROR, "ERROR: Callback buffer grow failed\n");
			size = rembuff;
		}
		else
		{
			url->buffer_len += size - rembuff;
			url->buffer = newbuff;
		}
	}

	memcpy(&url->buffer[url->buffer_pos], buffer, size);
	url->buffer_pos += size;

	return size;
}

static int fill_buffer(CURL_FILE *file, int want, int waittime)
{
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	int maxfd;
	struct timeval timeout;
	int rc;

	if ((!file->still_running) || (file->buffer_pos > want))
		return 0;

	do
	{
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		timeout.tv_sec = 60;
		timeout.tv_usec = 0;

		curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

		rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

		switch (rc)
		{
			case -1:
				break;

			case 0:
				break;

			default:
				while (curl_multi_perform(multi_handle, &file->still_running) == CURLM_CALL_MULTI_PERFORM);

			break;
		}
	} while (file->still_running && (file->buffer_pos < want));

	return 1;
}

static int use_buffer(CURL_FILE *file, int want)
{
	if ((file->buffer_pos - want) <= 0)
	{
		if (file->buffer)
			free(file->buffer);

		file->buffer = NULL;
		file->buffer_pos = 0;
		file->buffer_len = 0;
	}
	else
	{
		memmove(file->buffer, &file->buffer[want], (file->buffer_pos - want));

		file->buffer_pos -= want;
	}
	return 0;
}

CURL_FILE *curl_fopen(const char *url, const char *operation)
{
	CURL_FILE *file;

	file = malloc(sizeof(CURL_FILE));

	if (!file)
		return NULL;

	memset(file, 0, sizeof(CURL_FILE));

	file->handle.curl = curl_easy_init();

	curl_easy_setopt(file->handle.curl, CURLOPT_URL, url);
	curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION, write_callback);

	if (!multi_handle)
		multi_handle = curl_multi_init();

	curl_multi_add_handle(multi_handle, file->handle.curl);

	while (curl_multi_perform(multi_handle, &file->still_running) == CURLM_CALL_MULTI_PERFORM);

	if ((file->buffer_pos == 0) && (!file->still_running))
	{
		curl_multi_remove_handle(multi_handle, file->handle.curl);

		curl_easy_cleanup(file->handle.curl);

		free(file);

		file = NULL;
	}

	return file;
}

int curl_fclose(CURL_FILE *file)
{
	int ret = 0;

	curl_multi_remove_handle(multi_handle, file->handle.curl);

	curl_easy_cleanup(file->handle.curl);

	if (file->buffer)
		free(file->buffer);

	free(file);

	return ret;
}

char *curl_fgets(char *ptr, int size, CURL_FILE *file)
{
	int want = size - 1;
	int loop;

	fill_buffer(file, want, 1);

	if (!file->buffer_pos)
		return NULL;

	if (file->buffer_pos < want)
		want = file->buffer_pos;

	for (loop = 0; loop < want; loop++)
	{
		if (file->buffer[loop] == '\n')
		{
			want = loop + 1;
			break;
		}
	}

	memcpy(ptr, file->buffer, want);
	ptr[want] = 0;

	use_buffer(file, want);

	return ptr;
}

void metaserver_connect(void)
{
	char buf[HUGE_BUF];
	CURL_FILE *handle;

	handle = curl_fopen("http://meta.atrinik.org/", "r");

	while (curl_fgets(buf, HUGE_BUF, handle) != NULL)
	{
		parse_metaserver_data(buf);
	}

	if (handle)
		curl_fclose(handle);
}

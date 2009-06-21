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

#include <curl/curl.h>

struct fcurl_data
{
	union {
		CURL *curl;
		FILE *file;
	} handle;

	char *buffer;
	int buffer_len;
	int buffer_pos;
	int still_running;
};

typedef struct fcurl_data CURL_FILE;

CURL_FILE *curl_fopen(const char *url, const char *operation);
int curl_fclose(CURL_FILE *file);
char *curl_fgets(char *ptr, int size, CURL_FILE *file);

CURLM *multi_handle;

void metaserver_connect(void);

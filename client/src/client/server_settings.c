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
 *  */

#include <include.h>

/**
 * Find a face ID by name. Request the face by finding it, loading it or requesting it.
 * @param name Face name to find
 * @return Face ID if found, -1 otherwise */
static int get_bmap_id(char *name)
{
	int i;

	for (i=0;i<bmaptype_table_size;i++)
	{
		if (!strcmp(bmaptype_table[i].name,name))
		{
			request_face(i, 0);
			return i;
		}
	}

	return -1;
}

/**
 * Load settings file. */
void load_settings()
{
	FILE *stream;
	char buf[HUGE_BUF], buf1[HUGE_BUF], buf2[HUGE_BUF];
	char cmd[HUGE_BUF];
	char para[HUGE_BUF];
	int para_count = 0, last_cmd = 0;
	int tmp_level = 0;

	delete_server_chars();
	LOG(llevDebug, "Loading %s...\n", FILE_CLIENT_SETTINGS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SETTINGS, "rb")) != NULL)
	{
		while (fgets(buf, HUGE_BUF - 1, stream) != NULL)
		{
			if (buf[0] == '#' || buf[0] == '\0')
				continue;

			if (last_cmd == 0)
			{
				sscanf(adjust_string(buf), "%s %s", cmd, para);

				if (!strcmp(cmd, "char"))
				{
					_server_char *serv_char = malloc( sizeof(_server_char));

					memset(serv_char, 0, sizeof(_server_char));
					/* Copy name */
					serv_char->name = malloc(strlen(para) + 1);
					strcpy(serv_char->name, para);

					/* Get next legal line */
					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%s %d %d %d %d %d %d", buf1, &serv_char->bar[0], &serv_char->bar[1], &serv_char->bar[2], &serv_char->bar_add[0], &serv_char->bar_add[1], &serv_char->bar_add[2]);

					serv_char->pic_id = get_bmap_id(buf1);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[0], buf1, buf2);
					serv_char->char_arch[0] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[0], buf1);
					serv_char->face_id[0] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[1], buf1, buf2);
					serv_char->char_arch[1] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[1], buf1);
					serv_char->face_id[1] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[2], buf1, buf2);
					serv_char->char_arch[2] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[2], buf1);
					serv_char->face_id[2] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[3], buf1, buf2);
					serv_char->char_arch[3] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[3], buf1);
					serv_char->face_id[3] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &serv_char->stat_points, &serv_char->stats[0], &serv_char->stats_min[0], &serv_char->stats_max[0], &serv_char->stats[1], &serv_char->stats_min[1], &serv_char->stats_max[1], &serv_char->stats[2], &serv_char->stats_min[2], &serv_char->stats_max[2], &serv_char->stats[3], &serv_char->stats_min[3], &serv_char->stats_max[3], &serv_char->stats[4], &serv_char->stats_min[4], &serv_char->stats_max[4], &serv_char->stats[5], &serv_char->stats_min[5], &serv_char->stats_max[5], &serv_char->stats[6], &serv_char->stats_min[6], &serv_char->stats_max[6]);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[0] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[0], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[1] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[1], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[2] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[2], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[3] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[3], buf);

					/* Add this char template to list */
					if (!first_server_char)
						first_server_char = serv_char;
					else
					{
						_server_char *tmpc;

						for (tmpc = first_server_char; tmpc->next; tmpc = tmpc->next);

						tmpc->next = serv_char;
						serv_char->prev = tmpc;
					}
				}
				else if (!strcmp(cmd, "level"))
				{
					tmp_level = atoi(para);

					if (tmp_level < 0 || tmp_level > 450)
					{
						fclose(stream);
						LOG(llevError, "ERROR: load_settings(): Level command out of bounds! >%s<\n", buf);
						return;
					}

					server_level.level = tmp_level;
					/* Command 'level' */
					last_cmd = 1;
					para_count = 0;
				}
				/* We close here... better we include later a fallback to login */
				else
				{
					fclose(stream);
					LOG(llevError, "ERROR: Unknown command in client_settings! >%s<\n", buf);
					return;
				}
			}
			else if (last_cmd == 1)
			{
				server_level.exp[para_count++] = strtoull(buf, NULL, 16);

				if (para_count >tmp_level)
					last_cmd = 0;
			}
		}

		fclose(stream);
	}

	if (first_server_char)
	{
		int g;

		memcpy(&new_character, first_server_char, sizeof(_server_char));

		/* Adjust gender */
		for (g = 0; g < 4; g++)
		{
			if (new_character.gender[g])
			{
				new_character.gender_selected = g;
				break;
			}
		}
	}
}


/**
 * Read settings file. */
void read_settings()
{
	FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_SETTINGS].len = 0;
	srv_client_files[SRV_CLIENT_SETTINGS].crc = 0;
	LOG(llevDebug, "Reading %s...", FILE_CLIENT_SETTINGS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SETTINGS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SETTINGS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_SETTINGS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		fclose(stream);
		LOG(llevDebug, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_SETTINGS].len, srv_client_files[SRV_CLIENT_SETTINGS].crc);
	}

	LOG(llevDebug, " Done.\n");
}

/**
 * In the settings file we have a list of character templates
 * for character building. This function deletes this list. */
void delete_server_chars()
{
	_server_char *tmp, *tmp1;

	for (tmp1 = tmp = first_server_char; tmp1; tmp = tmp1)
	{
		tmp1 = tmp->next;
		free(tmp->name);
		free(tmp->desc[0]);
		free(tmp->desc[1]);
		free(tmp->desc[2]);
		free(tmp->desc[3]);
		free(tmp->char_arch[0]);
		free(tmp->char_arch[1]);
		free(tmp->char_arch[2]);
		free(tmp->char_arch[3]);
		free(tmp);
	}

	first_server_char = NULL;
}

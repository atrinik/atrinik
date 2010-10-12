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
 * Load temporary animations. */
static int load_anim_tmp()
{
	int anim_len = 0, new_anim = 1;
	uint8 faces = 0;
	FILE *stream;
	char buf[HUGE_BUF];
	unsigned char anim_cmd[2048];
	size_t count = 0;

	if (animations_num)
	{
		size_t i;

		/* Clear both animation tables. */
		for (i = 0; i < animations_num; i++)
		{
			if (animations[i].faces)
			{
				free(animations[i].faces);
			}

			if (anim_table[i].anim_cmd)
			{
				free(anim_table[i].anim_cmd);
			}
		}

		free(animations);
		animations = NULL;
		free(anim_table);
		anim_table = NULL;
		animations_num = 0;
	}

	anim_table = malloc(sizeof(_anim_table));

	/* Animation #0 is like face id #0. */
	anim_cmd[0] = (unsigned char) ((count >> 8) & 0xff);
	anim_cmd[1] = (unsigned char) (count & 0xff);
	anim_cmd[2] = 0;
	anim_cmd[3] = 1;
	anim_cmd[4] = 0;
	anim_cmd[5] = 0;

	anim_table[count].anim_cmd = malloc(6);
	memcpy(anim_table[count].anim_cmd, anim_cmd, 6);
	anim_table[count].len = 6;
	count++;

	if ((stream = fopen_wrapper(FILE_ANIMS_TMP, "rt")) == NULL)
	{
		LOG(llevError, "load_anim_tmp: Error reading anim.tmp!\n");
	}

	while (fgets(buf, sizeof(buf) - 1, stream) != NULL)
	{
		/* Are we outside an anim body? */
		if (new_anim == 1)
		{
			if (!strncmp(buf, "anim ", 5))
			{
				new_anim = 0;
				faces = 0;
				anim_cmd[0] = (unsigned char)((count >> 8) & 0xff);
				anim_cmd[1] = (unsigned char)(count & 0xff);
				faces = 1;
				anim_len = 4;
			}
			/* we should never hit this point */
			else
			{
				LOG(llevBug, "load_anim_tmp(): Error parsing anims.tmp - unknown cmd: >%s<!\n", buf);
			}
		}
		/* No, we are inside! */
		else
		{
			if (!strncmp(buf, "facings ", 8))
			{
				faces = atoi(buf + 8);
			}
			else if (!strncmp(buf, "mina", 4))
			{
				anim_table = realloc(anim_table, sizeof(_anim_table) * (count + 1));
				anim_cmd[2] = 0;
				anim_cmd[3] = faces;
				anim_table[count].len = anim_len;
				anim_table[count].anim_cmd = malloc(anim_len);
				memcpy(anim_table[count].anim_cmd, anim_cmd, anim_len);
				count++;
				new_anim = 1;
			}
			else
			{
				uint16 face_id = atoi(buf);

				anim_cmd[anim_len++] = (unsigned char) ((face_id >> 8) & 0xff);
				anim_cmd[anim_len++] = (unsigned char) (face_id & 0xff);
			}
		}
	}

	animations_num = count;
	animations = calloc(animations_num, sizeof(Animations));
	fclose(stream);
	return 1;
}

/**
 * Read temporary animations. */
int read_anim_tmp()
{
	FILE *stream, *ftmp;
	int i, new_anim = 1, count = 1;
	char buf[HUGE_BUF], cmd[HUGE_BUF];
	struct stat	stat_bmap, stat_anim, stat_tmp;

	/* if this fails, we have a urgent problem somewhere before */
	if ((stream = fopen_wrapper(FILE_BMAPS_TMP, "rb" )) == NULL)
	{
		LOG(llevError, "read_anim_tmp:Error reading bmap.tmp for anim.tmp!\n");
	}
	fstat(fileno(stream), &stat_bmap);
	fclose(stream);

	if ( (stream = server_file_open(SERVER_FILE_ANIMS)) == NULL )
	{
		LOG(llevError,"read_anim_tmp:Error reading bmap.tmp for anim.tmp!\n");
	}
	fstat(fileno(stream), &stat_anim);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_ANIMS_TMP, "rb" )) != NULL )
	{
		fstat(fileno(stream), &stat_tmp);
		fclose( stream );

		/* our anim file must be newer as our default anim file */
		if (difftime(stat_tmp.st_mtime, stat_anim.st_mtime) > 0.0f)
		{
			/* our anim file must be newer as our bmaps.tmp */
			if (difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
				return load_anim_tmp(); /* all fine - load file */
		}
	}

	unlink(FILE_ANIMS_TMP); /* for some reason - recreate this file */
	if ( (ftmp = fopen_wrapper(FILE_ANIMS_TMP, "wt" )) == NULL )
	{
		LOG(llevError,"read_anim_tmp:Error opening anims.tmp!\n");
	}

	if ( (stream = server_file_open(SERVER_FILE_ANIMS)) == NULL )
	{
		LOG(llevError, "read_anim_tmp:Error reading client_anims for anims.tmp!\n");
	}

	while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%s",cmd);
		if (new_anim == 1) /* we are outside a anim body ? */
		{
			if (!strncmp(buf, "anim ",5))
			{
				sprintf(cmd, "anim %d -> %s",count++, buf);
				fputs(cmd,ftmp); /* safe this key string! */
				new_anim = 0;
			}
			else /* we should never hit this point */
			{
				LOG(llevBug,"read_anim_tmp:Error parsing client_anim - unknown cmd: >%s<!\n", cmd);
			}
		}
		else /* no, we are inside! */
		{
			if (!strncmp(buf, "facings ",8))
			{
				fputs(buf, ftmp); /* safe this key word! */
			}
			else if (!strncmp(cmd, "mina",4))
			{
				fputs(buf, ftmp); /* safe this key word! */
				new_anim = 1;
			}
			else
			{
				/* this is really slow when we have more pictures - we
				 * browsing #anim * #bmaps times the same table -
				 * pretty bad - when we stay to long here, we must create
				 * for bmaps.tmp entries a hash table too. */
				for (i=0;i<bmaptype_table_size;i++)
				{
					if (!strcmp(bmaptype_table[i].name,cmd))
						break;
				}

				if (i>=bmaptype_table_size)
				{
					/* if we are here then we have a picture name in the anims file
					 * which we don't have in our bmaps file! Pretty bad. But because
					 * face #0 is ALWAYS bug.101 - we simply use it here! */
					i=0;
					LOG(llevBug,"read_anim_tmp: Invalid anim name >%s< - set to #0 (bug.101)!\n", cmd);
				}
				sprintf(cmd, "%d\n",i);
				fputs(cmd, ftmp);
			}
		}
	}

	fclose( stream );
	fclose( ftmp );
	return load_anim_tmp(); /* all fine - load file */
}

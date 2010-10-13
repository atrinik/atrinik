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
 * Read bmaps from atrinik.p0, calculate checksums, etc. */
void read_bmaps_p0()
{
	FILE *fp;
	size_t tmp_buf_size, pos;
	char *tmp_buf, buf[MAX_BUF], *cp;
	size_t len;
	_bmaptype *at;

	fp = fopen_wrapper(FILE_ATRINIK_P0, "rb");

	if (!fp)
	{
		LOG(llevError, "%s doesn't exist.\n", FILE_ATRINIK_P0);
	}

	memset((void *) bmap_table, 0, BMAPTABLE * sizeof(_bmaptype *));

	tmp_buf_size = 24 * 1024;
	tmp_buf = malloc(tmp_buf_size);

	while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
	{
		if (strncmp(buf, "IMAGE ", 6))
		{
			LOG(llevError, "The file %s is corrupted.\n", FILE_ATRINIK_P0);
		}

		/* Skip accross the image ID data. */
		for (cp = buf + 6; *cp != ' '; cp++)
		{
		}

		len = atoi(cp);

		/* Skip accross the length data. */
		for (cp = cp + 1; *cp != ' '; cp++)
		{
		}

		/* Adjust the buffer if necessary. */
		if (len > tmp_buf_size)
		{
			tmp_buf_size = len;
			tmp_buf = realloc(tmp_buf, tmp_buf_size);
		}

		pos = ftell(fp);

		if (!fread(tmp_buf, 1, len, fp))
		{
			break;
		}

		at = (_bmaptype *) malloc(sizeof(_bmaptype));
		at->name = strdup(cp);
		at->crc = crc32(1L, (const unsigned char FAR *) tmp_buf, len);
		at->len = len;
		at->pos = pos;
		add_bmap(at);
	}

	free(tmp_buf);
	fclose(fp);
}

/**
 * Free temporary bitmaps. */
void delete_bmap_tmp()
{
	int i;

	bmaptype_table_size = 0;

	for (i = 0; i < MAX_BMAPTYPE_TABLE; i++)
	{
		if (bmaptype_table[i].name)
			free(bmaptype_table[i].name);

		bmaptype_table[i].name = NULL;
	}
}

/**
 * Load temporary bitmaps. */
static int load_bmap_tmp()
{
	FILE *stream;
	char buf[HUGE_BUF],name[HUGE_BUF];
	int i=0,len, pos;
	unsigned int crc;

	delete_bmap_tmp();
	if ( (stream = fopen_wrapper(FILE_BMAPS_TMP, "rt" )) == NULL )
	{
		LOG(llevError,"bmaptype_table(): error open file <bmap.tmp>\n");
	}

	while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%d %d %x %s\n", &pos, &len, &crc, name);
		bmaptype_table[i].crc = crc;
		bmaptype_table[i].len = len;
		bmaptype_table[i].pos = pos;
		bmaptype_table[i].name =(char*) malloc(strlen(name)+1);
		strcpy(bmaptype_table[i].name,name);
		i++;
	}

	bmaptype_table_size=i;
	fclose( stream );
	return 0;
}

/**
 * Read temporary bitmaps file. */
int read_bmap_tmp()
{
	FILE *stream, *fbmap0;
	char buf[HUGE_BUF],name[HUGE_BUF];
	struct stat	stat_bmap, stat_tmp, stat_bp0;
	int len;
	unsigned int crc;
	_bmaptype *at;

	if ( (stream = server_file_open(SERVER_FILE_BMAPS)) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}

	fstat(fileno(stream), &stat_bmap);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_BMAPS_P0, "rb" )) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}

	fstat(fileno(stream), &stat_bp0);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_BMAPS_TMP, "rb" )) == NULL )
		goto create_bmap_tmp;

	fstat(fileno(stream), &stat_tmp);
	fclose( stream );

	/* ok - client_bmap & bmaps.p0 are there - now check
	 * our bmap_tmp is newer - is not newer as both, we
	 * create it new - then it is newer. */

	if (difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
	{
		if (difftime(stat_tmp.st_mtime, stat_bp0.st_mtime) > 0.0f)
			return load_bmap_tmp(); /* all fine */
	}

create_bmap_tmp:
	unlink(FILE_BMAPS_TMP);

	/* NOW we are sure... we must create us a new bmaps.tmp */
	if ( (stream = server_file_open(SERVER_FILE_BMAPS)) != NULL )
	{
		/* we can use text mode here, its local */
		if ( (fbmap0 = fopen_wrapper(FILE_BMAPS_TMP, "wt" )) != NULL )
		{
			/* read in the bmaps from the server, check with the
			 * loaded bmap table (from bmaps.p0) and create with
			 * this information the bmaps.tmp file. */
			while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
			{
				sscanf(buf,"%x %x %s", (unsigned int *)&len, &crc, name);
				at=find_bmap(name);

				/* now we can check, our local file package has
				 * the right png - if not, we mark this pictures
				 * as "in cache". We don't check it here now -
				 * that will happens at runtime.
				 * That can change when we include later a forbidden
				 * flag in the server (no face send) - then we need
				 * to break and upddate the picture and/or check the cache. */
				/* position -1 mark "not i the atrinik.p0 file */
				if (!at || at->len != len || at->crc != crc) /* is different or not there! */
					sprintf(buf,"-1 %d %x %s\n", len, crc, name);
				else /* we have it */
					sprintf(buf,"%d %d %x %s\n", at->pos, len, crc, name);
				fputs(buf, fbmap0);
			}

			fclose( fbmap0 );
		}

		fclose( stream );
	}

	return load_bmap_tmp(); /* all fine */
}

/**
 * Read bitmaps file. */
void read_bmaps()
{
	FILE *fp = server_file_open(SERVER_FILE_BMAPS);

	if (!fp)
	{
		unlink(FILE_BMAPS_TMP);
		return;
	}

	fclose(fp);
	read_bmap_tmp();
	read_anim_tmp();
}

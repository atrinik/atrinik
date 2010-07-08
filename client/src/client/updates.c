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
 * Handles code for file updates by the server. */

#include <include.h>

/**
 * How many file updates have been requested. This is used to block the
 * login: it's not possible to login unless this value is 0, to ensure
 * everything is downloaded intact from the server first. */
static size_t file_updates_requested = 0;

/**
 * Initialize the file updates, reading len/CRC32 from the srv file. */
void file_updates_init()
{
	FILE *fp;
	struct stat sb;
	size_t st_size;
	char *contents;

	srv_client_files[SRV_FILE_UPDATES].len = 0;
	srv_client_files[SRV_FILE_UPDATES].crc = 0;

	LOG(llevDebug, "Reading %s...\n", FILE_UPDATES);
	fp = fopen_wrapper(FILE_UPDATES, "rb");

	if (!fp)
	{
		return;
	}

	fstat(fileno(fp), &sb);
	st_size = sb.st_size;
	srv_client_files[SRV_FILE_UPDATES].len = st_size;

	contents = malloc(st_size);
	fread(contents, 1, st_size, fp);
	fclose(fp);

	srv_client_files[SRV_FILE_UPDATES].crc = crc32(1L, (const unsigned char FAR *) contents, st_size);
	free(contents);
}

/**
 * Request the server to send us an updated copy of a file.
 * @param filename What to request. */
static void file_updates_request(char *filename)
{
	SockList sl;
	unsigned char buf[HUGE_BUF];

	sl.buf = buf;
	sl.len = 0;
	SockList_AddString(&sl, "upf ");
	SockList_AddString(&sl, filename);
	send_socklist(sl);

	file_updates_requested++;
}

/**
 * We have received the file update command, in which the updated file
 * is, so parse it.
 * @param data Data to parse.
 * @param len Length of 'data'. */
void cmd_request_update(unsigned char *data, int len)
{
	char filename[MAX_BUF], c;
	size_t pos = 0, i = 0;
	unsigned long ucomp_len;
	unsigned char *dest;
	FILE *fp;

	filename[0] = '\0';
	file_updates_requested--;

	while ((c = (char) (data[pos++])))
	{
		filename[i++] = c;
	}

	filename[i] = '\0';
	len -= i;
	ucomp_len = GetInt_String(data + pos);
	pos += 4;
	len -= 4;

	/* Uncompress it. */
	dest = malloc(ucomp_len);
	uncompress((Bytef *) dest, (uLongf *) &ucomp_len, (const Bytef *) data + pos, (uLong) len);
	data = dest;
	len = ucomp_len;

	fp = fopen(filename, "wb");

	if (!fp)
	{
		LOG(llevError, "ERROR: Could not open file '%s' for writing.\n", filename);
		return;
	}

	/* Update the file. */
	fwrite(data, 1, len, fp);
	fclose(fp);
	LOG(llevMsg, "Updated file '%s'.\n", filename);
}

/**
 * Check if we have finished downloading updated files from the server.
 * @return 1 if we have finished, 0 otherwise. */
int file_updates_finished()
{
	return file_updates_requested == 0;
}

/**
 * Parse the updates srv file, and request updated files as needed. */
void file_updates_parse()
{
	FILE *fp;
	char buf[HUGE_BUF];

	/* Is the feature disabled? */
	if (options.disable_updates)
	{
		return;
	}

	fp = fopen_wrapper(FILE_UPDATES, "rb");

	if (!fp)
	{
		return;
	}

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		char filename[MAX_BUF], crc_buf[MAX_BUF], *contents;
		size_t size, st_size;
		FILE *fp2;
		unsigned long crc;
		struct stat sb;

		if (sscanf(buf, "%s %ld %s", filename, &size, crc_buf) != 3)
		{
			continue;
		}

		fp2 = fopen(filename, "rb");

		/* No such file? Then we'll want to update this. */
		if (!fp2)
		{
			file_updates_request(filename);
			continue;
		}

		fstat(fileno(fp2), &sb);
		st_size = sb.st_size;
		contents = malloc(st_size);
		fread(contents, 1, st_size, fp2);
		fclose(fp2);

		/* Get the CRC32... */
		crc = crc32(1L, (const unsigned char FAR *) contents, st_size);
		free(contents);

		/* If the checksum or the size doesn't match, we'll want to update it. */
		if (crc != strtoul(crc_buf, NULL, 16) || st_size != size)
		{
			file_updates_request(filename);
		}
	}

	fclose(fp);
}

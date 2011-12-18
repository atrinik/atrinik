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
 * Handles file updates that the client may request. */

#include <global.h>
#include "zlib.h"

/**
 * All the files the client can request an update for, sorted using
 * Quicksort.. */
static update_file_struct *update_files;
/** Number of ::update_files. */
static size_t update_files_num;

/**
 * Add a new file to ::update_files.
 * @param filename Path to the file to add.
 * @param sb Stat buffer. */
static void updates_file_new(const char *filename, struct stat *sb)
{
	char *contents, *compressed;
	size_t st_size, numread;
	FILE *fp;

	update_files = realloc(update_files, sizeof(update_file_struct) * (update_files_num + 1));

	if (!update_files)
	{
		LOG(llevError, "updates_file_new(): Out of memory.\n");
		return;
	}

	st_size = sb->st_size;
	/* Allocate a buffer to hold the whole file. */
	contents = malloc(st_size);

	if (!contents)
	{
		LOG(llevError, "updates_file_new(): Out of memory.\n");
		return;
	}

	fp = fopen(filename, "rb");

	if (!fp)
	{
		LOG(llevError, "updates_file_new(): Could not open file '%s' for reading.\n", filename);
		return;
	}

	numread = fread(contents, 1, st_size, fp);
	fclose(fp);

	update_files[update_files_num].filename = strdup(filename + strlen(UPDATES_DIR_NAME) + 1);
	update_files[update_files_num].checksum = crc32(1L, (const unsigned char FAR *) contents, numread);
	update_files[update_files_num].ucomp_len = numread;
	/* Calculate the upper bound of the compressed size. */
	numread = compressBound(st_size);
	/* Allocate a buffer to hold the compressed file. */
	compressed = malloc(numread);

	if (!compressed)
	{
		LOG(llevError, "updates_file_new(): Out of memory.\n");
		return;
	}

	compress2((Bytef *) compressed, (uLong *) &numread, (const unsigned char FAR *) contents, st_size, Z_BEST_COMPRESSION);
	update_files[update_files_num].contents = malloc(numread);

	if (!update_files[update_files_num].contents)
	{
		LOG(llevError, "updates_file_new(): Out of memory.\n");
		return;
	}

	memcpy(update_files[update_files_num].contents, compressed, numread);
	update_files[update_files_num].len = numread;

	/* Free temporary buffers. */
	free(contents);
	free(compressed);

	update_files[update_files_num].packet = packet_new(CLIENT_CMD_FILE_UPDATE, 0, 0);
	packet_append_string_terminated(update_files[update_files_num].packet, update_files[update_files_num].filename);
	packet_append_uint32(update_files[update_files_num].packet, update_files[update_files_num].ucomp_len);
	packet_append_data_len(update_files[update_files_num].packet, update_files[update_files_num].contents, update_files[update_files_num].len);

	LOG(llevDebug, "  Loaded '%s': ucomp: %"FMT64U", comp: %"FMT64U" (%3.1f%%), CRC32: %lx.\n", filename, (uint64) update_files[update_files_num].ucomp_len, (uint64) numread, (float) (numread * 100) / update_files[update_files_num].ucomp_len, update_files[update_files_num].checksum);
	update_files_num++;
}

/**
 * Compare two entries of ::update_files.
 * @param a First entry to compare.
 * @param b Second entry to compare.
 * @return Return value of strcmp(). */
static int updates_file_compare(const void *a, const void *b)
{
	return strcmp(((update_file_struct *) a)->filename, ((update_file_struct *) b)->filename);
}

/**
 * Find an entry in ::update_files based on its filename.
 * @param filename What to look for.
 * @return The found entry, NULL if not found. */
static update_file_struct *updates_file_find(const char *filename)
{
	update_file_struct key;

	key.filename = (char *) filename;
	return bsearch((void *) &key, (void *) update_files, update_files_num, sizeof(update_file_struct), updates_file_compare);
}

/**
 * Recursively traverse through the updates directory.
 * @param path Path we're traversing through. */
static void updates_traverse(const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *d;
	char filename[HUGE_BUF];
	struct stat sb;

	if (!dir)
	{
		LOG(llevError, "traverse_updates(): Could not open directory '%s'.\n", path);
		return;
	}

	while ((d = readdir(dir)))
	{
		/* Ignore . and .. entries. */
		if (!strncmp(d->d_name, ".", NAMLEN(d)) || !strncmp(d->d_name, "..", NAMLEN(d)))
		{
			continue;
		}

		snprintf(filename, sizeof(filename), "%s/%s", path, d->d_name);
		stat(filename, &sb);

		/* Go recursively through directories. */
		if (S_ISDIR(sb.st_mode))
		{
			updates_traverse(filename);
			continue;
		}

		updates_file_new(filename, &sb);
	}

	closedir(dir);
}

/**
 * Initialize all the file updates, traversing the updates directory,
 * creating the srv updates file, etc. */
void updates_init(void)
{
	char path[HUGE_BUF];
	FILE *fp;
	size_t i;

	update_files = NULL;
	update_files_num = 0;

	LOG(llevDebug, "Loading client updates...\n");
	updates_traverse(UPDATES_DIR_NAME);
	/* Sort the entries. */
	qsort((void *) update_files, update_files_num, sizeof(update_file_struct), (void *) (int (*)()) updates_file_compare);

	snprintf(path, sizeof(path), "%s/%s", settings.localdir, UPDATES_FILE_NAME);
	LOG(llevDebug, "Creating '%s'...\n", path);
	fp = fopen(path, "wb");

	if (!fp)
	{
		LOG(llevError, "updates_init(): Could not open file '%s' for writing.\n", path);
		return;
	}

	for (i = 0; i < update_files_num; i++)
	{
		update_file_struct *tmp = &update_files[i];

		fprintf(fp, "%s %"FMT64U" %lx\n", tmp->filename, (uint64) tmp->ucomp_len, tmp->checksum);
	}

	fclose(fp);
}

void socket_command_request_update(socket_struct *ns, player *pl, uint8 *data, size_t len, size_t pos)
{
	char buf[MAX_BUF];
	update_file_struct *tmp;

	if (ns->status != Ns_Add)
	{
		return;
	}

	/* Try to find the file. */
	tmp = updates_file_find(packet_to_string(data, len, &pos, buf, sizeof(buf)));

	/* Invalid file. */
	if (!tmp)
	{
		return;
	}

	socket_send_packet(ns, packet_dup(tmp->packet));
}

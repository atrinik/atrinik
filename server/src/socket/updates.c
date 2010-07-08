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
		LOG(llevError, "ERROR: updates_file_new(): Out of memory.\n");
		return;
	}

	st_size = sb->st_size;
	/* Allocate a buffer to hold the whole file. */
	contents = malloc(st_size);

	if (!contents)
	{
		LOG(llevError, "ERROR: updates_file_new(): Out of memory.\n");
		return;
	}

	fp = fopen(filename, "rb");

	if (!fp)
	{
		LOG(llevError, "ERROR: updates_file_new(): Could not open file '%s' for reading.\n", filename);
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
		LOG(llevError, "ERROR: updates_file_new(): Out of memory.\n");
		return;
	}

	compress2((Bytef *) compressed, (uLong *) &numread, (const unsigned char FAR *) contents, st_size, Z_BEST_COMPRESSION);
	update_files[update_files_num].contents = malloc(numread);

	if (!update_files[update_files_num].contents)
	{
		LOG(llevError, "ERROR: updates_file_new(): Out of memory.\n");
		return;
	}

	memcpy(update_files[update_files_num].contents, compressed, numread);
	update_files[update_files_num].len = numread;

	/* Free temporary buffers. */
	free(contents);
	free(compressed);

	/* 1 for the command type, xxx for the filename, 1 for trailing newline
	 * of the filename, 4 for original uncompressed length. */
	update_files[update_files_num].sl.buf = malloc(1 + strlen(filename) + 1 + 4 + update_files[update_files_num].len);
	/* Set the type. */
	SOCKET_SET_BINARY_CMD(&update_files[update_files_num].sl, BINARY_CMD_FILE_UPD);
	/* Add the filename. */
	SockList_AddString(&update_files[update_files_num].sl, update_files[update_files_num].filename);
	/* The uncompressed length. */
	SockList_AddInt(&update_files[update_files_num].sl, update_files[update_files_num].ucomp_len);
	/* Add the file contents. */
	memcpy(update_files[update_files_num].sl.buf + update_files[update_files_num].sl.len, update_files[update_files_num].contents, update_files[update_files_num].len);
	update_files[update_files_num].sl.len += update_files[update_files_num].len;

	LOG(llevDebug, "  Loaded '%s': ucomp: %d, comp: %d (%3.1f%%), CRC32: %x.\n", filename, update_files[update_files_num].ucomp_len, numread, (float) (numread * 100) / update_files[update_files_num].ucomp_len, update_files[update_files_num].checksum);
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
		LOG(llevError, "ERROR: traverse_updates(): Could not open directory '%s'.\n", path);
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
void updates_init()
{
	char path[HUGE_BUF];
	FILE *fp;
	size_t i;

	update_files = NULL;
	update_files_num = 0;

	LOG(llevInfo, "INFO: Loading client updates...\n");
	updates_traverse(UPDATES_DIR_NAME);
	/* Sort the entries. */
	qsort((void *) update_files, update_files_num, sizeof(update_file_struct), (void *) (int (*)()) updates_file_compare);

	snprintf(path, sizeof(path), "%s/%s", settings.localdir, UPDATES_FILE_NAME);
	LOG(llevInfo, "INFO: Creating '%s'...\n", path);
	fp = fopen(path, "wb");

	if (!fp)
	{
		LOG(llevError, "ERROR: updates_init(): Could not open file '%s' for writing.\n", path);
		return;
	}

	for (i = 0; i < update_files_num; i++)
	{
		update_file_struct *tmp = &update_files[i];

		fprintf(fp, "%s %lu %lx\n", tmp->filename, (unsigned long) tmp->ucomp_len, tmp->checksum);
	}

	fclose(fp);
}

/**
 * Client has requested us to send it the specified file.
 * @param buf Data.
 * @param len Length of 'buf'.
 * @param ns Client's socket. */
void cmd_request_update(char *buf, int len, socket_struct *ns)
{
	update_file_struct *tmp;

	/* We assume all the update files will have at least 2 letters
	 * (including directories they are in). */
	if (ns->status != Ns_Add || !buf || len < 2)
	{
		return;
	}

	/* Try to find the file. */
	tmp = updates_file_find(buf);

	/* Invalid file. */
	if (!tmp)
	{
		return;
	}

	Send_With_Handling(ns, &tmp->sl);
}

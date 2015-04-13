/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
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
#include <packet.h>
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

    update_files = erealloc(update_files, sizeof(update_file_struct) * (update_files_num + 1));

    st_size = sb->st_size;
    /* Allocate a buffer to hold the whole file. */
    contents = emalloc(st_size);

    fp = fopen(filename, "rb");

    if (!fp) {
        logger_print(LOG(ERROR), "Could not open file '%s' for reading.", filename);
        exit(1);
    }

    numread = fread(contents, 1, st_size, fp);
    fclose(fp);

    update_files[update_files_num].filename = estrdup(filename + strlen(UPDATES_DIR_NAME) + 1);
    update_files[update_files_num].checksum = crc32(1L, (const unsigned char FAR *) contents, numread);
    update_files[update_files_num].ucomp_len = numread;
    /* Calculate the upper bound of the compressed size. */
    numread = compressBound(st_size);
    /* Allocate a buffer to hold the compressed file. */
    compressed = emalloc(numread);
    compress2((Bytef *) compressed, (uLong *) & numread, (const unsigned char FAR *) contents, st_size, Z_BEST_COMPRESSION);
    update_files[update_files_num].contents = emalloc(numread);
    memcpy(update_files[update_files_num].contents, compressed, numread);
    update_files[update_files_num].len = numread;

    /* Free temporary buffers. */
    efree(contents);
    efree(compressed);

    update_files[update_files_num].packet = packet_new(CLIENT_CMD_FILE_UPDATE, 0, 0);
    packet_append_string_terminated(update_files[update_files_num].packet, update_files[update_files_num].filename);
    packet_append_uint32(update_files[update_files_num].packet, update_files[update_files_num].ucomp_len);
    packet_append_data_len(update_files[update_files_num].packet, update_files[update_files_num].contents, update_files[update_files_num].len);
    update_files_num++;
}

/**
 * Compare two entries of ::update_files.
 * @param a First entry to compare.
 * @param b Second entry to compare.
 * @return Return value of strcmp(). */
static int updates_file_compare(const void *a, const void *b)
{
    return strcmp(((const update_file_struct *) a)->filename, ((const update_file_struct *) b)->filename);
}

/**
 * Find an entry in ::update_files based on its filename.
 * @param filename What to look for.
 * @return The found entry, NULL if not found. */
static update_file_struct *updates_file_find(const char *filename)
{
    update_file_struct key;

    key.filename = (char *) filename;
    return bsearch(&key, update_files, update_files_num, sizeof(update_file_struct), updates_file_compare);
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

    if (!dir) {
        logger_print(LOG(ERROR), "Could not open directory '%s'.", path);
        exit(1);
    }

    while ((d = readdir(dir))) {
        /* Ignore . and .. entries. */
        if (!strncmp(d->d_name, ".", NAMLEN(d)) || !strncmp(d->d_name, "..", NAMLEN(d))) {
            continue;
        }

        snprintf(filename, sizeof(filename), "%s/%s", path, d->d_name);
        stat(filename, &sb);

        /* Go recursively through directories. */
        if (S_ISDIR(sb.st_mode)) {
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

    if (path_exists(UPDATES_DIR_NAME)) {
        updates_traverse(UPDATES_DIR_NAME);

        /* Sort the entries. */
        if (update_files != NULL) {
            qsort(update_files, update_files_num, sizeof(update_file_struct), updates_file_compare);
        }
    }

    snprintf(path, sizeof(path), "%s/%s", settings.datapath, UPDATES_FILE_NAME);
    fp = fopen(path, "wb");

    if (!fp) {
        logger_print(LOG(ERROR), "Could not open file '%s' for writing.", path);
        exit(1);
    }

    for (i = 0; i < update_files_num; i++) {
        update_file_struct *tmp = &update_files[i];

        fprintf(fp, "%s %"PRIu64 " %lx\n", tmp->filename, (uint64_t) tmp->ucomp_len, tmp->checksum);
    }

    fclose(fp);
}

void socket_command_request_update(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos)
{
    char buf[MAX_BUF];
    update_file_struct *tmp;

    if (ns->state != ST_LOGIN) {
        return;
    }

    /* Try to find the file. */
    tmp = updates_file_find(packet_to_string(data, len, &pos, buf, sizeof(buf)));

    /* Invalid file. */
    if (!tmp) {
        return;
    }

    socket_send_packet(ns, packet_dup(tmp->packet));
}

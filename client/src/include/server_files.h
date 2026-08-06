/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Header file for servers files declarations.
 */

#ifndef SERVER_FILES_H
#define SERVER_FILES_H

#include <toolkit/curl.h>
#include <asset.h>
#include <asset_source.h>

#define SERVER_FILES_HTTP_DIR "data"
#define SERVER_FILES_HTTP_LISTING "listing.txt"

#define SERVER_FILE_ANIMS "anims"
#define SERVER_FILE_BMAPS "bmaps"
#define SERVER_FILE_UPDATES "updates"
#define SERVER_FILE_SETTINGS "settings"
#define SERVER_FILE_EFFECTS "effects"
#define SERVER_FILE_HFILES "hfiles"

/** One server file. */
typedef struct server_files_struct {
    /** Name of the server file. */
    char *name;

    /** If 0, will be (re-)loaded. */
    uint8_t loaded;

    /**
     * Update status of this file:
     *
     * - 0: Not being updated, or just finished updating.
     * - 1: Start updating the file the next time server_files_updating()
     *      is called.
     * - -1: The file is being updated.
     */
    int8_t update;

    /** Size of the file. */
    size_t size;

    /** Calculated checksum. */
    unsigned long crc32;

    /** HTTP-first download with in-band QUIC fallback. */
    asset_source_t *source;

    /** Init-time function. */
    void (*init_func)(void);

    /** Function to call when re-loading. */
    void (*reload_func)(void);

    /** Loading/parsing function. */
    void (*parse_func)(void);

    /** Hash handle. */
    UT_hash_handle hh;
} server_files_struct;

/** Public API implemented in src/client/server_files.c. */

extern void server_files_init(void);

extern void server_files_deinit(void);

extern void server_files_init_all(void);

extern server_files_struct *server_files_create(const char *name);

extern server_files_struct *server_files_find(const char *name);

extern void server_files_load(int post_load);

extern void server_files_listing_retrieve(void);

extern int server_files_listing_processed(void);

extern int server_files_processed(void);

extern FILE *server_file_open(server_files_struct *tmp);

extern FILE *server_file_open_name(const char *name);

extern bool server_file_save(server_files_struct *tmp, unsigned char *data, size_t len);

/** Public API implemented in src/client/updates.c. */

extern void socket_command_file_update(uint8_t *data, size_t len, size_t pos);

extern int file_updates_finished(void);

extern void file_updates_parse(void);

#endif

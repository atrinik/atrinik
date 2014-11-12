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
 * Header file for servers files declarations.
 */

#ifndef SERVER_FILES_H
#define SERVER_FILES_H

#define SERVER_FILES_HTTP_DIR       "data"
#define SERVER_FILES_HTTP_LISTING   "listing.txt"

#define SERVER_FILE_ANIMS           "anims"
#define SERVER_FILE_BMAPS           "bmaps"
#define SERVER_FILE_UPDATES         "updates"
#define SERVER_FILE_SETTINGS        "settings"
#define SERVER_FILE_EFFECTS         "effects"
#define SERVER_FILE_HFILES          "hfiles"

/** One server file. */
typedef struct server_files_struct {
    /** Name of the server file. */
    char *name;

    /** If 0, will be (re-)loaded. */
    uint8 loaded;

    /**
     * Update status of this file:
     *
     * - 0: Not being updated, or just finished updating.
     * - 1: Start updating the file the next time server_files_updating()
     *      is called.
     * - -1: The file is being updated. */
    sint8 update;

    /** Size of the file. */
    size_t size;

    /** Calculated checksum. */
    unsigned long crc32;

    /** cURL data. */
    curl_data *dl_data;

    /** Init-time function. */
    void (*init_func)(void);

    /** Function to call when re-loading. */
    void (*reload_func)(void);

    /** Loading/parsing function. */
    void (*parse_func)(void);

    /** Hash handle. */
    UT_hash_handle hh;
} server_files_struct;

#endif

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Manages server file updates.
 *
 * @author Alex Tokar */

#include <global.h>

/** File names of the server files inside srv_files directory. */
static const char *const server_file_names[SERVER_FILES_MAX] =
{
    "bmaps", "updates", "settings",
    "anims", "effects", "hfiles"
};

/** Post-loading functions to call. */
static void (*server_file_funcs[SERVER_FILES_MAX]) () =
{
    read_bmaps, file_updates_parse, server_settings_init,
    read_anims, effects_init, hfiles_init
};

/** Functions to call if the server file was already loaded. */
static void (*server_file_funcs_reload[SERVER_FILES_MAX]) () =
{
    NULL, NULL, NULL,
    anims_reset, effects_reinit, NULL
};

/**
 * Init-time functions to call. Needed for things like help files, which
 * are necessary before we connect to a server. */
static void (*server_file_funcs_init[SERVER_FILES_MAX]) () =
{
    NULL, NULL, NULL,
    NULL, NULL, hfiles_init
};

/** The server files. */
static server_files_struct server_files[SERVER_FILES_MAX];

/**
 * Initialize the necessary structures. */
void server_files_init(void)
{
    size_t i;

    memset(&server_files, 0, sizeof(server_files));

    for (i = 0; i < SERVER_FILES_MAX; i++) {
        if (server_file_funcs_init[i]) {
            server_file_funcs_init[i]();
        }
    }
}

/**
 * Load the server files. If they haven't changed since last load, no
 * loading will be done.
 * @param post_load Unless 1, (re)parsing the server files will not be done. */
void server_files_load(int post_load)
{
    size_t i;
    FILE *fp;
    struct stat sb;
    size_t st_size, numread;
    char *contents;

    for (i = 0; i < SERVER_FILES_MAX; i++) {
        /* Invalid server file. */
        if (!server_file_names[i]) {
            continue;
        }

        /* Server file was loaded previously. */
        if (post_load && server_files[i].loaded) {
            if (server_file_funcs_reload[i]) {
                server_file_funcs_reload[i]();
            }

            continue;
        }

        /* Open the file. */
        fp = server_file_open(i);

        if (!fp) {
            return;
        }

        /* Get and store the size. */
        fstat(fileno(fp), &sb);
        st_size = sb.st_size;
        server_files[i].size = st_size;

        /* Allocate temporary buffer and read into it the file. */
        contents = malloc(st_size);
        numread = fread(contents, 1, st_size, fp);

        /* Calculate and store the checksum, free the temporary buffer
         * and close the file pointer. */
        server_files[i].crc32 = crc32(1L, (const unsigned char FAR *) contents, numread);
        free(contents);
        fclose(fp);

        if (post_load) {
            /* Mark that we have loaded this file. */
            server_files[i].loaded = 1;

            if (server_file_funcs[i]) {
                server_file_funcs[i]();
            }
        }
    }
}

/**
 * Construct a path to the specified @ref SERVER_FILE_xxx "file".
 * @param id ID of the server file, one of @ref SERVER_FILE_xxx.
 * @param[out] buf Will contain the constructed path.
 * @param buf_size Size of 'buf'.
 * @return 'buf'. */
static char *server_file_path(size_t id, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "srv_files/%s", server_file_names[id]);
    return buf;
}

/**
 * Open a server file for reading.
 * @param id ID of the server file, one of @ref SERVER_FILE_xxx.
 * @return The file pointer, or NULL on failure of opening the file. */
FILE *server_file_open(size_t id)
{
    char buf[MAX_BUF];

    /* Doesn't exist. */
    if (!server_file_names[id]) {
        return NULL;
    }

    server_file_path(id, buf, sizeof(buf));
    return fopen_wrapper(buf, "rb");
}

/**
 * We have received the server file we asked for, so save it to disk.
 * @param id ID of the server file, one of @ref SERVER_FILE_xxx.
 * @param data The data to save.
 * @param len Length of 'data'. */
void server_file_save(size_t id, unsigned char *data, size_t len)
{
    char path[MAX_BUF];
    FILE *fp;

    /* Finished updating. */
    server_files[id].update = 0;

    server_file_path(id, path, sizeof(path));
    fp = fopen_wrapper(path, "wb");

    if (!fp) {
        logger_print(LOG(BUG), "Can't open %s for writing.", path);
        return;
    }

    if (fwrite(data, 1, len, fp) != len) {
        logger_print(LOG(BUG), "Failed to write to %s.", path);
    }
    else {
        /* Mark the server file for reload. */
        server_files[id].loaded = 0;
    }

    fclose(fp);
}

/**
 * Figure out whether there are server files being updated.
 * @return 1 if there are any server files being updated, 0 otherwise. */
int server_files_updating(void)
{
    size_t i;

    /* Check all files. */
    for (i = 0; i < SERVER_FILES_MAX; i++) {
        /* The server file was marked for update previously, so start
         * updating. */
        if (server_files[i].update == 1) {
            packet_struct *packet;

            packet = packet_new(SERVER_CMD_REQUEST_FILE, 1, 0);
            packet_append_uint8(packet, i);
            socket_send_packet(packet);

            /* Mark the file as 'being updated'. */
            server_files[i].update = -1;
            return 1;
        }
        /* The file is being updated. */
        else if (server_files[i].update == -1) {
            return 1;
        }
    }

    return 0;
}

/**
 * Add data about server files we have to the setup packet that is sent
 * to the server.
 * @param packet The packet for appending. */
void server_files_setup_add(packet_struct *packet)
{
    size_t i;

    /* Load up the files. */
    server_files_load(0);

    for (i = 0; i < SERVER_FILES_MAX; i++) {
        /* Invalid file. */
        if (!server_file_names[i]) {
            continue;
        }

        /* Add the server file identifier, its size and the checksum. */
        packet_append_uint8(packet, CMD_SETUP_SERVER_FILE);
        packet_append_uint8(packet, i);
        packet_append_uint64(packet, server_files[i].size);
        packet_append_uint64(packet, server_files[i].crc32);
    }
}

/**
 * Mark the specified server file for update.
 * @param id The file to update. */
void server_files_mark_update(size_t i)
{
    if (!server_file_names[i]) {
        return;
    }

    server_files[i].update = 1;
}

/**
 * Clear update flag from all server files. */
void server_files_clear_update(void)
{
    size_t i;

    for (i = 0; i < SERVER_FILES_MAX; i++) {
        /* Invalid file. */
        if (!server_file_names[i]) {
            continue;
        }

        server_files[i].update = 0;
    }
}

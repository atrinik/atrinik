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
 * Handles code for file updates by the server.
 *
 * @author Zoey Rose
 */

#include <global.h>
#include <client_socket.h>
#include <packet_payload.h>
#include <toolkit/packet.h>
#include <toolkit/path.h>
#include <wrapper.h>

/** Bound server-directed allocations and decompression work. */
#define FILE_UPDATE_UNCOMPRESSED_MAX (64U * 1024U * 1024U)

/**
 * How many file updates have been requested. This is used to block the
 * login: it's not possible to login unless this value is 0, to ensure
 * everything is downloaded intact from the server first.
 */
static size_t file_updates_requested = 0;

/**
 * Request the server to send us an updated copy of a file.
 * @param filename
 * What to request.
 */
static void file_updates_request(char *filename) {
    packet_struct *packet;

    file_updates_requested++;

    packet = packet_new(SERVER_CMD_REQUEST_UPDATE, 64, 64);
    packet_writer_write_cstring(packet, filename);
    socket_send_packet(packet);
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_file_update(uint8_t *data, size_t len, size_t pos) {
    char filename[MAX_BUF];
    uint32_t expected_size;
    packet_view_t compressed;

    if (file_updates_requested == 0) {
        LOG(ERROR, "Ignoring unsolicited file update.");
        return;
    }

    if (!client_packet_parse_file_update(data,
                                         len,
                                         pos,
                                         VS(filename),
                                         &expected_size,
                                         &compressed)) {
        return;
    }
    if (!path_is_safe_relative(filename)) {
        LOG(ERROR, "Refusing unsafe file update path '%s'.", filename);
        return;
    }
    if (expected_size == 0 || expected_size > FILE_UPDATE_UNCOMPRESSED_MAX) {
        LOG(ERROR,
            "Refusing file update '%s' with invalid uncompressed size %" PRIu32 ".",
            filename,
            expected_size);
        return;
    }

    /* Uncompress it. */
    unsigned char *dest = xmalloc(expected_size);
    uLongf actual_size = expected_size;
    uLong consumed_size = compressed.len;
    int status = uncompress2(dest, &actual_size, compressed.data, &consumed_size);
    if (status != Z_OK || actual_size != expected_size || consumed_size != compressed.len) {
        LOG(ERROR,
            "Could not decompress file update '%s' (zlib status: %d, expected: %" PRIu32
            ", actual: %lu, compressed: %" PRIu64 ", consumed: %lu).",
            filename,
            status,
            expected_size,
            (unsigned long)actual_size,
            (uint64_t)compressed.len,
            consumed_size);
        free(dest);
        return;
    }

    char *path = file_path(filename, "wb");
    bool saved = path_write_atomic(path, dest, actual_size, 0600);
    free(path);
    free(dest);
    if (!saved) {
        LOG(ERROR, "Could not atomically write file update '%s'.", filename);
        return;
    }
    file_updates_requested--;
}

/**
 * Check if we have finished downloading updated files from the server.
 * @return
 * 1 if we have finished, 0 otherwise.
 */
int file_updates_finished(void) {
    return file_updates_requested == 0;
}

/**
 * Parse the updates srv file, and request updated files as needed.
 */
void file_updates_parse(void) {
    FILE *fp;
    char buf[HUGE_BUF];

    /* Is the feature disabled? */
    if (setting_get_int(OPT_CAT_CLIENT, OPT_DISABLE_FILE_UPDATES)) {
        return;
    }

    fp = server_file_open_name(SERVER_FILE_UPDATES);

    if (!fp) {
        return;
    }

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        char filename[MAX_BUF], crc_buf[MAX_BUF], *contents;
        uint64_t size;
        size_t st_size, numread;
        FILE *fp2;
        unsigned long crc;
        struct stat sb;

        if (sscanf(buf, "%s %" PRIu64 " %s", filename, &size, crc_buf) != 3) {
            continue;
        }

        fp2 = path_fopen(filename, "rb");

        /* No such file? Then we'll want to update this. */
        if (!fp2) {
            file_updates_request(filename);
            continue;
        }

        fstat(fileno(fp2), &sb);
        st_size = sb.st_size;
        contents = xmalloc(st_size);
        numread = fread(contents, 1, st_size, fp2);
        fclose(fp2);

        /* Get the CRC32... */
        crc = crc32(1L, (const unsigned char FAR *)contents, numread);
        free(contents);

        /* If the checksum or the size doesn't match, we'll want to update it.
         * */
        if (crc != strtoul(crc_buf, NULL, 16) || st_size != (size_t)size) {
            file_updates_request(filename);
        }
    }

    fclose(fp);
}

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
 * Handles code for file updates by the server.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/path.h>

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
static void file_updates_request(char *filename)
{
    packet_struct *packet;

    file_updates_requested++;

    packet = packet_new(SERVER_CMD_REQUEST_UPDATE, 64, 64);
    packet_append_string_terminated(packet, filename);
    socket_send_packet(packet);

}

/** @copydoc socket_command_struct::handle_func */
void socket_command_file_update(uint8_t *data, size_t len, size_t pos)
{
    char filename[MAX_BUF];
    unsigned long ucomp_len;
    unsigned char *dest;
    FILE *fp;

    if (file_updates_requested != 0) {
        file_updates_requested--;
    }

    packet_to_string(data, len, &pos, filename, sizeof(filename));
    ucomp_len = packet_to_uint32(data, len, &pos);
    len -= pos;

    /* Uncompress it. */
    dest = emalloc(ucomp_len);
    uncompress((Bytef *) dest, (uLongf *) & ucomp_len, (const Bytef *) data + pos, (uLong) len);
    data = dest;
    len = ucomp_len;

    fp = path_fopen(filename, "wb");

    if (!fp) {
        LOG(BUG, "Could not open file '%s' for writing.", filename);
        efree(dest);
        return;
    }

    /* Update the file. */
    fwrite(data, 1, len, fp);
    fclose(fp);
    efree(dest);
}

/**
 * Check if we have finished downloading updated files from the server.
 * @return
 * 1 if we have finished, 0 otherwise.
 */
int file_updates_finished(void)
{
    return file_updates_requested == 0;
}

/**
 * Parse the updates srv file, and request updated files as needed.
 */
void file_updates_parse(void)
{
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

        if (sscanf(buf, "%s %"PRIu64 " %s", filename, &size, crc_buf) != 3) {
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
        contents = emalloc(st_size);
        numread = fread(contents, 1, st_size, fp2);
        fclose(fp2);

        /* Get the CRC32... */
        crc = crc32(1L, (const unsigned char FAR *) contents, numread);
        efree(contents);

        /* If the checksum or the size doesn't match, we'll want to update it.
         * */
        if (crc != strtoul(crc_buf, NULL, 16) || st_size != (size_t) size) {
            file_updates_request(filename);
        }
    }

    fclose(fp);
}

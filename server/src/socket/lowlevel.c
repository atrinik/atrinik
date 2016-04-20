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
 * Low level socket related functions.
 */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>

static void socket_packet_enqueue(socket_struct *ns, packet_struct *packet)
{
#ifndef DEBUG
    {
        char *cp, *cp2;

        LOG(DUMPTX, "Enqueuing packet with command type %d (%" PRIu64
                " bytes):", packet->type, (uint64_t) packet->len);

        cp = packet_get_debug(packet);

        if (cp[0] != '\0') {
            LOG(DUMPTX, "  Debug info:\n");
            cp2 = strtok(cp, "\n");

            while (cp2 != NULL) {
                LOG(DUMPTX, "  %s", cp2);
                cp2 = strtok(NULL, "\n");
            }
        }

        efree(cp);

        cp = emalloc(sizeof(*cp) * (packet->len * 3 + 1));
        string_tohex(packet->data, packet->len, cp, packet->len * 3 + 1, true);
        LOG(DUMPTX, "  Hexadecimal: %s", cp);
        efree(cp);
    }
#endif

    DL_APPEND(ns->packets, packet);
}

/**
 * Dequeue all socket buffers in the queue.
 * @param ns
 * Socket to clear the socket buffers for.
 */
void socket_buffer_clear(socket_struct *ns)
{
    packet_struct *packet, *tmp;
    DL_FOREACH_SAFE(ns->packets, packet, tmp) {
        packet_free(packet);
    }

    ns->packets = NULL;
}

/**
 * Write data to socket.
 * @param ns
 * The socket we are writing to.
 */
void socket_buffer_write(socket_struct *ns)
{
    while (ns->packets != NULL) {
        packet_struct *packet = ns->packets;

        if (packet->ndelay) {
            socket_opt_ndelay(ns->sc, true);
        }

        size_t amt;
        bool success = socket_write(ns->sc, (const void *) (packet->data +
                packet->pos), packet->len - packet->pos, &amt);

        if (packet->ndelay) {
            socket_opt_ndelay(ns->sc, false);
        }

        if (!success) {
            ns->state = ST_DEAD;
            break;
        }

        packet->pos += amt;

        if (packet->len - packet->pos == 0) {
            DL_DELETE(ns->packets, packet);
            packet_free(packet);
        }
    }
}

void socket_send_packet(socket_struct *ns, struct packet_struct *packet)
{
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(packet != NULL);

    if (ns->state == ST_DEAD) {
        packet_free(packet);
        return;
    }

    if (packet->len + 1 > UINT16_MAX) {
        log_error("Sending packet with size >%u", UINT16_MAX);
        packet_free(packet);
        return;
    }

    packet_compress(packet);

    if (packet->len + 1 > UINT16_MAX) {
        log_error("Sending packet with size >%u", UINT16_MAX);
        packet_free(packet);
        return;
    }

    packet_struct *packet_meta = packet_new(0, 4, 0);
    packet_meta->ndelay = packet->ndelay;

    if (socket_is_secure(ns->sc)) {
        // TODO: figure out checksum_only flag
        packet = socket_crypto_encrypt(ns->sc, packet, packet_meta, false);
        if (packet == NULL) {
            /* Logging already done. */
            ns->state = ST_DEAD;
            return;
        }
    } else {
        packet_append_uint16(packet_meta, (uint16_t) packet->len + 1);
        packet_append_uint8(packet_meta, packet->type);
    }

    socket_packet_enqueue(ns, packet_meta);

    if (packet->len != 0) {
        socket_packet_enqueue(ns, packet);
    } else {
        packet_free(packet);
    }
}

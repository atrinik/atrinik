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
 * Low level socket related functions.
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <toolkit/socket_crypto.h>
#include <network_metrics.h>

#define SOCKET_QUEUE_BULK_LIMIT (1024U * 1024U)
#define SOCKET_QUEUE_HARD_LIMIT (4U * 1024U * 1024U)
#define SOCKET_QUEUE_PACKET_LIMIT 4096U

static void socket_packet_enqueue(socket_struct *ns, packet_struct *packet) {
#ifndef DEBUG
    {
        char *cp, *cp2;

        LOG(DUMPTX,
            "Enqueuing packet with command type %d (%" PRIu64 " bytes):",
            packet->type,
            (uint64_t)packet->len);

        cp = packet_get_debug(packet);

        if (cp[0] != '\0') {
            LOG(DUMPTX, "  Debug info:\n");
            cp2 = strtok(cp, "\n");

            while (cp2 != NULL) {
                LOG(DUMPTX, "  %s", cp2);
                cp2 = strtok(NULL, "\n");
            }
        }

        free(cp);

        cp = xmalloc(sizeof(*cp) * (packet->len * 3 + 1));
        string_tohex(packet->data, packet->len, cp, packet->len * 3 + 1, true);
        LOG(DUMPTX, "  Hexadecimal: %s", cp);
        free(cp);
    }
#endif

    DL_APPEND(ns->packets, packet);
    ns->packet_queue_bytes += packet->len - packet->pos;
    ns->packet_queue_count++;
    ns->packet_queue_peak_bytes = MAX(ns->packet_queue_peak_bytes, ns->packet_queue_bytes);
    server_metrics_queue_changed((int64_t)(packet->len - packet->pos),
                                 ns->packet_queue_bytes,
                                 false);
}

bool socket_buffer_can_enqueue(const socket_struct *ns, size_t bytes, bool bulk) {
    HARD_ASSERT(ns != NULL);

    size_t limit = bulk ? SOCKET_QUEUE_BULK_LIMIT : SOCKET_QUEUE_HARD_LIMIT;
    return bytes <= limit && ns->packet_queue_bytes <= limit - bytes &&
           ns->packet_queue_count < SOCKET_QUEUE_PACKET_LIMIT;
}

/**
 * Dequeue all socket buffers in the queue.
 * @param ns
 * Socket to clear the socket buffers for.
 */
void socket_buffer_clear(socket_struct *ns) {
    size_t queued_bytes = ns->packet_queue_bytes;
    packet_struct *packet, *tmp;
    DL_FOREACH_SAFE(ns->packets, packet, tmp) {
        packet_free(packet);
    }

    ns->packets = NULL;
    ns->packet_queue_bytes = 0;
    ns->packet_queue_count = 0;
    if (queued_bytes != 0) {
        server_metrics_queue_changed(-(int64_t)queued_bytes, 0, false);
    }
}

/**
 * Write data to socket.
 * @param ns
 * The socket we are writing to.
 */
void socket_buffer_write(socket_struct *ns) {
    while (ns->packets != NULL) {
        packet_struct *packet = ns->packets;

        if (packet->ndelay) {
            socket_opt_ndelay(ns->sc, true);
        }

        size_t amt;
        bool success = socket_write(ns->sc,
                                    (const void *)(packet->data + packet->pos),
                                    packet->len - packet->pos,
                                    &amt);

        if (packet->ndelay) {
            socket_opt_ndelay(ns->sc, false);
        }

        if (!success) {
            ns->state = ST_DEAD;
            break;
        }

        packet->pos += amt;
        HARD_ASSERT(ns->packet_queue_bytes >= amt);
        ns->packet_queue_bytes -= amt;
        server_metrics_queue_changed(-(int64_t)amt, ns->packet_queue_bytes, false);

        if (packet->len - packet->pos == 0) {
            DL_DELETE(ns->packets, packet);
            HARD_ASSERT(ns->packet_queue_count != 0);
            ns->packet_queue_count--;
            packet_free(packet);
            continue;
        }

        /* A nonblocking transport made only partial (or no) progress. */
        break;
    }
}

void socket_send_packet(socket_struct *ns, struct packet_struct *packet) {
    HARD_ASSERT(ns != NULL);
    HARD_ASSERT(packet != NULL);

    if (ns->state == ST_DEAD || ns->state == ST_ZOMBIE) {
        packet_free(packet);
        return;
    }

    if (packet->len + 1 > UINT16_MAX) {
        log_error("Sending packet with size >%u", UINT16_MAX);
        packet_free(packet);
        return;
    }

    packet_struct *packet_meta = packet_new(0, 4, 0);
    packet_meta->ndelay = packet->ndelay;

    if (socket_is_secure(ns->sc)) {
        bool checksum_only = !socket_crypto_server_should_encrypt(packet->type);
        packet = socket_crypto_encrypt(ns->sc, packet, packet_meta, checksum_only);
        if (packet == NULL) {
            /* Logging already done. */
            packet_free(packet_meta);
            ns->state = ST_DEAD;
            return;
        }
    } else {
        packet_compress(packet);
        uint32_t payload_len = (uint32_t)packet->len + 1;
        if (payload_len < 0x8000) {
            packet_append_uint16(packet_meta, (uint16_t)payload_len);
        } else {
            packet_append_uint8(packet_meta, (uint8_t)(0x80 | (payload_len >> 16)));
            packet_append_uint16(packet_meta, (uint16_t)(payload_len & 0xffff));
        }
        packet_append_uint8(packet_meta, packet->type);
    }

    size_t queued_size = packet_meta->len + packet->len;
    size_t queued_packets = packet->len != 0 ? 2 : 1;
    if (!socket_buffer_can_enqueue(ns, queued_size, false) ||
        ns->packet_queue_count > SOCKET_QUEUE_PACKET_LIMIT - queued_packets) {
        ns->packet_queue_rejected++;
        server_metrics_queue_changed(0, ns->packet_queue_bytes, true);
        LOG(ERROR,
            "Connection %s exceeded its outbound queue limit "
            "(queued=%" PRIu64 ", rejected=%" PRIu64 ")",
            socket_get_id(ns->sc),
            (uint64_t)ns->packet_queue_bytes,
            (uint64_t)queued_size);
        packet_free(packet_meta);
        packet_free(packet);
        ns->state = ST_ZOMBIE;
        return;
    }

    socket_packet_enqueue(ns, packet_meta);

    if (packet->len != 0) {
        socket_packet_enqueue(ns, packet);
    } else {
        packet_free(packet);
    }
}

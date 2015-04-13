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
 * Packet API header file.
 *
 * @author Alex Tokar */

#ifndef PACKET_H
#define PACKET_H

typedef struct StringBuffer_struct StringBuffer;

/**
 * A single data packet. */
typedef struct packet_struct {
    /**
     * Next packet to send. */
    struct packet_struct *next;

    /**
     * Previous packet. */
    struct packet_struct *prev;

    /**
     * The data. */
    uint8 *data;

    /**
     * Length of 'data'. */
    size_t len;

    /**
     * Current size of 'data'. */
    size_t size;

    /**
     * Expand size. */
    size_t expand;

    /**
     * Position in 'data'. */
    size_t pos;

    /**
     * Whether to enable NDELAY on this packet. */
    uint8 ndelay;

    /**
     * The packet's command type. */
    uint8 type;

#ifndef NDEBUG
    /**
     * StringBuffer instance used to describe the packet contents.
     */
    StringBuffer *sb;
#endif
} packet_struct;

/**
 * How many packet structures to allocate when expanding the packets
 * memory pool. */
#define PACKET_EXPAND 10

#ifndef NDEBUG
#define packet_debug(_packet, _indent, _fmt, ...) \
    do { \
        stringbuffer_append_printf((_packet)->sb, "%*s" _fmt, (_indent), "", \
                                   ## __VA_ARGS__); \
    } while (0)
#else
#define packet_debug(_packet, _indent, _fmt, ...)
#endif

/* Prototypes */

void toolkit_packet_init(void);
void toolkit_packet_deinit(void);
packet_struct *packet_new(uint8 type, size_t size, size_t expand);
void packet_free(packet_struct *packet);
void packet_compress(packet_struct *packet);
void packet_enable_ndelay(packet_struct *packet);
void packet_set_pos(packet_struct *packet, size_t pos);
size_t packet_get_pos(packet_struct *packet);
packet_struct *packet_dup(packet_struct *packet);
void packet_delete(packet_struct *packet, size_t pos, size_t len);
void packet_merge(packet_struct *src, packet_struct *dst);
char *packet_get_debug(packet_struct *packet);
void packet_append_uint8(packet_struct *packet, uint8 data);
void packet_append_sint8(packet_struct *packet, sint8 data);
void packet_append_uint16(packet_struct *packet, uint16 data);
void packet_append_sint16(packet_struct *packet, sint16 data);
void packet_append_uint32(packet_struct *packet, uint32 data);
void packet_append_sint32(packet_struct *packet, sint32 data);
void packet_append_uint64(packet_struct *packet, uint64 data);
void packet_append_sint64(packet_struct *packet, sint64 data);
void packet_append_float(packet_struct *packet, float data);
void packet_append_double(packet_struct *packet, double data);
void packet_append_data_len(packet_struct *packet, const uint8 *data,
        size_t len);
void packet_append_string(packet_struct *packet, const char *data);
void packet_append_string_terminated(packet_struct *packet, const char *data);
uint8 packet_to_uint8(uint8 *data, size_t len, size_t *pos);
sint8 packet_to_sint8(uint8 *data, size_t len, size_t *pos);
uint16 packet_to_uint16(uint8 *data, size_t len, size_t *pos);
sint16 packet_to_sint16(uint8 *data, size_t len, size_t *pos);
uint32 packet_to_uint32(uint8 *data, size_t len, size_t *pos);
sint32 packet_to_sint32(uint8 *data, size_t len, size_t *pos);
uint64 packet_to_uint64(uint8 *data, size_t len, size_t *pos);
sint64 packet_to_sint64(uint8 *data, size_t len, size_t *pos);
float packet_to_float(uint8 *data, size_t len, size_t *pos);
double packet_to_double(uint8 *data, size_t len, size_t *pos);
char *packet_to_string(uint8 *data, size_t len, size_t *pos, char *dest,
        size_t dest_size);
void packet_to_stringbuffer(uint8 *data, size_t len, size_t *pos,
        StringBuffer *sb);

#endif

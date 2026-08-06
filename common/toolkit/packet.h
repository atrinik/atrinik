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
 * Packet API header file.
 *
 * @author Zoey Rose
 */

#ifndef TOOLKIT_PACKET_H
#define TOOLKIT_PACKET_H

#include "toolkit.h"
#include "stringbuffer_dec.h"
#include "packet_dec.h"

/** Maximum payload carried by one legacy game-protocol envelope. */
#define PACKET_PAYLOAD_MAX (UINT16_MAX - 1U)

/** Explicit packet parsing and construction failures. */
typedef enum packet_error {
    PACKET_ERROR_NONE,
    PACKET_ERROR_TRUNCATED,
    PACKET_ERROR_INVALID_ENCODING,
    PACKET_ERROR_LIMIT_EXCEEDED,
    PACKET_ERROR_UNSUPPORTED,
    PACKET_ERROR_TRAILING_DATA,
    PACKET_ERROR_SIZE_OVERFLOW,
    PACKET_ERROR_ALLOCATION,
} packet_error_t;

/** A borrowed, non-owning byte range. */
typedef struct packet_view {
    const uint8_t *data;
    size_t len;
} packet_view_t;

/** Bounded cursor for decoding untrusted packet data. */
typedef struct packet_reader {
    const uint8_t *data;
    size_t len;
    size_t pos;
    packet_error_t error;
    /** Optional legacy position mirror used only during whole-tree migration. */
    size_t *position;
    struct packet_reader_scope *scope;
} packet_reader_t;

/** One dispatch-level error and completion boundary. */
typedef struct packet_reader_scope {
    const uint8_t *data;
    size_t len;
    size_t pos;
    packet_error_t error;
    bool initialized;
    struct packet_reader_scope *previous;
} packet_reader_scope_t;

/**
 * A single data packet.
 */
struct packet_struct {
    /**
     * Next packet to send.
     */
    struct packet_struct *next;

    /**
     * Previous packet.
     */
    struct packet_struct *prev;

    /**
     * The data.
     */
    uint8_t *data;

    /**
     * Length of 'data'.
     */
    size_t len;

    /**
     * Current size of 'data'.
     */
    size_t size;

    /**
     * Expand size.
     */
    size_t expand;

    /**
     * Position in 'data'.
     */
    size_t pos;

    /** Maximum payload size accepted by writer operations. */
    size_t limit;

    /** Sticky writer failure. */
    packet_error_t error;

    /**
     * Whether to enable NDELAY on this packet.
     */
    uint8_t ndelay;

    /**
     * The packet's command type.
     */
    uint8_t type;

#ifndef NDEBUG
    /**
     * StringBuffer instance used to describe the packet contents.
     */
    StringBuffer *sb;
#endif
};

/** The owned packet is also the protocol's bounded writer cursor. */
typedef struct packet_struct packet_writer_t;

/**
 * Structure used to save state of the packet so that one can go back to it.
 */
typedef struct packet_writer_mark {
    /**
     * Position to save.
     */
    size_t pos;

#ifndef NDEBUG
    /**
     * StringBuffer instance position to save.
     */
    size_t sb_pos;
#endif
} packet_writer_mark_t;

/**
 * How many packet structures to allocate when expanding the packets
 * memory pool.
 */
#define PACKET_EXPAND 10

#ifndef NDEBUG
#define packet_debug(_packet, _indent, _fmt, ...)                                            \
    do {                                                                                     \
        stringbuffer_append_printf((_packet)->sb, "%*s" _fmt, (_indent), "", ##__VA_ARGS__); \
    } while (0)
#define packet_debug_data(_packet, _indent, _fmt, ...) \
    packet_debug(_packet, _indent, _fmt ": ", ##__VA_ARGS__)
#else
#define packet_debug(_packet, _indent, _fmt, ...)
#define packet_debug_data(_packet, _indent, _fmt, ...)
#endif

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(packet);

void toolkit_packet_deinit(void);
packet_struct *packet_new(uint8_t type, size_t size, size_t expand);
void packet_free(packet_struct *packet);
void packet_compress(packet_struct *packet);
void packet_enable_ndelay(packet_struct *packet);
packet_struct *packet_dup(packet_struct *packet);
void packet_delete(packet_struct *packet, size_t pos, size_t len);
void packet_writer_mark(packet_writer_t *writer, packet_writer_mark_t *mark);
void packet_writer_rollback(packet_writer_t *writer, const packet_writer_mark_t *mark);
char *packet_get_debug(packet_struct *packet);
packet_error_t packet_writer_error(const packet_writer_t *writer);
bool packet_writer_finish(packet_writer_t *writer);
void packet_writer_write_uint8(packet_struct *packet, uint8_t data);
void packet_writer_write_int8(packet_struct *packet, int8_t data);
void packet_writer_write_uint16(packet_struct *packet, uint16_t data);
void packet_writer_write_int16(packet_struct *packet, int16_t data);
void packet_writer_write_uint32(packet_struct *packet, uint32_t data);
void packet_writer_write_int32(packet_struct *packet, int32_t data);
void packet_writer_write_uint64(packet_struct *packet, uint64_t data);
void packet_writer_write_int64(packet_struct *packet, int64_t data);
void packet_writer_write_float(packet_struct *packet, float data);
void packet_writer_write_double(packet_struct *packet, double data);
void packet_writer_write_bytes(packet_struct *packet, const uint8_t *data, size_t len);
void packet_writer_write_string_n(packet_struct *packet, const char *data, size_t len);
void packet_writer_write_string(packet_struct *packet, const char *data);
void packet_writer_write_cstring_n(packet_struct *packet, const char *data, size_t len);
void packet_writer_write_cstring(packet_struct *packet, const char *data);
void packet_writer_write_packet(packet_struct *packet, packet_struct *src);

void packet_reader_init(packet_reader_t *reader, const void *data, size_t len);
void packet_reader_init_at(packet_reader_t *reader, const void *data, size_t len, size_t pos);
void packet_reader_init_cursor(packet_reader_t *reader,
                               const void *data,
                               size_t len,
                               size_t *position);
size_t packet_reader_remaining(const packet_reader_t *reader);
packet_error_t packet_reader_error(const packet_reader_t *reader);
const char *packet_error_string(packet_error_t error);
void packet_reader_set_error(packet_reader_t *reader, packet_error_t error);
bool packet_reader_finish(packet_reader_t *reader);
void packet_reader_scope_begin(packet_reader_scope_t *scope);
packet_error_t packet_reader_scope_finish(packet_reader_scope_t *scope);
bool packet_reader_skip(packet_reader_t *reader, size_t len);
packet_view_t packet_reader_read_view(packet_reader_t *reader, size_t len);
packet_view_t packet_reader_read_string_view(packet_reader_t *reader, size_t max_len);
bool packet_reader_read_string_bounded(packet_reader_t *reader,
                                       char *dest,
                                       size_t dest_size,
                                       size_t max_len);
bool packet_reader_read_string(packet_reader_t *reader, char *dest, size_t dest_size);
bool packet_reader_read_stringbuffer_bounded(packet_reader_t *reader,
                                             StringBuffer *sb,
                                             size_t max_len);
bool packet_reader_read_stringbuffer(packet_reader_t *reader, StringBuffer *sb);
uint8_t packet_reader_read_uint8(packet_reader_t *reader);
int8_t packet_reader_read_int8(packet_reader_t *reader);
uint16_t packet_reader_read_uint16(packet_reader_t *reader);
int16_t packet_reader_read_int16(packet_reader_t *reader);
uint32_t packet_reader_read_uint32(packet_reader_t *reader);
int32_t packet_reader_read_int32(packet_reader_t *reader);
uint64_t packet_reader_read_uint64(packet_reader_t *reader);
int64_t packet_reader_read_int64(packet_reader_t *reader);
float packet_reader_read_float(packet_reader_t *reader);
double packet_reader_read_double(packet_reader_t *reader);
bool packet_reader_read_count8(packet_reader_t *reader, size_t maximum, size_t *count);
bool packet_reader_read_count16(packet_reader_t *reader, size_t maximum, size_t *count);
bool packet_reader_read_count32(packet_reader_t *reader, size_t maximum, size_t *count);

#endif

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
 * Packet construction/management.
 *
 * @author Zoey Rose
 */

#include "packet.h"
#include "string.h"
#include "mempool.h"

#include <zlib.h>

/**
 * The packets memory pool.
 */
static mempool_struct *pool_packet;
static _Thread_local packet_reader_scope_t *packet_active_reader_scope;

static void packet_debugger(void *ptr, char *buf, size_t size);
static void packet_reader_fail(packet_reader_t *reader, packet_error_t error);

TOOLKIT_API(DEPENDS(mempool));

TOOLKIT_INIT_FUNC(packet) {
    pool_packet = mempool_create("packets",
                                 PACKET_EXPAND,
                                 sizeof(packet_struct),
                                 MEMPOOL_ALLOW_FREEING,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL);
    mempool_set_debugger(pool_packet, packet_debugger);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(packet) {}
TOOLKIT_DEINIT_FUNC_FINISH

/** @copydoc chunk_debugger */
static void packet_debugger(void *ptr, char *buf, size_t size) {
    packet_struct *packet = ptr;
    snprintf(buf,
             size,
             "type: %d length: %" PRIu64 " size: %" PRIu64,
             packet->type,
             (uint64_t)packet->len,
             (uint64_t)packet->size);

    if (packet->data != NULL && packet->len != 0) {
#define MAXHEXLEN 256
        char hexbuf[MAXHEXLEN * 3 + 1];

        string_tohex(packet->data, packet->len, hexbuf, sizeof(hexbuf), true);
        snprintfcat(buf, size, " data: %s", hexbuf);

        if (packet->len > MAXHEXLEN) {
            snprintfcat(buf,
                        size,
                        " (%" PRId64 " bytes follow)",
                        (uint64_t)(packet->len - MAXHEXLEN));
        }
#undef MAXHEXLEN
    }
}

/**
 * Allocates a new packet.
 * @param type
 * The packet's command type.
 * @param size
 * Initial number of bytes to allocate for the packet's
 * data.
 * @param expand
 * The minimum size to expand by when there is not enough
 * bytes allocated.
 * @return
 * The allocated packet.
 */
packet_struct *packet_new(uint8_t type, size_t size, size_t expand) {
    packet_struct *packet;

    TOOLKIT_PROTECT();

    packet = mempool_get(pool_packet);
    packet->size = size;
    packet->expand = expand;
    packet->limit = PACKET_PAYLOAD_MAX;
    packet->error = PACKET_ERROR_NONE;

    /* Allocate the initial data block. */
    if (packet->size) {
        packet->data = xmalloc(packet->size);
    }

    packet->type = type;

#ifndef NDEBUG
    packet->sb = stringbuffer_new();
#endif

    return packet;
}

/**
 * Free a previously allocated data packet.
 * @param packet
 * Packet to free.
 */
void packet_free(packet_struct *packet) {
    TOOLKIT_PROTECT();

    free(packet->data);

#ifndef NDEBUG
    if (packet->sb != NULL) {
        free(stringbuffer_finish(packet->sb));
    }
#endif

    mempool_return(pool_packet, packet);
}

/**
 * Compress a data packet, if possible.
 * @param packet
 * Packet to try to compress.
 */
void packet_compress(packet_struct *packet) {
    TOOLKIT_PROTECT();
    HARD_ASSERT(packet != NULL);

#if defined(COMPRESS_DATA_PACKETS) && COMPRESS_DATA_PACKETS
    if (packet->len <= COMPRESS_DATA_PACKETS_SIZE) {
        return;
    }

    size_t new_size = compressBound(packet->len);
    uint8_t *dest = xmalloc(new_size + 5);
    dest[0] = packet->type;
    /* Add original length of the packet. */
    dest[1] = (packet->len >> 24) & 0xff;
    dest[2] = (packet->len >> 16) & 0xff;
    dest[3] = (packet->len >> 8) & 0xff;
    dest[4] = (packet->len) & 0xff;
    /* Compress it. */
    compress2((Bytef *)dest + 5,
              (uLong *)&new_size,
              (const unsigned char FAR *)packet->data,
              packet->len,
              Z_BEST_COMPRESSION);

    if (new_size >= packet->len) {
        free(dest);
        return;
    }

    free(packet->data);
    packet->data = dest;
    packet->size = packet->len = new_size + 5;
    packet->type = CLIENT_CMD_COMPRESSED;
#endif
}

/**
 * Enables NDELAY on the specified packet.
 */
void packet_enable_ndelay(packet_struct *packet) {
    TOOLKIT_PROTECT();
    packet->ndelay = 1;
}

packet_struct *packet_dup(packet_struct *packet) {
    packet_struct *cp;

    TOOLKIT_PROTECT();

    cp = packet_new(packet->type, packet->size, packet->expand);
    cp->ndelay = packet->ndelay;
    cp->limit = packet->limit;
    cp->error = packet->error;

    if (packet->data != NULL) {
        packet_writer_write_bytes(cp, packet->data, packet->len);
    }

    return cp;
}

void packet_delete(packet_struct *packet, size_t pos, size_t len) {
    TOOLKIT_PROTECT();

    if (pos > packet->len || len > packet->len - pos) {
        return;
    }

    size_t trailing = packet->len - pos - len;
    if (trailing != 0) {
        memmove(packet->data + pos, packet->data + pos + len, trailing);
    }

    packet->len -= len;
}

void packet_writer_mark(packet_writer_t *writer, packet_writer_mark_t *mark) {
    HARD_ASSERT(writer != NULL);
    HARD_ASSERT(mark != NULL);

    mark->pos = writer->len;

#ifndef NDEBUG
    mark->sb_pos = stringbuffer_length(writer->sb);
#endif
}

void packet_writer_rollback(packet_writer_t *writer, const packet_writer_mark_t *mark) {
    HARD_ASSERT(writer != NULL);
    HARD_ASSERT(mark != NULL);
    HARD_ASSERT(mark->pos <= writer->len);

    writer->len = mark->pos;

#ifndef NDEBUG
    stringbuffer_seek(writer->sb, mark->sb_pos);
#endif
}

/**
 * Ensure 'size' bytes are available for writing in the packet. If not,
 * will allocate more.
 * @param packet
 * Packet.
 * @param size
 * How many bytes we need.
 */
static bool packet_ensure(packet_struct *packet, size_t size) {
    TOOLKIT_PROTECT();

    if (packet->error != PACKET_ERROR_NONE) {
        return false;
    }

    if (packet->len > packet->limit || size > packet->limit - packet->len) {
        packet->error = PACKET_ERROR_LIMIT_EXCEEDED;
        return false;
    }

    if (packet->len <= packet->size && size <= packet->size - packet->len) {
        return true;
    }

    size_t growth = MAX(packet->expand, size);
    if (packet->size > packet->limit || growth > packet->limit - packet->size) {
        growth = packet->limit - MIN(packet->size, packet->limit);
    }
    if (growth < size || packet->size > SIZE_MAX - growth) {
        packet->error = PACKET_ERROR_SIZE_OVERFLOW;
        return false;
    }

    packet->size += growth;
    packet->data = xrealloc(packet->data, packet->size);
    return true;
}

char *packet_get_debug(packet_struct *packet) {
    char *cp;

    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);

#ifndef NDEBUG
    HARD_ASSERT(packet->sb != NULL);
    cp = stringbuffer_finish(packet->sb);
    packet->sb = NULL;
#else
    cp = xstrdup("");
#endif

    return cp;
}

packet_error_t packet_writer_error(const packet_writer_t *writer) {
    HARD_ASSERT(writer != NULL);
    return writer->error;
}

void packet_writer_set_limit(packet_writer_t *writer, size_t limit) {
    HARD_ASSERT(writer != NULL);
    if (writer->len > limit) {
        writer->error = PACKET_ERROR_LIMIT_EXCEEDED;
        return;
    }
    writer->limit = limit;
}

bool packet_writer_finish(packet_writer_t *writer) {
    HARD_ASSERT(writer != NULL);
    return writer->error == PACKET_ERROR_NONE;
}

static void packet_writer_write_uint8_internal(packet_struct *packet, uint8_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 1)) {
        return;
    }

    packet->data[packet->len++] = data;
}

void packet_writer_write_uint8(packet_struct *packet, uint8_t data) {
    TOOLKIT_PROTECT();

    packet_writer_write_uint8_internal(packet, data);
    packet_debug(packet, 0, "%u\n", data);
}

void packet_writer_write_int8(packet_struct *packet, int8_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 1)) {
        return;
    }

    packet->data[packet->len++] = data;
    packet_debug(packet, 0, "%d\n", data);
}

void packet_writer_write_uint16(packet_struct *packet, uint16_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 2)) {
        return;
    }

    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%u\n", data);
}

void packet_writer_write_int16(packet_struct *packet, int16_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 2)) {
        return;
    }

    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%d\n", data);
}

static void packet_writer_write_uint32_internal(packet_struct *packet, uint32_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 4)) {
        return;
    }

    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
}

void packet_writer_write_uint32(packet_struct *packet, uint32_t data) {
    TOOLKIT_PROTECT();

    packet_writer_write_uint32_internal(packet, data);
    packet_debug(packet, 0, "%u\n", data);
}

void packet_writer_write_int32(packet_struct *packet, int32_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 4)) {
        return;
    }

    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%d\n", data);
}

static void packet_writer_write_uint64_internal(packet_struct *packet, uint64_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 8)) {
        return;
    }

    packet->data[packet->len++] = (data >> 56) & 0xff;
    packet->data[packet->len++] = (data >> 48) & 0xff;
    packet->data[packet->len++] = (data >> 40) & 0xff;
    packet->data[packet->len++] = (data >> 32) & 0xff;
    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
}

void packet_writer_write_uint64(packet_struct *packet, uint64_t data) {
    TOOLKIT_PROTECT();

    packet_writer_write_uint64_internal(packet, data);
    packet_debug(packet, 0, "%" PRIu64 "\n", data);
}

void packet_writer_write_int64(packet_struct *packet, int64_t data) {
    TOOLKIT_PROTECT();
    if (!packet_ensure(packet, 8)) {
        return;
    }

    packet->data[packet->len++] = (data >> 56) & 0xff;
    packet->data[packet->len++] = (data >> 48) & 0xff;
    packet->data[packet->len++] = (data >> 40) & 0xff;
    packet->data[packet->len++] = (data >> 32) & 0xff;
    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%" PRId64 "\n", data);
}

void packet_writer_write_float(packet_struct *packet, float data) {
    uint32_t val;

    TOOLKIT_PROTECT();

    memcpy(&val, &data, sizeof(val));
    packet_writer_write_uint32_internal(packet, val);
    packet_debug(packet, 0, "%f\n", data);
}

void packet_writer_write_double(packet_struct *packet, double data) {
    uint64_t val;

    TOOLKIT_PROTECT();

    memcpy(&val, &data, sizeof(val));
    packet_writer_write_uint64_internal(packet, val);
    packet_debug(packet, 0, "%f\n", data);
}

static void
packet_writer_write_bytes_internal(packet_struct *packet, const uint8_t *data, size_t len) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    if (!packet_ensure(packet, len)) {
        return;
    }
    memcpy(packet->data + packet->len, data, len);
    packet->len += len;
}

void packet_writer_write_bytes(packet_struct *packet, const uint8_t *data, size_t len) {
    size_t old_len;

    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    old_len = packet->len;
    packet_writer_write_bytes_internal(packet, data, len);

#ifndef NDEBUG
    if (packet->len != old_len) {
        char *hex;

        hex = xmalloc(sizeof(*hex) * (len * 3 + 1));
        string_tohex(data, len, hex, len * 3 + 1, true);
        packet_debug(packet, 0, "%s\n", hex);
        free(hex);
    }
#endif
}

void packet_writer_write_string_n(packet_struct *packet, const char *data, size_t len) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    if (!packet_ensure(packet, len)) {
        return;
    }
    memcpy(packet->data + packet->len, data, len);
    packet->len += len;
    packet_debug(packet, 0, "%.*s", (int)len, data);
}

void packet_writer_write_string(packet_struct *packet, const char *data) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    packet_writer_write_string_n(packet, data, strlen(data));
}

void packet_writer_write_cstring_n(packet_struct *packet, const char *data, size_t len) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == SIZE_MAX) {
        packet->error = PACKET_ERROR_SIZE_OVERFLOW;
        return;
    }
    if (!packet_ensure(packet, len + 1)) {
        return;
    }
    memcpy(packet->data + packet->len, data, len);
    packet->len += len;
    packet->data[packet->len++] = '\0';
    packet_debug(packet, 0, "%.*s", (int)len, data);
    packet_debug(packet, 0, "\n");
}

void packet_writer_write_cstring(packet_struct *packet, const char *data) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    packet_writer_write_cstring_n(packet, data, strlen(data));
}

void packet_writer_write_packet(packet_struct *packet, packet_struct *src) {
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    HARD_ASSERT(src != NULL);

    if (src->data != NULL) {
        packet_writer_write_bytes_internal(packet, src->data, src->len);
    }

#ifndef NDEBUG
    if (packet->sb != NULL && src->sb != NULL) {
        char *cp;

        cp = stringbuffer_sub(src->sb, 0, 0);
        stringbuffer_append_string(packet->sb, cp);
        free(cp);
    }
#endif
}

void packet_reader_init_at(packet_reader_t *reader, const void *data, size_t len, size_t pos) {
    HARD_ASSERT(reader != NULL);

    packet_reader_scope_t *scope = packet_active_reader_scope;
    if (scope != NULL && scope->initialized && (scope->data != data || scope->len != len)) {
        scope = NULL;
    }
    *reader = (packet_reader_t){
        .data = data,
        .len = len,
        .pos = pos,
        .scope = scope,
    };
    if (reader->scope != NULL) {
        if (!reader->scope->initialized) {
            reader->scope->data = data;
            reader->scope->len = len;
            reader->scope->pos = pos;
            reader->scope->initialized = true;
        }
    }
    if ((data == NULL && len != 0) || pos > len) {
        reader->pos = 0;
        packet_reader_fail(reader, PACKET_ERROR_INVALID_ENCODING);
    }
}

void packet_reader_init(packet_reader_t *reader, const void *data, size_t len) {
    packet_reader_init_at(reader, data, len, 0);
}

void packet_reader_init_cursor(packet_reader_t *reader,
                               const void *data,
                               size_t len,
                               size_t *position) {
    HARD_ASSERT(position != NULL);
    packet_reader_init_at(reader, data, len, *position);
    reader->position = position;
}

size_t packet_reader_remaining(const packet_reader_t *reader) {
    HARD_ASSERT(reader != NULL);
    return reader->pos <= reader->len ? reader->len - reader->pos : 0;
}

packet_error_t packet_reader_error(const packet_reader_t *reader) {
    HARD_ASSERT(reader != NULL);
    return reader->error;
}

const char *packet_error_string(packet_error_t error) {
    static const char *const names[] = {
        [PACKET_ERROR_NONE] = "none",
        [PACKET_ERROR_TRUNCATED] = "truncated input",
        [PACKET_ERROR_INVALID_ENCODING] = "invalid encoding",
        [PACKET_ERROR_LIMIT_EXCEEDED] = "limit exceeded",
        [PACKET_ERROR_UNSUPPORTED] = "unsupported value",
        [PACKET_ERROR_TRAILING_DATA] = "trailing data",
        [PACKET_ERROR_SIZE_OVERFLOW] = "size overflow",
        [PACKET_ERROR_ALLOCATION] = "allocation failure",
    };

    if ((size_t)error >= arraysize(names) || names[error] == NULL) {
        return "unknown packet error";
    }
    return names[error];
}

void packet_reader_set_error(packet_reader_t *reader, packet_error_t error) {
    HARD_ASSERT(reader != NULL);
    HARD_ASSERT(error != PACKET_ERROR_NONE);
    packet_reader_fail(reader, error);
}

static void packet_reader_fail(packet_reader_t *reader, packet_error_t error) {
    if (reader->error == PACKET_ERROR_NONE) {
        reader->error = error;
    }
    if (reader->scope != NULL && reader->scope->error == PACKET_ERROR_NONE) {
        reader->scope->error = error;
    }
}

bool packet_reader_finish(packet_reader_t *reader) {
    HARD_ASSERT(reader != NULL);
    if (reader->error == PACKET_ERROR_NONE && reader->pos != reader->len) {
        packet_reader_fail(reader, PACKET_ERROR_TRAILING_DATA);
    }
    return reader->error == PACKET_ERROR_NONE;
}

void packet_reader_scope_begin(packet_reader_scope_t *scope) {
    HARD_ASSERT(scope != NULL);
    *scope = (packet_reader_scope_t){.previous = packet_active_reader_scope};
    packet_active_reader_scope = scope;
}

packet_error_t packet_reader_scope_finish(packet_reader_scope_t *scope) {
    HARD_ASSERT(scope != NULL);
    HARD_ASSERT(packet_active_reader_scope == scope);
    if (scope->error == PACKET_ERROR_NONE && scope->pos != scope->len) {
        scope->error = PACKET_ERROR_TRAILING_DATA;
    }
    packet_active_reader_scope = scope->previous;
    if (scope->previous != NULL && scope->error != PACKET_ERROR_NONE &&
        scope->previous->error == PACKET_ERROR_NONE) {
        scope->previous->error = scope->error;
    }
    return scope->error;
}

static const uint8_t *packet_reader_take(packet_reader_t *reader, size_t len) {
    if (reader->error != PACKET_ERROR_NONE) {
        return NULL;
    }
    if (len > packet_reader_remaining(reader)) {
        packet_reader_fail(reader, PACKET_ERROR_TRUNCATED);
        return NULL;
    }
    if (len == 0) {
        return reader->data;
    }

    const uint8_t *data = reader->data + reader->pos;
    reader->pos += len;
    if (reader->scope != NULL) {
        reader->scope->pos = MAX(reader->scope->pos, reader->pos);
    }
    if (reader->position != NULL) {
        *reader->position = reader->pos;
    }
    return data;
}

bool packet_reader_skip(packet_reader_t *reader, size_t len) {
    if (len == 0 && reader->error == PACKET_ERROR_NONE) {
        return true;
    }
    return packet_reader_take(reader, len) != NULL;
}

packet_view_t packet_reader_read_view(packet_reader_t *reader, size_t len) {
    const uint8_t *data = packet_reader_take(reader, len);
    if (data == NULL) {
        return (packet_view_t){0};
    }
    return (packet_view_t){.data = data, .len = len};
}

packet_view_t packet_reader_read_string_view(packet_reader_t *reader, size_t max_len) {
    if (reader->error != PACKET_ERROR_NONE) {
        return (packet_view_t){0};
    }

    size_t remaining = packet_reader_remaining(reader);
    if (remaining == 0) {
        packet_reader_fail(reader, PACKET_ERROR_TRUNCATED);
        return (packet_view_t){0};
    }
    size_t search_len = MIN(remaining, max_len + (max_len != SIZE_MAX));
    const uint8_t *end = memchr(reader->data + reader->pos, '\0', search_len);
    if (end == NULL) {
        packet_reader_fail(reader,
                           remaining > max_len ? PACKET_ERROR_LIMIT_EXCEEDED
                                               : PACKET_ERROR_TRUNCATED);
        return (packet_view_t){0};
    }

    size_t len = (size_t)(end - (reader->data + reader->pos));
    if (len > max_len) {
        packet_reader_fail(reader, PACKET_ERROR_LIMIT_EXCEEDED);
        return (packet_view_t){0};
    }

    packet_view_t view = {.data = reader->data + reader->pos, .len = len};
    reader->pos += len + 1;
    if (reader->scope != NULL) {
        reader->scope->pos = MAX(reader->scope->pos, reader->pos);
    }
    if (reader->position != NULL) {
        *reader->position = reader->pos;
    }
    return view;
}

bool packet_reader_read_string_bounded(packet_reader_t *reader,
                                       char *dest,
                                       size_t dest_size,
                                       size_t max_len) {
    HARD_ASSERT(dest != NULL);
    HARD_ASSERT(dest_size != 0);

    packet_view_t view = packet_reader_read_string_view(reader, max_len);
    if (reader->error != PACKET_ERROR_NONE) {
        return false;
    }
    if (view.len >= dest_size) {
        packet_reader_fail(reader, PACKET_ERROR_LIMIT_EXCEEDED);
        return false;
    }

    memcpy(dest, view.data, view.len);
    dest[view.len] = '\0';
    return true;
}

bool packet_reader_read_string(packet_reader_t *reader, char *dest, size_t dest_size) {
    HARD_ASSERT(dest_size != 0);
    return packet_reader_read_string_bounded(reader, dest, dest_size, dest_size - 1);
}

bool packet_reader_read_stringbuffer_bounded(packet_reader_t *reader,
                                             StringBuffer *sb,
                                             size_t max_len) {
    HARD_ASSERT(sb != NULL);
    packet_view_t view = packet_reader_read_string_view(reader, max_len);
    if (reader->error != PACKET_ERROR_NONE) {
        return false;
    }
    stringbuffer_append_string_len(sb, (const char *)view.data, view.len);
    return true;
}

bool packet_reader_read_stringbuffer(packet_reader_t *reader, StringBuffer *sb) {
    return packet_reader_read_stringbuffer_bounded(reader, sb, PACKET_PAYLOAD_MAX);
}

uint8_t packet_reader_read_uint8(packet_reader_t *reader) {
    const uint8_t *data = packet_reader_take(reader, 1);
    return data != NULL ? data[0] : 0;
}

int8_t packet_reader_read_int8(packet_reader_t *reader) {
    uint8_t value = packet_reader_read_uint8(reader);
    int8_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

uint16_t packet_reader_read_uint16(packet_reader_t *reader) {
    const uint8_t *data = packet_reader_take(reader, 2);
    return data != NULL ? ((uint16_t)data[0] << 8) | (uint16_t)data[1] : 0;
}

int16_t packet_reader_read_int16(packet_reader_t *reader) {
    uint16_t value = packet_reader_read_uint16(reader);
    int16_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

uint32_t packet_reader_read_uint32(packet_reader_t *reader) {
    const uint8_t *data = packet_reader_take(reader, 4);
    return data != NULL ? ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                              ((uint32_t)data[2] << 8) | (uint32_t)data[3]
                        : 0;
}

int32_t packet_reader_read_int32(packet_reader_t *reader) {
    uint32_t value = packet_reader_read_uint32(reader);
    int32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

uint64_t packet_reader_read_uint64(packet_reader_t *reader) {
    const uint8_t *data = packet_reader_take(reader, 8);
    return data != NULL
               ? ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) |
                     ((uint64_t)data[3] << 32) | ((uint64_t)data[4] << 24) |
                     ((uint64_t)data[5] << 16) | ((uint64_t)data[6] << 8) | (uint64_t)data[7]
               : 0;
}

int64_t packet_reader_read_int64(packet_reader_t *reader) {
    uint64_t value = packet_reader_read_uint64(reader);
    int64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

float packet_reader_read_float(packet_reader_t *reader) {
    uint32_t value = packet_reader_read_uint32(reader);
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

double packet_reader_read_double(packet_reader_t *reader) {
    uint64_t value = packet_reader_read_uint64(reader);
    double result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool
packet_reader_check_count(packet_reader_t *reader, uint32_t value, size_t maximum, size_t *count) {
    HARD_ASSERT(count != NULL);
    if (reader->error != PACKET_ERROR_NONE) {
        return false;
    }
    if (value > maximum) {
        packet_reader_fail(reader, PACKET_ERROR_LIMIT_EXCEEDED);
        return false;
    }
    *count = value;
    return true;
}

bool packet_reader_read_count8(packet_reader_t *reader, size_t maximum, size_t *count) {
    return packet_reader_check_count(reader, packet_reader_read_uint8(reader), maximum, count);
}

bool packet_reader_read_count16(packet_reader_t *reader, size_t maximum, size_t *count) {
    return packet_reader_check_count(reader, packet_reader_read_uint16(reader), maximum, count);
}

bool packet_reader_read_count32(packet_reader_t *reader, size_t maximum, size_t *count) {
    return packet_reader_check_count(reader, packet_reader_read_uint32(reader), maximum, count);
}

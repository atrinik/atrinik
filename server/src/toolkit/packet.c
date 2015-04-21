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
 * Packet construction/management.
 *
 * @author Alex Tokar */

#ifndef __CPROTO__

#include <global.h>
#include <zlib.h>
#include <packet.h>
#include <toolkit_string.h>

/**
 * The packets memory pool. */
static mempool_struct *pool_packet;

static void packet_debugger(packet_struct *packet, char *buf, size_t size);

TOOLKIT_API(DEPENDS(mempool));

TOOLKIT_INIT_FUNC(packet)
{
    pool_packet = mempool_create("packets", PACKET_EXPAND,
            sizeof(packet_struct), MEMPOOL_ALLOW_FREEING,
            NULL, NULL, NULL, NULL);
    mempool_set_debugger(pool_packet, (chunk_debugger) packet_debugger);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(packet)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

/** @copydoc chunk_debugger */
static void packet_debugger(packet_struct *packet, char *buf, size_t size)
{
    snprintf(buf, size, "type: %d length: %"PRIu64" size: %"PRIu64,
            packet->type, (uint64_t) packet->len, (uint64_t) packet->size);

    if (packet->data != NULL && packet->len != 0) {
#define MAXHEXLEN 256
        char hexbuf[MAXHEXLEN * 3 + 1];

        string_tohex(packet->data, packet->len, hexbuf, sizeof(hexbuf), true);
        snprintfcat(buf, size, " data: %s", hexbuf);

        if (packet->len > MAXHEXLEN) {
            snprintfcat(buf, size, " (%"PRId64" bytes follow)",
                    (uint64_t) (packet->len - MAXHEXLEN));
        }
#undef MAXHEXLEN
    }
}

/**
 * Allocates a new packet.
 * @param type The packet's command type.
 * @param size Initial number of bytes to allocate for the packet's
 * data.
 * @param expand The minimum size to expand by when there is not enough
 * bytes allocated.
 * @return The allocated packet. */
packet_struct *packet_new(uint8_t type, size_t size, size_t expand)
{
    packet_struct *packet;

    TOOLKIT_PROTECT();

    packet = mempool_get(pool_packet);
    packet->size = size;
    packet->expand = expand;

    /* Allocate the initial data block. */
    if (packet->size) {
        packet->data = emalloc(packet->size);
    }

    packet->type = type;

#ifndef NDEBUG
    packet->sb = stringbuffer_new();
#endif

    return packet;
}

/**
 * Free a previously allocated data packet.
 * @param packet Packet to free. */
void packet_free(packet_struct *packet)
{
    TOOLKIT_PROTECT();

    if (packet->data) {
        efree(packet->data);
    }

#ifndef NDEBUG
    if (packet->sb != NULL) {
        efree(stringbuffer_finish(packet->sb));
    }
#endif

    mempool_return(pool_packet, packet);
}

/**
 * Compress a data packet, if possible.
 * @param packet Packet to try to compress. */
void packet_compress(packet_struct *packet)
{
    TOOLKIT_PROTECT();

#if defined(COMPRESS_DATA_PACKETS) && COMPRESS_DATA_PACKETS

    if (packet->len > COMPRESS_DATA_PACKETS_SIZE && packet->type != CLIENT_CMD_DATA) {
        size_t new_size = compressBound(packet->len);
        uint8_t *dest;

        dest = emalloc(new_size + 5);
        dest[0] = packet->type;
        /* Add original length of the packet. */
        dest[1] = (packet->len >> 24) & 0xff;
        dest[2] = (packet->len >> 16) & 0xff;
        dest[3] = (packet->len >> 8) & 0xff;
        dest[4] = (packet->len) & 0xff;
        packet->size = new_size + 5;
        /* Compress it. */
        compress2((Bytef *) dest + 5, (uLong *) & new_size, (const unsigned char FAR *) packet->data, packet->len, Z_BEST_COMPRESSION);

        efree(packet->data);
        packet->data = dest;
        packet->len = new_size + 5;
        packet->type = CLIENT_CMD_COMPRESSED;
    }
#else
    (void) packet;
#endif
}

/**
 * Enables NDELAY on the specified packet. */
void packet_enable_ndelay(packet_struct *packet)
{
    TOOLKIT_PROTECT();
    packet->ndelay = 1;
}

void packet_set_pos(packet_struct *packet, size_t pos)
{
    TOOLKIT_PROTECT();
    packet->len = pos;
}

size_t packet_get_pos(packet_struct *packet)
{
    TOOLKIT_PROTECT();
    return packet->len;
}

packet_struct *packet_dup(packet_struct *packet)
{
    packet_struct *cp;

    TOOLKIT_PROTECT();

    cp = packet_new(packet->type, packet->size, packet->expand);
    cp->ndelay = packet->ndelay;

    if (packet->data != NULL) {
        packet_append_data_len(cp, packet->data, packet->len);
    }

    return cp;
}

void packet_delete(packet_struct *packet, size_t pos, size_t len)
{
    TOOLKIT_PROTECT();

    if (packet->len - len + pos) {
        memmove(packet->data + pos, packet->data + pos + len, packet->len - len + pos);
    }

    packet->len -= len;
}

void packet_save(packet_struct *packet, packet_save_t *packet_save_buf)
{
    HARD_ASSERT(packet != NULL);
    HARD_ASSERT(packet_save_buf != NULL);

    packet_save_buf->pos = packet->len;

#ifndef NDEBUG
    packet_save_buf->sb_pos = packet->sb->pos;
#endif
}

void packet_load(packet_struct *packet, const packet_save_t *packet_save_buf)
{
    HARD_ASSERT(packet != NULL);
    HARD_ASSERT(packet_save_buf != NULL);

    packet->len = packet_save_buf->pos;

#ifndef NDEBUG
    packet->sb->pos = packet_save_buf->sb_pos;
#endif
}

/**
 * Ensure 'size' bytes are available for writing in the packet. If not,
 * will allocate more.
 * @param packet Packet.
 * @param size How many bytes we need. */
static void packet_ensure(packet_struct *packet, size_t size)
{
    TOOLKIT_PROTECT();

    if (packet->len + size < packet->size) {
        return;
    }

    packet->size += MAX(packet->expand, size);
    packet->data = erealloc(packet->data, packet->size);
}

char *packet_get_debug(packet_struct *packet)
{
    char *cp;

    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);

#ifndef NDEBUG
    HARD_ASSERT(packet->sb != NULL);
    cp = stringbuffer_finish(packet->sb);
    packet->sb = NULL;
#else
    cp = estrdup("");
#endif

    return cp;
}

static void packet_append_uint8_internal(packet_struct *packet, uint8_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 1);

    packet->data[packet->len++] = data;
}

void packet_append_uint8(packet_struct *packet, uint8_t data)
{
    TOOLKIT_PROTECT();

    packet_append_uint8_internal(packet, data);
    packet_debug(packet, 0, "%u\n", data);
}

void packet_append_int8(packet_struct *packet, int8_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 1);

    packet->data[packet->len++] = data;
    packet_debug(packet, 0, "%d\n", data);
}

void packet_append_uint16(packet_struct *packet, uint16_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 2);

    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%u\n", data);
}

void packet_append_int16(packet_struct *packet, int16_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 2);

    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%d\n", data);
}

static void packet_append_uint32_internal(packet_struct *packet, uint32_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 4);

    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
}

void packet_append_uint32(packet_struct *packet, uint32_t data)
{
    TOOLKIT_PROTECT();

    packet_append_uint32_internal(packet, data);
    packet_debug(packet, 0, "%u\n", data);
}

void packet_append_int32(packet_struct *packet, int32_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 4);

    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
    packet_debug(packet, 0, "%d\n", data);
}

static void packet_append_uint64_internal(packet_struct *packet, uint64_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 8);

    packet->data[packet->len++] = (data >> 56) & 0xff;
    packet->data[packet->len++] = (data >> 48) & 0xff;
    packet->data[packet->len++] = (data >> 40) & 0xff;
    packet->data[packet->len++] = (data >> 32) & 0xff;
    packet->data[packet->len++] = (data >> 24) & 0xff;
    packet->data[packet->len++] = (data >> 16) & 0xff;
    packet->data[packet->len++] = (data >> 8) & 0xff;
    packet->data[packet->len++] = data & 0xff;
}

void packet_append_uint64(packet_struct *packet, uint64_t data)
{
    TOOLKIT_PROTECT();

    packet_append_uint64_internal(packet, data);
    packet_debug(packet, 0, "%" PRIu64 "\n", data);
}

void packet_append_int64(packet_struct *packet, int64_t data)
{
    TOOLKIT_PROTECT();
    packet_ensure(packet, 8);

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

void packet_append_float(packet_struct *packet, float data)
{
    uint32_t val;

    TOOLKIT_PROTECT();

    memcpy(&val, &data, sizeof(val));
    packet_append_uint32_internal(packet, val);
    packet_debug(packet, 0, "%f\n", data);
}

void packet_append_double(packet_struct *packet, double data)
{
    uint64_t val;

    TOOLKIT_PROTECT();

    memcpy(&val, &data, sizeof(val));
    packet_append_uint64_internal(packet, val);
    packet_debug(packet, 0, "%f\n", data);
}

static void packet_append_data_len_internal(packet_struct *packet,
        const uint8_t *data, size_t len)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    packet_ensure(packet, len);
    memcpy(packet->data + packet->len, data, len);
    packet->len += len;
}

void packet_append_data_len(packet_struct *packet, const uint8_t *data,
        size_t len)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    packet_append_data_len_internal(packet, data, len);

#ifndef NDEBUG
    {
        char *hex;

        hex = emalloc(sizeof(*hex) * (len * 3 + 1));
        string_tohex(data, len, hex, len * 3 + 1, true);
        packet_debug(packet, 0, "%s\n", hex);
        efree(hex);
    }
#endif
}

void packet_append_string_len(packet_struct *packet, const char *data,
        size_t len)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    if (len == 0) {
        return;
    }

    packet_ensure(packet, len);
    memcpy(packet->data + packet->len, data, len);
    packet->len += len;
    packet_debug(packet, 0, "%s", data);
}

void packet_append_string(packet_struct *packet, const char *data)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    packet_append_string_len(packet, data, strlen(data));
}

void packet_append_string_len_terminated(packet_struct *packet,
        const char *data, size_t len)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    packet_append_string_len(packet, data, len);
    packet_append_uint8_internal(packet, '\0');
    packet_debug(packet, 0, "\n");
}

void packet_append_string_terminated(packet_struct *packet, const char *data)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    SOFT_ASSERT(data != NULL, "Data is NULL.");

    packet_append_string_len_terminated(packet, data, strlen(data));
}

void packet_append_packet(packet_struct *packet, packet_struct *src)
{
    TOOLKIT_PROTECT();

    HARD_ASSERT(packet != NULL);
    HARD_ASSERT(src != NULL);

    if (src->data != NULL) {
        packet_append_data_len_internal(packet, src->data, src->len);
    }

#ifndef NDEBUG
    if (packet->sb != NULL && src->sb != NULL) {
        char *cp;

        cp = stringbuffer_sub(src->sb, 0, 0);
        stringbuffer_append_string(packet->sb, cp);
        efree(cp);
    }
#endif
}

uint8_t packet_to_uint8(uint8_t *data, size_t len, size_t *pos)
{
    uint8_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 1) {
        *pos = len;
        return 0;
    }

    ret = data[*pos];
    *pos += 1;

    return ret;
}

int8_t packet_to_int8(uint8_t *data, size_t len, size_t *pos)
{
    int8_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 1) {
        *pos = len;
        return 0;
    }

    ret = data[*pos];
    *pos += 1;

    return ret;
}

uint16_t packet_to_uint16(uint8_t *data, size_t len, size_t *pos)
{
    uint16_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 2) {
        *pos = len;
        return 0;
    }

    ret = (data[*pos] << 8) + data[*pos + 1];
    *pos += 2;

    return ret;
}

int16_t packet_to_int16(uint8_t *data, size_t len, size_t *pos)
{
    int16_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 2) {
        *pos = len;
        return 0;
    }

    ret = (data[*pos] << 8) + data[*pos + 1];
    *pos += 2;

    return ret;
}

uint32_t packet_to_uint32(uint8_t *data, size_t len, size_t *pos)
{
    uint32_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 4) {
        *pos = len;
        return 0;
    }

    ret = (data[*pos] << 24) + (data[*pos + 1] << 16) + (data[*pos + 2] << 8) + data[*pos + 3];
    *pos += 4;

    return ret;
}

int32_t packet_to_int32(uint8_t *data, size_t len, size_t *pos)
{
    int32_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 4) {
        *pos = len;
        return 0;
    }

    ret = (data[*pos] << 24) + (data[*pos + 1] << 16) + (data[*pos + 2] << 8) + data[*pos + 3];
    *pos += 4;

    return ret;
}

uint64_t packet_to_uint64(uint8_t *data, size_t len, size_t *pos)
{
    uint64_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 8) {
        *pos = len;
        return 0;
    }

    ret = ((uint64_t) data[*pos] << 56) + ((uint64_t) data[*pos + 1] << 48) + ((uint64_t) data[*pos + 2] << 40) + ((uint64_t) data[*pos + 3] << 32) + ((uint64_t) data[*pos + 4] << 24) + ((uint64_t) data[*pos + 5] << 16) + ((uint64_t) data[*pos + 6] << 8) + (uint64_t) data[*pos + 7];
    *pos += 8;

    return ret;
}

int64_t packet_to_int64(uint8_t *data, size_t len, size_t *pos)
{
    int64_t ret;

    TOOLKIT_PROTECT();

    if (len - *pos < 8) {
        *pos = len;
        return 0;
    }

    ret = ((int64_t) data[*pos] << 56) + ((int64_t) data[*pos + 1] << 48) + ((int64_t) data[*pos + 2] << 40) + ((int64_t) data[*pos + 3] << 32) + ((int64_t) data[*pos + 4] << 24) + ((int64_t) data[*pos + 5] << 16) + ((int64_t) data[*pos + 6] << 8) + (int64_t) data[*pos + 7];
    *pos += 8;

    return ret;
}

float packet_to_float(uint8_t *data, size_t len, size_t *pos)
{
    uint32_t val;
    float ret;

    TOOLKIT_PROTECT();

    val = packet_to_uint32(data, len, pos);
    memcpy(&ret, &val, sizeof(ret));

    return ret;
}

double packet_to_double(uint8_t *data, size_t len, size_t *pos)
{
    uint64_t val;
    double ret;

    TOOLKIT_PROTECT();

    val = packet_to_uint64(data, len, pos);
    memcpy(&ret, &val, sizeof(ret));

    return ret;
}

char *packet_to_string(uint8_t *data, size_t len, size_t *pos, char *dest, size_t dest_size)
{
    size_t i = 0;
    char c;

    TOOLKIT_PROTECT();

    while (*pos < len && (c = (char) (data[(*pos)++]))) {
        if (i < dest_size - 1) {
            dest[i++] = c;
        }
    }

    dest[i] = '\0';
    return dest;
}

void packet_to_stringbuffer(uint8_t *data, size_t len, size_t *pos, StringBuffer *sb)
{
    char *str;

    TOOLKIT_PROTECT();

    str = (char *) (data + *pos);
    stringbuffer_append_string_len(sb, str, strnlen(str, len - *pos));
    *pos += strlen(str) + 1;
}

#endif

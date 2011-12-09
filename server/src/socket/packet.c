/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

#include <global.h>
#include <zlib.h>

/**
 * Allocates a new packet.
 * @param type The packet's command type.
 * @param size Initial number of bytes to allocate for the packet's
 * data.
 * @param expand The minimum size to expand by when there is not enough
 * bytes allocated.
 * @return The allocated packet. */
packet_struct *packet_new(uint8 type, size_t size, size_t expand)
{
	packet_struct *packet;

	packet = get_poolchunk(pool_packets);
	packet->next = packet->prev = NULL;
	packet->pos = 0;
	packet->size = size;
	packet->expand = expand;
	packet->len = 0;
	/* Allocate the initial data block. */
	packet->data = malloc(packet->size);
	packet->ndelay = 0;
	packet->type = type;

	return packet;
}

/**
 * Free a previously allocated data packet.
 * @param packet Packet to free. */
void packet_free(packet_struct *packet)
{
	if (packet->data)
	{
		free(packet->data);
	}

	return_poolchunk(packet, pool_packets);
}

/**
 * Compress a data packet, if possible.
 * @param packet Packet to try to compress. */
void packet_compress(packet_struct *packet)
{
#if COMPRESS_DATA_PACKETS
	if (packet->len > COMPRESS_DATA_PACKETS_SIZE && packet->type != BINARY_CMD_DATA)
	{
		size_t new_size = compressBound(packet->len);
		uint8 *dest;

		dest = malloc(new_size + 5);
		dest[0] = packet->type;
		/* Add original length of the packet. */
		dest[1] = (packet->len >> 24) & 0xff;
		dest[2] = (packet->len >> 16) & 0xff;
		dest[3] = (packet->len >> 8) & 0xff;
		dest[4] = (packet->len) & 0xff;
		packet->size = new_size + 5;
		/* Compress it. */
		compress2((Bytef *) dest + 5, (uLong *) &new_size, (const unsigned char FAR *) packet->data, packet->len, Z_BEST_COMPRESSION);

		free(packet->data);
		packet->data = dest;
		packet->len = new_size + 5;
		packet->type = BINARY_CMD_COMPRESSED;
	}
#else
	(void) packet;
#endif
}

/**
 * Enables NDELAY on the specified packet. */
void packet_enable_ndelay(packet_struct *packet)
{
	packet->ndelay = 1;
}

void packet_set_pos(packet_struct *packet, size_t pos)
{
	packet->len = pos;
}

size_t packet_get_pos(packet_struct *packet)
{
	return packet->len;
}

/**
 * Ensure 'size' bytes are available for writing in the packet. If not,
 * will allocate more.
 * @param packet Packet.
 * @param size How many bytes we need. */
static void packet_ensure(packet_struct *packet, size_t size)
{
	if (packet->len + size < packet->size)
	{
		return;
	}

	packet->size += MAX(packet->expand, size);
	packet->data = realloc(packet->data, packet->size);

	if (!packet->data)
	{
		LOG(llevError, "packet_ensure(): Out of memory.\n");
	}
}

void packet_merge(packet_struct *src, packet_struct *dst)
{
	packet_append_data_len(dst, src->data, src->len);
}

void packet_append_uint8(packet_struct *packet, uint8 data)
{
	packet_ensure(packet, 1);

	packet->data[packet->len++] = data;
}

void packet_append_uint16(packet_struct *packet, uint16 data)
{
	packet_ensure(packet, 2);

	packet->data[packet->len++] = (data >> 8) & 0xff;
	packet->data[packet->len++] = data & 0xff;
}

void packet_append_uint32(packet_struct *packet, uint32 data)
{
	packet_ensure(packet, 4);

	packet->data[packet->len++] = (data >> 24) & 0xff;
	packet->data[packet->len++] = (data >> 16) & 0xff;
	packet->data[packet->len++] = (data >> 8) & 0xff;
	packet->data[packet->len++] = data & 0xff;
}

void packet_append_uint64(packet_struct *packet, uint64 data)
{
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

void packet_append_data_len(packet_struct *packet, const uint8 *data, size_t len)
{
	if (!data)
	{
		return;
	}

	packet_ensure(packet, len);
	memcpy(packet->data + packet->len, data, len);
	packet->len += len;
}

void packet_append_string(packet_struct *packet, const char *data)
{
	packet_append_data_len(packet, (const uint8 *) data, strlen(data));
}

void packet_append_string_terminated(packet_struct *packet, const char *data)
{
	packet_append_string(packet, data);
	packet_append_uint8(packet, '\0');
}

void packet_append_map_name(packet_struct *packet, object *op, object *map_info)
{
	packet_append_string(packet, "<b><o=0,0,0>");
	packet_append_string(packet, map_info && map_info->race ? map_info->race : op->map->name);
	packet_append_string_terminated(packet, "</o></b>");
}

void packet_append_map_music(packet_struct *packet, object *op, object *map_info)
{
	packet_append_string_terminated(packet, map_info && map_info->slaying ? map_info->slaying : (op->map->bg_music ? op->map->bg_music : "no_music"));
}

void packet_append_map_weather(packet_struct *packet, object *op, object *map_info)
{
	packet_append_string_terminated(packet, map_info && map_info->title ? map_info->title : (op->map->weather ? op->map->weather : "none"));
}

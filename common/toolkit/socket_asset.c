/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/**
 * @file
 * Encoding and validation for the in-band QUIC asset protocol.
 */

#include "socket.h"
#include "packet.h"

void
socket_asset_request_append (packet_struct *packet,
                             const char    *path,
                             uint32_t       offset,
                             uint32_t       cached_size,
                             uint32_t       cached_checksum)
{
    packet_append_string_terminated(packet, path);
    packet_append_uint32(packet, offset);
    packet_append_uint32(packet, cached_size);
    packet_append_uint32(packet, cached_checksum);
}

bool
socket_asset_request_parse (uint8_t                *data,
                            size_t                  len,
                            size_t                  pos,
                            socket_asset_request_t *request)
{
    if (data == NULL || request == NULL || pos > len ||
        packet_to_string(data, len, &pos, VS(request->path)) == NULL ||
        len - pos != 12) {
        return false;
    }

    request->offset = packet_to_uint32(data, len, &pos);
    request->cached_size = packet_to_uint32(data, len, &pos);
    request->cached_checksum = packet_to_uint32(data, len, &pos);
    return *request->path != '\0' &&
           (request->offset == 0 ||
            (request->cached_size == 0 && request->cached_checksum == 0));
}

void
socket_asset_response_append_status (packet_struct *packet,
                                     uint8_t        status,
                                     const char    *path)
{
    packet_append_uint8(packet, status);
    packet_append_string_terminated(packet, path);
}

void
socket_asset_response_append_ok (packet_struct *packet,
                                 const char    *path,
                                 uint32_t       total_size,
                                 uint32_t       offset,
                                 uint32_t       checksum,
                                 const uint8_t *data,
                                 size_t         data_size)
{
    socket_asset_response_append_status(packet, ASSET_STATUS_OK, path);
    packet_append_uint32(packet, total_size);
    packet_append_uint32(packet, offset);
    packet_append_uint32(packet, checksum);
    packet_append_data_len(packet, data, data_size);
}

bool
socket_asset_response_parse (uint8_t                 *data,
                             size_t                   len,
                             size_t                   pos,
                             socket_asset_response_t *response)
{
    if (data == NULL || response == NULL || pos >= len) {
        return false;
    }

    memset(response, 0, sizeof(*response));
    response->status = packet_to_uint8(data, len, &pos);
    if (packet_to_string(data, len, &pos, VS(response->path)) == NULL) {
        return false;
    }
    if (*response->path == '\0') {
        return false;
    }

    if (response->status == ASSET_STATUS_NOT_FOUND ||
        response->status == ASSET_STATUS_NOT_MODIFIED) {
        return pos == len;
    }
    if (response->status != ASSET_STATUS_OK || len - pos < 12) {
        return false;
    }

    response->total_size = packet_to_uint32(data, len, &pos);
    response->offset = packet_to_uint32(data, len, &pos);
    response->checksum = packet_to_uint32(data, len, &pos);
    response->data = data + pos;
    response->data_size = len - pos;

    return response->total_size <= ASSET_MAX_SIZE &&
           response->offset <= response->total_size &&
           (response->offset == 0 || response->checksum == 0) &&
           response->data_size <= ASSET_CHUNK_SIZE &&
           response->data_size <= response->total_size - response->offset;
}

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

void socket_asset_request_append(packet_struct *packet,
                                 const char *path,
                                 uint32_t offset,
                                 uint32_t cached_size,
                                 const uint8_t cached_digest[ASSET_DIGEST_SIZE],
                                 uint8_t flags) {
    packet_writer_write_cstring(packet, path);
    packet_writer_write_uint32(packet, offset);
    packet_writer_write_uint32(packet, cached_size);
    packet_writer_write_bytes(packet, cached_digest, ASSET_DIGEST_SIZE);
    packet_writer_write_uint8(packet, flags);
}

bool socket_asset_request_parse(const uint8_t *data,
                                size_t len,
                                size_t pos,
                                socket_asset_request_t *request) {
    if (request == NULL) {
        return false;
    }

    socket_asset_request_t parsed = {0};
    packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);
    packet_reader_read_string(&reader, VS(parsed.path));
    parsed.offset = packet_reader_read_uint32(&reader);
    parsed.cached_size = packet_reader_read_uint32(&reader);
    packet_view_t digest = packet_reader_read_view(&reader, ASSET_DIGEST_SIZE);
    parsed.flags = packet_reader_read_uint8(&reader);
    if (!packet_reader_finish(&reader) || *parsed.path == '\0') {
        return false;
    }
    memcpy(parsed.cached_digest, digest.data, digest.len);

    static const uint8_t empty_digest[ASSET_DIGEST_SIZE];
    if ((parsed.flags & ~ASSET_REQUEST_METADATA) != 0 ||
        ((parsed.flags & ASSET_REQUEST_METADATA) != 0 &&
         (parsed.offset != 0 || parsed.cached_size != 0 ||
          memcmp(parsed.cached_digest, empty_digest, ASSET_DIGEST_SIZE) != 0)) ||
        (parsed.offset != 0 &&
         (parsed.cached_size != 0 ||
          memcmp(parsed.cached_digest, empty_digest, ASSET_DIGEST_SIZE) != 0))) {
        return false;
    }

    *request = parsed;
    return true;
}

void socket_asset_response_append_metadata(packet_struct *packet,
                                           const char *path,
                                           uint32_t total_size,
                                           const uint8_t digest[ASSET_DIGEST_SIZE]) {
    socket_asset_response_append_status(packet, ASSET_STATUS_METADATA, path);
    packet_writer_write_uint32(packet, total_size);
    packet_writer_write_bytes(packet, digest, ASSET_DIGEST_SIZE);
}

void socket_asset_response_append_status(packet_struct *packet, uint8_t status, const char *path) {
    packet_writer_write_uint8(packet, status);
    packet_writer_write_cstring(packet, path);
}

void socket_asset_response_append_ok(packet_struct *packet,
                                     const char *path,
                                     uint32_t total_size,
                                     uint32_t offset,
                                     const uint8_t digest[ASSET_DIGEST_SIZE],
                                     const uint8_t *data,
                                     size_t data_size) {
    socket_asset_response_append_status(packet, ASSET_STATUS_OK, path);
    packet_writer_write_uint32(packet, total_size);
    packet_writer_write_uint32(packet, offset);
    packet_writer_write_bytes(packet, digest, ASSET_DIGEST_SIZE);
    packet_writer_write_bytes(packet, data, data_size);
}

bool socket_asset_response_parse(const uint8_t *data,
                                 size_t len,
                                 size_t pos,
                                 socket_asset_response_t *response) {
    if (response == NULL) {
        return false;
    }

    socket_asset_response_t parsed = {0};
    packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);
    parsed.status = packet_reader_read_uint8(&reader);
    packet_reader_read_string(&reader, VS(parsed.path));
    if (packet_reader_error(&reader) != PACKET_ERROR_NONE || *parsed.path == '\0') {
        return false;
    }

    if (parsed.status == ASSET_STATUS_NOT_FOUND ||
        parsed.status == ASSET_STATUS_METADATA_NOT_FOUND ||
        parsed.status == ASSET_STATUS_NOT_MODIFIED) {
        if (!packet_reader_finish(&reader)) {
            return false;
        }
        *response = parsed;
        return true;
    }
    if (parsed.status == ASSET_STATUS_METADATA) {
        parsed.total_size = packet_reader_read_uint32(&reader);
        packet_view_t digest = packet_reader_read_view(&reader, ASSET_DIGEST_SIZE);
        if (!packet_reader_finish(&reader) || parsed.total_size > ASSET_MAX_SIZE) {
            return false;
        }
        memcpy(parsed.digest, digest.data, digest.len);
        *response = parsed;
        return true;
    }
    if (parsed.status != ASSET_STATUS_OK) {
        return false;
    }

    parsed.total_size = packet_reader_read_uint32(&reader);
    parsed.offset = packet_reader_read_uint32(&reader);
    packet_view_t digest = packet_reader_read_view(&reader, ASSET_DIGEST_SIZE);
    packet_view_t payload = packet_reader_read_view(&reader, packet_reader_remaining(&reader));
    if (!packet_reader_finish(&reader) || parsed.total_size > ASSET_MAX_SIZE ||
        parsed.offset > parsed.total_size || payload.len > ASSET_CHUNK_SIZE ||
        payload.len > parsed.total_size - parsed.offset) {
        return false;
    }
    memcpy(parsed.digest, digest.data, digest.len);
    parsed.data = payload.data;
    parsed.data_size = payload.len;
    *response = parsed;
    return true;
}

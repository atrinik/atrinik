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
                                 uint32_t cached_size,
                                 const uint8_t cached_digest[ASSET_DIGEST_SIZE],
                                 uint8_t flags) {
    packet_writer_write_cstring(packet, path);
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
         (parsed.cached_size != 0 ||
          memcmp(parsed.cached_digest, empty_digest, ASSET_DIGEST_SIZE) != 0))) {
        return false;
    }

    *request = parsed;
    return true;
}

void socket_asset_response_append_metadata(packet_struct *packet,
                                           uint32_t total_size,
                                           const uint8_t digest[ASSET_DIGEST_SIZE]) {
    socket_asset_response_append_status(packet, ASSET_STATUS_METADATA, total_size, digest);
}

void socket_asset_response_append_status(packet_struct *packet,
                                         uint8_t status,
                                         uint32_t total_size,
                                         const uint8_t digest[ASSET_DIGEST_SIZE]) {
    packet_writer_write_uint8(packet, status);
    packet_writer_write_uint32(packet, total_size);
    static const uint8_t empty_digest[ASSET_DIGEST_SIZE];
    packet_writer_write_bytes(packet, digest != NULL ? digest : empty_digest, ASSET_DIGEST_SIZE);
}

void socket_asset_response_append_ok(packet_struct *packet,
                                     uint32_t total_size,
                                     const uint8_t digest[ASSET_DIGEST_SIZE]) {
    socket_asset_response_append_status(packet, ASSET_STATUS_OK, total_size, digest);
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
    parsed.total_size = packet_reader_read_uint32(&reader);
    packet_view_t digest = packet_reader_read_view(&reader, ASSET_DIGEST_SIZE);
    static const uint8_t empty_digest[ASSET_DIGEST_SIZE];
    if (!packet_reader_finish(&reader) || parsed.status > ASSET_STATUS_METADATA_NOT_FOUND ||
        parsed.total_size > ASSET_MAX_SIZE ||
        ((parsed.status == ASSET_STATUS_OK || parsed.status == ASSET_STATUS_METADATA) &&
         digest.len != ASSET_DIGEST_SIZE) ||
        ((parsed.status == ASSET_STATUS_NOT_FOUND ||
          parsed.status == ASSET_STATUS_METADATA_NOT_FOUND) &&
         (parsed.total_size != 0 || memcmp(digest.data, empty_digest, ASSET_DIGEST_SIZE) != 0))) {
        return false;
    }
    memcpy(parsed.digest, digest.data, digest.len);
    *response = parsed;
    return true;
}

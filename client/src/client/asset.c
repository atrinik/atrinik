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
 * Cached asset transfer over the established QUIC game connection.
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>

#define ASSET_MAX_SIZE (128U * 1024U * 1024U)

struct asset_request {
    UT_hash_handle hh;
    char *path;
    uint8_t *data;
    size_t size;
    size_t received;
    size_t references;
    asset_request_state_t state;
};

static asset_request_t *asset_requests;

static void
asset_request_send (asset_request_t *request)
{
    LOG(DEBUG,
        "Requesting QUIC asset %s at offset %" PRIu64,
        request->path,
        (uint64_t) request->received);
    packet_struct *packet = packet_new(SERVER_CMD_ASSET, 128, 128);
    packet_append_string_terminated(packet, request->path);
    packet_append_uint32(packet, (uint32_t) request->received);
    socket_send_packet(packet);
}

asset_request_t *
asset_request_start (const char *path)
{
    if (!cpl.asset_transport || !socket_is_quic(csocket.sc) ||
        path == NULL || *path == '\0' || strlen(path) >= MAX_BUF) {
        return NULL;
    }

    asset_request_t *request;
    HASH_FIND_STR(asset_requests, path, request);
    if (request != NULL) {
        request->references++;
        return request;
    }

    request = ecalloc(1, sizeof(*request));
    request->path = estrdup(path);
    request->references = 1;
    request->state = ASSET_REQUEST_PENDING;
    HASH_ADD_KEYPTR(hh,
                    asset_requests,
                    request->path,
                    strlen(request->path),
                    request);
    asset_request_send(request);
    return request;
}

asset_request_state_t
asset_request_get_state (const asset_request_t *request)
{
    return request != NULL ? request->state : ASSET_REQUEST_ERROR;
}

const uint8_t *
asset_request_get_data (const asset_request_t *request, size_t *size)
{
    if (size != NULL) {
        *size = request != NULL ? request->size : 0;
    }
    if (request == NULL || request->state != ASSET_REQUEST_COMPLETE) {
        return NULL;
    }
    return request->data;
}

void
asset_request_free (asset_request_t *request)
{
    if (request == NULL || --request->references != 0) {
        return;
    }

    HASH_DEL(asset_requests, request);
    efree(request->path);
    if (request->data != NULL) {
        efree(request->data);
    }
    efree(request);
}

void
socket_command_asset (uint8_t *data, size_t len, size_t pos)
{
    uint8_t status = packet_to_uint8(data, len, &pos);
    char path[MAX_BUF];
    if (packet_to_string(data, len, &pos, VS(path)) == NULL) {
        return;
    }

    asset_request_t *request;
    HASH_FIND_STR(asset_requests, path, request);
    if (request == NULL || request->state != ASSET_REQUEST_PENDING) {
        return;
    }

    if (status != ASSET_STATUS_OK) {
        request->state = ASSET_REQUEST_ERROR;
        return;
    }

    uint32_t total = packet_to_uint32(data, len, &pos);
    uint32_t offset = packet_to_uint32(data, len, &pos);
    size_t chunk_size = len - pos;
    LOG(DEBUG,
        "Received QUIC asset %s offset %" PRIu32 "/%" PRIu32
        " (%" PRIu64 " bytes)",
        path,
        offset,
        total,
        (uint64_t) chunk_size);
    if (total > ASSET_MAX_SIZE ||
        offset != request->received ||
        chunk_size > ASSET_CHUNK_SIZE ||
        (size_t) offset + chunk_size > total ||
        (request->data != NULL && request->size != total)) {
        LOG(ERROR,
            "Rejected malformed QUIC asset chunk for %s: expected offset "
            "%" PRIu64 ", received offset %" PRIu32 ", total %" PRIu32
            ", chunk size %" PRIu64,
            path,
            (uint64_t) request->received,
            offset,
            total,
            (uint64_t) chunk_size);
        request->state = ASSET_REQUEST_ERROR;
        return;
    }

    if (request->data == NULL) {
        request->data = emalloc((size_t) total + 1);
        request->size = total;
    }

    memcpy(request->data + request->received, data + pos, chunk_size);
    request->received += chunk_size;
    request->data[request->received] = '\0';

    if (request->received == request->size) {
        request->state = ASSET_REQUEST_COMPLETE;
    } else if (chunk_size == 0) {
        request->state = ASSET_REQUEST_ERROR;
    } else {
        asset_request_send(request);
    }
}

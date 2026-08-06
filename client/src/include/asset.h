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
 * In-band QUIC asset download declarations.
 */

#ifndef CLIENT_ASSET_H
#define CLIENT_ASSET_H

typedef struct asset_request asset_request_t;

typedef enum asset_request_state {
    ASSET_REQUEST_PENDING,
    ASSET_REQUEST_COMPLETE,
    ASSET_REQUEST_ERROR,
} asset_request_state_t;

asset_request_t *asset_request_start(const char *path);

asset_request_t *asset_request_start_cached(const char *path, const char *cache_path);

asset_request_t *asset_request_start_metadata(const char *path);

asset_request_state_t asset_request_get_state(asset_request_t *request);

const uint8_t *asset_request_get_data(const asset_request_t *request, size_t *size);

bool asset_request_get_metadata(const asset_request_t *request,
                                size_t *size,
                                uint8_t digest[ASSET_DIGEST_SIZE]);

void asset_request_free(asset_request_t *request);

bool asset_requests_service(socket_t *sc, bool *pending);

void asset_requests_disconnect(void);

void asset_requests_deinit(void);

#endif

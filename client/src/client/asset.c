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
#include <toolkit/path.h>
#include <toolkit/string.h>
#include <openssl/evp.h>

struct asset_request {
    UT_hash_handle hh;
    char *key;
    char *path;
    char *cache_path;
    uint8_t *data;
    size_t size;
    size_t received;
    size_t references;
    uint8_t cached_digest[ASSET_DIGEST_SIZE];
    uint8_t expected_digest[ASSET_DIGEST_SIZE];
    bool cache_loaded;
    bool metadata_only;
    asset_request_state_t state;
};

static asset_request_t *asset_requests;

static void asset_request_send(asset_request_t *request) {
    LOG(DEBUG,
        "Requesting QUIC asset %s at offset %" PRIu64,
        request->path,
        (uint64_t)request->received);
    packet_struct *packet = packet_new(SERVER_CMD_ASSET, 128, 128);
    static const uint8_t empty_digest[ASSET_DIGEST_SIZE];
    socket_asset_request_append(
        packet,
        request->path,
        (uint32_t)request->received,
        request->received == 0 && request->cache_loaded ? (uint32_t)request->size : 0,
        request->received == 0 && request->cache_loaded ? request->cached_digest : empty_digest,
        request->metadata_only ? ASSET_REQUEST_METADATA : 0);
    socket_send_packet(packet);
}

static void asset_request_cache_load(asset_request_t *request) {
    if (request->cache_path == NULL) {
        return;
    }

    FILE *fp = path_fopen(request->cache_path, "rb");
    struct stat sb;
    if (fp == NULL || fstat(fileno(fp), &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_size < 0 ||
        (uint64_t)sb.st_size > ASSET_MAX_SIZE) {
        if (fp != NULL) {
            fclose(fp);
        }
        return;
    }

    uint8_t *data = xmalloc((size_t)sb.st_size + 1);
    bool success = fread(data, 1, (size_t)sb.st_size, fp) == (size_t)sb.st_size;
    if (fclose(fp) != 0) {
        success = false;
    }
    if (!success) {
        free(data);
        return;
    }

    data[(size_t)sb.st_size] = '\0';
    request->data = data;
    request->size = (size_t)sb.st_size;
    unsigned int digest_size = 0;
    if (EVP_Digest(request->data,
                   request->size,
                   request->cached_digest,
                   &digest_size,
                   EVP_sha256(),
                   NULL) != 1 ||
        digest_size != ASSET_DIGEST_SIZE) {
        free(request->data);
        request->data = NULL;
        request->size = 0;
        return;
    }
    request->cache_loaded = true;
    LOG(DEBUG,
        "Loaded cached QUIC asset %s (%" PRIu64 " bytes)",
        request->path,
        (uint64_t)request->size);
}

static void asset_request_cache_save(const asset_request_t *request) {
    if (request->cache_path == NULL) {
        return;
    }

    char *path = file_path(request->cache_path, "wb");
    bool success = path_write_atomic(path, request->data, request->size, 0600);
    free(path);
    if (!success) {
        LOG(ERROR, "Could not write QUIC asset cache %s", request->cache_path);
    } else {
        LOG(DEBUG, "Cached QUIC asset %s at %s", request->path, request->cache_path);
    }
}

static asset_request_t *
asset_request_start_internal(const char *path, const char *cache_path, bool metadata_only) {
    if (!cpl.asset_transport || csocket.sc == NULL || !socket_is_quic(csocket.sc) || path == NULL ||
        *path == '\0' || strlen(path) >= MAX_BUF) {
        return NULL;
    }

    char key[MAX_BUF + 3];
    snprintf(VS(key), "%c:%s", metadata_only ? 'M' : 'D', path);
    asset_request_t *request;
    HASH_FIND_STR(asset_requests, key, request);
    if (request != NULL) {
        request->references++;
        return request;
    }

    request = xcalloc(1, sizeof(*request));
    request->key = xstrdup(key);
    request->path = xstrdup(path);
    if (cache_path != NULL) {
        request->cache_path = xstrdup(cache_path);
    }
    request->references = 1;
    request->metadata_only = metadata_only;
    request->state = ASSET_REQUEST_PENDING;
    HASH_ADD_KEYPTR(hh, asset_requests, request->key, strlen(request->key), request);
    asset_request_cache_load(request);
    asset_request_send(request);
    return request;
}

asset_request_t *asset_request_start(const char *path) {
    return asset_request_start_internal(path, NULL, false);
}

asset_request_t *asset_request_start_cached(const char *path, const char *cache_path) {
    return asset_request_start_internal(path, cache_path, false);
}

asset_request_t *asset_request_start_metadata(const char *path) {
    return asset_request_start_internal(path, NULL, true);
}

asset_request_state_t asset_request_get_state(const asset_request_t *request) {
    return request != NULL ? request->state : ASSET_REQUEST_ERROR;
}

const uint8_t *asset_request_get_data(const asset_request_t *request, size_t *size) {
    if (size != NULL) {
        *size = request != NULL ? request->size : 0;
    }
    if (request == NULL || request->state != ASSET_REQUEST_COMPLETE) {
        return NULL;
    }
    return request->data;
}

bool asset_request_get_metadata(const asset_request_t *request,
                                size_t *size,
                                uint8_t digest[ASSET_DIGEST_SIZE]) {
    if (request == NULL || !request->metadata_only || request->state != ASSET_REQUEST_COMPLETE) {
        return false;
    }
    if (size != NULL) {
        *size = request->size;
    }
    if (digest != NULL) {
        memcpy(digest, request->expected_digest, ASSET_DIGEST_SIZE);
    }
    return true;
}

void asset_request_free(asset_request_t *request) {
    if (request == NULL || --request->references != 0) {
        return;
    }

    HASH_DEL(asset_requests, request);
    free(request->key);
    free(request->path);
    free(request->cache_path);
    free(request->data);
    free(request);
}

void socket_command_asset(uint8_t *data, size_t len, size_t pos) {
    socket_asset_response_t response;
    if (!socket_asset_response_parse(data, len, pos, &response)) {
        LOG(ERROR, "Rejected malformed QUIC asset response");
        asset_request_t *request, *next;
        HASH_ITER(hh, asset_requests, request, next) {
            if (request->state == ASSET_REQUEST_PENDING) {
                request->state = ASSET_REQUEST_ERROR;
            }
        }
        return;
    }

    bool metadata_response = response.status == ASSET_STATUS_METADATA ||
                             response.status == ASSET_STATUS_METADATA_NOT_FOUND;
    char key[MAX_BUF + 3];
    snprintf(VS(key), "%c:%s", metadata_response ? 'M' : 'D', response.path);
    asset_request_t *request;
    HASH_FIND_STR(asset_requests, key, request);
    if (request == NULL || request->state != ASSET_REQUEST_PENDING) {
        return;
    }

    if (response.status == ASSET_STATUS_METADATA) {
        request->size = response.total_size;
        memcpy(request->expected_digest, response.digest, ASSET_DIGEST_SIZE);
        request->state = ASSET_REQUEST_COMPLETE;
        return;
    }

    if (response.status == ASSET_STATUS_NOT_MODIFIED) {
        if (!request->cache_loaded) {
            LOG(ERROR, "Server accepted missing QUIC asset cache for %s", response.path);
            request->state = ASSET_REQUEST_ERROR;
            return;
        }
        LOG(DEBUG, "Server confirmed cached QUIC asset %s is current", response.path);
        request->state = ASSET_REQUEST_COMPLETE;
        return;
    }

    if (response.status != ASSET_STATUS_OK) {
        LOG(ERROR,
            "QUIC asset request for %s failed with status %" PRIu8,
            response.path,
            response.status);
        request->state = ASSET_REQUEST_ERROR;
        return;
    }

    LOG(DEBUG,
        "Received QUIC asset %s offset %" PRIu32 "/%" PRIu32 " (%" PRIu64 " bytes)",
        response.path,
        response.offset,
        response.total_size,
        (uint64_t)response.data_size);
    if (response.offset != request->received ||
        (request->data != NULL && !request->cache_loaded && request->size != response.total_size)) {
        LOG(ERROR,
            "Rejected malformed QUIC asset chunk for %s: expected offset "
            "%" PRIu64 ", received offset %" PRIu32 ", total %" PRIu32 ", chunk size %" PRIu64,
            response.path,
            (uint64_t)request->received,
            response.offset,
            response.total_size,
            (uint64_t)response.data_size);
        request->state = ASSET_REQUEST_ERROR;
        return;
    }

    if (response.offset == 0) {
        free(request->data);
        request->data = NULL;
        request->size = 0;
        request->cache_loaded = false;
        memcpy(request->expected_digest, response.digest, ASSET_DIGEST_SIZE);
    } else if (memcmp(request->expected_digest, response.digest, ASSET_DIGEST_SIZE) != 0) {
        LOG(ERROR, "Rejected changing QUIC asset digest for %s", response.path);
        request->state = ASSET_REQUEST_ERROR;
        return;
    }

    if (request->data == NULL) {
        request->data = xmalloc((size_t)response.total_size + 1);
        request->size = response.total_size;
    }

    memcpy(request->data + request->received, response.data, response.data_size);
    request->received += response.data_size;
    request->data[request->received] = '\0';

    if (request->received == request->size) {
        uint8_t actual_digest[ASSET_DIGEST_SIZE];
        unsigned int digest_size = 0;
        if (EVP_Digest(request->data,
                       request->size,
                       actual_digest,
                       &digest_size,
                       EVP_sha256(),
                       NULL) != 1 ||
            digest_size != ASSET_DIGEST_SIZE ||
            memcmp(actual_digest, request->expected_digest, ASSET_DIGEST_SIZE) != 0) {
            LOG(ERROR, "Rejected QUIC asset %s: SHA-256 mismatch", response.path);
            request->state = ASSET_REQUEST_ERROR;
            return;
        }
        asset_request_cache_save(request);
        request->state = ASSET_REQUEST_COMPLETE;
    } else if (response.data_size == 0) {
        request->state = ASSET_REQUEST_ERROR;
    } else {
        asset_request_send(request);
    }
}

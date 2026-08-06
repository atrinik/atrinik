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
 * Bounded explicit-QUIC-stream asset scheduler and receiver.
 */

#include <global.h>
#include <wrapper.h>
#include <client_socket.h>
#include <network_graph.h>
#include <toolkit/packet.h>
#include <toolkit/path.h>
#include <toolkit/string.h>
#include <openssl/evp.h>

#define ASSET_RESPONSE_HEADER_SIZE (1U + 4U + ASSET_DIGEST_SIZE)
#define ASSET_STREAM_ERROR_CANCELLED 2U
#define ASSET_STREAM_ERROR_PROTOCOL 3U

typedef enum asset_transport_state {
    ASSET_TRANSPORT_QUEUED,
    ASSET_TRANSPORT_SEND_REQUEST,
    ASSET_TRANSPORT_READ_HEADER,
    ASSET_TRANSPORT_READ_BODY,
    ASSET_TRANSPORT_WAIT_FIN,
} asset_transport_state_t;

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
    bool cancelled;
    bool cache_needs_save;
    asset_request_state_t state;
    asset_transport_state_t transport_state;
    socket_stream_t *stream;
    packet_struct *wire_request;
    size_t wire_pos;
    uint8_t response_header[ASSET_RESPONSE_HEADER_SIZE];
    size_t response_header_pos;
    socket_asset_response_t response;
    EVP_MD_CTX *digest;
};

static asset_request_t *asset_requests;
static SDL_Mutex *asset_mutex;

static bool asset_lock(void) {
    if (asset_mutex == NULL) {
        asset_mutex = SDL_CreateMutex();
    }
    if (asset_mutex == NULL) {
        return false;
    }
    SDL_LockMutex(asset_mutex);
    return true;
}

static void asset_request_destroy(asset_request_t *request) {
    if (request->stream != NULL) {
        socket_stream_destroy(request->stream);
    }
    if (request->wire_request != NULL) {
        packet_free(request->wire_request);
    }
    EVP_MD_CTX_free(request->digest);
    free(request->key);
    free(request->path);
    free(request->cache_path);
    free(request->data);
    free(request);
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

    uint64_t started = SDL_GetTicksNS();
    char *path = file_path(request->cache_path, "wb");
    bool success = path_write_atomic(path, request->data, request->size, 0600);
    free(path);
    if (success) {
        LOG(DEBUG,
            "Wrote QUIC asset cache %s in %.3f ms",
            request->cache_path,
            (double)(SDL_GetTicksNS() - started) / 1000000.0);
    } else {
        LOG(ERROR,
            "Could not write QUIC asset cache %s in %.3f ms",
            request->cache_path,
            (double)(SDL_GetTicksNS() - started) / 1000000.0);
    }
}

static size_t asset_pending_count(void) {
    size_t count = 0;
    asset_request_t *request, *next;
    HASH_ITER(hh, asset_requests, request, next) {
        if (request->state == ASSET_REQUEST_PENDING && !request->cancelled) {
            count++;
        }
    }
    return count;
}

static asset_request_t *
asset_request_start_internal(const char *path, const char *cache_path, bool metadata_only) {
    if (!cpl.asset_transport || csocket.sc == NULL || !socket_is_quic(csocket.sc) || path == NULL ||
        *path == '\0' || strlen(path) >= MAX_BUF || !asset_lock()) {
        return NULL;
    }

    char key[MAX_BUF + 3];
    snprintf(VS(key), "%c:%s", metadata_only ? 'M' : 'D', path);
    asset_request_t *request;
    HASH_FIND_STR(asset_requests, key, request);
    if (request != NULL) {
        if (request->cancelled) {
            SDL_UnlockMutex(asset_mutex);
            return NULL;
        }
        request->references++;
        SDL_UnlockMutex(asset_mutex);
        return request;
    }
    if (asset_pending_count() >= ASSET_REQUEST_PENDING_MAX) {
        LOG(ERROR, "Refusing QUIC asset %s: pending request limit reached", path);
        SDL_UnlockMutex(asset_mutex);
        return NULL;
    }

    request = xcalloc(1, sizeof(*request));
    request->key = xstrdup(key);
    request->path = xstrdup(path);
    request->cache_path = cache_path != NULL ? xstrdup(cache_path) : NULL;
    request->references = 1;
    request->metadata_only = metadata_only;
    request->state = ASSET_REQUEST_PENDING;
    request->transport_state = ASSET_TRANSPORT_QUEUED;
    asset_request_cache_load(request);

    request->wire_request = packet_new(0, 128, 128);
    static const uint8_t empty_digest[ASSET_DIGEST_SIZE];
    socket_asset_request_append(request->wire_request,
                                request->path,
                                request->cache_loaded ? (uint32_t)request->size : 0,
                                request->cache_loaded ? request->cached_digest : empty_digest,
                                request->metadata_only ? ASSET_REQUEST_METADATA : 0);
    if (!packet_writer_finish(request->wire_request)) {
        asset_request_destroy(request);
        SDL_UnlockMutex(asset_mutex);
        return NULL;
    }

    HASH_ADD_KEYPTR(hh, asset_requests, request->key, strlen(request->key), request);
    LOG(DEBUG, "Queued QUIC asset %s%s", request->path, metadata_only ? " (metadata)" : "");
    SDL_UnlockMutex(asset_mutex);
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

asset_request_state_t asset_request_get_state(asset_request_t *request) {
    if (request == NULL || !asset_lock()) {
        return ASSET_REQUEST_ERROR;
    }
    bool save_cache = request->state == ASSET_REQUEST_COMPLETE && request->cache_needs_save;
    if (save_cache) {
        request->cache_needs_save = false;
        request->references++;
    }
    asset_request_state_t state = request->state;
    SDL_UnlockMutex(asset_mutex);
    if (save_cache) {
        /* The temporary reference keeps this request alive while potentially
         * slow filesystem I/O runs without stalling the transport thread. */
        asset_request_cache_save(request);
        asset_request_free(request);
    }
    return state;
}

const uint8_t *asset_request_get_data(const asset_request_t *request, size_t *size) {
    if (request == NULL || !asset_lock()) {
        if (size != NULL) {
            *size = 0;
        }
        return NULL;
    }
    if (size != NULL) {
        *size = request->size;
    }
    const uint8_t *data = request->state == ASSET_REQUEST_COMPLETE ? request->data : NULL;
    SDL_UnlockMutex(asset_mutex);
    return data;
}

bool asset_request_get_metadata(const asset_request_t *request,
                                size_t *size,
                                uint8_t digest[ASSET_DIGEST_SIZE]) {
    if (request == NULL || !asset_lock()) {
        return false;
    }
    bool valid = request->metadata_only && request->state == ASSET_REQUEST_COMPLETE;
    if (valid && size != NULL) {
        *size = request->size;
    }
    if (valid && digest != NULL) {
        memcpy(digest, request->expected_digest, ASSET_DIGEST_SIZE);
    }
    SDL_UnlockMutex(asset_mutex);
    return valid;
}

void asset_request_free(asset_request_t *request) {
    if (request == NULL || !asset_lock()) {
        return;
    }
    HARD_ASSERT(request->references != 0);
    request->references--;
    if (request->references == 0) {
        if (request->stream != NULL) {
            request->cancelled = true;
        } else {
            HASH_DEL(asset_requests, request);
            asset_request_destroy(request);
        }
    }
    SDL_UnlockMutex(asset_mutex);
}

static void asset_request_fail(asset_request_t *request, const char *reason) {
    LOG(ERROR, "QUIC asset %s failed: %s", request->path, reason);
    if (request->stream != NULL) {
        socket_stream_reset(request->stream, ASSET_STREAM_ERROR_PROTOCOL);
        socket_stream_destroy(request->stream);
        request->stream = NULL;
    }
    EVP_MD_CTX_free(request->digest);
    request->digest = NULL;
    request->state = ASSET_REQUEST_ERROR;
}

static bool asset_request_header(asset_request_t *request) {
    if (!socket_asset_response_parse(request->response_header,
                                     sizeof(request->response_header),
                                     0,
                                     &request->response)) {
        asset_request_fail(request, "malformed response header");
        return false;
    }
    request->size = request->response.total_size;
    memcpy(request->expected_digest, request->response.digest, ASSET_DIGEST_SIZE);

    if (request->response.status == ASSET_STATUS_OK && !request->metadata_only) {
        free(request->data);
        request->data = xmalloc(request->size + 1);
        request->data[0] = '\0';
        request->received = 0;
        request->cache_loaded = false;
        request->digest = EVP_MD_CTX_new();
        if (request->digest == NULL ||
            EVP_DigestInit_ex(request->digest, EVP_sha256(), NULL) != 1) {
            asset_request_fail(request, "could not initialize SHA-256");
            return false;
        }
        request->transport_state =
            request->size == 0 ? ASSET_TRANSPORT_WAIT_FIN : ASSET_TRANSPORT_READ_BODY;
        return true;
    }
    if (request->response.status == ASSET_STATUS_METADATA && request->metadata_only) {
        request->transport_state = ASSET_TRANSPORT_WAIT_FIN;
        return true;
    }
    if (request->response.status == ASSET_STATUS_NOT_MODIFIED && !request->metadata_only &&
        request->cache_loaded && request->response.total_size == request->size &&
        memcmp(request->response.digest, request->cached_digest, ASSET_DIGEST_SIZE) == 0) {
        request->size = request->response.total_size;
        request->transport_state = ASSET_TRANSPORT_WAIT_FIN;
        return true;
    }
    asset_request_fail(request, "server rejected request or returned an invalid status");
    return false;
}

static void asset_request_finish(asset_request_t *request) {
    if (request->response.status == ASSET_STATUS_OK) {
        uint8_t digest[ASSET_DIGEST_SIZE];
        unsigned int digest_size = 0;
        if (request->received != request->size ||
            EVP_DigestFinal_ex(request->digest, digest, &digest_size) != 1 ||
            digest_size != ASSET_DIGEST_SIZE ||
            memcmp(digest, request->expected_digest, ASSET_DIGEST_SIZE) != 0) {
            asset_request_fail(request, "early EOF, declared-size violation, or SHA-256 mismatch");
            return;
        }
        request->data[request->size] = '\0';
        request->cache_needs_save = request->cache_path != NULL;
    }
    EVP_MD_CTX_free(request->digest);
    request->digest = NULL;
    socket_stream_destroy(request->stream);
    request->stream = NULL;
    request->state = ASSET_REQUEST_COMPLETE;
    LOG(DEBUG,
        "Completed QUIC asset %s (%" PRIu64 " bytes)",
        request->path,
        (uint64_t)request->size);
}

static bool asset_request_service(asset_request_t *request) {
    if (request->cancelled) {
        socket_stream_reset(request->stream, ASSET_STREAM_ERROR_CANCELLED);
        socket_stream_destroy(request->stream);
        request->stream = NULL;
        return true;
    }

    if (request->transport_state == ASSET_TRANSPORT_SEND_REQUEST) {
        size_t amount = 0;
        socket_stream_result_t result =
            socket_stream_write(request->stream,
                                request->wire_request->data + request->wire_pos,
                                request->wire_request->len - request->wire_pos,
                                &amount);
        if (result == SOCKET_STREAM_RESULT_ERROR || result == SOCKET_STREAM_RESULT_FINISHED) {
            asset_request_fail(request, "request stream closed while writing");
            return true;
        }
        request->wire_pos += amount;
        network_graph_update(NETWORK_GRAPH_TYPE_ASSET, NETWORK_GRAPH_TRAFFIC_TX, amount);
        if (request->wire_pos == request->wire_request->len) {
            if (!socket_stream_conclude(request->stream)) {
                asset_request_fail(request, "could not conclude request");
                return true;
            }
            packet_free(request->wire_request);
            request->wire_request = NULL;
            request->transport_state = ASSET_TRANSPORT_READ_HEADER;
        }
        return amount != 0;
    }

    uint8_t discard;
    void *buffer = &discard;
    size_t capacity = 1;
    if (request->transport_state == ASSET_TRANSPORT_READ_HEADER) {
        buffer = request->response_header + request->response_header_pos;
        capacity = sizeof(request->response_header) - request->response_header_pos;
    } else if (request->transport_state == ASSET_TRANSPORT_READ_BODY) {
        capacity = MIN((size_t)ASSET_STREAM_QUANTUM, request->size - request->received);
        buffer = request->data + request->received;
    }

    size_t amount = 0;
    socket_stream_result_t result = socket_stream_read(request->stream, buffer, capacity, &amount);
    if (result == SOCKET_STREAM_RESULT_ERROR) {
        asset_request_fail(request, "stream reset or connection error");
        return true;
    }
    if (result == SOCKET_STREAM_RESULT_FINISHED) {
        if (request->transport_state == ASSET_TRANSPORT_WAIT_FIN) {
            asset_request_finish(request);
        } else {
            asset_request_fail(request, "early EOF");
        }
        return true;
    }
    if (amount == 0) {
        return false;
    }
    network_graph_update(NETWORK_GRAPH_TYPE_ASSET, NETWORK_GRAPH_TRAFFIC_RX, amount);

    if (request->transport_state == ASSET_TRANSPORT_READ_HEADER) {
        request->response_header_pos += amount;
        if (request->response_header_pos == sizeof(request->response_header)) {
            asset_request_header(request);
        }
    } else if (request->transport_state == ASSET_TRANSPORT_READ_BODY) {
        if (EVP_DigestUpdate(request->digest, buffer, amount) != 1) {
            asset_request_fail(request, "SHA-256 update failed");
            return true;
        }
        request->received += amount;
        if (request->received == request->size) {
            request->transport_state = ASSET_TRANSPORT_WAIT_FIN;
        }
    } else {
        asset_request_fail(request, "received surplus body bytes");
    }
    return true;
}

bool asset_requests_service(socket_t *sc, bool *pending) {
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(pending != NULL);
    *pending = false;
    if (asset_mutex == NULL) {
        return false;
    }
    SDL_LockMutex(asset_mutex);

    size_t active = 0;
    asset_request_t *request, *next;
    HASH_ITER(hh, asset_requests, request, next) {
        if (request->stream != NULL) {
            active++;
        }
    }
    HASH_ITER(hh, asset_requests, request, next) {
        if (active >= ASSET_STREAM_ACTIVE_MAX) {
            break;
        }
        if (request->state != ASSET_REQUEST_PENDING || request->cancelled ||
            request->transport_state != ASSET_TRANSPORT_QUEUED) {
            continue;
        }
        request->stream = socket_stream_open(sc, SOCKET_STREAM_ASSET);
        if (request->stream == NULL) {
            break;
        }
        request->transport_state = ASSET_TRANSPORT_SEND_REQUEST;
        active++;
    }

    bool progressed = false;
    HASH_ITER(hh, asset_requests, request, next) {
        if (request->stream != NULL) {
            progressed |= asset_request_service(request);
        }
        if (request->cancelled && request->stream == NULL) {
            HASH_DEL(asset_requests, request);
            asset_request_destroy(request);
        }
    }
    *pending = asset_pending_count() != 0;
    SDL_UnlockMutex(asset_mutex);
    return progressed;
}

void asset_requests_disconnect(void) {
    if (asset_mutex == NULL) {
        return;
    }
    SDL_LockMutex(asset_mutex);
    asset_request_t *request, *next;
    HASH_ITER(hh, asset_requests, request, next) {
        if (request->stream != NULL) {
            socket_stream_reset(request->stream, ASSET_STREAM_ERROR_CANCELLED);
            socket_stream_destroy(request->stream);
            request->stream = NULL;
        }
        if (request->state == ASSET_REQUEST_PENDING) {
            request->state = ASSET_REQUEST_ERROR;
        }
        if (request->references == 0) {
            HASH_DEL(asset_requests, request);
            asset_request_destroy(request);
        }
    }
    SDL_UnlockMutex(asset_mutex);
}

void asset_requests_deinit(void) {
    if (asset_mutex == NULL) {
        return;
    }
    SDL_LockMutex(asset_mutex);
    asset_request_t *request, *next;
    HASH_ITER(hh, asset_requests, request, next) {
        HASH_DEL(asset_requests, request);
        asset_request_destroy(request);
    }
    SDL_UnlockMutex(asset_mutex);
    SDL_DestroyMutex(asset_mutex);
    asset_mutex = NULL;
}

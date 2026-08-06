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
 * Serves an immutable, startup-cached game asset snapshot over QUIC.
 */

#include <global.h>
#include <server_main.h>
#include <server.h>
#include <initialization.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <toolkit/datetime.h>
#include <resources.h>
#include <network_metrics.h>
#include <openssl/evp.h>

#define ASSET_CACHE_MAX_TOTAL (1024ULL * 1024ULL * 1024ULL)
#define ASSET_RATE_BYTES_PER_SECOND (8U * 1024U * 1024U)
#define ASSET_RATE_REQUESTS_PER_SECOND 256U
#define ASSET_TOKEN_BUCKET_CAPACITY ASSET_RATE_BYTES_PER_SECOND
#define ASSET_REQUEST_WIRE_MAX (MAX_BUF + 4U + ASSET_DIGEST_SIZE + 1U)
#define ASSET_STREAM_ACCEPT_QUANTUM 16U

typedef struct asset_cache_entry {
    UT_hash_handle hh;
    char *name;
    uint8_t *data;
    uint32_t size;
    uint8_t digest[ASSET_DIGEST_SIZE];
    size_t references;
} asset_cache_entry_t;

typedef enum asset_server_stream_state {
    ASSET_SERVER_READ_REQUEST,
    ASSET_SERVER_SEND_HEADER,
    ASSET_SERVER_SEND_BODY,
} asset_server_stream_state_t;

struct asset_stream_state {
    struct asset_stream_state *next;
    struct asset_stream_state *prev;
    socket_stream_t *stream;
    asset_cache_entry_t *entry;
    asset_server_stream_state_t state;
    uint64_t started_us;
    uint8_t request[ASSET_REQUEST_WIRE_MAX];
    size_t request_size;
    packet_struct *header;
    size_t header_pos;
    size_t body_pos;
    bool concluded;
};

static asset_cache_entry_t *asset_cache;
static uint64_t asset_cache_size;
static size_t asset_cache_rss;

static bool asset_simple_name(const char *name) {
    return name != NULL && *name != '\0' && strchr(name, '/') == NULL &&
           strchr(name, '\\') == NULL && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

static bool asset_resolve_path(const char *asset, char *path, size_t path_size) {
    if (strcmp(asset, "data/listing.txt") == 0) {
        return snprintf(path, path_size, "%s/data/listing.txt", settings.httppath) < (int)path_size;
    }

    if (string_startswith(asset, "data/")) {
        const char *name = asset + sizeof("data/") - 1;
        size_t length = strlen(name);
        if (!asset_simple_name(name) || length <= sizeof(".zz") - 1 ||
            strcmp(name + length - (sizeof(".zz") - 1), ".zz") != 0) {
            return false;
        }
        return snprintf(path, path_size, "%s/data/%s", settings.httppath, name) < (int)path_size;
    }

    if (string_startswith(asset, "resources/")) {
        const char *name = asset + sizeof("resources/") - 1;
        if (resources_find(name) == NULL) {
            return false;
        }
        return snprintf(path, path_size, "%s/%s", settings.resourcespath, name) < (int)path_size;
    }

    if (string_startswith(asset, "client-maps/")) {
        const char *name = asset + sizeof("client-maps/") - 1;
        size_t length = strlen(name);
        bool extension = length > 4 && (strcmp(name + length - 4, ".png") == 0 ||
                                        strcmp(name + length - 4, ".def") == 0);
        if (!asset_simple_name(name) || !extension) {
            return false;
        }
        return snprintf(path, path_size, "%s/client-maps/%s", settings.httppath, name) <
               (int)path_size;
    }

    return false;
}

static bool asset_cache_add(const char *name, const char *path) {
#ifdef O_NOFOLLOW
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    FILE *fp = fd >= 0 ? fdopen(fd, "rb") : NULL;
    if (fd >= 0 && fp == NULL) {
        close(fd);
    }
#else
    FILE *fp = fopen(path, "rb");
#endif
    struct stat sb;
    if (fp == NULL || fstat(fileno(fp), &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_size < 0 ||
        (uint64_t)sb.st_size > ASSET_MAX_SIZE ||
        asset_cache_size + (uint64_t)sb.st_size > ASSET_CACHE_MAX_TOTAL) {
        if (fp != NULL) {
            fclose(fp);
        }
        LOG(ERROR, "Cannot cache game asset %s from %s", name, path);
        return false;
    }

    asset_cache_entry_t *entry = xcalloc(1, sizeof(*entry));
    entry->name = xstrdup(name);
    entry->size = (uint32_t)sb.st_size;
    entry->data = xmalloc(MAX((size_t)entry->size, (size_t)1));
    bool ok = fread(entry->data, 1, entry->size, fp) == entry->size;
    if (fclose(fp) != 0) {
        ok = false;
    }
    unsigned int digest_size = 0;
    ok = ok &&
         EVP_Digest(entry->data, entry->size, entry->digest, &digest_size, EVP_sha256(), NULL) ==
             1 &&
         digest_size == ASSET_DIGEST_SIZE;
    if (!ok) {
        LOG(ERROR, "Cannot read or hash game asset %s from %s", name, path);
        free(entry->data);
        free(entry->name);
        free(entry);
        return false;
    }

    HASH_ADD_KEYPTR(hh, asset_cache, entry->name, strlen(entry->name), entry);
    asset_cache_size += entry->size;
    asset_cache_rss +=
        sizeof(*entry) + strlen(entry->name) + 1 + MAX((size_t)entry->size, (size_t)1);
    return true;
}

static void
asset_cache_directory(const char *root, const char *relative, const char *prefix, bool recursive) {
    char directory[HUGE_BUF];
    int length = snprintf(VS(directory), "%s%s%s", root, *relative != '\0' ? "/" : "", relative);
    if (length < 0 || (size_t)length >= sizeof(directory)) {
        return;
    }
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        return;
    }

    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
        if (item->d_name[0] == '.') {
            continue;
        }
        char child_relative[HUGE_BUF];
        length = snprintf(VS(child_relative),
                          "%s%s%s",
                          relative,
                          *relative != '\0' ? "/" : "",
                          item->d_name);
        if (length < 0 || (size_t)length >= sizeof(child_relative)) {
            continue;
        }
        char path[HUGE_BUF];
        length = snprintf(VS(path), "%s/%s", root, child_relative);
        if (length < 0 || (size_t)length >= sizeof(path)) {
            continue;
        }
        struct stat sb;
        if (stat(path, &sb) != 0) {
            continue;
        }
        if (S_ISDIR(sb.st_mode) && recursive) {
            asset_cache_directory(root, child_relative, prefix, true);
            continue;
        }
        if (!S_ISREG(sb.st_mode)) {
            continue;
        }

        char name[HUGE_BUF];
        length = snprintf(VS(name), "%s/%s", prefix, child_relative);
        if (length < 0 || (size_t)length >= sizeof(name)) {
            continue;
        }
        char resolved[HUGE_BUF];
        if (asset_resolve_path(name, VS(resolved))) {
            asset_cache_add(name, resolved);
        }
    }
    closedir(dir);
}

void socket_assets_init(void) {
    char path[HUGE_BUF];
    snprintf(VS(path), "%s/data", settings.httppath);
    asset_cache_directory(path, "", "data", false);
    asset_cache_directory(settings.resourcespath, "", "resources", true);
    snprintf(VS(path), "%s/client-maps", settings.httppath);
    asset_cache_directory(path, "", "client-maps", false);
    LOG(INFO, "Cached %" PRIu64 " bytes of game assets in memory", asset_cache_size);
    server_metrics_asset_cache(asset_cache_rss);
}

void socket_assets_deinit(void) {
    asset_cache_entry_t *entry, *next;
    HASH_ITER(hh, asset_cache, entry, next) {
        HARD_ASSERT(entry->references == 0);
        HASH_DEL(asset_cache, entry);
        free(entry->data);
        free(entry->name);
        free(entry);
    }
    asset_cache_size = 0;
    asset_cache_rss = 0;
    server_metrics_asset_cache(0);
}

static asset_cache_entry_t *asset_cache_find(const char *name) {
    asset_cache_entry_t *entry;
    HASH_FIND_STR(asset_cache, name, entry);
    return entry;
}

static bool asset_request_rate_allow(socket_struct *ns) {
    uint64_t now = datetime_monotonic_ms();
    if (ns->asset_request_window_ms == 0 || now - ns->asset_request_window_ms >= 1000) {
        ns->asset_request_window_ms = now;
        ns->asset_window_requests = 0;
    }
    if (ns->asset_window_requests >= ASSET_RATE_REQUESTS_PER_SECOND) {
        LOG(ERROR,
            "Connection %s exceeded the in-band asset request-rate limit",
            socket_get_id(ns->sc));
        ns->state = ST_ZOMBIE;
        return false;
    }
    ns->asset_window_requests++;
    return true;
}

static size_t asset_tokens_available(socket_struct *ns) {
    uint64_t now = datetime_monotonic_ms();
    if (ns->asset_token_updated_ms == 0) {
        ns->asset_token_updated_ms = now;
        ns->asset_tokens = ASSET_TOKEN_BUCKET_CAPACITY;
        return ns->asset_tokens;
    }
    uint64_t elapsed = now - ns->asset_token_updated_ms;
    if (elapsed != 0) {
        if (elapsed >= 1000U) {
            ns->asset_tokens = ASSET_TOKEN_BUCKET_CAPACITY;
        } else {
            uint64_t refill = elapsed * ASSET_RATE_BYTES_PER_SECOND / 1000U;
            ns->asset_tokens =
                (size_t)MIN((uint64_t)ASSET_TOKEN_BUCKET_CAPACITY, ns->asset_tokens + refill);
        }
        ns->asset_token_updated_ms = now;
    }
    return ns->asset_tokens;
}

static void
asset_stream_free(socket_struct *ns, asset_stream_state_t *state, bool reset, bool rejected) {
    if (reset) {
        socket_stream_reset(state->stream, SOCKET_STREAM_ERROR_SERVER_PROTOCOL);
    }
    socket_stream_destroy(state->stream);
    if (state->entry != NULL) {
        HARD_ASSERT(state->entry->references != 0);
        state->entry->references--;
    }
    if (state->header != NULL) {
        packet_free(state->header);
    }
    DL_DELETE(ns->asset_streams, state);
    HARD_ASSERT(ns->asset_stream_count != 0);
    ns->asset_stream_count--;
    server_metrics_asset_stream(-1, 0, rejected);
    free(state);
}

static void asset_stream_header(asset_stream_state_t *state,
                                uint8_t status,
                                uint32_t total_size,
                                const uint8_t digest[ASSET_DIGEST_SIZE]) {
    state->header = packet_new(0, SOCKET_ASSET_RESPONSE_HEADER_SIZE, 0);
    socket_asset_response_append_status(state->header, status, total_size, digest);
    HARD_ASSERT(packet_writer_finish(state->header));
    state->state = ASSET_SERVER_SEND_HEADER;
}

static bool asset_stream_prepare(socket_struct *ns, asset_stream_state_t *state) {
    socket_asset_request_t request;
    if (!socket_asset_request_parse(state->request, state->request_size, 0, &request)) {
        LOG(ERROR, "Connection %s sent a malformed QUIC asset request", socket_get_id(ns->sc));
        return false;
    }
    if (!asset_request_rate_allow(ns)) {
        return false;
    }
    if (*settings.join_password != '\0' && !ns->join_authenticated) {
        LOG(ERROR,
            "Connection %s opened an asset stream before authentication",
            socket_get_id(ns->sc));
        return false;
    }

    char resolved[HUGE_BUF];
    asset_cache_entry_t *entry = NULL;
    if (asset_resolve_path(request.path, VS(resolved))) {
        entry = asset_cache_find(request.path);
    }
    if (entry == NULL) {
        asset_stream_header(state,
                            request.flags & ASSET_REQUEST_METADATA ? ASSET_STATUS_METADATA_NOT_FOUND
                                                                   : ASSET_STATUS_NOT_FOUND,
                            0,
                            NULL);
    } else if (request.flags & ASSET_REQUEST_METADATA) {
        asset_stream_header(state, ASSET_STATUS_METADATA, entry->size, entry->digest);
    } else if (request.cached_size == entry->size &&
               memcmp(request.cached_digest, entry->digest, ASSET_DIGEST_SIZE) == 0) {
        asset_stream_header(state, ASSET_STATUS_NOT_MODIFIED, entry->size, entry->digest);
    } else {
        state->entry = entry;
        entry->references++;
        asset_stream_header(state, ASSET_STATUS_OK, entry->size, entry->digest);
    }
    LOG(DEBUG,
        "Connection %s opened QUIC asset stream for %s",
        socket_get_id(ns->sc),
        request.path);
    return true;
}

static bool asset_stream_read_request(socket_struct *ns, asset_stream_state_t *state) {
    uint8_t surplus;
    void *buffer = &surplus;
    size_t capacity = 1;
    if (state->request_size < sizeof(state->request)) {
        buffer = state->request + state->request_size;
        capacity = sizeof(state->request) - state->request_size;
    }
    size_t amount = 0;
    socket_stream_result_t result = socket_stream_read(state->stream, buffer, capacity, &amount);
    if (result == SOCKET_STREAM_RESULT_ERROR) {
        return false;
    }
    if (result == SOCKET_STREAM_RESULT_FINISHED) {
        return asset_stream_prepare(ns, state);
    }
    if (state->request_size == sizeof(state->request) && amount != 0) {
        return false;
    }
    state->request_size += amount;
    return true;
}

static bool asset_stream_write(socket_struct *ns, asset_stream_state_t *state) {
    const uint8_t *data;
    size_t remaining;
    if (state->state == ASSET_SERVER_SEND_HEADER) {
        data = state->header->data + state->header_pos;
        remaining = state->header->len - state->header_pos;
    } else {
        HARD_ASSERT(state->entry != NULL);
        data = state->entry->data + state->body_pos;
        remaining = state->entry->size - state->body_pos;
        size_t tokens = asset_tokens_available(ns);
        if (tokens == 0) {
            server_metrics_asset_paced();
            return true;
        }
        remaining = MIN(remaining, MIN((size_t)ASSET_STREAM_QUANTUM, tokens));
    }

    size_t amount = 0;
    socket_stream_result_t result = socket_stream_write(state->stream, data, remaining, &amount);
    if (result == SOCKET_STREAM_RESULT_ERROR || result == SOCKET_STREAM_RESULT_FINISHED) {
        return false;
    }
    if (state->state == ASSET_SERVER_SEND_HEADER) {
        state->header_pos += amount;
        if (state->header_pos == state->header->len) {
            server_metrics_asset_response(datetime_monotonic_us() - state->started_us);
            packet_free(state->header);
            state->header = NULL;
            if (state->entry == NULL || state->entry->size == 0) {
                state->concluded = socket_stream_conclude(state->stream);
                return false;
            }
            state->state = ASSET_SERVER_SEND_BODY;
        }
    } else {
        HARD_ASSERT(ns->asset_tokens >= amount);
        ns->asset_tokens -= amount;
        state->body_pos += amount;
        server_metrics_asset_stream(0, amount, false);
        if (state->body_pos == state->entry->size) {
            state->concluded = socket_stream_conclude(state->stream);
            return false;
        }
    }
    return true;
}

static void asset_stream_accept(socket_struct *ns) {
    for (size_t accepted = 0; accepted < ASSET_STREAM_ACCEPT_QUANTUM; accepted++) {
        socket_stream_t *stream = socket_stream_accept(ns->sc, SOCKET_STREAM_ASSET);
        if (stream == NULL) {
            break;
        }
        if (ns->asset_stream_count >= ASSET_STREAM_ACTIVE_MAX) {
            LOG(ERROR,
                "Connection %s exceeded the active asset-stream limit",
                socket_get_id(ns->sc));
            socket_stream_reset(stream, SOCKET_STREAM_ERROR_LIMIT);
            socket_stream_destroy(stream);
            server_metrics_asset_stream(0, 0, true);
            continue;
        }
        asset_stream_state_t *state = xcalloc(1, sizeof(*state));
        state->stream = stream;
        state->started_us = datetime_monotonic_us();
        DL_APPEND(ns->asset_streams, state);
        ns->asset_stream_count++;
        server_metrics_asset_stream(1, 0, false);
    }
}

bool socket_assets_service(socket_struct *ns) {
    HARD_ASSERT(ns != NULL);
    if (!socket_is_quic(ns->sc)) {
        return true;
    }
    asset_stream_accept(ns);

    asset_stream_state_t *state, *next;
    DL_FOREACH_SAFE(ns->asset_streams, state, next) {
        bool keep = state->state == ASSET_SERVER_READ_REQUEST ? asset_stream_read_request(ns, state)
                                                              : asset_stream_write(ns, state);
        if (!keep) {
            asset_stream_free(ns, state, !state->concluded, !state->concluded);
        }
    }
    if (ns->asset_streams != NULL && ns->asset_streams->next != NULL) {
        asset_stream_state_t *first = ns->asset_streams;
        DL_DELETE(ns->asset_streams, first);
        DL_APPEND(ns->asset_streams, first);
    }
    return ns->state != ST_DEAD && ns->state != ST_ZOMBIE;
}

bool socket_assets_pending(const socket_struct *ns) {
    HARD_ASSERT(ns != NULL);
    asset_stream_state_t *state;
    DL_FOREACH(ns->asset_streams, state) {
        if (state->state != ASSET_SERVER_READ_REQUEST) {
            return true;
        }
    }
    return false;
}

void socket_assets_connection_clear(socket_struct *ns) {
    asset_stream_state_t *state, *next;
    DL_FOREACH_SAFE(ns->asset_streams, state, next) {
        asset_stream_free(ns, state, true, false);
    }
}

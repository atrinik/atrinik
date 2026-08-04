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
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <toolkit/datetime.h>
#include <resources.h>
#include <network_metrics.h>
#include <openssl/evp.h>

#define ASSET_CACHE_MAX_TOTAL (1024ULL * 1024ULL * 1024ULL)
#define ASSET_RATE_BYTES_PER_SECOND (8U * 1024U * 1024U)
#define ASSET_RATE_REQUESTS_PER_SECOND 256U

typedef struct asset_cache_entry {
    UT_hash_handle hh;
    char *name;
    uint8_t *data;
    uint32_t size;
    uint8_t digest[ASSET_DIGEST_SIZE];
} asset_cache_entry_t;

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

static bool asset_rate_allow(socket_struct *ns, size_t bytes, bool count_request) {
    uint64_t now = datetime_monotonic_ms();
    if (ns->asset_window_ms == 0 || now - ns->asset_window_ms >= 1000) {
        ns->asset_window_ms = now;
        ns->asset_window_bytes = 0;
        ns->asset_window_requests = 0;
    }
    if ((count_request && ns->asset_window_requests >= ASSET_RATE_REQUESTS_PER_SECOND) ||
        ns->asset_window_bytes + bytes > ASSET_RATE_BYTES_PER_SECOND) {
        LOG(ERROR,
            "Connection %s exceeded the in-band asset transfer budget",
            socket_get_id(ns->sc));
        server_metrics_asset_response(0, true);
        ns->state = ST_ZOMBIE;
        return false;
    }
    if (count_request) {
        ns->asset_window_requests++;
    }
    ns->asset_window_bytes += bytes;
    return true;
}

static void asset_send_error(socket_struct *ns, const char *asset, bool metadata) {
    packet_struct *packet = packet_new(CLIENT_CMD_ASSET, 128, 128);
    socket_asset_response_append_status(packet,
                                        metadata ? ASSET_STATUS_METADATA_NOT_FOUND
                                                 : ASSET_STATUS_NOT_FOUND,
                                        asset);
    socket_send_packet(ns, packet);
}

void socket_command_asset(socket_struct *ns, player *pl, uint8_t *data, size_t len, size_t pos) {
    (void)pl;
    uint64_t started_us = datetime_monotonic_us();

    if (!socket_is_quic(ns->sc) || (*settings.join_password != '\0' && !ns->join_authenticated)) {
        return;
    }

    socket_asset_request_t request;
    if (!socket_asset_request_parse(data, len, pos, &request)) {
        LOG(ERROR, "Connection %s sent a malformed QUIC asset request", socket_get_id(ns->sc));
        return;
    }
    if (!asset_rate_allow(ns, 0, true)) {
        return;
    }
    LOG(DEBUG,
        "Connection %s requested QUIC asset %s at offset %" PRIu32 "%s",
        socket_get_id(ns->sc),
        request.path,
        request.offset,
        request.flags & ASSET_REQUEST_METADATA ? " (metadata)" : "");

    char path[HUGE_BUF];
    if (!asset_resolve_path(request.path, VS(path))) {
        asset_send_error(ns, request.path, request.flags & ASSET_REQUEST_METADATA);
        server_metrics_asset_response(datetime_monotonic_us() - started_us, false);
        return;
    }

    asset_cache_entry_t *entry = asset_cache_find(request.path);
    if (entry == NULL || request.offset > entry->size) {
        asset_send_error(ns, request.path, request.flags & ASSET_REQUEST_METADATA);
        server_metrics_asset_response(datetime_monotonic_us() - started_us, false);
        return;
    }

    if (request.flags & ASSET_REQUEST_METADATA) {
        packet_struct *packet = packet_new(CLIENT_CMD_ASSET, 128, 128);
        socket_asset_response_append_metadata(packet, request.path, entry->size, entry->digest);
        socket_send_packet(ns, packet);
        server_metrics_asset_response(datetime_monotonic_us() - started_us, false);
        return;
    }

    if (request.offset == 0 && request.cached_size == entry->size &&
        memcmp(request.cached_digest, entry->digest, ASSET_DIGEST_SIZE) == 0) {
        packet_struct *packet = packet_new(CLIENT_CMD_ASSET, 128, 128);
        socket_asset_response_append_status(packet, ASSET_STATUS_NOT_MODIFIED, request.path);
        socket_send_packet(ns, packet);
        server_metrics_asset_response(datetime_monotonic_us() - started_us, false);
        return;
    }

    size_t chunk_size = MIN((size_t)(entry->size - request.offset), (size_t)ASSET_CHUNK_SIZE);
    if (!asset_rate_allow(ns, chunk_size, false)) {
        return;
    }
    if (!socket_buffer_can_enqueue(ns, chunk_size + 256, true)) {
        server_metrics_asset_response(0, true);
        LOG(ERROR, "Connection %s exceeded the bulk-asset queue reserve", socket_get_id(ns->sc));
        ns->state = ST_ZOMBIE;
        return;
    }

    packet_struct *packet = packet_new(CLIENT_CMD_ASSET, chunk_size + 128, 128);
    socket_asset_response_append_ok(packet,
                                    request.path,
                                    entry->size,
                                    request.offset,
                                    entry->digest,
                                    entry->data + request.offset,
                                    chunk_size);
    socket_send_packet(ns, packet);
    server_metrics_asset_response(datetime_monotonic_us() - started_us, false);
}

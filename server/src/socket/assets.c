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
 * Serves cached game assets over an established QUIC connection.
 */

#include <global.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>
#include <resources.h>
#include <zlib.h>

static bool
asset_simple_name (const char *name)
{
    return name != NULL && *name != '\0' &&
           strchr(name, '/') == NULL &&
           strchr(name, '\\') == NULL &&
           strcmp(name, ".") != 0 &&
           strcmp(name, "..") != 0;
}

static bool
asset_resolve_path (const char *asset, char *path, size_t path_size)
{
    if (strcmp(asset, "data/listing.txt") == 0) {
        return snprintf(path,
                        path_size,
                        "%s/data/listing.txt",
                        settings.httppath) < (int) path_size;
    }

    if (string_startswith(asset, "data/")) {
        const char *name = asset + sizeof("data/") - 1;
        size_t length = strlen(name);
        if (!asset_simple_name(name) ||
            length <= sizeof(".zz") - 1 ||
            strcmp(name + length - (sizeof(".zz") - 1), ".zz") != 0) {
            return false;
        }
        return snprintf(path,
                        path_size,
                        "%s/data/%s",
                        settings.httppath,
                        name) < (int) path_size;
    }

    if (string_startswith(asset, "resources/")) {
        const char *name = asset + sizeof("resources/") - 1;
        if (resources_find(name) == NULL) {
            return false;
        }
        return snprintf(path,
                        path_size,
                        "%s/%s",
                        settings.resourcespath,
                        name) < (int) path_size;
    }

    if (string_startswith(asset, "client-maps/")) {
        const char *name = asset + sizeof("client-maps/") - 1;
        size_t length = strlen(name);
        bool extension = length > 4 &&
            (strcmp(name + length - 4, ".png") == 0 ||
             strcmp(name + length - 4, ".def") == 0);
        if (!asset_simple_name(name) || !extension) {
            return false;
        }
        return snprintf(path,
                        path_size,
                        "%s/client-maps/%s",
                        settings.httppath,
                        name) < (int) path_size;
    }

    return false;
}

static void
asset_send_error (socket_struct *ns, const char *asset)
{
    packet_struct *packet = packet_new(CLIENT_CMD_ASSET, 128, 128);
    socket_asset_response_append_status(packet,
                                        ASSET_STATUS_NOT_FOUND,
                                        asset);
    socket_send_packet(ns, packet);
}

static bool
asset_checksum (FILE *fp, uint32_t size, uint32_t *checksum)
{
    uint8_t buffer[8192];
    uLong value = 1L;

    if (fseek(fp, 0, SEEK_SET) != 0) {
        return false;
    }

    uint32_t remaining = size;
    while (remaining != 0) {
        size_t requested = MIN((size_t) remaining, sizeof(buffer));
        size_t length = fread(buffer, 1, requested, fp);
        if (length != requested) {
            return false;
        }
        value = crc32(value,
                      (const unsigned char FAR *) buffer,
                      (uInt) length);
        remaining -= (uint32_t) length;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return false;
    }

    *checksum = (uint32_t) value;
    return true;
}

void
socket_command_asset (socket_struct *ns,
                      player        *pl,
                      uint8_t       *data,
                      size_t         len,
                      size_t         pos)
{
    if (!socket_is_quic(ns->sc) ||
        (*settings.join_password != '\0' && !ns->join_authenticated)) {
        return;
    }

    socket_asset_request_t request;
    if (!socket_asset_request_parse(data, len, pos, &request)) {
        LOG(ERROR,
            "Connection %s sent a malformed QUIC asset request",
            socket_get_id(ns->sc));
        return;
    }
    LOG(DEBUG,
        "Connection %s requested QUIC asset %s at offset %" PRIu32
        " (cached size %" PRIu32 ", CRC32 %" PRIu32 ")",
        socket_get_id(ns->sc),
        request.path,
        request.offset,
        request.cached_size,
        request.cached_checksum);

    char path[HUGE_BUF];
    if (!asset_resolve_path(request.path, VS(path))) {
        asset_send_error(ns, request.path);
        return;
    }

    FILE *fp = fopen(path, "rb");
    struct stat sb;
    if (fp == NULL ||
        fstat(fileno(fp), &sb) != 0 ||
        !S_ISREG(sb.st_mode) ||
        sb.st_size < 0 ||
        (uint64_t) sb.st_size > ASSET_MAX_SIZE ||
        request.offset > (uint32_t) sb.st_size) {
        if (fp != NULL) {
            fclose(fp);
        }
        asset_send_error(ns, request.path);
        return;
    }

    uint32_t checksum = 0;
    if (request.offset == 0 &&
        !asset_checksum(fp, (uint32_t) sb.st_size, &checksum)) {
        fclose(fp);
        asset_send_error(ns, request.path);
        return;
    }

    if (request.offset == 0 &&
        request.cached_size == (uint32_t) sb.st_size &&
        request.cached_checksum == checksum) {
        fclose(fp);
        packet_struct *packet = packet_new(CLIENT_CMD_ASSET, 128, 128);
        socket_asset_response_append_status(packet,
                                            ASSET_STATUS_NOT_MODIFIED,
                                            request.path);
        LOG(DEBUG,
            "Connection %s confirmed cached QUIC asset %s (size %" PRIu32
            ", CRC32 %" PRIu32 ")",
            socket_get_id(ns->sc),
            request.path,
            request.cached_size,
            request.cached_checksum);
        socket_send_packet(ns, packet);
        return;
    }

    if (fseek(fp, (long) request.offset, SEEK_SET) != 0) {
        fclose(fp);
        asset_send_error(ns, request.path);
        return;
    }

    uint8_t chunk[ASSET_CHUNK_SIZE];
    size_t chunk_size = fread(chunk, 1, sizeof(chunk), fp);
    if (ferror(fp)) {
        fclose(fp);
        asset_send_error(ns, request.path);
        return;
    }
    fclose(fp);

    packet_struct *packet =
        packet_new(CLIENT_CMD_ASSET, chunk_size + 128, 128);
    socket_asset_response_append_ok(packet,
                                    request.path,
                                    (uint32_t) sb.st_size,
                                    request.offset,
                                    checksum,
                                    chunk,
                                    chunk_size);
    LOG(DEBUG,
        "Connection %s sending QUIC asset %s offset %" PRIu32 "/%" PRIu32
        " (%" PRIu64 " bytes)",
        socket_get_id(ns->sc),
        request.path,
        request.offset,
        (uint32_t) sb.st_size,
        (uint64_t) chunk_size);
    socket_send_packet(ns, packet);
}

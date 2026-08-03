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
    packet_append_uint8(packet, ASSET_STATUS_NOT_FOUND);
    packet_append_string_terminated(packet, asset);
    socket_send_packet(ns, packet);
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

    char asset[MAX_BUF];
    if (packet_to_string(data, len, &pos, VS(asset)) == NULL) {
        return;
    }
    uint32_t offset = packet_to_uint32(data, len, &pos);
    LOG(DEBUG,
        "Connection %s requested QUIC asset %s at offset %" PRIu32,
        socket_get_id(ns->sc),
        asset,
        offset);

    char path[HUGE_BUF];
    if (!asset_resolve_path(asset, VS(path))) {
        asset_send_error(ns, asset);
        return;
    }

    FILE *fp = fopen(path, "rb");
    struct stat sb;
    if (fp == NULL ||
        fstat(fileno(fp), &sb) != 0 ||
        !S_ISREG(sb.st_mode) ||
        sb.st_size < 0 ||
        (uint64_t) sb.st_size > UINT32_MAX ||
        offset > (uint32_t) sb.st_size ||
        fseek(fp, (long) offset, SEEK_SET) != 0) {
        if (fp != NULL) {
            fclose(fp);
        }
        asset_send_error(ns, asset);
        return;
    }

    uint8_t chunk[ASSET_CHUNK_SIZE];
    size_t chunk_size = fread(chunk, 1, sizeof(chunk), fp);
    if (ferror(fp)) {
        fclose(fp);
        asset_send_error(ns, asset);
        return;
    }
    fclose(fp);

    packet_struct *packet =
        packet_new(CLIENT_CMD_ASSET, chunk_size + 128, 128);
    packet_append_uint8(packet, ASSET_STATUS_OK);
    packet_append_string_terminated(packet, asset);
    packet_append_uint32(packet, (uint32_t) sb.st_size);
    packet_append_uint32(packet, offset);
    packet_append_data_len(packet, chunk, chunk_size);
    LOG(DEBUG,
        "Connection %s sending QUIC asset %s offset %" PRIu32 "/%" PRIu32
        " (%" PRIu64 " bytes)",
        socket_get_id(ns->sc),
        asset,
        offset,
        (uint32_t) sb.st_size,
        (uint64_t) chunk_size);
    socket_send_packet(ns, packet);
}

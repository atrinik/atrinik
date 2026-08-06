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

/** @file Validate the framed CLIENT_CMD_MAP wire format. */

#include "map_protocol.h"
#include "packet.h"
#include "socket.h"

typedef packet_reader_t map_packet_reader_t;

/** Advance a MAP validation cursor without reading beyond its packet. */
static bool map_packet_skip(map_packet_reader_t *reader, size_t size) {
    return packet_reader_skip(reader, size);
}

/** Read an unsigned byte from a MAP validation cursor. */
static bool map_packet_read_uint8(map_packet_reader_t *reader, uint8_t *value) {
    *value = packet_reader_read_uint8(reader);
    return packet_reader_error(reader) == PACKET_ERROR_NONE;
}

/** Read a signed byte from a MAP validation cursor. */
static bool map_packet_read_int8(map_packet_reader_t *reader, int8_t *value) {
    *value = packet_reader_read_int8(reader);
    return packet_reader_error(reader) == PACKET_ERROR_NONE;
}

/** Read a network-order uint16 from a MAP validation cursor. */
static bool map_packet_read_uint16(map_packet_reader_t *reader, uint16_t *value) {
    *value = packet_reader_read_uint16(reader);
    return packet_reader_error(reader) == PACKET_ERROR_NONE;
}

/** Read a network-order uint32 from a MAP validation cursor. */
static bool map_packet_read_uint32(map_packet_reader_t *reader, uint32_t *value) {
    *value = packet_reader_read_uint32(reader);
    return packet_reader_error(reader) == PACKET_ERROR_NONE;
}

/** Skip one required NUL-terminated string in a MAP command. */
static bool map_packet_skip_string(map_packet_reader_t *reader) {
    (void)packet_reader_read_string_view(reader, PACKET_PAYLOAD_MAX);
    return packet_reader_error(reader) == PACKET_ERROR_NONE;
}

/** Validate one framed MAP level without changing client state. */
static bool
socket_command_map_validate_level(map_packet_reader_t *reader, int wire_width, int wire_height) {
    const uint16_t known_mask = MAP2_MASK_SUPPORT_HEIGHT | MAP2_MASK_CLEAR | MAP2_MASK_HARD_CLEAR |
                                MAP2_MASK_LIGHT_LEVEL | MAP2_MASK_LIGHT_LEVEL_MORE | MAP2_MASK_FOW;
    const uint32_t known_flags2 = MAP2_FLAG2_ALPHA | MAP2_FLAG2_ROTATE | MAP2_FLAG2_ZOOM |
                                  MAP2_FLAG2_TARGET | MAP2_FLAG2_PROBE | MAP2_FLAG2_PRIORITY |
                                  MAP2_FLAG2_SECONDPASS | MAP2_FLAG2_GLOW | MAP2_FLAG2_ROOF |
                                  MAP2_FLAG2_DOOR;

    while (reader->pos < reader->len) {
        uint16_t mask;

        if (!map_packet_read_uint16(reader, &mask)) {
            return false;
        }

        int x = (mask >> 11) & 0x1f;
        int y = (mask >> 6) & 0x1f;
        uint16_t values = mask & 0x3f;
        if (x >= wire_width || y >= wire_height || (values & ~known_mask) != 0) {
            return false;
        }

        if (values & MAP2_MASK_CLEAR) {
            if ((values & ~(MAP2_MASK_CLEAR | MAP2_MASK_HARD_CLEAR)) != 0) {
                return false;
            }

            continue;
        }

        if (values & MAP2_MASK_HARD_CLEAR) {
            return false;
        }

        if ((values & MAP2_MASK_SUPPORT_HEIGHT) && !map_packet_skip(reader, sizeof(int16_t))) {
            return false;
        }

        if (values & MAP2_MASK_FOW) {
            uint8_t fow;

            if (!map_packet_read_uint8(reader, &fow) || fow > 1) {
                return false;
            }
        }
        if ((values & MAP2_MASK_LIGHT_LEVEL) && !map_packet_skip(reader, sizeof(uint8_t))) {
            return false;
        }
        if ((values & MAP2_MASK_LIGHT_LEVEL_MORE) &&
            !map_packet_skip(reader, MAP2_PROTOCOL_SUB_LAYERS - 1)) {
            return false;
        }

        uint8_t num_layers;
        if (!map_packet_read_uint8(reader, &num_layers) || num_layers > MAP2_PROTOCOL_REAL_LAYERS) {
            return false;
        }

        bool seen_layers[MAP2_PROTOCOL_REAL_LAYERS] = {0};
        for (uint8_t i = 0; i < num_layers; i++) {
            uint8_t type;

            if (!map_packet_read_uint8(reader, &type)) {
                return false;
            }

            uint8_t layer = type;
            if (type == MAP2_LAYER_CLEAR && !map_packet_read_uint8(reader, &layer)) {
                return false;
            }
            if (layer >= MAP2_PROTOCOL_REAL_LAYERS || seen_layers[layer]) {
                return false;
            }
            seen_layers[layer] = true;

            if (type == MAP2_LAYER_CLEAR) {
                continue;
            }

            uint8_t flags;
            if (!map_packet_skip(reader, sizeof(uint16_t) + sizeof(uint8_t)) ||
                !map_packet_read_uint8(reader, &flags)) {
                return false;
            }

            if ((flags & MAP2_FLAG_MULTI) && !map_packet_skip(reader, sizeof(uint8_t))) {
                return false;
            }
            if ((flags & MAP2_FLAG_NAME) &&
                (!map_packet_skip_string(reader) || !map_packet_skip_string(reader))) {
                return false;
            }
            if (flags & MAP2_FLAG_ANIMATION) {
                uint8_t anim_flags;

                if (!map_packet_skip(reader, 2) || !map_packet_read_uint8(reader, &anim_flags) ||
                    (anim_flags & ~(ANIM_FLAG_MOVING | ANIM_FLAG_ATTACKING | ANIM_FLAG_STOP_MOVING |
                                    ANIM_FLAG_STOP_ATTACKING)) != 0 ||
                    ((anim_flags & ANIM_FLAG_MOVING) &&
                     !map_packet_skip(reader, sizeof(uint8_t)))) {
                    return false;
                }
            }
            if ((flags & MAP2_FLAG_HEIGHT) && !map_packet_skip(reader, sizeof(int16_t))) {
                return false;
            }
            if ((flags & MAP2_FLAG_ALIGN) && !map_packet_skip(reader, sizeof(int16_t))) {
                return false;
            }

            if (flags & MAP2_FLAG_MORE) {
                uint32_t flags2;

                if (!map_packet_read_uint32(reader, &flags2) || (flags2 & ~known_flags2) != 0) {
                    return false;
                }
                if ((flags2 & MAP2_FLAG2_ALPHA) && !map_packet_skip(reader, sizeof(uint8_t))) {
                    return false;
                }
                if ((flags2 & MAP2_FLAG2_ROTATE) && !map_packet_skip(reader, sizeof(int16_t))) {
                    return false;
                }
                if ((flags2 & MAP2_FLAG2_ZOOM) && !map_packet_skip(reader, sizeof(uint16_t) * 2)) {
                    return false;
                }
                if (flags2 & MAP2_FLAG2_TARGET) {
                    uint8_t target_is_friend;

                    if (!map_packet_skip(reader, sizeof(uint32_t)) ||
                        !map_packet_read_uint8(reader, &target_is_friend) || target_is_friend > 1) {
                        return false;
                    }
                }
                if (flags2 & MAP2_FLAG2_PROBE) {
                    uint8_t probe;

                    if (!map_packet_read_uint8(reader, &probe) || probe > 100) {
                        return false;
                    }
                }
                if ((flags2 & MAP2_FLAG2_GLOW) && (!map_packet_skip_string(reader) ||
                                                   !map_packet_skip(reader, sizeof(uint8_t)))) {
                    return false;
                }
            }
        }

        uint8_t ext_flags;
        if (!map_packet_read_uint8(reader, &ext_flags) || (ext_flags & ~MAP2_FLAG_EXT_ANIM) != 0) {
            return false;
        }

        if (ext_flags & MAP2_FLAG_EXT_ANIM) {
            uint8_t anim_num;
            bool seen_sub_layers[MAP2_PROTOCOL_SUB_LAYERS] = {0};

            if (!map_packet_read_uint8(reader, &anim_num) || anim_num > MAP2_PROTOCOL_SUB_LAYERS) {
                return false;
            }

            for (uint8_t i = 0; i < anim_num; i++) {
                uint8_t sub_layer, anim_type;

                if (!map_packet_read_uint8(reader, &sub_layer) ||
                    !map_packet_read_uint8(reader, &anim_type) ||
                    sub_layer >= MAP2_PROTOCOL_SUB_LAYERS || seen_sub_layers[sub_layer] ||
                    (anim_type != ANIM_DAMAGE && anim_type != ANIM_KILL) ||
                    !map_packet_skip(reader, sizeof(int16_t))) {
                    return false;
                }
                seen_sub_layers[sub_layer] = true;
            }
        }
    }

    return reader->pos == reader->len;
}

/** Validate a complete MAP command before any metadata or cache mutation. */
bool map_protocol_validate(const uint8_t *data,
                           size_t len,
                           size_t pos,
                           int map_width_limit,
                           int map_height_limit) {
    if (data == NULL || pos > len || map_width_limit <= 0 || map_width_limit > 32 ||
        map_height_limit <= 0 || map_height_limit > 32) {
        return false;
    }

    map_packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);
    uint8_t mapstat;
    int new_map_width = 0, new_map_height = 0;

    if (!map_packet_read_uint8(&reader, &mapstat) || mapstat > MAP_UPDATE_CMD_CONNECTED) {
        return false;
    }

    if (mapstat != MAP_UPDATE_CMD_SAME) {
        uint8_t height_diff, region_has_map;

        if (!map_packet_skip_string(&reader) || !map_packet_skip_string(&reader) ||
            !map_packet_skip_string(&reader) || !map_packet_read_uint8(&reader, &height_diff) ||
            height_diff > 1 || !map_packet_read_uint8(&reader, &region_has_map) ||
            region_has_map > 1 || !map_packet_skip_string(&reader) ||
            !map_packet_skip_string(&reader) || !map_packet_skip_string(&reader)) {
            return false;
        }

        if (mapstat == MAP_UPDATE_CMD_NEW) {
            uint8_t width, height;

            if (!map_packet_read_uint8(&reader, &width) ||
                !map_packet_read_uint8(&reader, &height) || width == 0 || height == 0) {
                return false;
            }
            new_map_width = width;
            new_map_height = height;
        } else {
            uint8_t tile;
            int8_t ignored_offset, zoff;

            if (!map_packet_read_uint8(&reader, &tile) ||
                !map_packet_read_int8(&reader, &ignored_offset) ||
                !map_packet_read_int8(&reader, &ignored_offset) ||
                !map_packet_read_int8(&reader, &zoff) || tile < MAP_UPDATE_TILE_MIN ||
                tile > MAP_UPDATE_TILE_MAX || (tile == MAP_UPDATE_TILE_UP && zoff != 1) ||
                (tile == MAP_UPDATE_TILE_DOWN && zoff != -1) ||
                (tile < MAP_UPDATE_TILE_UP && zoff != 0)) {
                return false;
            }
        }
    }

    uint8_t xpos, ypos, player_sub_layer, level_count;
    if (!map_packet_read_uint8(&reader, &xpos) || !map_packet_read_uint8(&reader, &ypos) ||
        !map_packet_read_uint8(&reader, &player_sub_layer) ||
        player_sub_layer >= MAP2_PROTOCOL_SUB_LAYERS ||
        (new_map_width != 0 && (xpos >= new_map_width || ypos >= new_map_height)) ||
        !map_packet_read_uint8(&reader, &level_count) || level_count == 0 ||
        level_count > MAP2_LEVELS) {
        return false;
    }

    uint16_t level_mask = 0;
    for (uint8_t i = 0; i < level_count; i++) {
        int8_t depth;
        uint32_t level_size;

        if (!map_packet_read_int8(&reader, &depth) ||
            !map_packet_read_uint32(&reader, &level_size) || depth < -MAP2_MAX_DEPTH ||
            depth > MAP2_MAX_DEPTH || level_size > packet_reader_remaining(&reader)) {
            return false;
        }

        uint16_t level_bit = UINT16_C(1) << MAP2_DEPTH_INDEX(depth);
        if (level_mask & level_bit) {
            return false;
        }
        level_mask |= level_bit;

        packet_view_t level = packet_reader_read_view(&reader, level_size);
        map_packet_reader_t level_reader;
        packet_reader_init(&level_reader, level.data, level.len);
        if (!socket_command_map_validate_level(&level_reader, map_width_limit, map_height_limit)) {
            return false;
        }
    }

    return packet_reader_finish(&reader) &&
           (level_mask & (UINT16_C(1) << MAP2_DEPTH_INDEX(0))) != 0;
}

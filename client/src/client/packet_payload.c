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

#include <packet_payload.h>

bool client_packet_parse_image(const uint8_t *data,
                               size_t len,
                               size_t pos,
                               uint32_t *face_id,
                               packet_view_t *image) {
    packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);

    uint32_t parsed_face_id = packet_reader_read_uint32(&reader);
    uint32_t image_size = packet_reader_read_uint32(&reader);
    packet_view_t parsed_image = packet_reader_read_view(&reader, image_size);
    if (!packet_reader_finish(&reader)) {
        return false;
    }

    *face_id = parsed_face_id;
    *image = parsed_image;
    return true;
}

bool client_packet_parse_file_update(const uint8_t *data,
                                     size_t len,
                                     size_t pos,
                                     char *filename,
                                     size_t filename_size,
                                     uint32_t *uncompressed_size,
                                     packet_view_t *compressed) {
    packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);

    if (!packet_reader_read_string(&reader, filename, filename_size)) {
        return false;
    }
    uint32_t parsed_size = packet_reader_read_uint32(&reader);
    packet_view_t parsed_compressed =
        packet_reader_read_view(&reader, packet_reader_remaining(&reader));
    if (!packet_reader_finish(&reader)) {
        return false;
    }

    *uncompressed_size = parsed_size;
    *compressed = parsed_compressed;
    return true;
}

bool client_packet_parse_resource(const uint8_t *data,
                                  size_t len,
                                  size_t pos,
                                  char *name,
                                  size_t name_size,
                                  size_t digest_size,
                                  packet_view_t *digest) {
    packet_reader_t reader;
    packet_reader_init_at(&reader, data, len, pos);

    if (!packet_reader_read_string(&reader, name, name_size)) {
        return false;
    }
    packet_view_t parsed_digest = packet_reader_read_view(&reader, digest_size);
    if (!packet_reader_finish(&reader)) {
        return false;
    }

    *digest = parsed_digest;
    return true;
}

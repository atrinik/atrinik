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

#ifndef PACKET_PAYLOAD_H
#define PACKET_PAYLOAD_H

#include <toolkit/packet.h>

bool client_packet_parse_image(const uint8_t *data,
                               size_t len,
                               size_t pos,
                               uint32_t *face_id,
                               packet_view_t *image);
bool client_packet_parse_file_update(const uint8_t *data,
                                     size_t len,
                                     size_t pos,
                                     char *filename,
                                     size_t filename_size,
                                     uint32_t *uncompressed_size,
                                     packet_view_t *compressed);
bool client_packet_parse_resource(const uint8_t *data,
                                  size_t len,
                                  size_t pos,
                                  char *name,
                                  size_t name_size,
                                  size_t digest_size,
                                  packet_view_t *digest);

#endif

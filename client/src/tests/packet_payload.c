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

#include <stdlib.h>
#include <string.h>

#define TEST_CHECK(condition) \
    do {                      \
        if (!(condition)) {   \
            abort();          \
        }                     \
    } while (0)

static void test_image(void) {
    packet_struct *packet = packet_new(0, 16, 16);
    const uint8_t bytes[] = {1, 2, 3};
    packet_writer_write_uint32(packet, 42);
    packet_writer_write_uint32(packet, sizeof(bytes));
    packet_writer_write_bytes(packet, bytes, sizeof(bytes));

    uint32_t face_id = 0;
    packet_view_t image = {0};
    TEST_CHECK(client_packet_parse_image(packet->data, packet->len, 0, &face_id, &image));
    TEST_CHECK(face_id == 42);
    TEST_CHECK(image.len == sizeof(bytes));
    TEST_CHECK(memcmp(image.data, bytes, sizeof(bytes)) == 0);

    packet_writer_write_uint8(packet, 0xff);
    TEST_CHECK(!client_packet_parse_image(packet->data, packet->len, 0, &face_id, &image));
    packet_free(packet);

    const uint8_t truncated[] = {0, 0, 0, 42, 0, 0, 0, 4, 1, 2, 3};
    TEST_CHECK(!client_packet_parse_image(truncated, sizeof(truncated), 0, &face_id, &image));
}

static void test_file_update(void) {
    packet_struct *packet = packet_new(0, 32, 16);
    const uint8_t bytes[] = {4, 5, 6};
    packet_writer_write_cstring(packet, "sound/effect.ogg");
    packet_writer_write_uint32(packet, 1234);
    packet_writer_write_bytes(packet, bytes, sizeof(bytes));

    char filename[64];
    uint32_t uncompressed_size = 0;
    packet_view_t compressed = {0};
    TEST_CHECK(client_packet_parse_file_update(packet->data,
                                               packet->len,
                                               0,
                                               filename,
                                               sizeof(filename),
                                               &uncompressed_size,
                                               &compressed));
    TEST_CHECK(strcmp(filename, "sound/effect.ogg") == 0);
    TEST_CHECK(uncompressed_size == 1234);
    TEST_CHECK(compressed.len == sizeof(bytes));
    TEST_CHECK(memcmp(compressed.data, bytes, sizeof(bytes)) == 0);
    packet_free(packet);

    const uint8_t unterminated[] = {'b', 'a', 'd'};
    TEST_CHECK(!client_packet_parse_file_update(unterminated,
                                                sizeof(unterminated),
                                                0,
                                                filename,
                                                sizeof(filename),
                                                &uncompressed_size,
                                                &compressed));
}

static void test_resource(void) {
    packet_struct *packet = packet_new(0, 80, 16);
    uint8_t digest_bytes[64];
    memset(digest_bytes, 0xab, sizeof(digest_bytes));
    packet_writer_write_cstring(packet, "painting.xml");
    packet_writer_write_bytes(packet, digest_bytes, sizeof(digest_bytes));

    char name[64];
    packet_view_t digest = {0};
    TEST_CHECK(client_packet_parse_resource(packet->data,
                                            packet->len,
                                            0,
                                            name,
                                            sizeof(name),
                                            sizeof(digest_bytes),
                                            &digest));
    TEST_CHECK(strcmp(name, "painting.xml") == 0);
    TEST_CHECK(digest.len == sizeof(digest_bytes));
    TEST_CHECK(memcmp(digest.data, digest_bytes, sizeof(digest_bytes)) == 0);

    packet_writer_write_uint8(packet, 0xff);
    TEST_CHECK(!client_packet_parse_resource(packet->data,
                                             packet->len,
                                             0,
                                             name,
                                             sizeof(name),
                                             sizeof(digest_bytes),
                                             &digest));
    packet_free(packet);

    const uint8_t truncated[] = {'x', '\0', 1, 2, 3};
    TEST_CHECK(!client_packet_parse_resource(truncated,
                                             sizeof(truncated),
                                             0,
                                             name,
                                             sizeof(name),
                                             sizeof(digest_bytes),
                                             &digest));
}

int main(void) {
    toolkit_import(packet);
    test_image();
    test_file_update();
    test_resource();
    toolkit_deinit();
    return 0;
}

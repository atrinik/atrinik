/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <toolkit/map_protocol.h>
#include <toolkit/packet.h>
#include <toolkit/string.h>

#define packet_verify_data(packet, str)                                                 \
    {                                                                                   \
        char *hex;                                                                      \
        hex = xmalloc(sizeof(*hex) * ((packet)->len * 2 + 1));                          \
        string_tohex((packet)->data, (packet)->len, hex, (packet)->len * 2 + 1, false); \
        ck_assert_str_eq(hex, (str));                                                   \
        free(hex);                                                                      \
    }

START_TEST(test_packet_new) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    ck_assert_ptr_ne(packet, NULL);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 0);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 5);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 100);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 100, 0);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 100, 100);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 0, 100);
    packet_writer_write_cstring(packet, "hello world");
    ck_assert_str_eq((const char *)packet->data, "hello world");
    packet_free(packet);
}
END_TEST

/** Build the invariant prefix of a same-map protocol-v1068 MAP update. */
static packet_struct *map_protocol_test_packet(uint8_t level_count) {
    packet_struct *packet = packet_new(0, 16, 16);
    packet_writer_write_uint8(packet, MAP_UPDATE_CMD_SAME);
    packet_writer_write_uint8(packet, 0);
    packet_writer_write_uint8(packet, 0);
    packet_writer_write_uint8(packet, 0);
    packet_writer_write_uint8(packet, level_count);
    return packet;
}

/** Append one framed MAP level to a test packet. */
static void map_protocol_test_level(packet_struct *packet, int8_t depth, packet_struct *level) {
    packet_writer_write_int8(packet, depth);
    packet_writer_write_uint32(packet, level != NULL ? level->len : 0);
    if (level != NULL) {
        packet_writer_write_packet(packet, level);
    }
}

START_TEST(test_map_protocol_validate_minimal_and_truncation) {
    packet_struct *packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, NULL);

    ck_assert(map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    ck_assert(!map_protocol_validate(NULL, 0, 0, 21, 21));
    ck_assert(!map_protocol_validate(packet->data, packet->len, packet->len + 1, 21, 21));
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 0, 21));
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 33));
    for (size_t len = 0; len < packet->len; len++) {
        ck_assert(!map_protocol_validate(packet->data, len, 0, 21, 21));
    }

    packet_writer_write_uint8(packet, 0);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
}
END_TEST

START_TEST(test_map_protocol_rejects_duplicate_depth) {
    packet_struct *packet = map_protocol_test_packet(2);
    map_protocol_test_level(packet, 0, NULL);
    map_protocol_test_level(packet, 0, NULL);

    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
}
END_TEST

START_TEST(test_map_protocol_enforces_level_framing) {
    packet_struct *packet = map_protocol_test_packet(2);
    map_protocol_test_level(packet, -1, NULL);
    map_protocol_test_level(packet, 0, NULL);
    ck_assert(map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);

    packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 1, NULL);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);

    packet = map_protocol_test_packet(1);
    packet_writer_write_int8(packet, 0);
    packet_writer_write_uint32(packet, 1);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);

    packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, MAP2_MAX_DEPTH + 1, NULL);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
}
END_TEST

START_TEST(test_map_protocol_rejects_bad_tile_and_layer_indices) {
    packet_struct *level = packet_new(0, 16, 16);
    packet_writer_write_uint16(level, 21 << 11);
    packet_writer_write_uint8(level, 0);
    packet_writer_write_uint8(level, 0);
    packet_struct *packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, level);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
    packet_free(level);

    level = packet_new(0, 16, 16);
    packet_writer_write_uint16(level, 0);
    packet_writer_write_uint8(level, 1);
    packet_writer_write_uint8(level, MAP2_PROTOCOL_REAL_LAYERS);
    packet_writer_write_uint8(level, 0);
    packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, level);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
    packet_free(level);

    level = packet_new(0, 16, 16);
    packet_writer_write_uint16(level, 0);
    packet_writer_write_uint8(level, 1);
    packet_writer_write_uint8(level, MAP2_LAYER_CLEAR);
    packet_writer_write_uint8(level, MAP2_PROTOCOL_REAL_LAYERS);
    packet_writer_write_uint8(level, 0);
    packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, level);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
    packet_free(level);
}
END_TEST

START_TEST(test_map_protocol_validates_tile_record_flags) {
    packet_struct *level = packet_new(0, 16, 16);
    packet_writer_write_uint16(level, MAP2_MASK_FOW | MAP2_MASK_LIGHT_LEVEL);
    packet_writer_write_uint8(level, 1);
    packet_writer_write_uint8(level, 128);
    packet_writer_write_uint8(level, 0);
    packet_writer_write_uint8(level, 0);
    packet_struct *packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, level);
    ck_assert(map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
    packet_free(level);

    level = packet_new(0, 16, 16);
    packet_writer_write_uint16(level, 0);
    packet_writer_write_uint8(level, 0);
    packet_writer_write_uint8(level, 2);
    packet = map_protocol_test_packet(1);
    map_protocol_test_level(packet, 0, level);
    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
    packet_free(level);
}
END_TEST

START_TEST(test_map_protocol_rejects_unterminated_metadata) {
    packet_struct *packet = packet_new(0, 16, 16);
    packet_writer_write_uint8(packet, MAP_UPDATE_CMD_NEW);
    packet_writer_write_string(packet, "unterminated");

    ck_assert(!map_protocol_validate(packet->data, packet->len, 0, 21, 21));
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_dup) {
    packet_struct *packet, *packet2;

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, 50);
    packet_verify_data(packet, "00000032");
    packet2 = packet_dup(packet);
    packet_verify_data(packet2, "00000032");
    packet_free(packet);
    packet_free(packet2);

    packet = packet_new(0, 0, 0);
    packet_verify_data(packet, "");
    packet2 = packet_dup(packet);
    packet_verify_data(packet2, "");
    packet_free(packet);
    packet_free(packet2);
}
END_TEST

START_TEST(test_packet_writer_mark) {
    packet_struct *packet;
    packet_writer_mark_t packet_save_buf;

    packet = packet_new(0, 0, 0);
    packet_writer_mark(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 0);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_writer_mark(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_writer_mark(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_writer_write_uint32(packet, 42);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_rollback) {
    packet_struct *packet;
    packet_writer_mark_t packet_save_buf;

    packet = packet_new(0, 0, 0);
    packet_writer_mark(packet, &packet_save_buf);
    packet_writer_rollback(packet, &packet_save_buf);
    ck_assert_uint_eq(packet->len, 0);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_writer_mark(packet, &packet_save_buf);
    packet_writer_rollback(packet, &packet_save_buf);
    ck_assert_uint_eq(packet->len, 4);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_writer_mark(packet, &packet_save_buf);
    packet_writer_rollback(packet, &packet_save_buf);
    ck_assert_uint_eq(packet->len, 4);
    packet_writer_write_uint32(packet, 42);
    ck_assert_uint_eq(packet->len, 8);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_writer_mark(packet, &packet_save_buf);
    packet_writer_write_uint32(packet, 42);
    packet_writer_rollback(packet, &packet_save_buf);
    ck_assert_uint_eq(packet->len, 4);
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_uint8) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint8(packet, 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint8(packet, 42);
    packet_verify_data(packet, "2A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint8(packet, UINT8_MAX);
    packet_verify_data(packet, "FF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_int8) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_int8(packet, 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int8(packet, 42);
    packet_verify_data(packet, "2A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int8(packet, -42);
    packet_verify_data(packet, "D6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int8(packet, INT8_MAX);
    packet_verify_data(packet, "7F");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int8(packet, INT8_MIN);
    packet_verify_data(packet, "80");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_uint16) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint16(packet, 0);
    packet_verify_data(packet, "0000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint16(packet, 42);
    packet_verify_data(packet, "002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint16(packet, UINT16_MAX);
    packet_verify_data(packet, "FFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_int16) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_int16(packet, 0);
    packet_verify_data(packet, "0000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int16(packet, 42);
    packet_verify_data(packet, "002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int16(packet, -42);
    packet_verify_data(packet, "FFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int16(packet, INT16_MAX);
    packet_verify_data(packet, "7FFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int16(packet, INT16_MIN);
    packet_verify_data(packet, "8000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_uint32) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 0);
    packet_verify_data(packet, "00000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, 42);
    packet_verify_data(packet, "0000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint32(packet, UINT32_MAX);
    packet_verify_data(packet, "FFFFFFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_int32) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, 0);
    packet_verify_data(packet, "00000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, 42);
    packet_verify_data(packet, "0000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, -42);
    packet_verify_data(packet, "FFFFFFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, INT32_MAX);
    packet_verify_data(packet, "7FFFFFFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int32(packet, INT32_MIN);
    packet_verify_data(packet, "80000000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_uint64) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint64(packet, 0);
    packet_verify_data(packet, "0000000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint64(packet, 42);
    packet_verify_data(packet, "000000000000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_uint64(packet, UINT64_MAX);
    packet_verify_data(packet, "FFFFFFFFFFFFFFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_int64) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_int64(packet, 0);
    packet_verify_data(packet, "0000000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int64(packet, 42);
    packet_verify_data(packet, "000000000000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int64(packet, -42);
    packet_verify_data(packet, "FFFFFFFFFFFFFFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int64(packet, INT64_MAX);
    packet_verify_data(packet, "7FFFFFFFFFFFFFFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_int64(packet, INT64_MIN);
    packet_verify_data(packet, "8000000000000000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_float) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_float(packet, 1.0);
    packet_verify_data(packet, "3F800000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_float(packet, 0.0001);
    packet_verify_data(packet, "38D1B717");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_float(packet, -10.0);
    packet_verify_data(packet, "C1200000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_float(packet, 12345678.);
    packet_verify_data(packet, "4B3C614E");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_float(packet, -109.56);
    packet_verify_data(packet, "C2DB1EB8");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_double) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_double(packet, 1.0);
    packet_verify_data(packet, "3FF0000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_double(packet, 0.0001);
    packet_verify_data(packet, "3F1A36E2EB1C432D");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_double(packet, -10.0);
    packet_verify_data(packet, "C024000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_double(packet, 123456789);
    packet_verify_data(packet, "419D6F3454000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_double(packet, -109.56);
    packet_verify_data(packet, "C05B63D70A3D70A4");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_bytes) {
    packet_struct *packet;
    const uint8_t data[] = {0xff, 0x03, 0x00};

    packet = packet_new(0, 0, 0);
    packet_writer_write_bytes(packet, data, 0);
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_bytes(packet, data, 1);
    packet_verify_data(packet, "FF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_bytes(packet, data, 3);
    packet_verify_data(packet, "FF0300");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_string_n) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_string_n(packet, "", 0);
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_string_n(packet, "test", 4);
    packet_verify_data(packet, "74657374");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_string_n(packet, "test", 2);
    packet_verify_data(packet, "7465");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_string) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_string(packet, "");
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_string(packet, "test");
    packet_verify_data(packet, "74657374");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_cstring_n) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_cstring_n(packet, "", 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_cstring_n(packet, "test", 2);
    packet_verify_data(packet, "746500");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_cstring) {
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_writer_write_cstring(packet, "");
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_writer_write_cstring(packet, "test");
    packet_verify_data(packet, "7465737400");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_writer_write_packet) {
    packet_struct *packet, *packet2;

    packet = packet_new(0, 0, 0);
    packet2 = packet_new(0, 0, 0);
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "");
    packet_writer_write_packet(packet, packet2);
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "");
    packet_free(packet);
    packet_free(packet2);

    packet = packet_new(0, 0, 0);
    packet2 = packet_new(0, 0, 0);
    packet_writer_write_cstring(packet2, "test");
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "7465737400");
    packet_writer_write_packet(packet, packet2);
    packet_verify_data(packet, "7465737400");
    packet_verify_data(packet2, "7465737400");
    packet_free(packet);
    packet_free(packet2);
}
END_TEST

START_TEST(test_packet_reader_scalars_round_trip) {
    const uint8_t data[] = {0x7f, 0x80, 0x12, 0x34, 0xff, 0xfe, 0x89, 0xab, 0xcd, 0xef, 0xff,
                            0xff, 0xff, 0xfe, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    packet_reader_t reader;
    packet_reader_init(&reader, data, sizeof(data));

    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 0x7f);
    ck_assert_int_eq(packet_reader_read_int8(&reader), INT8_MIN);
    ck_assert_uint_eq(packet_reader_read_uint16(&reader), 0x1234);
    ck_assert_int_eq(packet_reader_read_int16(&reader), -2);
    ck_assert_uint_eq(packet_reader_read_uint32(&reader), UINT32_C(0x89abcdef));
    ck_assert_int_eq(packet_reader_read_int32(&reader), -2);
    ck_assert_uint_eq(packet_reader_read_uint64(&reader), UINT64_C(0x0123456789abcdef));
    ck_assert(packet_reader_finish(&reader));
}
END_TEST

START_TEST(test_packet_reader_truncation_is_sticky) {
    for (size_t len = 0; len < sizeof(uint64_t); len++) {
        uint8_t data[sizeof(uint64_t)] = {0};
        packet_reader_t reader;
        packet_reader_init(&reader, data, len);

        ck_assert_uint_eq(packet_reader_read_uint64(&reader), 0);
        ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_TRUNCATED);
        size_t failed_pos = reader.pos;
        ck_assert_uint_eq(packet_reader_read_uint8(&reader), 0);
        ck_assert_uint_eq(reader.pos, failed_pos);
        ck_assert(!packet_reader_finish(&reader));
    }
}
END_TEST

START_TEST(test_packet_reader_strings_views_and_limits) {
    const uint8_t data[] = {'h', 'e', 'l', 'l', 'o', '\0', 0x2a};
    packet_reader_t reader;
    packet_reader_init(&reader, data, sizeof(data));
    packet_view_t view = packet_reader_read_string_view(&reader, 5);

    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_NONE);
    ck_assert_uint_eq(view.len, 5);
    ck_assert_int_eq(memcmp(view.data, "hello", view.len), 0);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 42);
    ck_assert(packet_reader_finish(&reader));

    char dest[5] = "keep";
    packet_reader_init(&reader, data, sizeof(data));
    ck_assert(!packet_reader_read_string_bounded(&reader, VS(dest), 5));
    ck_assert_str_eq(dest, "keep");
    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_LIMIT_EXCEEDED);

    packet_reader_init(&reader, data, sizeof(data));
    (void)packet_reader_read_string_view(&reader, 4);
    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_LIMIT_EXCEEDED);

    packet_reader_init(&reader, "hello", 5);
    (void)packet_reader_read_string_view(&reader, 5);
    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_TRUNCATED);
}
END_TEST

START_TEST(test_packet_reader_finish_and_counts) {
    const uint8_t data[] = {3, 0xff};
    packet_reader_t reader;
    size_t count = 0;
    packet_reader_init(&reader, data, sizeof(data));

    ck_assert(packet_reader_read_count8(&reader, 3, &count));
    ck_assert_uint_eq(count, 3);
    ck_assert(!packet_reader_finish(&reader));
    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_TRAILING_DATA);

    packet_reader_init(&reader, data, sizeof(data));
    ck_assert(!packet_reader_read_count8(&reader, 2, &count));
    ck_assert_int_eq(packet_reader_error(&reader), PACKET_ERROR_LIMIT_EXCEEDED);
}
END_TEST

START_TEST(test_packet_writer_limit_is_sticky) {
    packet_writer_t *writer = packet_new(0, 0, 0);
    packet_writer_set_limit(writer, 2);

    packet_writer_write_uint16(writer, UINT16_C(0x1234));
    ck_assert(packet_writer_finish(writer));
    ck_assert_uint_eq(writer->len, 2);
    packet_writer_write_uint8(writer, 1);
    ck_assert(!packet_writer_finish(writer));
    ck_assert_int_eq(packet_writer_error(writer), PACKET_ERROR_LIMIT_EXCEEDED);
    ck_assert_uint_eq(writer->len, 2);
    packet_writer_write_uint8(writer, 2);
    ck_assert_uint_eq(writer->len, 2);
    packet_free(writer);
}
END_TEST

START_TEST(test_packet_writer_strings_fail_atomically) {
    packet_writer_t *writer = packet_new(0, 0, 0);
    writer->limit = 4;

    packet_writer_write_string_n(writer, "oversized", 9);
    ck_assert_int_eq(packet_writer_error(writer), PACKET_ERROR_LIMIT_EXCEEDED);
    ck_assert_uint_eq(writer->len, 0);
    packet_free(writer);

    writer = packet_new(0, 0, 0);
    writer->limit = 4;
    packet_writer_write_cstring_n(writer, "four", 4);
    ck_assert_int_eq(packet_writer_error(writer), PACKET_ERROR_LIMIT_EXCEEDED);
    ck_assert_uint_eq(writer->len, 0);
    packet_free(writer);
}
END_TEST

START_TEST(test_packet_delete_moves_only_trailing_bytes) {
    packet_writer_t *writer = packet_new(0, 0, 0);
    packet_writer_write_bytes(writer, (const uint8_t *)"0123456789", 10);

    packet_delete(writer, 3, 2);
    ck_assert_uint_eq(writer->len, 8);
    ck_assert_mem_eq(writer->data, "01256789", 8);

    packet_delete(writer, 6, 2);
    ck_assert_uint_eq(writer->len, 6);
    ck_assert_mem_eq(writer->data, "012567", 6);
    packet_free(writer);
}
END_TEST

START_TEST(test_packet_reader_scope_tracks_completion_and_errors) {
    const uint8_t data[] = {1, 2};
    packet_reader_scope_t scope;
    packet_reader_t reader;

    packet_reader_scope_begin(&scope);
    packet_reader_init(&reader, data, sizeof(data));
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 1);
    ck_assert_int_eq(packet_reader_scope_finish(&scope), PACKET_ERROR_TRAILING_DATA);

    packet_reader_scope_begin(&scope);
    packet_reader_init(&reader, data, 1);
    (void)packet_reader_read_uint16(&reader);
    ck_assert_int_eq(packet_reader_scope_finish(&scope), PACKET_ERROR_TRUNCATED);

    packet_reader_scope_begin(&scope);
    packet_reader_init(&reader, data, sizeof(data));
    (void)packet_reader_read_uint16(&reader);
    ck_assert_int_eq(packet_reader_scope_finish(&scope), PACKET_ERROR_NONE);

    packet_reader_scope_begin(&scope);
    packet_reader_init(&reader, data, sizeof(data));
    packet_reader_t nested;
    packet_reader_init(&nested, data + 1, 1);
    (void)packet_reader_read_uint16(&reader);
    ck_assert_int_eq(packet_reader_scope_finish(&scope), PACKET_ERROR_NONE);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("packet");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_packet_new);
    tcase_add_test(tc_core, test_packet_dup);
    tcase_add_test(tc_core, test_packet_writer_mark);
    tcase_add_test(tc_core, test_packet_writer_rollback);
    tcase_add_test(tc_core, test_packet_writer_write_uint8);
    tcase_add_test(tc_core, test_packet_writer_write_int8);
    tcase_add_test(tc_core, test_packet_writer_write_uint16);
    tcase_add_test(tc_core, test_packet_writer_write_int16);
    tcase_add_test(tc_core, test_packet_writer_write_uint32);
    tcase_add_test(tc_core, test_packet_writer_write_int32);
    tcase_add_test(tc_core, test_packet_writer_write_uint64);
    tcase_add_test(tc_core, test_packet_writer_write_int64);
    tcase_add_test(tc_core, test_packet_writer_write_float);
    tcase_add_test(tc_core, test_packet_writer_write_double);
    tcase_add_test(tc_core, test_packet_writer_write_bytes);
    tcase_add_test(tc_core, test_packet_writer_write_string_n);
    tcase_add_test(tc_core, test_packet_writer_write_string);
    tcase_add_test(tc_core, test_packet_writer_write_cstring_n);
    tcase_add_test(tc_core, test_packet_writer_write_cstring);
    tcase_add_test(tc_core, test_packet_writer_write_packet);
    tcase_add_test(tc_core, test_packet_reader_scalars_round_trip);
    tcase_add_test(tc_core, test_packet_reader_truncation_is_sticky);
    tcase_add_test(tc_core, test_packet_reader_strings_views_and_limits);
    tcase_add_test(tc_core, test_packet_reader_finish_and_counts);
    tcase_add_test(tc_core, test_packet_writer_limit_is_sticky);
    tcase_add_test(tc_core, test_packet_writer_strings_fail_atomically);
    tcase_add_test(tc_core, test_packet_delete_moves_only_trailing_bytes);
    tcase_add_test(tc_core, test_packet_reader_scope_tracks_completion_and_errors);
    tcase_add_test(tc_core, test_map_protocol_validate_minimal_and_truncation);
    tcase_add_test(tc_core, test_map_protocol_rejects_duplicate_depth);
    tcase_add_test(tc_core, test_map_protocol_enforces_level_framing);
    tcase_add_test(tc_core, test_map_protocol_rejects_bad_tile_and_layer_indices);
    tcase_add_test(tc_core, test_map_protocol_validates_tile_record_flags);
    tcase_add_test(tc_core, test_map_protocol_rejects_unterminated_metadata);

    return s;
}

void check_server_packet(void) {
    check_run_suite(suite(), __FILE__);
}

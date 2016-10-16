/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
#include <toolkit/packet.h>
#include <toolkit/string.h>

#define packet_verify_data(packet, str) \
{ \
    char *hex; \
    hex = emalloc(sizeof(*hex) * ((packet)->len * 2 + 1)); \
    string_tohex((packet)->data, (packet)->len, hex, (packet)->len * 2 + 1, \
                 false); \
    ck_assert_str_eq(hex, (str)); \
    efree(hex); \
}

START_TEST(test_packet_new)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    ck_assert_ptr_ne(packet, NULL);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 0);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 5);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 5, 100);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 100, 0);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 100, 100);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);

    packet = packet_new(0, 0, 100);
    packet_append_string_terminated(packet, "hello world");
    ck_assert_str_eq((const char *) packet->data, "hello world");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_dup)
{
    packet_struct *packet, *packet2;

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, 50);
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

START_TEST(test_packet_save)
{
    packet_struct *packet;
    packet_save_t packet_save_buf;

    packet = packet_new(0, 0, 0);
    packet_save(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 0);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint32(packet, 42);
    packet_save(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint32(packet, 42);
    packet_save(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_append_uint32(packet, 42);
    ck_assert_uint_eq(packet_save_buf.pos, 4);
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_load)
{
    packet_struct *packet;
    packet_save_t packet_save_buf;

    packet = packet_new(0, 0, 0);
    ck_assert_uint_eq(packet_get_pos(packet), 0);
    packet_save(packet, &packet_save_buf);
    packet_load(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_get_pos(packet), 0);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    ck_assert_uint_eq(packet_get_pos(packet), 0);
    packet_append_uint32(packet, 42);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_save(packet, &packet_save_buf);
    packet_load(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    ck_assert_uint_eq(packet_get_pos(packet), 0);
    packet_append_uint32(packet, 42);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_save(packet, &packet_save_buf);
    packet_load(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_append_uint32(packet, 42);
    ck_assert_uint_eq(packet_get_pos(packet), 8);
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    ck_assert_uint_eq(packet_get_pos(packet), 0);
    packet_append_uint32(packet, 42);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_save(packet, &packet_save_buf);
    packet_append_uint32(packet, 42);
    packet_load(packet, &packet_save_buf);
    ck_assert_uint_eq(packet_get_pos(packet), 4);
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_uint8)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_uint8(packet, 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint8(packet, 42);
    packet_verify_data(packet, "2A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint8(packet, UINT8_MAX);
    packet_verify_data(packet, "FF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_int8)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_int8(packet, 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int8(packet, 42);
    packet_verify_data(packet, "2A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int8(packet, -42);
    packet_verify_data(packet, "D6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int8(packet, INT8_MAX);
    packet_verify_data(packet, "7F");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int8(packet, INT8_MIN);
    packet_verify_data(packet, "80");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_uint16)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_uint16(packet, 0);
    packet_verify_data(packet, "0000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint16(packet, 42);
    packet_verify_data(packet, "002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint16(packet, UINT16_MAX);
    packet_verify_data(packet, "FFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_int16)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_int16(packet, 0);
    packet_verify_data(packet, "0000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int16(packet, 42);
    packet_verify_data(packet, "002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int16(packet, -42);
    packet_verify_data(packet, "FFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int16(packet, INT16_MAX);
    packet_verify_data(packet, "7FFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int16(packet, INT16_MIN);
    packet_verify_data(packet, "8000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_uint32)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_uint32(packet, 0);
    packet_verify_data(packet, "00000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint32(packet, 42);
    packet_verify_data(packet, "0000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint32(packet, UINT32_MAX);
    packet_verify_data(packet, "FFFFFFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_int32)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, 0);
    packet_verify_data(packet, "00000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, 42);
    packet_verify_data(packet, "0000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, -42);
    packet_verify_data(packet, "FFFFFFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, INT32_MAX);
    packet_verify_data(packet, "7FFFFFFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int32(packet, INT32_MIN);
    packet_verify_data(packet, "80000000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_uint64)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_uint64(packet, 0);
    packet_verify_data(packet, "0000000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint64(packet, 42);
    packet_verify_data(packet, "000000000000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_uint64(packet, UINT64_MAX);
    packet_verify_data(packet, "FFFFFFFFFFFFFFFF");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_int64)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_int64(packet, 0);
    packet_verify_data(packet, "0000000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int64(packet, 42);
    packet_verify_data(packet, "000000000000002A");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int64(packet, -42);
    packet_verify_data(packet, "FFFFFFFFFFFFFFD6");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int64(packet, INT64_MAX);
    packet_verify_data(packet, "7FFFFFFFFFFFFFFF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_int64(packet, INT64_MIN);
    packet_verify_data(packet, "8000000000000000");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_float)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_float(packet, 1.0);
    packet_verify_data(packet, "3F800000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_float(packet, 0.0001);
    packet_verify_data(packet, "38D1B717");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_float(packet, -10.0);
    packet_verify_data(packet, "C1200000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_float(packet, 12345678.);
    packet_verify_data(packet, "4B3C614E");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_float(packet, -109.56);
    packet_verify_data(packet, "C2DB1EB8");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_double)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_double(packet, 1.0);
    packet_verify_data(packet, "3FF0000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_double(packet, 0.0001);
    packet_verify_data(packet, "3F1A36E2EB1C432D");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_double(packet, -10.0);
    packet_verify_data(packet, "C024000000000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_double(packet, 123456789);
    packet_verify_data(packet, "419D6F3454000000");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_double(packet, -109.56);
    packet_verify_data(packet, "C05B63D70A3D70A4");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_data_len)
{
    packet_struct *packet;
    const uint8_t data[] = {0xff, 0x03, 0x00};

    packet = packet_new(0, 0, 0);
    packet_append_data_len(packet, data, 0);
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_data_len(packet, data, 1);
    packet_verify_data(packet, "FF");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_data_len(packet, data, 3);
    packet_verify_data(packet, "FF0300");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_string_len)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_string_len(packet, "", 0);
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string_len(packet, "test", 4);
    packet_verify_data(packet, "74657374");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string_len(packet, "test", 2);
    packet_verify_data(packet, "7465");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_string)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_string(packet, "");
    packet_verify_data(packet, "");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string(packet, "test");
    packet_verify_data(packet, "74657374");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_string_len_terminated)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_string_len_terminated(packet, "", 0);
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string_len_terminated(packet, "test", 2);
    packet_verify_data(packet, "746500");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_string_terminated)
{
    packet_struct *packet;

    packet = packet_new(0, 0, 0);
    packet_append_string_terminated(packet, "");
    packet_verify_data(packet, "00");
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    packet_append_string_terminated(packet, "test");
    packet_verify_data(packet, "7465737400");
    packet_free(packet);
}
END_TEST

START_TEST(test_packet_append_packet)
{
    packet_struct *packet, *packet2;

    packet = packet_new(0, 0, 0);
    packet2 = packet_new(0, 0, 0);
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "");
    packet_append_packet(packet, packet2);
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "");
    packet_free(packet);
    packet_free(packet2);

    packet = packet_new(0, 0, 0);
    packet2 = packet_new(0, 0, 0);
    packet_append_string_terminated(packet2, "test");
    packet_verify_data(packet, "");
    packet_verify_data(packet2, "7465737400");
    packet_append_packet(packet, packet2);
    packet_verify_data(packet, "7465737400");
    packet_verify_data(packet2, "7465737400");
    packet_free(packet);
    packet_free(packet2);
}
END_TEST

START_TEST(test_packet_to_uint8)
{
    size_t pos;

    pos = 0;
    ck_assert_uint_eq(packet_to_uint8((uint8_t []) {0x00}, 1, &pos), 0);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint8((uint8_t []) {0x2A}, 1, &pos), 42);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint8((uint8_t []) {0xFF}, 1, &pos), UINT8_MAX);
}
END_TEST

START_TEST(test_packet_to_int8)
{
    size_t pos;

    pos = 0;
    ck_assert_int_eq(packet_to_int8((uint8_t []) {0x00}, 1, &pos), 0);

    pos = 0;
    ck_assert_int_eq(packet_to_int8((uint8_t []) {0x2A}, 1, &pos), 42);

    pos = 0;
    ck_assert_int_eq(packet_to_int8((uint8_t []) {0x7F}, 1, &pos), INT8_MAX);

    pos = 0;
    ck_assert_int_eq(packet_to_int8((uint8_t []) {0x80}, 1, &pos), INT8_MIN);
}
END_TEST

START_TEST(test_packet_to_uint16)
{
    size_t pos;

    pos = 0;
    ck_assert_uint_eq(packet_to_uint16((uint8_t []) {0x00, 0x00}, 2, &pos), 0);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint16((uint8_t []) {0x00, 0x2A}, 2, &pos), 42);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint16((uint8_t []) {0xFF, 0xFF}, 2, &pos),
            UINT16_MAX);
}
END_TEST

START_TEST(test_packet_to_int16)
{
    size_t pos;

    pos = 0;
    ck_assert_int_eq(packet_to_int16((uint8_t []) {0x00, 0x00}, 2, &pos), 0);

    pos = 0;
    ck_assert_int_eq(packet_to_int16((uint8_t []) {0x00, 0x2A}, 2, &pos), 42);

    pos = 0;
    ck_assert_int_eq(packet_to_int16((uint8_t []) {0xFF, 0xD6}, 2, &pos), -42);

    pos = 0;
    ck_assert_int_eq(packet_to_int16((uint8_t []) {0x7F, 0xFF}, 2, &pos),
            INT16_MAX);

    pos = 0;
    ck_assert_int_eq(packet_to_int16((uint8_t []) {0x80, 0x00}, 2, &pos),
            INT16_MIN);
}
END_TEST

START_TEST(test_packet_to_uint32)
{
    size_t pos;

    pos = 0;
    ck_assert_uint_eq(packet_to_uint32((uint8_t []) {0x00, 0x00, 0x00, 0x00}, 4,
            &pos), 0);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint32((uint8_t []) {0x00, 0x00, 0x00, 0x2A}, 4,
            &pos), 42);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint32((uint8_t []) {0xFF, 0xFF, 0xFF, 0xFF}, 4,
            &pos), UINT32_MAX);
}
END_TEST

START_TEST(test_packet_to_int32)
{
    size_t pos;

    pos = 0;
    ck_assert_int_eq(packet_to_int32((uint8_t []) {0x00, 0x00, 0x00, 0x00}, 4,
            &pos), 0);

    pos = 0;
    ck_assert_int_eq(packet_to_int32((uint8_t []) {0x00, 0x00, 0x00, 0x2A}, 4,
            &pos), 42);

    pos = 0;
    ck_assert_int_eq(packet_to_int32((uint8_t []) {0xFF, 0xFF, 0xFF, 0xD6}, 4,
            &pos), -42);

    pos = 0;
    ck_assert_int_eq(packet_to_int32((uint8_t []) {0x7F, 0xFF, 0xFF, 0xFF}, 4,
            &pos), INT32_MAX);

    pos = 0;
    ck_assert_int_eq(packet_to_int32((uint8_t []) {0x80, 0x00, 0x00, 0x00}, 4,
            &pos), INT32_MIN);
}
END_TEST

START_TEST(test_packet_to_uint64)
{
    size_t pos;

    pos = 0;
    ck_assert_uint_eq(packet_to_uint64((uint8_t []) {0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00}, 8, &pos), 0);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint64((uint8_t []) {0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x2A}, 8, &pos), 42);

    pos = 0;
    ck_assert_uint_eq(packet_to_uint64((uint8_t []) {0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF}, 8, &pos), UINT64_MAX);
}
END_TEST

START_TEST(test_packet_to_int64)
{
    size_t pos;

    pos = 0;
    ck_assert_int_eq(packet_to_int64((uint8_t []) {0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00}, 8, &pos), 0);

    pos = 0;
    ck_assert_int_eq(packet_to_int64((uint8_t []) {0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x2A}, 8, &pos), 42);

    pos = 0;
    ck_assert_int_eq(packet_to_int64((uint8_t []) {0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xD6}, 8, &pos), -42);

    pos = 0;
    ck_assert_int_eq(packet_to_int64((uint8_t []) {0x7F, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF}, 8, &pos), INT64_MAX);

    pos = 0;
    ck_assert_int_eq(packet_to_int64((uint8_t []) {0x80, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00}, 8, &pos), INT64_MIN);
}
END_TEST

START_TEST(test_packet_to_float)
{
    size_t pos;
    char buf[MAX_BUF], buf2[MAX_BUF];

    pos = 0;
    snprintf(VS(buf), "%f", 1.0);
    snprintf(VS(buf2), "%f", packet_to_float((uint8_t []) {0x3F, 0x80,
            0x00, 0x00}, 4, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", 0.0001);
    snprintf(VS(buf2), "%f", packet_to_float((uint8_t []) {0x38, 0xD1,
            0xB7, 0x17}, 4, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", -10.0);
    snprintf(VS(buf2), "%f", packet_to_float((uint8_t []) {0xC1, 0x20,
            0x00, 0x00}, 4, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", 12345678.);
    snprintf(VS(buf2), "%f", packet_to_float((uint8_t []) {0x4B, 0x3C,
            0x61, 0x4E}, 4, &pos));
    ck_assert_str_eq(buf, buf2);
}
END_TEST

START_TEST(test_packet_to_double)
{
    size_t pos;
    char buf[MAX_BUF], buf2[MAX_BUF];

    pos = 0;
    snprintf(VS(buf), "%f", 1.0);
    snprintf(VS(buf2), "%f", packet_to_double((uint8_t []) {0x3F, 0xF0,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", 0.0001);
    snprintf(VS(buf2), "%f", packet_to_double((uint8_t []) {0x3F, 0x1A,
            0x36, 0xE2, 0xEB, 0x1C, 0x43, 0x2D}, 8, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", -10.0);
    snprintf(VS(buf2), "%f", packet_to_double((uint8_t []) {0xC0, 0x24,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", 123456789.0);
    snprintf(VS(buf2), "%f", packet_to_double((uint8_t []) {0x41, 0x9D,
            0x6F, 0x34, 0x54, 0x00, 0x00, 0x00}, 8, &pos));
    ck_assert_str_eq(buf, buf2);

    pos = 0;
    snprintf(VS(buf), "%f", -109.56);
    snprintf(VS(buf2), "%f", packet_to_double((uint8_t []) {0xC0, 0x5B,
            0x63, 0xD7, 0x0A, 0x3D, 0x70, 0xA4}, 8, &pos));
    ck_assert_str_eq(buf, buf2);
}
END_TEST

START_TEST(test_packet_to_string)
{
    size_t pos;
    char buf[MAX_BUF], buf2[2];

    pos = 0;
    ck_assert_str_eq(packet_to_string((uint8_t []) {0x74, 0x65,
            0x73, 0x74, 0x00}, 5, &pos, VS(buf)), "test");

    pos = 0;
    ck_assert_str_eq(packet_to_string((uint8_t []) {0x74, 0x65,
            0x73, 0x74, 0x00}, 4, &pos, VS(buf)), "test");

    pos = 0;
    ck_assert_str_eq(packet_to_string((uint8_t []) {0x74, 0x65,
            0x73, 0x74}, 4, &pos, VS(buf)), "test");

    pos = 0;
    ck_assert_str_eq(packet_to_string((uint8_t []) {0x74, 0x65,
            0x73, 0x74}, 4, &pos, VS(buf2)), "t");

    pos = 0;
    ck_assert_str_eq(packet_to_string((uint8_t []) {0x74, 0x65,
            0x73, 0x74, 0x00}, 5, &pos, VS(buf2)), "t");
}
END_TEST

START_TEST(test_packet_to_stringbuffer)
{
    size_t pos;
    StringBuffer *sb;
    char *cp;

    pos = 0;
    sb = stringbuffer_new();
    packet_to_stringbuffer((uint8_t []) {0x74, 0x65, 0x73, 0x74, 0x00}, 5,
            &pos, sb);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "test");
    efree(cp);

    pos = 0;
    sb = stringbuffer_new();
    packet_to_stringbuffer((uint8_t []) {0x74, 0x65, 0x73, 0x74, 0x00}, 4,
            &pos, sb);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "test");
    efree(cp);

    pos = 0;
    sb = stringbuffer_new();
    packet_to_stringbuffer((uint8_t []) {0x74, 0x65, 0x73, 0x74}, 4,
            &pos, sb);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "test");
    efree(cp);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("packet");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_packet_new);
    tcase_add_test(tc_core, test_packet_dup);
    tcase_add_test(tc_core, test_packet_save);
    tcase_add_test(tc_core, test_packet_load);
    tcase_add_test(tc_core, test_packet_append_uint8);
    tcase_add_test(tc_core, test_packet_append_int8);
    tcase_add_test(tc_core, test_packet_append_uint16);
    tcase_add_test(tc_core, test_packet_append_int16);
    tcase_add_test(tc_core, test_packet_append_uint32);
    tcase_add_test(tc_core, test_packet_append_int32);
    tcase_add_test(tc_core, test_packet_append_uint64);
    tcase_add_test(tc_core, test_packet_append_int64);
    tcase_add_test(tc_core, test_packet_append_float);
    tcase_add_test(tc_core, test_packet_append_double);
    tcase_add_test(tc_core, test_packet_append_data_len);
    tcase_add_test(tc_core, test_packet_append_string_len);
    tcase_add_test(tc_core, test_packet_append_string);
    tcase_add_test(tc_core, test_packet_append_string_len_terminated);
    tcase_add_test(tc_core, test_packet_append_string_terminated);
    tcase_add_test(tc_core, test_packet_append_packet);
    tcase_add_test(tc_core, test_packet_to_uint8);
    tcase_add_test(tc_core, test_packet_to_int8);
    tcase_add_test(tc_core, test_packet_to_uint16);
    tcase_add_test(tc_core, test_packet_to_int16);
    tcase_add_test(tc_core, test_packet_to_uint32);
    tcase_add_test(tc_core, test_packet_to_int32);
    tcase_add_test(tc_core, test_packet_to_uint64);
    tcase_add_test(tc_core, test_packet_to_int64);
    tcase_add_test(tc_core, test_packet_to_float);
    tcase_add_test(tc_core, test_packet_to_double);
    tcase_add_test(tc_core, test_packet_to_string);
    tcase_add_test(tc_core, test_packet_to_stringbuffer);

    return s;
}

void check_server_packet(void)
{
    check_run_suite(suite(), __FILE__);
}

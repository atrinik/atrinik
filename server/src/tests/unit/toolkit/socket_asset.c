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

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <toolkit/packet.h>

START_TEST(test_socket_asset_request_round_trip)
{
    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_request_append(packet,
                                "client-maps/test.png",
                                0,
                                123456,
                                0x89abcdefU);

    socket_asset_request_t request;
    ck_assert(socket_asset_request_parse(packet->data,
                                         packet->len,
                                         0,
                                         &request));
    ck_assert_str_eq(request.path, "client-maps/test.png");
    ck_assert_uint_eq(request.offset, 0);
    ck_assert_uint_eq(request.cached_size, 123456);
    ck_assert_uint_eq(request.cached_checksum, 0x89abcdefU);
    packet_free(packet);
}
END_TEST

START_TEST(test_socket_asset_request_rejects_malformed)
{
    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_request_append(packet, "data/listing.txt", 0, 0, 0);

    socket_asset_request_t request;
    ck_assert(!socket_asset_request_parse(packet->data,
                                          packet->len - 1,
                                          0,
                                          &request));
    packet_append_uint8(packet, 0);
    ck_assert(!socket_asset_request_parse(packet->data,
                                          packet->len,
                                          0,
                                          &request));
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    socket_asset_request_append(packet, "data/listing.txt", 1, 2, 3);
    ck_assert(!socket_asset_request_parse(packet->data,
                                          packet->len,
                                          0,
                                          &request));
    packet_free(packet);
}
END_TEST

START_TEST(test_socket_asset_response_round_trip)
{
    static const uint8_t chunk[] = {0xde, 0xad, 0xbe, 0xef};
    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_response_append_ok(packet,
                                    "client-maps/test.def",
                                    sizeof(chunk),
                                    0,
                                    0x12345678U,
                                    chunk,
                                    sizeof(chunk));

    socket_asset_response_t response;
    ck_assert(socket_asset_response_parse(packet->data,
                                          packet->len,
                                          0,
                                          &response));
    ck_assert_uint_eq(response.status, ASSET_STATUS_OK);
    ck_assert_str_eq(response.path, "client-maps/test.def");
    ck_assert_uint_eq(response.total_size, sizeof(chunk));
    ck_assert_uint_eq(response.offset, 0);
    ck_assert_uint_eq(response.checksum, 0x12345678U);
    ck_assert_uint_eq(response.data_size, sizeof(chunk));
    ck_assert_mem_eq(response.data, chunk, sizeof(chunk));
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    socket_asset_response_append_status(packet,
                                        ASSET_STATUS_NOT_MODIFIED,
                                        "client-maps/test.def");
    ck_assert(socket_asset_response_parse(packet->data,
                                          packet->len,
                                          0,
                                          &response));
    ck_assert_uint_eq(response.status, ASSET_STATUS_NOT_MODIFIED);
    ck_assert_str_eq(response.path, "client-maps/test.def");
    packet_free(packet);
}
END_TEST

START_TEST(test_socket_asset_response_rejects_malformed)
{
    socket_asset_response_t response;
    uint8_t unknown[] = {0xff, 'x', '\0'};
    ck_assert(!socket_asset_response_parse(unknown,
                                           sizeof(unknown),
                                           0,
                                           &response));

    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_response_append_status(packet,
                                        ASSET_STATUS_NOT_MODIFIED,
                                        "client-maps/test.png");
    packet_append_uint8(packet, 0);
    ck_assert(!socket_asset_response_parse(packet->data,
                                           packet->len,
                                           0,
                                           &response));
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    socket_asset_response_append_ok(packet,
                                    "client-maps/test.png",
                                    3,
                                    0,
                                    0,
                                    (const uint8_t *) "four",
                                    4);
    ck_assert(!socket_asset_response_parse(packet->data,
                                           packet->len,
                                           0,
                                           &response));
    packet_free(packet);

    packet = packet_new(0, 0, 0);
    socket_asset_response_append_ok(packet,
                                    "client-maps/test.png",
                                    4,
                                    1,
                                    1,
                                    (const uint8_t *) "one",
                                    3);
    ck_assert(!socket_asset_response_parse(packet->data,
                                           packet->len,
                                           0,
                                           &response));
    packet_free(packet);
}
END_TEST

static Suite *
suite (void)
{
    Suite *s = suite_create("socket_asset");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_socket_asset_request_round_trip);
    tcase_add_test(tc_core, test_socket_asset_request_rejects_malformed);
    tcase_add_test(tc_core, test_socket_asset_response_round_trip);
    tcase_add_test(tc_core, test_socket_asset_response_rejects_malformed);

    return s;
}

void
check_server_socket_asset (void)
{
    check_run_suite(suite(), __FILE__);
}

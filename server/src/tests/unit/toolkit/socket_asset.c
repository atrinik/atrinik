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
#include <toolkit/path.h>

START_TEST(test_socket_asset_request_round_trip)
{
    uint8_t digest[ASSET_DIGEST_SIZE];
    memset(digest, 0x89, sizeof(digest));
    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_request_append(packet,
                                "client-maps/test.png",
                                0,
                                123456,
                                digest);

    socket_asset_request_t request;
    ck_assert(socket_asset_request_parse(packet->data,
                                         packet->len,
                                         0,
                                         &request));
    ck_assert_str_eq(request.path, "client-maps/test.png");
    ck_assert_uint_eq(request.offset, 0);
    ck_assert_uint_eq(request.cached_size, 123456);
    ck_assert_mem_eq(request.cached_digest, digest, sizeof(digest));
    packet_free(packet);
}
END_TEST

START_TEST(test_path_write_atomic_replaces_complete_file)
{
    char path[] = "/tmp/atrinik-path-test.XXXXXX";
    int fd = mkstemp(path);
    ck_assert_int_ne(fd, -1);
    ck_assert_int_eq(close(fd), 0);
    ck_assert_int_eq(unlink(path), 0);

    static const char first[] = "first";
    static const char second[] = "replacement";
    ck_assert(path_write_atomic(path, first, sizeof(first) - 1, 0600));
    ck_assert(path_write_atomic(path, second, sizeof(second) - 1, 0600));

    FILE *fp = fopen(path, "rb");
    ck_assert_ptr_nonnull(fp);
    char contents[sizeof(second)] = {0};
    ck_assert_uint_eq(fread(contents, 1, sizeof(second) - 1, fp),
                      sizeof(second) - 1);
    ck_assert_int_eq(fclose(fp), 0);
    ck_assert_str_eq(contents, second);
#ifndef WIN32
    struct stat sb;
    ck_assert_int_eq(stat(path, &sb), 0);
    ck_assert_int_eq(sb.st_mode & 0777, 0600);
#endif
    ck_assert_int_eq(unlink(path), 0);
}
END_TEST

START_TEST(test_socket_rendezvous_messages)
{
    const char *ticket =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char message[256];
    ck_assert(socket_rendezvous_message_render(VS(message),
                                                "client_candidate",
                                                "192.0.2.10",
                                                1730,
                                                SOCKET_CANDIDATE_NUM,
                                                ticket));
    char host[65], parsed_ticket[65];
    uint16_t port;
    ck_assert(socket_rendezvous_client_candidate_parse(message,
                                                        VS(host),
                                                        &port,
                                                        parsed_ticket));
    ck_assert_str_eq(host, "192.0.2.10");
    ck_assert_uint_eq(port, 1730);
    ck_assert_str_eq(parsed_ticket, ticket);

    ck_assert(socket_rendezvous_message_render(VS(message),
                                                "server_candidate",
                                                "2001:db8::1",
                                                1730,
                                                SOCKET_CANDIDATE_IPV6,
                                                ticket));
    socket_direct_candidate_t candidate;
    ck_assert(socket_rendezvous_server_candidate_parse(message,
                                                        ticket,
                                                        &candidate));
    ck_assert_str_eq(candidate.host, "2001:db8::1");
    ck_assert_uint_eq(candidate.port, 1730);
    ck_assert_int_eq(candidate.kind, SOCKET_CANDIDATE_IPV6);

    ck_assert(socket_rendezvous_message_render(VS(message),
                                                "complete",
                                                NULL,
                                                0,
                                                SOCKET_CANDIDATE_NUM,
                                                ticket));
    ck_assert(socket_rendezvous_complete_parse(message, ticket));
    ck_assert(!socket_rendezvous_client_candidate_parse(
        "{\"type\":\"client_candidate\",\"host\":\"example.com\","
        "\"port\":1730,\"ticket\":\"bad\"}",
        VS(host),
        &port,
        parsed_ticket));
}
END_TEST

START_TEST(test_socket_asset_request_rejects_malformed)
{
    packet_struct *packet = packet_new(0, 0, 0);
    uint8_t digest[ASSET_DIGEST_SIZE] = {0};
    socket_asset_request_append(packet, "data/listing.txt", 0, 0, digest);

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
    digest[0] = 3;
    socket_asset_request_append(packet, "data/listing.txt", 1, 2, digest);
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
    uint8_t digest[ASSET_DIGEST_SIZE];
    memset(digest, 0x12, sizeof(digest));
    packet_struct *packet = packet_new(0, 0, 0);
    socket_asset_response_append_ok(packet,
                                    "client-maps/test.def",
                                    sizeof(chunk),
                                    0,
                                    digest,
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
    ck_assert_mem_eq(response.digest, digest, sizeof(digest));
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
    uint8_t digest[ASSET_DIGEST_SIZE] = {0};
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
                                    digest,
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
                                    5,
                                    digest,
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
    tcase_add_test(tc_core, test_socket_rendezvous_messages);
    tcase_add_test(tc_core, test_path_write_atomic_replaces_complete_file);

    return s;
}

void
check_server_socket_asset (void)
{
    check_run_suite(suite(), __FILE__);
}

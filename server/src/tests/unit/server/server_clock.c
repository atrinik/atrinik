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
#include <server_clock_fake.h>

static void clock_test_setup(void) {
    server_clock_fake_install(UINT64_C(125000),
                              (server_tick_t){10},
                              (server_monotonic_t){UINT64_C(1000000)},
                              (server_wall_utc_t){INT64_C(1700000000)});
}

static void clock_test_teardown(void) {
    server_clock_fake_uninstall();
}

START_TEST(test_domains_advance_independently) {
    server_tick_t tick = server_tick_now();
    server_monotonic_t monotonic = server_monotonic_now();

    server_clock_fake_advance_ticks((server_tick_duration_t){3});
    ck_assert_uint_eq(server_tick_now().value, tick.value + 3);
    ck_assert_uint_eq(server_monotonic_now().microseconds, monotonic.microseconds);

    server_clock_fake_advance_monotonic(server_duration_from_milliseconds(25));
    ck_assert_uint_eq(server_tick_now().value, tick.value + 3);
    ck_assert_uint_eq(server_monotonic_now().microseconds,
                      monotonic.microseconds + UINT64_C(25000));
}
END_TEST

START_TEST(test_deadlines_expire_at_exact_boundary) {
    server_tick_t tick_deadline = server_tick_deadline_after((server_tick_duration_t){2});
    server_monotonic_t monotonic_deadline =
        server_monotonic_deadline_after(server_duration_from_milliseconds(10));

    server_clock_fake_advance_ticks((server_tick_duration_t){1});
    server_clock_fake_advance_monotonic(server_duration_from_milliseconds(9));
    ck_assert(!server_tick_expired(tick_deadline));
    ck_assert(!server_monotonic_expired(monotonic_deadline));

    server_clock_fake_advance_ticks((server_tick_duration_t){1});
    server_clock_fake_advance_monotonic(server_duration_from_milliseconds(1));
    ck_assert(server_tick_expired(tick_deadline));
    ck_assert(server_monotonic_expired(monotonic_deadline));
}
END_TEST

START_TEST(test_wall_clock_jumps_do_not_change_deadlines) {
    server_monotonic_t deadline = server_monotonic_deadline_after(server_duration_from_seconds(5));

    server_clock_fake_set_wall((server_wall_utc_t){INT64_C(1)});
    ck_assert(!server_monotonic_expired(deadline));
    server_clock_fake_set_wall((server_wall_utc_t){INT64_MAX});
    ck_assert(!server_monotonic_expired(deadline));

    server_clock_fake_advance_monotonic(server_duration_from_seconds(5));
    ck_assert(server_monotonic_expired(deadline));
}
END_TEST

START_TEST(test_prelogin_deadline_ignores_wall_clock_jumps) {
    socket_struct socket = {0};
    socket.prelogin_deadline =
        server_monotonic_deadline_after(server_duration_from_seconds(SOCKET_PRELOGIN_TIMEOUT));

    server_clock_fake_set_wall((server_wall_utc_t){INT64_MAX});
    ck_assert(!socket_prelogin_expired(&socket));
    server_clock_fake_advance_monotonic(server_duration_from_seconds(SOCKET_PRELOGIN_TIMEOUT));
    ck_assert(socket_prelogin_expired(&socket));
}
END_TEST

START_TEST(test_persisted_wall_deadlines_are_validated_and_clamped) {
    server_duration_t remaining;
    server_duration_t maximum = server_duration_from_seconds(60);

    ck_assert(!server_wall_utc_remaining((server_wall_utc_t){99},
                                         (server_wall_utc_t){100},
                                         maximum,
                                         &remaining));
    ck_assert_uint_eq(remaining.microseconds, 0);

    ck_assert(server_wall_utc_remaining((server_wall_utc_t){130},
                                        (server_wall_utc_t){100},
                                        maximum,
                                        &remaining));
    ck_assert_uint_eq(remaining.microseconds, server_duration_from_seconds(30).microseconds);

    ck_assert(server_wall_utc_remaining((server_wall_utc_t){INT64_MAX},
                                        (server_wall_utc_t){INT64_MIN},
                                        maximum,
                                        &remaining));
    ck_assert_uint_eq(remaining.microseconds, maximum.microseconds);
}
END_TEST

START_TEST(test_arithmetic_saturates_and_elapsed_clamps) {
    server_clock_fake_install(UINT64_C(125000),
                              (server_tick_t){UINT64_MAX - 2},
                              (server_monotonic_t){UINT64_MAX - 2},
                              (server_wall_utc_t){0});

    ck_assert_uint_eq(server_tick_deadline_after((server_tick_duration_t){10}).value, UINT64_MAX);
    ck_assert_uint_eq(server_monotonic_deadline_after((server_duration_t){10}).microseconds,
                      UINT64_MAX);

    server_clock_fake_advance_ticks((server_tick_duration_t){10});
    server_clock_fake_advance_monotonic((server_duration_t){10});
    ck_assert_uint_eq(server_tick_now().value, UINT64_MAX);
    ck_assert_uint_eq(server_monotonic_now().microseconds, UINT64_MAX);

    ck_assert_uint_eq(server_tick_difference((server_tick_t){1}, (server_tick_t){2}).value, 0);
    ck_assert_uint_eq(
        server_monotonic_difference((server_monotonic_t){1}, (server_monotonic_t){2}).microseconds,
        0);
}
END_TEST

START_TEST(test_tick_duration_conversions_are_checked) {
    server_tick_duration_t ticks;
    server_duration_t duration;

    ck_assert(server_duration_to_ticks(server_duration_from_seconds(1), &ticks));
    ck_assert_uint_eq(ticks.value, 8);
    ck_assert(server_duration_to_ticks((server_duration_t){UINT64_C(125001)}, &ticks));
    ck_assert_uint_eq(ticks.value, 2);
    ck_assert(server_ticks_to_duration((server_tick_duration_t){8}, &duration));
    ck_assert_uint_eq(duration.microseconds, UINT64_C(1000000));
    ck_assert(!server_ticks_to_duration((server_tick_duration_t){UINT64_MAX}, &duration));
}
END_TEST

START_TEST(test_fake_cleanup_restores_production_clock) {
    server_clock_fake_uninstall();
    server_clock_init(UINT64_C(500000), (server_tick_t){77});
    server_clock_fake_install(UINT64_C(1),
                              (server_tick_t){1},
                              (server_monotonic_t){1},
                              (server_wall_utc_t){1});
    server_clock_fake_uninstall();

    ck_assert_uint_eq(server_tick_now().value, 77);
    server_tick_duration_t ticks;
    ck_assert(server_duration_to_ticks(server_duration_from_seconds(1), &ticks));
    ck_assert_uint_eq(ticks.value, 2);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("server_clock");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, clock_test_setup, clock_test_teardown);
    tcase_add_test(tc_core, test_domains_advance_independently);
    tcase_add_test(tc_core, test_deadlines_expire_at_exact_boundary);
    tcase_add_test(tc_core, test_wall_clock_jumps_do_not_change_deadlines);
    tcase_add_test(tc_core, test_prelogin_deadline_ignores_wall_clock_jumps);
    tcase_add_test(tc_core, test_persisted_wall_deadlines_are_validated_and_clamped);
    tcase_add_test(tc_core, test_arithmetic_saturates_and_elapsed_clamps);
    tcase_add_test(tc_core, test_tick_duration_conversions_are_checked);
    tcase_add_test(tc_core, test_fake_cleanup_restores_production_clock);
    suite_add_tcase(s, tc_core);

    return s;
}

void check_server_server_clock(void) {
    check_run_suite(suite(), __FILE__);
}

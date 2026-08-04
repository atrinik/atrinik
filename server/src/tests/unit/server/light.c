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

START_TEST(test_light_level_anchors) {
    ck_assert_uint_eq(light_level_from_raw(-1), 0);
    ck_assert_uint_eq(light_level_from_raw(0), 0);
    ck_assert_uint_eq(light_level_from_raw(20), 45);
    ck_assert_uint_eq(light_level_from_raw(40), 80);
    ck_assert_uint_eq(light_level_from_raw(80), 120);
    ck_assert_uint_eq(light_level_from_raw(160), 165);
    ck_assert_uint_eq(light_level_from_raw(320), 215);
    ck_assert_uint_eq(light_level_from_raw(640), 245);
    ck_assert_uint_eq(light_level_from_raw(1280), 255);
    ck_assert_uint_eq(light_level_from_raw(4096), 255);
}
END_TEST

START_TEST(test_light_level_interpolation) {
    ck_assert_uint_eq(light_level_from_raw(10), 23);
    ck_assert_uint_eq(light_level_from_raw(30), 63);
    ck_assert_uint_eq(light_level_from_raw(60), 100);

    uint8_t previous = light_level_from_raw(0);
    for (int raw_light = 1; raw_light <= 2048; raw_light++) {
        uint8_t level = light_level_from_raw(raw_light);
        ck_assert_uint_ge(level, previous);
        previous = level;
    }
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("light");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_light_level_anchors);
    tcase_add_test(tc_core, test_light_level_interpolation);

    return s;
}

void check_server_light(void) {
    check_run_suite(suite(), __FILE__);
}

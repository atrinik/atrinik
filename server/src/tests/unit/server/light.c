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
#include <arch.h>
#include <object.h>

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

static void link_stacked_maps(mapstruct *lower, mapstruct *upper) {
    lower->tile_map[TILED_UP] = upper;
    upper->tile_map[TILED_DOWN] = lower;
}

static void add_light_source(mapstruct *map, int x, int y) {
    object *marker = arch_get("letter");
    marker->x = x;
    marker->y = y;
    object_insert_map(marker, map, NULL, 0);
    adjust_light_source(map, x, y, 13);
}

START_TEST(test_light_mask_propagates_in_three_dimensions) {
    mapstruct *lower = get_empty_map(9, 9);
    mapstruct *upper = get_empty_map(9, 9);
    link_stacked_maps(lower, upper);

    add_light_source(lower, 4, 4);

    ck_assert_int_eq(GET_MAP_SPACE_PTR(lower, 4, 4)->light_source_value, 1280);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(upper, 4, 4)->light_source_value, 640);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(upper, 5, 4)->light_source_value, 160);
}
END_TEST

START_TEST(test_light_mask_is_blocked_by_floors_in_both_directions) {
    mapstruct *lower = get_empty_map(9, 9);
    mapstruct *upper = get_empty_map(9, 9);
    link_stacked_maps(lower, upper);

    object *floor = arch_get("water_still");
    floor->x = 4;
    floor->y = 4;
    object_insert_map(floor, upper, NULL, 0);

    add_light_source(lower, 4, 4);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(upper, 4, 4)->light_source_value, 0);

    adjust_light_source(lower, 4, 4, -13);
    add_light_source(upper, 4, 4);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(lower, 4, 4)->light_source_value, 0);
}
END_TEST

START_TEST(test_light_mask_lights_exposed_upper_wall_face) {
    mapstruct *lower = get_empty_map(9, 9);
    mapstruct *upper = get_empty_map(9, 9);
    link_stacked_maps(lower, upper);

    object *floor = arch_get("water_still");
    floor->x = 5;
    floor->y = 4;
    object_insert_map(floor, upper, NULL, 0);
    GET_MAP_SPACE_PTR(upper, 5, 4)->flags |= P_BLOCKSVIEW;

    add_light_source(lower, 4, 4);

    ck_assert_int_eq(GET_MAP_SPACE_PTR(upper, 5, 4)->light_source_value, 160);
}
END_TEST

START_TEST(test_light_mask_recalculates_around_opaque_cells) {
    mapstruct *map = get_empty_map(9, 9);
    add_light_source(map, 3, 4);
    ck_assert_int_gt(GET_MAP_SPACE_PTR(map, 5, 4)->light_source_value, 0);

    GET_MAP_SPACE_PTR(map, 4, 4)->flags |= P_BLOCKSVIEW;
    recalculate_light_sources(map);
    ck_assert_int_gt(GET_MAP_SPACE_PTR(map, 4, 4)->light_source_value, 0);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(map, 5, 4)->light_source_value, 0);

    GET_MAP_SPACE_PTR(map, 4, 4)->flags &= ~P_BLOCKSVIEW;
    recalculate_light_sources(map);
    ck_assert_int_gt(GET_MAP_SPACE_PTR(map, 5, 4)->light_source_value, 0);
}
END_TEST

START_TEST(test_loaded_map_light_check_is_idempotent) {
    mapstruct *map = get_empty_map(9, 9);
    add_light_source(map, 4, 4);
    int expected = GET_MAP_SPACE_PTR(map, 4, 4)->light_source_value;

    check_light_source_list(map);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(map, 4, 4)->light_source_value, expected);

    check_light_source_list(map);
    ck_assert_int_eq(GET_MAP_SPACE_PTR(map, 4, 4)->light_source_value, expected);
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
    tcase_add_test(tc_core, test_light_mask_propagates_in_three_dimensions);
    tcase_add_test(tc_core, test_light_mask_is_blocked_by_floors_in_both_directions);
    tcase_add_test(tc_core, test_light_mask_lights_exposed_upper_wall_face);
    tcase_add_test(tc_core, test_light_mask_recalculates_around_opaque_cells);
    tcase_add_test(tc_core, test_loaded_map_light_check_is_idempotent);

    return s;
}

void check_server_light(void) {
    check_run_suite(suite(), __FILE__);
}

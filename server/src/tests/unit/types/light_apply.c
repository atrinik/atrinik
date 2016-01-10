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
#include <stdarg.h>
#include <arch.h>
#include <player.h>
#include <object.h>

/*
 * Player applies a torch on the ground. Ensure the torch is lit and not
 * applied.
 */
START_TEST(test_light_apply_apply_1)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = object_insert_map(arch_get("torch"), map, NULL, 0);
    player_apply(pl, torch, 0, 0);
    ck_assert(!QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], NULL);
}
END_TEST

/*
 * Player applies a lit torch on the ground Ensure the torch is extinguished and
 * not applied.
 */
START_TEST(test_light_apply_apply_2)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = object_insert_map(arch_get("torch"), map, NULL, 0);
    manual_apply(torch, torch, 0);
    player_apply(pl, torch, 0, 0);
    ck_assert(!QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_eq(torch->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], NULL);
}
END_TEST

/*
 * Player applies a torch in his inventory. Ensure it's lit and applied.
 */
START_TEST(test_light_apply_apply_3)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = object_insert_into(arch_get("torch"), pl, 0);
    player_apply(pl, torch, 0, 0);
    ck_assert(QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], torch);
}
END_TEST

/*
 * Player applies a lit torch in his inventory. Ensure it was extinguished and
 * not applied.
 */
START_TEST(test_light_apply_apply_4)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = object_insert_map(arch_get("torch"), map, NULL, 0);
    manual_apply(torch, torch, 0);
    ck_assert(!QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    object_remove(torch, 0);
    torch = object_insert_into(torch, pl, 0);
    ck_assert(!QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], NULL);

    player_apply(pl, torch, 0, 0);

    ck_assert(QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], torch);
}
END_TEST

/*
 * Player applies a torch in his inventory, then a torch on the ground. Ensure
 * both torches are lit, but only the one in inventory is applied.
 */
START_TEST(test_light_apply_apply_5)
{
    mapstruct *map;
    object *pl, *torch, *torch2;

    check_setup_env_pl(&map, &pl);

    torch = object_insert_into(arch_get("torch"), pl, 0);
    ck_assert_ptr_ne(torch, NULL);
    torch2 = object_insert_map(arch_get("torch"), map, NULL, 0);
    ck_assert_ptr_ne(torch2, NULL);

    player_apply(pl, torch, 0, 0);
    ck_assert(QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], torch);
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert(!QUERY_FLAG(torch2, FLAG_APPLIED));
    ck_assert_int_eq(torch2->glow_radius, 0);

    player_apply(pl, torch2, 0, 0);
    ck_assert(QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], torch);
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert(!QUERY_FLAG(torch2, FLAG_APPLIED));
    ck_assert_int_ne(torch2->glow_radius, 0);
}
END_TEST

/*
 * Player applies stacked torches, ensure a new torch is created and lit and
 * the original is not lit. Then apply the original and ensure it's lit and
 * applied, and that the old one gets extinguished and not applied.
 */
START_TEST(test_light_apply_apply_6)
{
    mapstruct *map;
    object *pl, *torch, *torch2;

    check_setup_env_pl(&map, &pl);

    torch = arch_get("torch");
    torch->nrof = 2;
    torch = object_insert_into(torch, pl, 0);
    ck_assert_ptr_ne(torch, NULL);

    player_apply(pl, torch, 0, 0);
    torch2 = torch;
    torch = CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT];

    ck_assert_ptr_ne(torch, torch2);
    ck_assert_ptr_ne(torch, NULL);
    ck_assert(QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_ne(torch->glow_radius, 0);
    ck_assert_uint_eq(torch->nrof, 1);
    ck_assert(!QUERY_FLAG(torch2, FLAG_APPLIED));
    ck_assert_int_eq(torch2->glow_radius, 0);

    player_apply(pl, torch2, 0, 0);
    ck_assert(!QUERY_FLAG(torch, FLAG_APPLIED));
    ck_assert_int_eq(torch->glow_radius, 0);
    ck_assert(QUERY_FLAG(torch2, FLAG_APPLIED));
    ck_assert_int_ne(torch2->glow_radius, 0);
    ck_assert_ptr_eq(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT], torch2);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("light_apply");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_light_apply_apply_1);
    tcase_add_test(tc_core, test_light_apply_apply_2);
    tcase_add_test(tc_core, test_light_apply_apply_3);
    tcase_add_test(tc_core, test_light_apply_apply_4);
    tcase_add_test(tc_core, test_light_apply_apply_5);
    tcase_add_test(tc_core, test_light_apply_apply_6);

    return s;
}

void check_types_light_apply(void)
{
    check_run_suite(suite(), __FILE__);
}

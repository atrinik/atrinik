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
#include <arch.h>
#include <monster_data.h>

START_TEST(test_attack_is_melee_range) {
    mapstruct *map;
    object *pl, *tmp, *tmp2;

    check_setup_env_pl(&map, &pl);
    ck_assert(attack_is_melee_range(pl, pl));

    tmp = arch_get("gazer_dread");
    ck_assert(!attack_is_melee_range(tmp, tmp));
    ck_assert(!attack_is_melee_range(pl, tmp));
    ck_assert(!attack_is_melee_range(tmp, pl));

    tmp->x = pl->x + 1;
    tmp->y = pl->y + 1;
    tmp = object_insert_map(tmp, pl->map, NULL, 0);
    ck_assert(attack_is_melee_range(tmp, tmp));
    ck_assert(attack_is_melee_range(pl, tmp));
    ck_assert(attack_is_melee_range(tmp, pl));

    tmp2 = arch_get("raas");
    ck_assert(!attack_is_melee_range(tmp2, tmp2));
    ck_assert(!attack_is_melee_range(pl, tmp2));
    ck_assert(!attack_is_melee_range(tmp2, pl));

    tmp2->x = pl->x + 2;
    tmp2->y = pl->y + 2;
    tmp2 = object_insert_map(tmp2, pl->map, NULL, 0);
    ck_assert(attack_is_melee_range(tmp2, tmp2));
    ck_assert(!attack_is_melee_range(pl, tmp2));
    ck_assert(!attack_is_melee_range(tmp2, pl));
    ck_assert(attack_is_melee_range(tmp, tmp2));
    ck_assert(attack_is_melee_range(tmp2, tmp));
}
END_TEST

START_TEST(test_attack_roll_adjust_describes_moved_target_penalty) {
    mapstruct *map;
    object *pl, *monster;

    check_setup_env_pl(&map, &pl);
    monster = arch_get("kobold");
    monster->x = pl->x + 1;
    monster->y = pl->y;
    monster = object_insert_map(monster, map, NULL, 0);
    monster_data_init(monster);

    CLEAR_FLAG(monster, FLAG_BLIND);
    CLEAR_FLAG(monster, FLAG_SCARED);
    CLEAR_FLAG(monster, FLAG_CONFUSED);
    CLEAR_FLAG(pl, FLAG_SCARED);
    CLEAR_FLAG(pl, FLAG_UNAGGRESSIVE);
    CLEAR_FLAG(pl, FLAG_CONFUSED);
    CLEAR_MULTI_FLAG(pl, FLAG_IS_INVISIBLE);
    CLEAR_MULTI_FLAG(pl, FLAG_FLYING);
    CLEAR_MULTI_FLAG(monster, FLAG_FLYING);

    monster_data_enemy_update(monster, pl);

    object_remove(pl, 0);
    pl->x = monster->x;
    pl->y = monster->y + 1;
    pl = object_insert_map(pl, map, NULL, 0);

    rv_vector rv;
    ck_assert(get_rangevector(monster, pl, &rv, 0));
    monster->direction = rv.direction;
    pl->direction = absdir(monster->direction + 4);

    StringBuffer *modifiers = stringbuffer_new();
    ck_assert_int_eq(attack_roll_adjust(pl, monster, modifiers), -6);
    ck_assert_str_eq(stringbuffer_data(modifiers), "target moved -6");
    stringbuffer_free(modifiers);
}
END_TEST

START_TEST(test_attack_roll_adjust_describes_positional_bonuses) {
    mapstruct *map;
    object *pl, *target;

    check_setup_env_pl(&map, &pl);
    target = arch_get("kobold");
    target->x = pl->x + 1;
    target->y = pl->y;
    target = object_insert_map(target, map, NULL, 0);

    CLEAR_FLAG(pl, FLAG_BLIND);
    CLEAR_FLAG(pl, FLAG_SCARED);
    CLEAR_FLAG(pl, FLAG_CONFUSED);
    CLEAR_FLAG(target, FLAG_SCARED);
    CLEAR_FLAG(target, FLAG_UNAGGRESSIVE);
    CLEAR_FLAG(target, FLAG_CONFUSED);
    CLEAR_MULTI_FLAG(target, FLAG_IS_INVISIBLE);
    CLEAR_MULTI_FLAG(pl, FLAG_FLYING);
    CLEAR_MULTI_FLAG(target, FLAG_FLYING);

    rv_vector rv;
    ck_assert(get_rangevector(pl, target, &rv, 0));
    pl->direction = rv.direction;

    target->direction = pl->direction;
    StringBuffer *modifiers = stringbuffer_new();
    ck_assert_int_eq(attack_roll_adjust(target, pl, modifiers), 5);
    ck_assert_str_eq(stringbuffer_data(modifiers), "backstab +5");
    stringbuffer_free(modifiers);

    target->direction = absdir(pl->direction + 1);
    modifiers = stringbuffer_new();
    ck_assert_int_eq(attack_roll_adjust(target, pl, modifiers), 2);
    ck_assert_str_eq(stringbuffer_data(modifiers), "sidestab +2");
    stringbuffer_free(modifiers);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("attack");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_attack_is_melee_range);
    tcase_add_test(tc_core, test_attack_roll_adjust_describes_positional_bonuses);
    tcase_add_test(tc_core, test_attack_roll_adjust_describes_moved_target_penalty);

    return s;
}

void check_server_attack(void) {
    check_run_suite(suite(), __FILE__);
}

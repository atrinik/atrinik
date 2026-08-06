/*************************************************************************
 * Atrinik server deterministic trap skill regression tests.
 ************************************************************************/

#include <global.h>
#include <server_item.h>
#include <check.h>
#include <checkstd.h>
#include <check_utils.h>
#include <arch.h>
#include <object_methods.h>
#include <player.h>
#include <rune.h>
#include <container.h>

START_TEST(test_trap_skill_ratings_scale_with_character_skill_and_stat) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    pl->level = 30;
    pl->stats.Int = 18;
    pl->stats.Dex = 18;
    CONTR(pl)->skill_ptr[SK_FIND_TRAPS]->level = 10;
    CONTR(pl)->skill_ptr[SK_REMOVE_TRAPS]->level = 10;

    ck_assert_int_eq(trap_skill_rating(pl, SK_FIND_TRAPS), 37);
    ck_assert_int_eq(trap_skill_rating(pl, SK_REMOVE_TRAPS), 37);
}
END_TEST

START_TEST(test_generated_trap_level_distribution) {
    int comfortable = 0;
    int challenging = 0;
    int dangerous = 0;
    int exceptional = 0;

    for (int i = 0; i < 20000; i++) {
        int level = rune_generate_level(50);

        ck_assert_int_ge(level, 40);
        ck_assert_int_le(level, 70);

        if (level <= 50) {
            comfortable++;
        } else if (level <= 55) {
            challenging++;
        } else if (level <= 60) {
            dangerous++;
        } else {
            exceptional++;
        }
    }

    ck_assert_int_gt(comfortable, 11000);
    ck_assert_int_lt(comfortable, 13000);
    ck_assert_int_gt(challenging, 4000);
    ck_assert_int_lt(challenging, 6000);
    ck_assert_int_gt(dangerous, 1500);
    ck_assert_int_lt(dangerous, 2500);
    ck_assert_int_gt(exceptional, 700);
    ck_assert_int_lt(exceptional, 1300);
}
END_TEST

START_TEST(test_generated_trap_inherits_monster_base_experience) {
    object *monster = arch_get("goblin");
    monster->stats.exp = 123;

    treasure_list_t *traps = treasure_list_find("traps");
    ck_assert_ptr_nonnull(traps);
    treasure_generate(traps, monster, 10, 0);

    object *trap = monster->inv;
    ck_assert_ptr_nonnull(trap);
    ck_assert_int_eq(trap->type, RUNE);
    ck_assert_int_eq(trap->stats.exp, monster->stats.exp);

    object_destroy(monster);
}
END_TEST

START_TEST(test_trap_see_is_deterministic_at_capability_boundary) {
    mapstruct *map;
    object *pl, *trap;

    check_setup_env_pl(&map, &pl);
    trap = arch_get("rune_fire");
    trap->x = pl->x + 1;
    trap->y = pl->y;
    trap->level = 10;
    trap = object_insert_map(trap, map, NULL, 0);

    int result = trap_see(pl, trap, 9);
    for (int i = 0; i < 100; i++) {
        ck_assert_int_eq(trap_see(pl, trap, 9), result);
    }
}
END_TEST

START_TEST(test_trap_find_can_succeed_or_fail_at_equal_rating) {
    mapstruct *map;
    object *pl;
    int successes = 0;

    check_setup_env_pl(&map, &pl);
    for (int i = 0; i < 1000; i++) {
        object *trap = arch_get("rune_fire");
        trap->level = 10;
        trap->stats.Int = 10;
        successes += trap_see(pl, trap, 10);
        object_destroy(trap);
    }

    ck_assert_int_gt(successes, 600);
    ck_assert_int_lt(successes, 800);
}
END_TEST

START_TEST(test_trap_disarm_always_retains_failure_and_trip_risk) {
    mapstruct *map;
    object *pl;
    int successes = 0;
    int tripped = 0;

    check_setup_env_pl(&map, &pl);
    pl->level = MAXLEVEL;
    pl->stats.Dex = MAX_STAT;

    for (int i = 0; i < 1000; i++) {
        object *trap = arch_get("rune_fire");
        trap->x = pl->x + 1;
        trap->y = pl->y;
        trap->level = 1;
        trap->stats.Int = 1;
        trap->stats.dam = 0;
        trap = object_insert_map(trap, map, NULL, 0);

        int result = trap_disarm(pl, trap);
        if (result == TRAP_DISARM_SUCCESS) {
            successes++;
        } else if (result == TRAP_DISARM_TRIPPED) {
            tripped++;
        }

        if (!QUERY_FLAG(trap, FLAG_REMOVED)) {
            object_remove(trap, 0);
        }
        object_destroy(trap);
    }

    ck_assert_int_gt(successes, 850);
    ck_assert_int_lt(successes, 950);
    ck_assert_int_gt(tripped, 20);
    ck_assert_int_lt(tripped, 90);
}
END_TEST

START_TEST(test_trap_successes_award_skill_and_character_experience_once) {
    mapstruct *map;
    object *pl, *trap;

    check_setup_env_pl(&map, &pl);
    pl->stats.Int = 10;
    pl->stats.Dex = 10;

    object *find_skill = CONTR(pl)->skill_ptr[SK_FIND_TRAPS];
    object *remove_skill = CONTR(pl)->skill_ptr[SK_REMOVE_TRAPS];
    ck_assert_int_eq(find_skill->level, 1);
    ck_assert_int_eq(remove_skill->level, 1);
    ck_assert(!QUERY_FLAG(find_skill, FLAG_STAND_STILL));
    ck_assert(!QUERY_FLAG(remove_skill, FLAG_STAND_STILL));

    int64_t character_exp = pl->stats.exp;
    int64_t find_exp = find_skill->stats.exp;
    int64_t remove_exp = remove_skill->stats.exp;

    int64_t expected_find_gain = 0;
    for (int i = 0; i < 100 && expected_find_gain == 0; i++) {
        trap = arch_get("rune_fire");
        trap->level = 1;
        trap->stats.Int = 2;
        trap->stats.exp = 100;
        int64_t expected = calc_skill_exp(pl, trap, find_skill->level);
        if (trap_see(pl, trap, trap_skill_rating(pl, SK_FIND_TRAPS))) {
            expected_find_gain = expected;
        } else {
            object_destroy(trap);
            trap = NULL;
        }
    }

    ck_assert_ptr_nonnull(trap);
    int64_t find_gain = find_skill->stats.exp - find_exp;
    ck_assert_int_eq(find_gain, expected_find_gain);
    ck_assert_int_eq(pl->stats.exp - character_exp, find_gain / 5);

    ck_assert_int_eq(trap_see(pl, trap, trap_skill_rating(pl, SK_FIND_TRAPS)), 1);
    ck_assert_int_eq(find_skill->stats.exp - find_exp, find_gain);
    object_destroy(trap);

    int64_t expected_remove_gain = 0;
    trap = NULL;
    for (int i = 0; i < 100 && expected_remove_gain == 0; i++) {
        trap = arch_get("rune_fire");
        trap->x = pl->x + 1;
        trap->y = pl->y;
        trap->level = 1;
        trap->stats.Int = 1;
        trap->stats.dam = 0;
        trap->stats.exp = 100;
        trap = object_insert_map(trap, map, NULL, 0);
        int64_t expected = calc_skill_exp(pl, trap, remove_skill->level);
        if (trap_disarm(pl, trap) == TRAP_DISARM_SUCCESS) {
            expected_remove_gain = expected;
        } else {
            if (!QUERY_FLAG(trap, FLAG_REMOVED)) {
                object_remove(trap, 0);
            }
            object_destroy(trap);
            trap = NULL;
        }
    }

    ck_assert_ptr_nonnull(trap);
    int64_t remove_gain = remove_skill->stats.exp - remove_exp;
    ck_assert_int_eq(remove_gain, expected_remove_gain);
    ck_assert_int_eq(pl->stats.exp - character_exp, find_gain / 5 + remove_gain / 5);
    object_destroy(trap);

    link_player_skills(pl);
    ck_assert_int_eq(pl->stats.exp, find_skill->stats.exp / 5 + remove_skill->stats.exp / 5);
}
END_TEST

START_TEST(test_traps_auto_disarm_container) {
    mapstruct *map;
    object *pl, *container, *trap;

    check_setup_env_pl(&map, &pl);
    pl->level = MAXLEVEL;
    ck_assert_ptr_nonnull(CONTR(pl)->skill_ptr[SK_FIND_TRAPS]);
    ck_assert_ptr_nonnull(CONTR(pl)->skill_ptr[SK_REMOVE_TRAPS]);

    container = arch_get("sack");
    container->x = pl->x + 1;
    container->y = pl->y;
    container = object_insert_map(container, map, NULL, 0);

    trap = arch_get("rune_fire");
    trap->level = 1;
    trap = object_insert_into(trap, container, 0);

    object_apply(container, pl, 0);

    if (container->inv != NULL) {
        ck_assert_int_ne(container->inv->type, RUNE);
        ck_assert(QUERY_FLAG(container->inv, FLAG_IS_USED_UP));
    }
    ck_assert_ptr_eq(CONTR(pl)->container, container);
}
END_TEST

START_TEST(test_corpse_trap_discovery_cancels_first_open) {
    mapstruct *map;
    object *pl, *corpse, *trap;

    check_setup_env_pl(&map, &pl);
    corpse = arch_get("corpse_default");
    corpse->x = pl->x + 1;
    corpse->y = pl->y;
    corpse = object_insert_map(corpse, map, NULL, 0);

    trap = arch_get("rune_fire");
    trap->level = 1;
    trap->stats.Int = 1;
    trap->stats.dam = 0;
    object_insert_into(trap, corpse, 0);

    object_apply(corpse, pl, 0);

    ck_assert_ptr_null(CONTR(pl)->container);
    ck_assert(!QUERY_FLAG(corpse, FLAG_APPLIED));
    ck_assert(!QUERY_FLAG(corpse, FLAG_BEEN_APPLIED));
    ck_assert(QUERY_FLAG(corpse, FLAG_IS_TRAPPED));
    char *name = object_get_base_name_s(corpse, pl);
    ck_assert_ptr_nonnull(strstr(name, "(trapped)"));
    free(name);

    object_apply(corpse, pl, 0);

    ck_assert_ptr_eq(CONTR(pl)->container, corpse);
    ck_assert(QUERY_FLAG(corpse, FLAG_APPLIED));
    ck_assert(QUERY_FLAG(corpse, FLAG_BEEN_APPLIED));
}
END_TEST

START_TEST(test_auto_disarm_trip_does_not_spring_reusable_trap_twice) {
    mapstruct *map;
    object *pl;
    int remaining = 0;

    check_setup_env_pl(&map, &pl);
    pl->level = MAXLEVEL;

    for (int i = 0; i < 250; i++) {
        object *container = arch_get("sack");
        container->x = pl->x + 1;
        container->y = pl->y;
        container = object_insert_map(container, map, NULL, 0);

        object *trap = arch_get("rune_fire");
        trap->level = 1;
        trap->stats.Int = 1;
        trap->stats.hp = 2;
        trap->stats.dam = 0;
        trap = object_insert_into(trap, container, 0);

        object_apply(container, pl, 0);
        if (container->inv != NULL) {
            remaining++;
            ck_assert_int_eq(container->inv->type, RUNE);
            ck_assert_int_eq(container->inv->stats.hp, 1);
            ck_assert(!QUERY_FLAG(container->inv, FLAG_IS_USED_UP));
        }

        container_close(pl, container);
        object_remove(container, 0);
        object_destroy(container);
    }

    ck_assert_int_gt(remaining, 5);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("rune");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_trap_skill_ratings_scale_with_character_skill_and_stat);
    tcase_add_test(tc_core, test_generated_trap_level_distribution);
    tcase_add_test(tc_core, test_generated_trap_inherits_monster_base_experience);
    tcase_add_test(tc_core, test_trap_see_is_deterministic_at_capability_boundary);
    tcase_add_test(tc_core, test_trap_find_can_succeed_or_fail_at_equal_rating);
    tcase_add_test(tc_core, test_trap_disarm_always_retains_failure_and_trip_risk);
    tcase_add_test(tc_core, test_trap_successes_award_skill_and_character_experience_once);
    tcase_add_test(tc_core, test_traps_auto_disarm_container);
    tcase_add_test(tc_core, test_corpse_trap_discovery_cancels_first_open);
    tcase_add_test(tc_core, test_auto_disarm_trip_does_not_spring_reusable_trap_twice);

    return s;
}

void check_server_rune(void) {
    check_run_suite(suite(), __FILE__);
}

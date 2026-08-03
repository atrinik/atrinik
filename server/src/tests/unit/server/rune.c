/*************************************************************************
 * Atrinik server deterministic trap skill regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <arch.h>
#include <object_methods.h>
#include <player.h>
#include <rune.h>

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

    for (int i = 0; i < 100; i++) {
        ck_assert_int_eq(trap_see(pl, trap, 9), 0);
    }
    ck_assert_int_eq(trap_see(pl, trap, 10), 1);
}
END_TEST

START_TEST(test_trap_disarm_succeeds_once_with_sufficient_capability) {
    mapstruct *map;
    object *pl, *trap;

    check_setup_env_pl(&map, &pl);
    pl->level = 10;
    pl->stats.Dex = 10;
    trap = arch_get("rune_fire");
    trap->x = pl->x + 1;
    trap->y = pl->y;
    trap->level = 10;
    trap = object_insert_map(trap, map, NULL, 0);

    ck_assert_int_eq(trap_disarm(pl, trap), 1);
    ck_assert(QUERY_FLAG(trap, FLAG_REMOVED));
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

    trap = arch_get("rune_fire");
    trap->x = pl->x + 1;
    trap->y = pl->y;
    trap->level = 1;
    trap->stats.Int = 20;
    trap->stats.exp = 100;
    trap = object_insert_map(trap, map, NULL, 0);

    ck_assert_int_eq(trap_see(pl, trap, trap_skill_rating(pl, SK_FIND_TRAPS)), 1);
    int64_t find_gain = find_skill->stats.exp - find_exp;
    ck_assert_int_gt(find_gain, 0);
    ck_assert_int_eq(pl->stats.exp - character_exp, find_gain / 5);

    ck_assert_int_eq(trap_see(pl, trap, trap_skill_rating(pl, SK_FIND_TRAPS)), 1);
    ck_assert_int_eq(find_skill->stats.exp - find_exp, find_gain);

    ck_assert_int_eq(trap_disarm(pl, trap), 1);
    int64_t remove_gain = remove_skill->stats.exp - remove_exp;
    ck_assert_int_gt(remove_gain, 0);
    ck_assert_int_eq(pl->stats.exp - character_exp, find_gain / 5 + remove_gain / 5);

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

    ck_assert(QUERY_FLAG(trap, FLAG_REMOVED));
    ck_assert_ptr_null(container->inv);
    ck_assert_ptr_eq(CONTR(pl)->container, container);
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
    tcase_add_test(tc_core, test_trap_disarm_succeeds_once_with_sufficient_capability);
    tcase_add_test(tc_core, test_trap_successes_award_skill_and_character_experience_once);
    tcase_add_test(tc_core, test_traps_auto_disarm_container);

    return s;
}

void check_server_rune(void) {
    check_run_suite(suite(), __FILE__);
}

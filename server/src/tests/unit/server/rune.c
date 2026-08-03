/*************************************************************************
 * Atrinik server deterministic trap skill regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <arch.h>

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
    pl->stats.Dex = 0;
    trap = arch_get("rune_fire");
    trap->x = pl->x + 1;
    trap->y = pl->y;
    trap->level = 10;
    trap = object_insert_map(trap, map, NULL, 0);

    ck_assert_int_eq(trap_disarm(pl, trap), 1);
    ck_assert(QUERY_FLAG(trap, FLAG_REMOVED));
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("rune");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_trap_see_is_deterministic_at_capability_boundary);
    tcase_add_test(tc_core, test_trap_disarm_succeeds_once_with_sufficient_capability);

    return s;
}

void check_server_rune(void) {
    check_run_suite(suite(), __FILE__);
}

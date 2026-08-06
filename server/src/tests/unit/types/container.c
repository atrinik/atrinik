/*************************************************************************
 * Atrinik server corpse lifecycle regression tests.
 ************************************************************************/

#include <global.h>
#include <object_methods.h>
#include <check.h>
#include <checkstd.h>
#include <check_utils.h>
#include <arch.h>
#include <container.h>
#include <object.h>

START_TEST(test_corpse_decay_windows_shorten_with_search_and_empty_state) {
    object *corpse = arch_get("corpse_default");
    ck_assert(container_is_corpse(corpse));

    corpse->last_eat = corpse->stats.food;
    container_update_corpse_decay(corpse);
    ck_assert_int_eq(corpse->stats.food, CORPSE_DECAY_FRESH);
    ck_assert_int_eq(corpse->last_eat, CORPSE_DECAY_FRESH);

    object *loot = object_insert_into(arch_get("ambercoin"), corpse, 0);
    SET_FLAG(corpse, FLAG_BEEN_APPLIED);
    container_update_corpse_decay(corpse);
    ck_assert_int_eq(corpse->stats.food, CORPSE_DECAY_SEARCHED);
    ck_assert_int_eq(corpse->last_eat, CORPSE_DECAY_SEARCHED);

    object_remove(loot, 0);
    object_destroy(loot);
    container_update_corpse_decay(corpse);
    ck_assert_int_eq(corpse->stats.food, CORPSE_DECAY_EMPTY);
    ck_assert_int_eq(corpse->last_eat, CORPSE_DECAY_EMPTY);

    object_destroy(corpse);
}
END_TEST

START_TEST(test_empty_personalized_corpse_skips_unlock_decay_phase) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    object *corpse = arch_get("corpse_default");
    corpse->x = pl->x + 1;
    corpse->y = pl->y;
    corpse->sub_type = ST1_CONTAINER_CORPSE_player;
    FREE_AND_ADD_REF_HASH(corpse->slaying, pl->name);
    SET_FLAG(corpse, FLAG_BEEN_APPLIED);
    corpse->stats.food = 1;
    corpse->last_eat = 1;
    corpse = object_insert_map(corpse, map, NULL, 0);
    tag_t tag = corpse->count;

    ck_assert_int_eq(common_object_process_pre(corpse), 1);
    ck_assert(OBJECT_DESTROYED(corpse, tag));
}
END_TEST

START_TEST(test_searched_corpse_with_loot_retains_unlock_phase) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    object *corpse = arch_get("corpse_default");
    corpse->x = pl->x + 1;
    corpse->y = pl->y;
    corpse->sub_type = ST1_CONTAINER_CORPSE_player;
    FREE_AND_ADD_REF_HASH(corpse->slaying, pl->name);
    SET_FLAG(corpse, FLAG_BEEN_APPLIED);
    corpse->stats.food = 1;
    corpse->last_eat = 1;
    object_insert_into(arch_get("ambercoin"), corpse, 0);
    corpse = object_insert_map(corpse, map, NULL, 0);
    tag_t tag = corpse->count;

    ck_assert_int_eq(common_object_process_pre(corpse), 1);
    ck_assert(!OBJECT_DESTROYED(corpse, tag));
    ck_assert_ptr_null(corpse->slaying);
    ck_assert_int_eq(corpse->stats.food, CORPSE_DECAY_SEARCHED);
    ck_assert_int_eq(corpse->last_eat, CORPSE_DECAY_SEARCHED);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("container");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_corpse_decay_windows_shorten_with_search_and_empty_state);
    tcase_add_test(tc_core, test_empty_personalized_corpse_skips_unlock_decay_phase);
    tcase_add_test(tc_core, test_searched_corpse_with_loot_retains_unlock_phase);

    return s;
}

void check_types_container(void) {
    check_run_suite(suite(), __FILE__);
}

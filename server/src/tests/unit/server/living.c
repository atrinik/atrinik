/*************************************************************************
 * Atrinik server living-system regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_utils.h>
#include <arch.h>
#include <disease.h>

START_TEST(test_depletion_tooltip_lists_current_stats) {
    object *depletion = arch_get("depletion");
    set_attr_value(&depletion->stats, 0, -1);
    set_attr_value(&depletion->stats, 2, -3);

    char *tooltip = stringbuffer_finish(depletion_get_tooltip(depletion, NULL));
    ck_assert_ptr_nonnull(strstr(tooltip, "use the remove depletion spell"));
    ck_assert_ptr_null(strstr(tooltip, "restoration"));
    ck_assert_ptr_nonnull(strstr(tooltip, "or see a priest"));
    ck_assert_ptr_nonnull(strstr(tooltip, "Currently depleted: strength (1), constitution (3)."));

    free(tooltip);
    object_destroy(depletion);
}
END_TEST

START_TEST(test_depletion_force_is_applied_before_stat_updates) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    drain_specific_stat(pl, 0);

    object *depletion = object_find_arch(pl, arch_find("depletion"));
    ck_assert_ptr_nonnull(depletion);
    ck_assert(QUERY_FLAG(depletion, FLAG_APPLIED));
    ck_assert_int_eq(get_attr_value(&depletion->stats, 0), -1);
}
END_TEST

START_TEST(test_reduce_symptoms_ignores_non_progressive_symptoms) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    object *symptom = arch_get("symptom");
    symptom->value = 0;
    symptom->speed_left = 7.0;
    symptom = object_insert_into(symptom, pl, 0);

    ck_assert(!disease_reduce_symptoms(pl, 5));
    ck_assert_int_eq(symptom->value, 0);
    ck_assert_double_eq(symptom->speed_left, 7.0);
}
END_TEST

START_TEST(test_reduce_symptoms_reduces_and_reschedules_progressive_symptoms) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    object *symptom = arch_get("symptom");
    symptom->value = 30;
    symptom->speed_left = 7.0;
    symptom = object_insert_into(symptom, pl, 0);

    ck_assert(disease_reduce_symptoms(pl, 5));
    ck_assert_int_eq(symptom->value, 20);
    ck_assert_double_eq(symptom->speed_left, 0.0);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("living");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_depletion_tooltip_lists_current_stats);
    tcase_add_test(tc_core, test_depletion_force_is_applied_before_stat_updates);
    tcase_add_test(tc_core, test_reduce_symptoms_ignores_non_progressive_symptoms);
    tcase_add_test(tc_core, test_reduce_symptoms_reduces_and_reschedules_progressive_symptoms);

    return s;
}

void check_server_living(void) {
    check_run_suite(suite(), __FILE__);
}

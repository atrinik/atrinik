/*************************************************************************
 * Atrinik server disease regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <arch.h>
#include <disease.h>

START_TEST(test_reduce_symptoms_ignores_non_progressive_disease) {
    mapstruct *map;
    object *pl, *symptom;

    check_setup_env_pl(&map, &pl);

    symptom = object_insert_into(arch_get("symptom"), pl, 0);
    symptom->value = 0;
    symptom->speed_left = 0.5;

    ck_assert(!disease_reduce_symptoms(pl, 10));
    ck_assert_int_eq(symptom->value, 0);
    ck_assert_double_eq_tol(symptom->speed_left, 0.5, 0.0001);
}
END_TEST

START_TEST(test_reduce_symptoms_reduces_progressive_disease) {
    mapstruct *map;
    object *pl, *symptom;

    check_setup_env_pl(&map, &pl);

    symptom = object_insert_into(arch_get("symptom"), pl, 0);
    symptom->value = 30;
    symptom->speed_left = 0.5;

    ck_assert(disease_reduce_symptoms(pl, 10));
    ck_assert_int_eq(symptom->value, 10);
    ck_assert_double_eq_tol(symptom->speed_left, 0.0, 0.0001);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("disease");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_reduce_symptoms_ignores_non_progressive_disease);
    tcase_add_test(tc_core, test_reduce_symptoms_reduces_progressive_disease);

    return s;
}

void check_types_disease(void) {
    check_run_suite(suite(), __FILE__);
}

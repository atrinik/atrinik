/*************************************************************************
 * Atrinik server living-system regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <arch.h>

START_TEST(test_depletion_tooltip_lists_current_stats) {
    object *depletion = arch_get("depletion");
    set_attr_value(&depletion->stats, 0, -1);
    set_attr_value(&depletion->stats, 2, -3);

    char *tooltip = stringbuffer_finish(depletion_get_tooltip(depletion, NULL));
    ck_assert_ptr_nonnull(strstr(tooltip, "Currently depleted: strength (1), constitution (3)."));

    free(tooltip);
    object_destroy(depletion);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("living");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_depletion_tooltip_lists_current_stats);

    return s;
}

void check_server_living(void) {
    check_run_suite(suite(), __FILE__);
}

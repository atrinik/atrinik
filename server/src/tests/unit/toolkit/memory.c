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
#include <server_main.h>
#include <check.h>
#include <checkstd.h>
#include <check_utils.h>

START_TEST(test_memory_zero_size) {
    void *ptr = xmalloc(0);
    ck_assert_ptr_ne(ptr, NULL);
    free(ptr);

    unsigned char *zero = xcalloc(0, 0);
    ck_assert_ptr_ne(zero, NULL);
    ck_assert_uint_eq(zero[0], 0);
    free(zero);

    ptr = xmalloc(1);
    ptr = xrealloc(ptr, 0);
    ck_assert_ptr_eq(ptr, NULL);
}
END_TEST

START_TEST(test_memory_array_reallocation) {
    uint32_t *values = xcalloc(2, sizeof(*values));
    ck_assert_uint_eq(values[0], 0);
    ck_assert_uint_eq(values[1], 0);

    values[0] = 42;
    values[1] = 84;
    values = xreallocarray(values, 4, sizeof(*values));
    ck_assert_uint_eq(values[0], 42);
    ck_assert_uint_eq(values[1], 84);
    free(values);
}
END_TEST

START_TEST(test_memory_array_overflow) {
    (void)xreallocarray(NULL, SIZE_MAX, 2);
}
END_TEST

START_TEST(test_memory_initial_array_overflow) {
    (void)xmallocarray(SIZE_MAX, 2);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("memory");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_memory_zero_size);
    tcase_add_test(tc_core, test_memory_array_reallocation);
    tcase_add_test_raise_signal(tc_core, test_memory_array_overflow, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_memory_initial_array_overflow, SIGABRT);

    return s;
}

void check_server_memory(void) {
    check_run_suite(suite(), __FILE__);
}

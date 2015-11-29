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
#include <malloc.h>

START_TEST(test_isqrt)
{
    ck_assert_uint_eq(isqrt(0), 0);
    ck_assert_uint_eq(isqrt(1), 1);
    ck_assert_uint_eq(isqrt(2), 1);
    ck_assert_uint_eq(isqrt(50), 7);
    ck_assert_uint_eq(isqrt(100), 10);
    ck_assert_uint_eq(isqrt(500), 22);
}
END_TEST

START_TEST(test_nearest_pow_two_exp)
{
    ck_assert_uint_eq(nearest_pow_two_exp(0), 0);
    ck_assert_uint_eq(nearest_pow_two_exp(1), 0);
    ck_assert_uint_eq(nearest_pow_two_exp(2), 1);
    ck_assert_uint_eq(nearest_pow_two_exp(3), 2);
    ck_assert_uint_eq(nearest_pow_two_exp(4), 2);
    ck_assert_uint_eq(nearest_pow_two_exp(128), 7);
}
END_TEST

START_TEST(test_math_point_in_ellipse)
{
    ck_assert(!math_point_in_ellipse(0, 0, 5, 5, 10, 10, 0));
    ck_assert(!math_point_in_ellipse(500, 0, 5, 5, 10, 10, 0));
    ck_assert(!math_point_in_ellipse(0, 500, 5, 5, 10, 10, 0));
    ck_assert(!math_point_in_ellipse(500, 500, 5, 5, 10, 10, 0));
    ck_assert(!math_point_in_ellipse(10, 10, 5, 5, 10, 10, 0));
    ck_assert(math_point_in_ellipse(5, 5, 5, 5, 10, 10, 0));
    ck_assert(math_point_in_ellipse(5, 5, 5, 5, 5, 10, 0));
    ck_assert(math_point_in_ellipse(5, 5, 5, 5, 10, 5, 0));
    ck_assert(math_point_in_ellipse(5, 5, 5, 5, 10, 10, 45));
}
END_TEST

START_TEST(test_math_point_edge_ellipse)
{
    ck_assert(!math_point_edge_ellipse(0, 0, 5, 5, 10, 10, 0, NULL));
    int deg;
    ck_assert(math_point_edge_ellipse(50, 2, 50, 50, 100, 100, 0, &deg));
    ck_assert_int_eq(deg, 0);
    ck_assert(math_point_edge_ellipse(99, 50, 50, 50, 100, 100, 0, &deg));
    ck_assert_int_eq(deg, 90);
    ck_assert(math_point_edge_ellipse(50, 99, 50, 50, 100, 100, 0, &deg));
    ck_assert_int_eq(deg, 180);
    ck_assert(math_point_edge_ellipse(2, 50, 50, 50, 100, 100, 0, &deg));
    ck_assert_int_eq(deg, 270);
    ck_assert(math_point_edge_ellipse(2, 49, 50, 50, 100, 100, 0, &deg));
    ck_assert_int_eq(deg, 272);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("math");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_isqrt);
    tcase_add_test(tc_core, test_nearest_pow_two_exp);
    tcase_add_test(tc_core, test_math_point_in_ellipse);
    tcase_add_test(tc_core, test_math_point_edge_ellipse);

    return s;
}

void check_server_math(void)
{
    check_run_suite(suite(), __FILE__);
}

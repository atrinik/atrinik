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

START_TEST(test_add_string)
{
    shstr *str1, *str2, *str3;
    char *temp;

    str1 = add_string("Hello world");
    ck_assert_ptr_ne(str1, NULL);
    temp = emalloc(strlen(str1) + 1);
    strcpy(temp, str1);
    str2 = add_string(temp);
    ck_assert_ptr_eq(str2, str1);
    str3 = add_string("");
    ck_assert_ptr_ne(str3, NULL);
    efree(temp);

    free_string_shared(str1);
    free_string_shared(str2);
    free_string_shared(str3);
}

END_TEST

START_TEST(test_add_refcount)
{
    shstr *str1, *str2;

    str1 = add_string("Refcount testing//..");
    str2 = add_refcount(str1);
    ck_assert_ptr_eq(str1, str2);
    ck_assert_int_eq(query_refcount(str1), 2);

    free_string_shared(str1);
    free_string_shared(str2);
}

END_TEST

START_TEST(test_query_refcount)
{
    shstr *str1;

    str1 = add_string("Hello World");
    ck_assert_int_eq(query_refcount(str1), 1);
    add_string("Hello World");
    ck_assert_int_eq(query_refcount(str1), 2);
    add_refcount(str1);
    ck_assert_int_eq(query_refcount(str1), 3);

    free_string_shared(str1);
    free_string_shared(str1);
    free_string_shared(str1);
}

END_TEST

START_TEST(test_find_string)
{
    shstr *str1, *str2, *str3, *result;

    str1 = add_string("Hello world");
    str2 = add_string("Bonjour le monde");
    result = find_string("Hello world");
    ck_assert_ptr_eq(str1, result);
    result = find_string("Bonjour le monde");
    ck_assert_ptr_eq(str2, result);
    result = find_string("Hola mundo");
    ck_assert_ptr_eq(result, NULL);
    str3 = add_string("");
    result = find_string("");
    ck_assert_ptr_eq(result, str3);
    free_string_shared(str2);
    result = find_string("Bonjour le monde");
    ck_assert_ptr_eq(result, NULL);

    free_string_shared(str1);
    free_string_shared(str3);
}

END_TEST

START_TEST(test_free_string_shared)
{
    shstr *str1, *str2;

    str1 = add_string("l33t");
    free_string_shared(str1);
    str2 = find_string("l33t");
    ck_assert_ptr_eq(str2, NULL);
    str1 = add_string("bleh");
    add_string("bleh");
    free_string_shared(str1);
    str2 = find_string("bleh");
    ck_assert_ptr_eq(str2, str1);
    free_string_shared(str1);
    str2 = find_string("bleh");
    ck_assert_ptr_eq(str2, NULL);
}

END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("shstr");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_add_string);
    tcase_add_test(tc_core, test_add_refcount);
    tcase_add_test(tc_core, test_query_refcount);
    tcase_add_test(tc_core, test_find_string);
    tcase_add_test(tc_core, test_free_string_shared);

    return s;
}

void check_server_shstr(void)
{
    check_run_suite(suite(), __FILE__);
}

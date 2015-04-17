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
#include <check_proto.h>
#include <toolkit_string.h>

START_TEST(test_cache)
{
    char *str = estrdup("hello world");
    cache_struct *res;
    int i;
    char buf[MAX_BUF];

    ck_assert(cache_add("cache_test", str, CACHE_FLAG_AUTOFREE));

    res = cache_find(find_string("cache_test"));
    ck_assert_ptr_ne(res, NULL);
    ck_assert_ptr_eq(res->ptr, str);
    ck_assert_str_eq(res->ptr, "hello world");

    ck_assert(cache_remove(find_string("cache_test")));
    ck_assert(!cache_find(find_string("cache_test")));

    for (i = 0; i <= 10; i++) {
        snprintf(buf, sizeof(buf), "hello, hello! %d", i);
        str = estrdup(buf);
        snprintf(buf, sizeof(buf), "cache_test_%d", i);
        ck_assert(cache_add(buf, str, CACHE_FLAG_AUTOFREE));
        ck_assert_ptr_ne(cache_find(find_string(buf)), NULL);
    }

    for (i = 0; i <= 10; i++) {
        snprintf(buf, sizeof(buf), "cache_test_%d", i);
        ck_assert_ptr_ne(cache_find(find_string(buf)), NULL);
    }

    ck_assert(cache_remove(find_string("cache_test_0")));
    ck_assert(cache_remove(find_string("cache_test_10")));
    ck_assert(cache_remove(find_string("cache_test_4")));
    ck_assert(cache_remove(find_string("cache_test_7")));
    ck_assert(cache_remove(find_string("cache_test_2")));
    ck_assert(cache_remove(find_string("cache_test_9")));

    ck_assert_ptr_ne(cache_find(find_string("cache_test_1")), NULL);
    ck_assert_ptr_ne(cache_find(find_string("cache_test_3")), NULL);
    ck_assert_ptr_ne(cache_find(find_string("cache_test_5")), NULL);
    ck_assert_ptr_ne(cache_find(find_string("cache_test_6")), NULL);
    ck_assert_ptr_ne(cache_find(find_string("cache_test_8")), NULL);

    cache_remove_all();
    ck_assert_ptr_eq(cache_find(find_string("cache_test_3")), NULL);
    ck_assert_ptr_eq(cache_find(find_string("cache_test_5")), NULL);
    ck_assert_ptr_eq(cache_find(find_string("cache_test_6")), NULL);
    ck_assert_ptr_eq(cache_find(find_string("cache_test_8")), NULL);

    ck_assert(!cache_remove(find_string("cache_test_8")));
    ck_assert(!cache_remove(find_string("cache_test_0")));

    str = estrdup("hello hello world!!!");
    ck_assert(cache_add("cache_rem_test", str, CACHE_FLAG_AUTOFREE));
    ck_assert(!cache_add("cache_rem_test", str, CACHE_FLAG_AUTOFREE));

    str = estrdup("leet");
    ck_assert(cache_add("raas", str, CACHE_FLAG_PYOBJ));
    ck_assert(cache_add("chair", str, CACHE_FLAG_PYOBJ));
    cache_remove_by_flags(CACHE_FLAG_PYOBJ);
    ck_assert_ptr_eq(cache_find(find_string("raas")), NULL);
    ck_assert_ptr_eq(cache_find(find_string("chair")), NULL);
    efree(str);

    ck_assert(cache_remove(find_string("cache_rem_test")));
}

END_TEST

static Suite *cache_suite(void)
{
    Suite *s = suite_create("cache");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_cache);

    return s;
}

void check_server_cache(void)
{
    Suite *s = cache_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/cache.xml");
    srunner_set_log(sr, "unit/server/cache.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

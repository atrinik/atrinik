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

START_TEST(test_stringbuffer_new)
{
    StringBuffer *sb;

    sb = stringbuffer_new();
    ck_assert_ptr_ne(sb, NULL);
    efree(stringbuffer_finish(sb));
}
END_TEST

START_TEST(test_stringbuffer_finish)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    cp = stringbuffer_finish(sb);
    ck_assert_ptr_ne(cp, NULL);
    ck_assert_str_eq(cp, "");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "hello");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_finish_shared)
{
    StringBuffer *sb;
    shstr *cp;

    sb = stringbuffer_new();
    cp = stringbuffer_finish_shared(sb);
    ck_assert_ptr_ne(cp, NULL);
    ck_assert_str_eq(cp, "");
    ck_assert_ptr_eq(cp, find_string(""));
    free_string_shared(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "hello");
    cp = stringbuffer_finish_shared(sb);
    ck_assert_str_eq(cp, "hello");
    ck_assert_ptr_eq(cp, find_string("hello"));
    free_string_shared(cp);
}
END_TEST

START_TEST(test_stringbuffer_append_string_len)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    stringbuffer_append_string_len(sb, "hello", 5);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string_len(sb, "hello world", 7);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello w");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string_len(sb, "hello world", 0);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string_len(sb, "hello ", 6);
    stringbuffer_append_string_len(sb, "world", 5);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_append_string)
{
    StringBuffer *sb;
    char *cp, *cp2;

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, " hello ");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, " hello ");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "hello ");
    stringbuffer_append_string(sb, "world");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    sb = stringbuffer_new();
    cp2 = string_repeat("hello", 1000);
    stringbuffer_append_string(sb, cp2);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, cp2);
    efree(cp);
    efree(cp2);
}
END_TEST

START_TEST(test_stringbuffer_append_printf)
{
    StringBuffer *sb;
    char *cp, *cp2;

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s", "hello");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s", "hello world");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s ", "hello");
    stringbuffer_append_printf(sb, "%s", "world");
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_printf(sb, "%s %d %u", "hello", 10, UINT32_MAX);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello 10 4294967295");
    efree(cp);

    sb = stringbuffer_new();
    cp2 = string_repeat("hello", 1000);
    stringbuffer_append_printf(sb, "%s", cp2);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, cp2);
    efree(cp);
    efree(cp2);
}
END_TEST

START_TEST(test_stringbuffer_append_stringbuffer)
{
    StringBuffer *sb, *sb2, *sb3;
    char *cp;

    sb = stringbuffer_new();
    sb2 = stringbuffer_new();
    stringbuffer_append_string(sb2, "hello");
    stringbuffer_append_stringbuffer(sb, sb2);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
    efree(stringbuffer_finish(sb2));

    sb = stringbuffer_new();
    sb2 = stringbuffer_new();
    sb3 = stringbuffer_new();
    stringbuffer_append_string(sb2, "hello ");
    stringbuffer_append_string(sb3, "world");
    stringbuffer_append_stringbuffer(sb, sb2);
    stringbuffer_append_stringbuffer(sb, sb3);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);
    efree(stringbuffer_finish(sb2));
    efree(stringbuffer_finish(sb3));
}
END_TEST

START_TEST(test_stringbuffer_append_char)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    stringbuffer_append_char(sb, 'a');
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "a");
    efree(cp);

    sb = stringbuffer_new();
    stringbuffer_append_char(sb, 'h');
    stringbuffer_append_char(sb, 'e');
    stringbuffer_append_char(sb, 'l');
    stringbuffer_append_char(sb, 'l');
    stringbuffer_append_char(sb, 'o');
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_length)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    ck_assert_uint_eq(stringbuffer_length(sb), 0);
    stringbuffer_append_string(sb, "hello");
    ck_assert_uint_eq(stringbuffer_length(sb), 5);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_index)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    ck_assert_int_eq(stringbuffer_index(sb, 'l'), -1);
    ck_assert_int_eq(stringbuffer_index(sb, 'a'), -1);
    stringbuffer_append_string(sb, "hello");
    ck_assert_int_eq(stringbuffer_index(sb, 'a'), -1);
    ck_assert_int_eq(stringbuffer_index(sb, 'h'), 0);
    ck_assert_int_eq(stringbuffer_index(sb, 'e'), 1);
    ck_assert_int_eq(stringbuffer_index(sb, 'l'), 2);
    ck_assert_int_eq(stringbuffer_index(sb, 'o'), 4);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_rindex)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    ck_assert_int_eq(stringbuffer_rindex(sb, 'l'), -1);
    ck_assert_int_eq(stringbuffer_rindex(sb, 'a'), -1);
    stringbuffer_append_string(sb, "hello");
    ck_assert_int_eq(stringbuffer_rindex(sb, 'a'), -1);
    ck_assert_int_eq(stringbuffer_rindex(sb, 'h'), 0);
    ck_assert_int_eq(stringbuffer_rindex(sb, 'e'), 1);
    ck_assert_int_eq(stringbuffer_rindex(sb, 'l'), 3);
    ck_assert_int_eq(stringbuffer_rindex(sb, 'o'), 4);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_sub)
{
    StringBuffer *sb;
    char *cp;

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "hello world");

    cp = stringbuffer_sub(sb, 1, -1);
    ck_assert_str_eq(cp, "ello worl");
    efree(cp);

    cp = stringbuffer_sub(sb, 0, 0);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = stringbuffer_sub(sb, 1, 0);
    ck_assert_str_eq(cp, "ello world");
    efree(cp);

    cp = stringbuffer_sub(sb, 0, -1);
    ck_assert_str_eq(cp, "hello worl");
    efree(cp);

    cp = stringbuffer_sub(sb, -1, -1);
    ck_assert_str_eq(cp, "l");
    efree(cp);

    cp = stringbuffer_sub(sb, 4, 0);
    ck_assert_str_eq(cp, "o world");
    efree(cp);

    cp = stringbuffer_sub(sb, -5, 0);
    ck_assert_str_eq(cp, "world");
    efree(cp);

    cp = stringbuffer_sub(sb, 20, 0);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = stringbuffer_sub(sb, -20, -20);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = stringbuffer_sub(sb, 0, -20);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = stringbuffer_sub(sb, -20, 0);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = stringbuffer_sub(sb, -500, 2);
    ck_assert_str_eq(cp, "he");
    efree(cp);

    cp = stringbuffer_sub(sb, 0, -500);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = stringbuffer_sub(sb, 5, -500);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);
}
END_TEST

START_TEST(test_stringbuffer_1)
{
    StringBuffer *sb, *sb2;
    char *cp;

    sb = stringbuffer_new();
    stringbuffer_append_string(sb, "hello");
    stringbuffer_append_char(sb, ' ');
    stringbuffer_append_printf(sb, "world %d", 1);
    stringbuffer_append_string_len(sb, " ", 1);
    sb2 = stringbuffer_new();
    stringbuffer_append_string(sb2, "test");
    stringbuffer_append_stringbuffer(sb, sb2);
    ck_assert_uint_eq(stringbuffer_length(sb), 18);
    cp = stringbuffer_finish(sb);
    ck_assert_str_eq(cp, "hello world 1 test");
    ck_assert_uint_eq(stringbuffer_length(sb2), 4);
    efree(cp);
    efree(stringbuffer_finish(sb2));
}
END_TEST

static Suite *stringbuffer_suite(void)
{
    Suite *s = suite_create("stringbuffer");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_stringbuffer_new);
    tcase_add_test(tc_core, test_stringbuffer_finish);
    tcase_add_test(tc_core, test_stringbuffer_finish_shared);
    tcase_add_test(tc_core, test_stringbuffer_append_string_len);
    tcase_add_test(tc_core, test_stringbuffer_append_string);
    tcase_add_test(tc_core, test_stringbuffer_append_printf);
    tcase_add_test(tc_core, test_stringbuffer_append_stringbuffer);
    tcase_add_test(tc_core, test_stringbuffer_append_char);
    tcase_add_test(tc_core, test_stringbuffer_length);
    tcase_add_test(tc_core, test_stringbuffer_index);
    tcase_add_test(tc_core, test_stringbuffer_rindex);
    tcase_add_test(tc_core, test_stringbuffer_sub);
    tcase_add_test(tc_core, test_stringbuffer_1);

    return s;
}

void check_server_stringbuffer(void)
{
    Suite *s = stringbuffer_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/stringbuffer.xml");
    srunner_set_log(sr, "unit/server/stringbuffer.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

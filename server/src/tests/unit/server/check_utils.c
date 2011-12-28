/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
#include <stdarg.h>

static void check_cleanup_string(const char *str, const char *expected)
{
	char *cp;

	cp = cleanup_string(strdup_local(str));
	fail_if(strcmp(cp, expected), "cleanup_string() cleaned up string '%s' to '%s' but it was not the expected string '%s'.", str, cp, expected);
}

START_TEST(test_cleanup_string)
{
	check_cleanup_string("  ~", "~");
	check_cleanup_string("  \b\aoa", "oa");
	fail_if(cleanup_string(strdup_local("   ")) != NULL, "cleanup_string() on whitespace-only string did not return NULL.");
}
END_TEST

static void check_adjust_player_name(const char *str, const char *expected)
{
	char *cp;

	cp = strdup_local(str);
	adjust_player_name(cp);
	fail_if(strcmp(cp, expected), "adjust_player_name() adjusted string '%s' to '%s' but it was not the expected string '%s'.", str, cp, expected);
	free(cp);
}

START_TEST(test_adjust_player_name)
{
	check_adjust_player_name("_AaA", "_aaa");
	check_adjust_player_name("bleh", "Bleh");
	check_adjust_player_name("oOooooO", "Ooooooo");
	check_adjust_player_name("19a4", "19a4");
	check_adjust_player_name("---   ~ ;   ", "---   ~ ;");
}
END_TEST

static void check_string_split(const char *str, size_t array_size, ...)
{
	char tmp[256], *array[64];
	size_t result, i;
	va_list arg;

	snprintf(tmp, sizeof(tmp), "%s", str);

	for (i = 0; i < sizeof(array) / sizeof(*array); i++)
	{
		array[i] = NULL;
	}

	result = string_split(tmp, array, array_size, ':');
	fail_if(result > array_size, "result == %zu > %zu == array_size", result, array_size);
	va_start(arg, array_size);

	for (i = 0; i < sizeof(array) / sizeof(*array); i++)
	{
		const char *expected_result = va_arg(arg, const char *);

		if (expected_result == NULL)
		{
			break;
		}

		if (i >= array_size)
		{
			fail("Internal error: too many arguments passed to check_string_split().");
		}

		if (i < result)
		{
			fail_if(strcmp(array[i], expected_result) != 0, "strcmp(array[%zu] == %s, %s) != 0", i, array[i], expected_result);
		}
		else
		{
			fail_if(array[i] != NULL, "array[%zu] == NULL", i);
		}
	}

	va_end(arg);
	fail_if(result != i, "%zu != %zu", result, i);
}

START_TEST(test_string_split)
{
	check_string_split("", 0, NULL);
	check_string_split("", 5, "", NULL);
	check_string_split(":", 5, "", "", NULL);
	check_string_split("::", 5, "", "", "", NULL);
	check_string_split("abc:def:ghi", 0, NULL);
	check_string_split("abc:def:ghi", 1, "abc:def:ghi", NULL);
	check_string_split("abc:def:ghi", 2, "abc", "def:ghi", NULL);
	check_string_split("abc:def:ghi", 3, "abc", "def", "ghi", NULL);
	check_string_split("abc:def:ghi", 4, "abc", "def", "ghi", NULL);
	check_string_split("::abc::def::", 0, NULL);
	check_string_split("::abc::def::", 1, "::abc::def::", NULL);
	check_string_split("::abc::def::", 2, "", ":abc::def::", NULL);
	check_string_split("::abc::def::", 3, "", "", "abc::def::", NULL);
	check_string_split("::abc::def::", 4, "", "", "abc", ":def::", NULL);
	check_string_split("::abc::def::", 5, "", "", "abc", "", "def::", NULL);
	check_string_split("::abc::def::", 6, "", "", "abc", "", "def", ":", NULL);
	check_string_split("::abc::def::", 7, "", "", "abc", "", "def", "", "", NULL);
	check_string_split("::abc::def::", 8, "", "", "abc", "", "def", "", "", NULL);
}
END_TEST

START_TEST(test_buf_overflow)
{
	int i;

	i = buf_overflow("1", "22", 3);
	fail_if(i == 0, "'1' + '22' can't fit in a 3 char buffer but buf_overflow told us there won't be any overflow.");
	i = buf_overflow("1", NULL, 1);
	fail_if(i == 0, "'1' + NULL can't fit in a 1 char buffer but buf_overflow told us there won't be any overflow.");
	i = buf_overflow("1", NULL, 2);
	fail_if(i == 1, "'1' + NULL can fit in a 2 char buffer but buf_overflow told us it won't.");
	i = buf_overflow("", NULL, 1);
	fail_if(i == 1, "EMPTY + NULL can fit in a 1 char buffer but buf_overflow told us it won't.");
	i = buf_overflow("", NULL, 0);
	fail_if(i == 0, "EMPTY + NULL can't fit in a 0 char buffer but buf_overflow() told us there won't be any overflow.");
}
END_TEST

static void check_cleanup_chat_string(const char *str, const char *expected)
{
	char *cp;

	cp = cleanup_chat_string(strdup_local(str));
	fail_if(strcmp(cp, expected), "cleanup_chat_string() adjusted string '%s' to '%s' but it was not the expected string '%s'.", str, cp, expected);
}

START_TEST(test_cleanup_chat_string)
{
	check_cleanup_chat_string("   ---   ~ ~;   ", "---   ~ ~;   ");
	check_cleanup_chat_string("   ^test^ ping", "^test^ ping");
	check_cleanup_chat_string("              ", "");
}
END_TEST

static void check_string_format_number_comma(uint64 num, const char *expected)
{
	char *cp;

	cp = string_format_number_comma(num);
	fail_if(strcmp(cp, expected), "string_format_number_comma() adjusted number '%"FMT64"' to '%s' but it was not the expected string '%s'.", num, cp, expected);
}

START_TEST(test_string_format_number_comma)
{
	check_string_format_number_comma(0, "0");
	check_string_format_number_comma(1, "1");
	check_string_format_number_comma(10, "10");
	check_string_format_number_comma(100, "100");
	check_string_format_number_comma(1000, "1,000");
	check_string_format_number_comma(10000, "10,000");
	check_string_format_number_comma(100000, "100,000");
	check_string_format_number_comma(1000000, "1,000,000");
	check_string_format_number_comma(10000000, "10,000,000");
	check_string_format_number_comma(100000000, "100,000,000");
	check_string_format_number_comma(1000000000, "1,000,000,000");
	check_string_format_number_comma(10000000000LLU, "10,000,000,000");
	check_string_format_number_comma(100000000000LLU, "100,000,000,000");
	check_string_format_number_comma(1000000000000LLU, "1,000,000,000,000");
	check_string_format_number_comma(10000000000000LLU, "10,000,000,000,000");
	check_string_format_number_comma(100000000000000LLU, "100,000,000,000,000");
}
END_TEST

static Suite *shstr_suite(void)
{
	Suite *s = suite_create("utils");
	TCase *tc_core = tcase_create("Core");

	tcase_add_checked_fixture(tc_core, NULL, NULL);

	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_cleanup_string);
	tcase_add_test(tc_core, test_adjust_player_name);
	tcase_add_test(tc_core, test_string_split);
	tcase_add_test(tc_core, test_buf_overflow);
	tcase_add_test(tc_core, test_cleanup_chat_string);
	tcase_add_test(tc_core, test_string_format_number_comma);

	return s;
}

void check_server_utils(void)
{
	Suite *s = shstr_suite();
	SRunner *sr = srunner_create(s);

	srunner_set_xml(sr, "unit/server/utils.xml");
	srunner_set_log(sr, "unit/server/utils.out");
	srunner_run_all(sr, CK_ENV);
	srunner_ntests_failed(sr);
	srunner_free(sr);
}

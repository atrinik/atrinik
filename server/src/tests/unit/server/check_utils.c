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
	tcase_add_test(tc_core, test_buf_overflow);
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

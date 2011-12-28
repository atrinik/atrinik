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

static void setup(void)
{
	toolkit_deinit();
	toolkit_import(shstr);
}

START_TEST(test_add_string)
{
	shstr *str1, *str2, *str3;
	char *temp;

	str1 = add_string("Hello world");
	fail_if(str1 == NULL, "add_string() should not return NULL when receiving content.");
	temp = malloc(strlen(str1) + 1);
	strcpy(temp, str1);
	str2 = add_string(temp);
	fail_if(str2 != str1, "add_string() should return same pointer for 2 same strings but str1 (%p -> '%s') != str2 (%p -> '%s').", str1, str1, str2, str2);
	str3 = add_string("");
	fail_if(str3 == NULL, "add_string() should gracefully gandle empty non-NULL strings.");
	free(temp);
}
END_TEST

START_TEST(test_add_refcount)
{
	shstr *str1, *str2;

	str1 = add_string("Refcount testing//..");
	str2 = add_refcount(str1);
	fail_if(str1 != str2, "Result of add_refcount() (%p) should be the same as original pointer (%p).", str2, str1);
	fail_if(query_refcount(str1) != 2, "add_refcount() (%p) should have made refcount to value 2 but was %d instead.", str1, query_refcount(str1));
}
END_TEST

START_TEST(test_query_refcount)
{
	shstr *str1;

	str1 = add_string("Hello World");
	fail_if(query_refcount(str1) != 1, "After add_string(), query_refcount() should return 1 but returned %d(0x%X) for %s.", query_refcount(str1), query_refcount(str1), str1);
	add_string("Hello World");
	fail_if(query_refcount(str1) != 2, "After twice add_string() with same string, query_refcount() should return 2 but returned %d(0x%X) for %s.", query_refcount(str1), query_refcount(str1), str1);
	add_refcount(str1);
	fail_if(query_refcount(str1) != 3, "After call to add_refcount(), query_refcount() should now return 3 but returned %d(0x%X) for %s.", query_refcount(str1), query_refcount(str1), str1);
}
END_TEST

START_TEST(test_find_string)
{
	shstr *str1, *str2, *result;

	str1 = add_string("Hello world");
	str2 = add_string("Bonjour le monde");
	result = find_string("Hello world");
	fail_if(str1 != result, "find_string() for %s should return %p but returned %p(%s).", str1, str1, result, result);
	result = find_string("Bonjour le monde");
	fail_if(str2 != result, "find_string() for %s should return %p but returned %p(%s).", str2, str2, result, result);
	result = find_string("Hola mundo");
	fail_if(result != NULL, "Searching for a nonexistent string should return NULL but returned %p(%s).", result, result);
	str1 = add_string("");
	result = find_string("");
	fail_if(result != str1, "Search for empty string should return it(%p), but returned %p.", str1, result);
	free_string_shared(str2);
	result = find_string("Bonjour le monde");
	fail_if(result != NULL, "After add_string() and free_string_shared(), find_string() should return NULL, but returned %p(%s).", result, result);
}
END_TEST

START_TEST(test_free_string_shared)
{
	shstr *str1, *str2;

	str1 = add_string("l33t");
	free_string_shared(str1);
	str2 = find_string("l33t");
	fail_if(str2 != NULL, "find_string() should return NULL after free_string_shared() but it returned %p (%s).", str2, str2);
	str1 = add_string("bleh");
	add_string("bleh");
	free_string_shared(str1);
	str2 = find_string("bleh");
	fail_if(str2 != str1, "find_string() should return the string(%p) after add_string(), add_string(), free_string_shared() but returned %p.", str1, str2);
	free_string_shared(str1);
	str2 = find_string("bleh");
	fail_if(str2 != NULL, "find_string() should return NULL after add_string(), add_string(), free_string_shared(), free_string_shared() but returned %p.", str2);
}
END_TEST

static Suite *shstr_suite(void)
{
	Suite *s = suite_create("shstr");
	TCase *tc_core = tcase_create("Core");

	tcase_add_checked_fixture(tc_core, setup, NULL);

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
	Suite *s = shstr_suite();
	SRunner *sr = srunner_create(s);

	srunner_set_xml(sr, "unit/server/shstr.xml");
	srunner_set_log(sr, "unit/server/shstr.out");
	srunner_run_all(sr, CK_ENV);
	srunner_ntests_failed(sr);
	srunner_free(sr);
}

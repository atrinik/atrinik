/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

START_TEST(test_cache)
{
	char *str = strdup_local("hello world");
	cache_struct *res;
	int i;
	char buf[MAX_BUF];

	fail_if(cache_add("cache_test", str, CACHE_FLAG_AUTOFREE) == 0, "Could not add cache entry.");
	res = cache_find(find_string("cache_test"));

	fail_if(res == NULL, "Could not find 'cache_test' cache.");

	if (res)
	{
		fail_if(res->ptr == NULL || (char *) res->ptr != str || strcmp((char *) res->ptr, "hello world"), "Did not find correct cache pointer.");
	}

	fail_if(cache_remove(find_string("cache_test")) == 0, "Could not remove cache entry.");
	fail_if(cache_find(find_string("cache_test")), "Found cache entry but it should have been removed.");

	for (i = 0; i <= 10; i++)
	{
		snprintf(buf, sizeof(buf), "hello, hello! %d", i);
		str = strdup_local(buf);
		snprintf(buf, sizeof(buf), "cache_test_%d", i);
		fail_if(cache_add(buf, str, CACHE_FLAG_AUTOFREE) == 0, "Could not add cache entry.");
		fail_if(cache_find(find_string(buf)) == NULL, "Could not find cache entry after it was added.");
	}

	for (i = 0; i <= 10; i++)
	{
		snprintf(buf, sizeof(buf), "cache_test_%d", i);
		fail_if(cache_find(find_string(buf)) == NULL, "Could not find cache entry that was previously added.");
	}

	fail_if(cache_remove(find_string("cache_test_0")) == 0, "Could not remove cache entry.");
	fail_if(cache_remove(find_string("cache_test_10")) == 0, "Could not remove cache entry.");
	fail_if(cache_remove(find_string("cache_test_4")) == 0, "Could not remove cache entry.");
	fail_if(cache_remove(find_string("cache_test_7")) == 0, "Could not remove cache entry.");
	fail_if(cache_remove(find_string("cache_test_2")) == 0, "Could not remove cache entry.");
	fail_if(cache_remove(find_string("cache_test_9")) == 0, "Could not remove cache entry.");

	fail_if(cache_find(find_string("cache_test_1")) == NULL, "Could not find cache entry.");
	fail_if(cache_find(find_string("cache_test_3")) == NULL, "Could not find cache entry.");
	fail_if(cache_find(find_string("cache_test_5")) == NULL, "Could not find cache entry.");
	fail_if(cache_find(find_string("cache_test_6")) == NULL, "Could not find cache entry.");
	fail_if(cache_find(find_string("cache_test_8")) == NULL, "Could not find cache entry.");

	cache_remove_all();
	fail_if(cache_find(find_string("cache_test_1")) != NULL, "Found cache entry after it should have been mass-removed.");
	fail_if(cache_find(find_string("cache_test_3")) != NULL, "Found cache entry after it should have been mass-removed.");
	fail_if(cache_find(find_string("cache_test_5")) != NULL, "Found cache entry after it should have been mass-removed.");
	fail_if(cache_find(find_string("cache_test_6")) != NULL, "Found cache entry after it should have been mass-removed.");
	fail_if(cache_find(find_string("cache_test_8")) != NULL, "Found cache entry after it should have been mass-removed.");

	fail_if(cache_remove(find_string("cache_test_8")) == 1, "Removed cache entry when it should have been removed already.");
	fail_if(cache_remove(find_string("cache_test_0")) == 1, "Removed cache entry when it should have been removed already.");

	str = strdup_local("hello hello world!!!");
	fail_if(cache_add("cache_rem_test", str, CACHE_FLAG_AUTOFREE) == 0, "Could not add cache entry.");
	fail_if(cache_add("cache_rem_test", str, CACHE_FLAG_AUTOFREE) == 1, "Added cache entry with same name.");

	str = strdup_local("leet");
	fail_if(cache_add("raas", str, CACHE_FLAG_PYOBJ) == 0, "Could not add cache entry.");
	fail_if(cache_add("chair", str, CACHE_FLAG_PYOBJ) == 0, "Could not add cache entry.");
	cache_remove_by_flags(CACHE_FLAG_PYOBJ);
	fail_if(cache_find(find_string("raas")) != NULL, "Found removed cache entry.");
	fail_if(cache_find(find_string("chair")) != NULL, "Found removed cache entry.");
	free(str);

	fail_if(cache_remove(find_string("cache_rem_test")) == 0, "Failed to remove previously added cache entry.");
}
END_TEST

static Suite *cache_suite()
{
	Suite *s = suite_create("cache");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_cache);

	return s;
}

void check_server_cache()
{
	Suite *s = cache_suite();
	SRunner *sr = srunner_create(s);

	srunner_set_xml(sr, "unit/server/cache.xml");
	srunner_set_log(sr, "unit/server/cache.out");
	srunner_run_all(sr, CK_ENV);
	srunner_ntests_failed(sr);
	srunner_free(sr);
}

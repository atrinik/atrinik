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

START_TEST(test_add_ban)
{
    ck_assert(!add_ban(estrdup("Tester/")));
    remove_ban(estrdup("Tester/"));
    ck_assert(!add_ban(estrdup("Tester/:")));
    remove_ban(estrdup("Tester/:"));
    ck_assert(add_ban(estrdup("Tester/:xxx.x.x.x")));
    remove_ban(estrdup("Tester/:xxx.x.x.x"));
    ck_assert(!add_ban(estrdup("Tester/:xxx.x.x.x:11")));
    remove_ban(estrdup("Tester/:xxx.x.x.x:11"));
}

END_TEST

START_TEST(test_checkbanned)
{
    shstr *str1, *str2;

    add_ban(estrdup("Noob/:127.0.0.1"));
    str1 = add_string("Noob/");
    ck_assert(checkbanned(str1, "127.0.0.1"));
    remove_ban(estrdup("Noob/:127.0.0.1"));

    add_ban(estrdup("Tester/:*"));
    str2 = add_string("Tester/");
    ck_assert(checkbanned(str2, "127.2.0.1"));
    remove_ban(estrdup("Tester/:*"));

    add_ban(estrdup("*:xxx.xxx.xxx"));
    ck_assert(checkbanned(NULL, "xxx.xxx.xxx"));
    remove_ban(estrdup("*:xxx.xxx.xxx"));

    ck_assert(!checkbanned(NULL, "10543./4t5vr.3546"));

    free_string_shared(str1);
    free_string_shared(str2);
}

END_TEST

START_TEST(test_remove_ban)
{
    add_ban(estrdup("Tester/:xxx.x.x.x"));
    ck_assert(remove_ban(estrdup("Tester/:xxx.x.x.x")));

    ck_assert(!remove_ban(estrdup("Tester~$#@:127.0.0.1")));
}

END_TEST

static Suite *ban_suite(void)
{
    Suite *s = suite_create("ban");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_add_ban);
    tcase_add_test(tc_core, test_remove_ban);
    tcase_add_test(tc_core, test_checkbanned);

    return s;
}

void check_server_ban(void)
{
    Suite *s = ban_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/ban.xml");
    srunner_set_log(sr, "unit/server/ban.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

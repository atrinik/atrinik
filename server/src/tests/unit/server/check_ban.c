/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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

START_TEST(test_add_ban)
{
    fail_if(add_ban(strdup("Tester/")) == 1, "Successfully added a ban with add_ban() but no IP was entered.");
    remove_ban(strdup("Tester/"));

    fail_if(add_ban(strdup("Tester/:")) == 1, "Successfully added a ban with add_ban() but no IP was entered except a colon.");
    remove_ban(strdup("Tester/:"));

    fail_if(add_ban(strdup("Tester/:xxx.x.x.x")) == 0, "Failed to add a new ban with add_ban().");
    remove_ban(strdup("Tester/:xxx.x.x.x"));

    fail_if(add_ban(strdup("Tester/:xxx.x.x.x:11")) == 1, "Successfully added a new ban with add_ban(), but the IP had colons in it.");
    remove_ban(strdup("Tester/:xxx.x.x.x:11"));
}
END_TEST

START_TEST(test_checkbanned)
{
    add_ban(strdup("Noob/:127.0.0.1"));
    fail_if(checkbanned(add_string("Noob/"), "127.0.0.1") == 0, "checkbanned() failed to match a previously banned name and IP.");
    remove_ban(strdup("Noob/:127.0.0.1"));

    add_ban(strdup("Tester/:*"));
    fail_if(checkbanned(add_string("Tester/"), "127.2.0.1") == 0, "checkbanned() failed to match a previously banned name.");
    remove_ban(strdup("Tester/:*"));

    add_ban(strdup("*:xxx.xxx.xxx"));
    fail_if(checkbanned(NULL, "xxx.xxx.xxx") == 0, "checkbanned() failed to match a previously banned IP.");
    remove_ban(strdup("*:xxx.xxx.xxx"));

    fail_if(checkbanned(NULL, "10543./4t5vr.3546") == 1, "checkbanned() returned 1 for an IP that was not previously banned.");
}
END_TEST

START_TEST(test_remove_ban)
{
    add_ban(strdup("Tester/:xxx.x.x.x"));
    fail_if(remove_ban(strdup("Tester/:xxx.x.x.x")) == 0, "remove_ban() failed to remove previously added ban.");

    fail_if(remove_ban(strdup("Tester~$#@:127.0.0.1")) == 1, "remove_ban() managed to remove nonexistent ban.");
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

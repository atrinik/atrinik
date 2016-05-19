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
#include <ban.h>
#include <player.h>
#include <object.h>

START_TEST(test_ban_add)
{
    ban_reset();
    ck_assert_int_eq(ban_add("   "), BAN_BADSYNTAX);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_OK);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_EXIST);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1 " *"), BAN_EXIST);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_OK);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_EXIST);
    ck_assert_int_eq(ban_add("\"" PLAYER_TESTING_NAME2 "\" "
            ACCOUNT_TESTING_NAME), BAN_OK);
    ck_assert_int_eq(ban_add("\"" PLAYER_TESTING_NAME2 "\" "
            ACCOUNT_TESTING_NAME), BAN_EXIST);
    ck_assert_int_eq(ban_add("* * 88.88.88.88/32"), BAN_OK);
    ck_assert_int_eq(ban_add("* * 88.88.88.88"), BAN_EXIST);
    ck_assert_int_eq(ban_add("* * 88.88.88.88/0"), BAN_BADPLEN);
    ck_assert_int_eq(ban_add("* * 88.88.88.88/64"), BAN_BADPLEN);
    ck_assert_int_eq(ban_add("* * 88.88.88.677"), BAN_BADIP);
    ck_assert_int_eq(ban_add("* * abc"), BAN_BADIP);
#ifdef HAVE_IPV6
    ck_assert_int_eq(ban_add("* * 2001:cdba:9abc:5678::/64"), BAN_OK);
    ck_assert_int_eq(ban_add("* * 2001:cdba:9abc:5678::/64"), BAN_EXIST);
    ck_assert_int_eq(ban_add("* * 2001:cdba:0000:0000:0000:0000:3257:9652"),
            BAN_OK);
    ck_assert_int_eq(ban_add("* * 2001:cdba:0000:0000:0000:0000:3257:9652/128"),
            BAN_EXIST);
    ck_assert_int_eq(ban_add("* * 2001:cdba::3257:9652/128"), BAN_EXIST);
    ck_assert_int_eq(ban_add("* * 2001:cdba:0000:0000:0000:0000:3257:9652/256"),
            BAN_BADPLEN);
    ck_assert_int_eq(ban_add("* * 2001:cdba:0000:0000:0000:0000:3257:9652/0"),
            BAN_BADPLEN);
#endif
    ban_reset();
}
END_TEST

START_TEST(test_ban_remove)
{
    ban_reset();
    ck_assert_int_eq(ban_remove("   "), BAN_BADSYNTAX);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_OK);
    ck_assert_int_eq(ban_remove(PLAYER_TESTING_NAME1), BAN_OK);
    ck_assert_int_eq(ban_remove(PLAYER_TESTING_NAME1), BAN_NOTEXIST);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_OK);
    ck_assert_int_eq(ban_remove(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_OK);
    ck_assert_int_eq(ban_remove(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_NOTEXIST);
    ck_assert_int_eq(ban_add("\"" PLAYER_TESTING_NAME2 "\" "
            ACCOUNT_TESTING_NAME), BAN_OK);
    ck_assert_int_eq(ban_add("* * 88.88.88.88/32"), BAN_OK);
    ck_assert_int_eq(ban_remove("\"" PLAYER_TESTING_NAME2 "\" "
            ACCOUNT_TESTING_NAME), BAN_OK);
    ck_assert_int_eq(ban_remove("* * 88.88.88.88"), BAN_OK);
#ifdef HAVE_IPV6
    ck_assert_int_eq(ban_add("* * 2001:cdba:9abc:5678::/64"), BAN_OK);
    ck_assert_int_eq(ban_remove("* * 2001:cdba:9abc:5678::/64"), BAN_OK);
    ck_assert_int_eq(ban_add("* * 2001:cdba:0000:0000:0000:0000:3257:9652"),
            BAN_OK);
    ck_assert_int_eq(ban_remove("* * 2001:cdba:0000:0000:0000:0000:3257:9652/"
            "128"), BAN_OK);
    ck_assert_int_eq(ban_remove("* * 2001:cdba:0000:0000:0000:0000:3257:9652/"
            "128"), BAN_NOTEXIST);
#endif
    ban_reset();
    ck_assert_int_eq(ban_remove("#1"), BAN_BADID);
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_OK);
    ck_assert_int_eq(ban_remove("#1"), BAN_OK);
    ck_assert_int_eq(ban_remove("#1"), BAN_REMOVED);
    ck_assert_int_eq(ban_remove("#0"), BAN_BADID);
    ck_assert_int_eq(ban_remove("#2"), BAN_BADID);
    ban_reset();
}
END_TEST

START_TEST(test_ban_check)
{
    ban_reset();
    object *pl = player_get_dummy(PLAYER_TESTING_NAME1, NULL);
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME2), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* " ACCOUNT_TESTING_NAME), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1 " " ACCOUNT_TESTING_NAME),
            BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
    pl = player_get_dummy(PLAYER_TESTING_NAME2, "8.8.8.8");
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 8.8.8.8"), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 8.8.4.4"), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 8.8.0.0/16"), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 7.7.0.0/16"), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("\"" PLAYER_TESTING_NAME2 "\" * 8.8.8.8"), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ban_reset();
#ifdef HAVE_IPV6
    pl = player_get_dummy(PLAYER_TESTING_NAME2, "2001:cdba::3257:9652");
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 2001:cdba::3257:9652"), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 2001:cdba::3257:9653"), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 2001:cdba::/64"), BAN_OK);
    ck_assert(ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert_int_eq(ban_add("* * 2002:cdba::/64"), BAN_OK);
    ck_assert(!ban_check(CONTR(pl)->cs, pl->name));
    ck_assert(!ban_check(CONTR(pl)->cs, NULL));
    ban_reset();
#endif

}
END_TEST

START_TEST(test_ban_reset)
{
    ban_reset();
    ck_assert_int_eq(ban_add(PLAYER_TESTING_NAME1), BAN_OK);
    ban_reset();
    ck_assert_int_eq(ban_remove("#1"), BAN_BADID);
    ck_assert_int_eq(ban_remove(PLAYER_TESTING_NAME1), BAN_NOTEXIST);
    ban_reset();
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("ban");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_ban_add);
    tcase_add_test(tc_core, test_ban_remove);
    tcase_add_test(tc_core, test_ban_check);
    tcase_add_test(tc_core, test_ban_reset);

    return s;
}

void check_server_ban(void)
{
    check_run_suite(suite(), __FILE__);
}

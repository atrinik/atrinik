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

/**
 * @file
 * This is a unit tests file for bug #85: Cursed minor shielding amulet.
 *
 * Location: http://bugzilla.atrinik.org/show_bug.cgi?id=85 */

#include <global.h>
#include <check.h>
#include <check_proto.h>

START_TEST(test_run)
{
    int i;
    treasurelist *list;
    object *tmp;

    list = find_treasurelist("random_talisman");
    ck_assert_msg(list != NULL, "Couldn't find 'random_talisman' treasure list "
            "to start the test.");

    for (i = 0; i < 2000; i++) {
        tmp = generate_treasure(list, 999, 100);
        ck_assert_msg(tmp != NULL, "Didn't generate anything: %d", i);

        if (strcmp(tmp->arch->name, "amulet_shielding") == 0) {
            if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)) {
                ck_abort_msg("Managed to create cursed amulet of minor "
                        "shielding (i: %d).", i);
            }
        }

        object_destroy(tmp);
    }
}

END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("bug");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_run);

    return s;
}

void check_bug_85(void)
{
    check_run_suite(suite(), __FILE__);
}

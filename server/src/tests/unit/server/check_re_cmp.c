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

static void check_re_cmp(const char *str, const char *regex)
{
    ck_assert_msg(re_cmp(str, regex) != NULL,
            "Failed to match '%s' with regex '%s'.", str, regex);
}

START_TEST(test_re_cmp)
{
    check_re_cmp("dragon183", "dragon[1-9]+$");
    check_re_cmp("dragon18", "dragon[1-9][1-9]");
    check_re_cmp("dragon18", "dragon[1-2][1-9]$");
    check_re_cmp("dragon18", "dragon[81]+");
    check_re_cmp("treasure", "^treas");
    check_re_cmp("treasure", "^treasure$");
    check_re_cmp("where is treasure", "treasure$");
    check_re_cmp("where is treasure?", "treasure[?.]$");
}

END_TEST

static Suite *re_cmp_suite(void)
{
    Suite *s = suite_create("re_cmp");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_re_cmp);

    return s;
}

void check_server_re_cmp(void)
{
    Suite *s = re_cmp_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/re_cmp.xml");
    srunner_set_log(sr, "unit/server/re_cmp.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

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
#include <stdarg.h>

START_TEST(test_sound_ambient_match_parse)
{
    object *ob;
    const char *match[] = {
        "hour == 1",
        "hour > 1",
        "hour < 1",
        "hour >= 1",
        "hour <= 1",

        "minute == 5",
        "minute > 5",
        "minute < 5",
        "minute >= 5",
        "minute <= 5",

        "hour + 5 == 1",
        "hour - 5 > 1",
        "hour * 5 < 1",
        "hour / 5 >= 1",
        "hour % 5 <= 1",

        "hour == 1 && minute == 5",
        "hour > 1 && minute > 5",
        "hour < 1 && minute < 5",
        "hour >= 1 && minute >= 5",
        "hour <= 1 && minute <= 5",

        "hour == 1 || minute == 5",
        "hour > 1 || minute > 5",
        "hour < 1 || minute < 5",
        "hour >= 1 || minute >= 5",
        "hour <= 1 || minute <= 5",

        "(hour == 1 || minute == 5)",
        "((hour == 1 || minute == 5))",
        "(hour == 1 || (minute == 5))",
        "((hour == 1) || minute == 5)",
        "((hour == 1) || (minute == 5))",
        "(hour == 1) || (minute == 5)",

        "hour < 10 && ((minute > 5 && hour < 1) || "
                "(minute < 10 && minute > 10)) && hour > 1",
        "(minute > 5 || minute < 10) && hour < 10 && hour > 1",

        NULL};
    size_t i;

    ob = get_archetype("sound_ambient");
    ck_assert_ptr_ne(ob, NULL);

    for (i = 0; match[i] != NULL; i++) {
        sound_ambient_match_parse(ob, match[i]);
        ck_assert_str_eq(match[i], sound_ambient_match_str(ob));
    }

    object_destroy(ob);
}

END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("sound_ambient");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_sound_ambient_match_parse);

    return s;
}

void check_types_sound_ambient(void)
{
    check_run_suite(suite(), __FILE__);
}

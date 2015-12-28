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
 * This is a unit tests file for issues related to cursed treasure generation,
 * when it shouldn't be.
 */

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <arch.h>
#include <object.h>

START_TEST(test_cursed_amulet_shielding)
{
    treasurelist *list = find_treasurelist("random_talisman");
    ck_assert_msg(list != NULL, "Couldn't find 'random_talisman' treasure list "
            "to start the test.");

    for (int i = 0; i < 2000; i++) {
        object *tmp = generate_treasure(list, 999, 100);
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

START_TEST(test_cursed_starting_items)
{
    treasurelist *list = find_treasurelist("player_male");
    ck_assert_msg(list != NULL, "Couldn't find 'player_male' treasure list "
            "to start the test.");

    object *inv = get_object();

    for (int i = 0; i < 2000; i++) {
        object_destroy_inv(inv);
        create_treasure(list, inv, GT_ONLY_GOOD, 1, T_STYLE_UNSET,
                ART_CHANCE_UNSET, 0, NULL);
        ck_assert_msg(inv->inv != NULL, "Didn't generate anything: %d", i);

        for (object *tmp = inv->inv; tmp != NULL; tmp = tmp->below) {
            if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)) {
                SET_FLAG(tmp, FLAG_IDENTIFIED);
                char *name = object_get_name_s(tmp, NULL);
                ck_abort_msg("Managed to create cursed item %s (%s) (i: %d).",
                        name, object_get_str(tmp), i);
                efree(name);
            }
        }
    }

    object_destroy(inv);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("cursed_treasures");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_cursed_amulet_shielding);
    tcase_add_test(tc_core, test_cursed_starting_items);

    return s;
}

void check_bug_cursed_treasures(void)
{
    check_run_suite(suite(), __FILE__);
}

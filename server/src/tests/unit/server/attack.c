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
#include <arch.h>

START_TEST(test_is_melee_range)
{
    mapstruct *map;
    object *pl, *tmp, *tmp2;

    check_setup_env_pl(&map, &pl);
    ck_assert(is_melee_range(pl, pl));

    tmp = arch_get("gazer_dread");
    ck_assert(!is_melee_range(tmp, tmp));
    ck_assert(!is_melee_range(pl, tmp));
    ck_assert(!is_melee_range(tmp, pl));

    tmp->x = pl->x + 1;
    tmp->y = pl->y + 1;
    tmp = insert_ob_in_map(tmp, pl->map, NULL, 0);
    ck_assert(is_melee_range(tmp, tmp));
    ck_assert(is_melee_range(pl, tmp));
    ck_assert(is_melee_range(tmp, pl));

    tmp2 = arch_get("raas");
    ck_assert(!is_melee_range(tmp2, tmp2));
    ck_assert(!is_melee_range(pl, tmp2));
    ck_assert(!is_melee_range(tmp2, pl));

    tmp2->x = pl->x + 2;
    tmp2->y = pl->y + 2;
    tmp2 = insert_ob_in_map(tmp2, pl->map, NULL, 0);
    ck_assert(is_melee_range(tmp2, tmp2));
    ck_assert(!is_melee_range(pl, tmp2));
    ck_assert(!is_melee_range(tmp2, pl));
    ck_assert(is_melee_range(tmp, tmp2));
    ck_assert(is_melee_range(tmp2, tmp));
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("attack");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_is_melee_range);

    return s;
}

void check_server_attack(void)
{
    check_run_suite(suite(), __FILE__);
}

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

START_TEST(test_item_matched_string)
{
    mapstruct *map;
    object *pl, *o1, *o2;

    check_setup_env_pl(&map, &pl);

    o1 = get_archetype("cloak");
    ck_assert_ptr_ne(o1, NULL);
    FREE_AND_COPY_HASH(o1->title, "of Moroch");
    CLEAR_FLAG(o1, FLAG_IDENTIFIED);

    ck_assert_int_eq(item_matched_string(pl, o1, "all"), 1);
    ck_assert_int_eq(item_matched_string(pl, o1, "Moroch"), 0);
    ck_assert_int_eq(item_matched_string(pl, o1, "random"), 0);

    SET_FLAG(o1, FLAG_IDENTIFIED);
    ck_assert_int_ne(item_matched_string(pl, o1, "Moroch"), 0);

    o2 = get_archetype("cloak");
    SET_FLAG(o2, FLAG_UNPAID);
    ck_assert_int_eq(item_matched_string(pl, o2, "unpaid"), 2);
    ck_assert_int_ne(item_matched_string(pl, o2, "cloak"), 0);
    ck_assert_int_eq(item_matched_string(pl, o2, "wrong"), 0);

    object_destroy(o1);
    object_destroy(o2);
}

END_TEST

START_TEST(test_arch_to_object)
{
    archetype *arch;
    object *obj;

    arch = find_archetype("empty_archetype");
    obj = arch_to_object(arch);
    ck_assert_ptr_ne(obj, NULL);
    object_destroy(obj);

    ck_assert_ptr_eq(arch_to_object(NULL), NULL);
}

END_TEST

START_TEST(test_create_singularity)
{
    object *obj;

    obj = create_singularity("JO3584jke");
    ck_assert_ptr_ne(obj, NULL);
    ck_assert_ptr_ne(obj->name, NULL);
    ck_assert(strstr(obj->name, "JO3584jke") != NULL);
    object_destroy(obj);
}

END_TEST

START_TEST(test_get_archetype)
{
    object *obj;

    obj = get_archetype("empty_archetype");
    ck_assert_ptr_ne(obj, NULL);
    object_destroy(obj);

    obj = get_archetype("AA938DFEPQ54FH");
    ck_assert_ptr_ne(obj, NULL);
    ck_assert_ptr_ne(obj->name, NULL);
    ck_assert(strstr(obj->name, "AA938DFEPQ54FH") != NULL);
    object_destroy(obj);
}

END_TEST

START_TEST(test_find_archetype)
{
    ck_assert_ptr_ne(find_archetype("empty_archetype"), NULL);
    ck_assert_ptr_eq(find_archetype("AA938DFEPQ54FH"), NULL);
}

END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("arch");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_item_matched_string);
    tcase_add_test(tc_core, test_arch_to_object);
    tcase_add_test(tc_core, test_create_singularity);
    tcase_add_test(tc_core, test_get_archetype);
    tcase_add_test(tc_core, test_find_archetype);

    return s;
}

void check_server_arch(void)
{
    check_run_suite(suite(), __FILE__);
}

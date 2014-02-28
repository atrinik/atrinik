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

START_TEST(test_item_matched_string)
{
    object *pl, *o1, *o2;
    int val;

    pl = get_archetype("raas");
    fail_if(pl == NULL, "Couldn't create raas.");
    pl->custom_attrset = (player *) calloc(1, sizeof(player));
    fail_if(CONTR(pl) == NULL, "Couldn't alloc CONTR.");

    o1 = get_archetype("cloak");
    fail_if(o1 == NULL, "Couldn't find cloak archetype");
    o1->title = add_string("of Moroch");
    CLEAR_FLAG(o1, FLAG_IDENTIFIED);

    val = item_matched_string(pl, o1, "all");
    fail_if(val != 1, "All didn't match cloak.");
    val = item_matched_string(pl, o1, "Moroch");
    fail_if(val != 0, "Unidentified cloak matched title with value %d.", val);
    val = item_matched_string(pl, o1, "random");
    fail_if(val != 0, "Unidentified cloak matched random value with value %d.", val);

    SET_FLAG(o1, FLAG_IDENTIFIED);
    val = item_matched_string(pl, o1, "Moroch");
    fail_if(val == 0, "Identified cloak didn't match title with value %d.", val);

    o2 = get_archetype("cloak");
    SET_FLAG(o2, FLAG_UNPAID);
    val = item_matched_string(pl, o2, "unpaid");
    fail_if(val != 2, "Unpaid cloak didn't match unpaid.");
    val = item_matched_string(pl, o2, "cloak");
    fail_if(val == 0, "Unpaid cloak didn't match cloak with %d.", val);
    val = item_matched_string(pl, o2, "wrong");
    fail_if(val != 0, "Unpaid cloak matched wrong name %d.", val);
}
END_TEST

START_TEST(test_arch_to_object)
{
    archetype *arch;
    object *obj;

    arch = find_archetype("empty_archetype");
    obj = arch_to_object(arch);
    fail_if(obj == NULL, "arch_to_object() with valid archetype should not return NULL.");
}
END_TEST

START_TEST(test_create_singularity)
{
    object *obj;

    obj = create_singularity("JO3584jke");
    fail_if(obj == NULL, "create_singularity() should not return NULL.");
    fail_if(strstr(obj->name, "JO3584jke") == 0, "create_singularity(\"JO3584jke\") should put JO3584jke somewhere in singularity name.");
}
END_TEST

START_TEST(test_get_archetype)
{
    object *obj;

    obj = get_archetype("empty_archetype");
    fail_if(obj == NULL, "create_archetype(\"empty_archetype\") should not return NULL.");
}
END_TEST

START_TEST(test_find_archetype)
{
    archetype *arch;

    arch = find_archetype("empty_archetype");
    fail_if(arch == NULL, "find_archetype(\"empty_archetype\") should not return NULL.");
    arch = find_archetype("AA938DFEPQ54FH");
    fail_if(arch != NULL, "find_archetype(\"AA938DFEPQ54FH\") should return NULL.");
}
END_TEST

static Suite *arch_suite(void)
{
    Suite *s = suite_create("arch");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

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
    Suite *s = arch_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/arch.xml");
    srunner_set_log(sr, "unit/server/arch.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

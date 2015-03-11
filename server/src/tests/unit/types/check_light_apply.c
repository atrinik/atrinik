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

START_TEST(test_light_apply_apply_1)
{
    object *pl, *torch;

    pl = player_get_dummy();
    fail_if(pl == NULL, "No player object.");

    torch = get_archetype("torch");
    player_apply(pl, torch, 0, 0);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch was applied.");
    fail_if(torch->glow_radius == 0, "Torch was not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Light object in player's equipment array is set.");

    object_destroy(torch);
}
END_TEST

START_TEST(test_light_apply_apply_2)
{
    object *pl, *torch;

    pl = player_get_dummy();
    fail_if(pl == NULL, "No player object.");

    torch = get_archetype("torch");
    manual_apply(torch, torch, 0);
    player_apply(pl, torch, 0, 0);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch was applied.");
    fail_if(torch->glow_radius != 0, "Torch was not extinguished.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Light object in player's equipment array is set.");

    object_destroy(torch);
}
END_TEST

START_TEST(test_light_apply_apply_3)
{
    object *pl, *torch;

    pl = player_get_dummy();
    fail_if(pl == NULL, "No player object.");

    torch = get_archetype("torch");
    insert_ob_in_ob(torch, pl);
    player_apply(pl, torch, 0, 0);
    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch was not applied.");
    fail_if(torch->glow_radius == 0, "Torch was not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch was not assigned to player's equipment array.");
}
END_TEST

START_TEST(test_light_apply_apply_4)
{
    object *pl, *torch;

    pl = player_get_dummy();
    fail_if(pl == NULL, "No player object.");

    torch = get_archetype("torch");
    manual_apply(torch, torch, 0);
    insert_ob_in_ob(torch, pl);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch was applied.");
    fail_if(torch->glow_radius == 0, "Torch was not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Torch was not assigned to player's equipment array.");

    player_apply(pl, torch, 0, 0);
    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch was not applied.");
    fail_if(torch->glow_radius == 0, "Torch was not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch was not assigned to player's equipment array.");
}
END_TEST

static Suite *light_apply_suite(void)
{
    Suite *s = suite_create("light_apply");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_light_apply_apply_1);
    tcase_add_test(tc_core, test_light_apply_apply_2);
    tcase_add_test(tc_core, test_light_apply_apply_3);
    tcase_add_test(tc_core, test_light_apply_apply_4);

    return s;
}

void check_types_light_apply(void)
{
    Suite *s = light_apply_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/types/light_apply.xml");
    srunner_set_log(sr, "unit/types/light_apply.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

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

/*
 * Player applies a torch on the ground. Ensure the torch is lit and not
 * applied.
 */
START_TEST(test_light_apply_apply_1)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = insert_ob_in_map(get_archetype("torch"), map, NULL, 0);
    player_apply(pl, torch, 0, 0);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch is applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Light object in player's equipment array is set.");
}
END_TEST

/*
 * Player applies a lit torch on the ground Ensure the torch is extinguished and
 * not applied.
 */
START_TEST(test_light_apply_apply_2)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = insert_ob_in_map(get_archetype("torch"), map, NULL, 0);
    manual_apply(torch, torch, 0);
    player_apply(pl, torch, 0, 0);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch is applied.");
    fail_if(torch->glow_radius != 0, "Torch is not extinguished.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Light object in player's equipment array is set.");
}
END_TEST

/*
 * Player applies a torch in his inventory. Ensure it's lit and applied.
 */
START_TEST(test_light_apply_apply_3)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = insert_ob_in_ob(get_archetype("torch"), pl);
    player_apply(pl, torch, 0, 0);
    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch is not applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch is not assigned to player's equipment array.");
}
END_TEST

/*
 * Player applies a lit torch in his inventory. Ensure it was extinguished and
 * not applied.
 */
START_TEST(test_light_apply_apply_4)
{
    mapstruct *map;
    object *pl, *torch;

    check_setup_env_pl(&map, &pl);

    torch = insert_ob_in_map(get_archetype("torch"), map, NULL, 0);
    manual_apply(torch, torch, 0);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch is applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    object_remove(torch, 0);
    torch = insert_ob_in_ob(torch, pl);
    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch is applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != NULL,
            "Torch is assigned to player's equipment array.");

    player_apply(pl, torch, 0, 0);

    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch is not applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch is not assigned to player's equipment array.");
}
END_TEST

/*
 * Player applies a torch in his inventory, then a torch on the ground. Ensure
 * both torches are lit, but only the one in inventory is applied.
 */
START_TEST(test_light_apply_apply_5)
{
    mapstruct *map;
    object *pl, *torch, *torch2;

    check_setup_env_pl(&map, &pl);

    torch = insert_ob_in_ob(get_archetype("torch"), pl);
    fail_if(torch == NULL, "Could not insert torch in player.");
    torch2 = insert_ob_in_map(get_archetype("torch"), map, NULL, 0);
    fail_if(torch2 == NULL, "Could not insert torch in map.");

    player_apply(pl, torch, 0, 0);
    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch is not applied.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch is not assigned to player's equipment array.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(QUERY_FLAG(torch2, FLAG_APPLIED), "Torch2 is applied.");
    fail_if(torch2->glow_radius != 0, "Torch2 is lit.");

    player_apply(pl, torch2, 0, 0);
    fail_if(QUERY_FLAG(torch2, FLAG_APPLIED), "Torch2 is applied.");
    fail_if(torch2->glow_radius == 0, "Torch2 is not lit.");

    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch is not applied.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch,
            "Torch is not assigned to player's equipment array.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
}
END_TEST

/*
 * Player applies stacked torches, ensure a new torch is created and lit and
 * the original is not lit. Then apply the original and ensure it's lit and
 * applied, and that the old one remains lit but not applied.
 */
START_TEST(test_light_apply_apply_6)
{
    mapstruct *map;
    object *pl, *torch, *torch2;

    check_setup_env_pl(&map, &pl);

    torch = get_archetype("torch");
    torch->nrof = 2;
    torch = insert_ob_in_ob(torch, pl);
    fail_if(torch == NULL, "Could not insert torch in player.");

    player_apply(pl, torch, 0, 0);
    torch2 = torch;
    torch = CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT];

    fail_if(torch == torch2, "New torch was not created");
    fail_if(torch == NULL, "Player has no light object in equipment array.");
    fail_if(!QUERY_FLAG(torch, FLAG_APPLIED), "Torch is not applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
    fail_if(torch->nrof != 1, "Torch nrof is not 1.");

    fail_if(QUERY_FLAG(torch2, FLAG_APPLIED), "Torch2 is applied.");
    fail_if(torch2->glow_radius != 0, "Torch2 is lit.");

    player_apply(pl, torch2, 0, 0);
    fail_if(!QUERY_FLAG(torch2, FLAG_APPLIED), "Torch2 is not applied.");
    fail_if(torch2->glow_radius == 0, "Torch2 is not lit.");
    fail_if(CONTR(pl)->equipment[PLAYER_EQUIP_LIGHT] != torch2,
            "Torch2 is not assigned to player's equipment array.");

    fail_if(QUERY_FLAG(torch, FLAG_APPLIED), "Torch is applied.");
    fail_if(torch->glow_radius == 0, "Torch is not lit.");
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
    tcase_add_test(tc_core, test_light_apply_apply_5);
    tcase_add_test(tc_core, test_light_apply_apply_6);

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

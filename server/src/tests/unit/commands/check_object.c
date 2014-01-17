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

START_TEST(test_put_object_in_sack)
{
    mapstruct *test_map;
    object *sack, *obj, *dummy;

    dummy = get_archetype("raas");
    dummy->custom_attrset = (player *) calloc(1, sizeof(player));
    dummy->type = PLAYER;
    SET_FLAG(dummy, FLAG_NO_FIX_PLAYER);

    test_map = get_empty_map(5, 5);
    fail_if(test_map == NULL, "Can't create test map.");

    sack = get_archetype("sack");
    insert_ob_in_map(sack, test_map, NULL, 0);
    fail_if(GET_MAP_OB(test_map, 0, 0) != sack);

    obj = get_archetype("letter");
    obj->nrof = 1;
    obj->x = 1;
    insert_ob_in_map(obj, test_map, NULL, 0);
    put_object_in_sack(dummy, sack, obj, 1);
    fail_if(GET_MAP_OB(test_map, 1, 0) != obj, "Object was removed from map?");
    fail_if(sack->inv != NULL, "Sack's inventory isn't null?");
    object_remove(sack, 0);

    /* Basic insertion. */
    sack = get_archetype("sack");
    sack->nrof = 1;
    fail_if(sack->type != CONTAINER, "Sack isn't a container?");
    insert_ob_in_map(sack, test_map, NULL, 0);
    fail_if(GET_MAP_OB(test_map, 0, 0) != sack, "Sack not put on map?");

    SET_FLAG(sack, FLAG_APPLIED);
    put_object_in_sack(dummy, sack, obj, 1);
    fail_if(sack->inv != obj, "Object not inserted into sack?");
    fail_if(GET_MAP_OB(test_map, 1, 0) != NULL, "Object wasn't removed from map?");

    object_remove(obj, 0);
    obj->x = 1;
    insert_ob_in_map(obj, test_map, NULL, 0);
    sack->weight_limit = 1;
    obj->weight = 5;

    put_object_in_sack(dummy, sack, obj, 1);
    fail_if(sack->inv != NULL, "Item was put in sack even if too heavy?");
    fail_if(GET_MAP_OB(test_map, 1, 0) != obj, "Object was removed from map?");
}
END_TEST

static Suite *object_suite(void)
{
    Suite *s = suite_create("object");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, NULL, NULL);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_put_object_in_sack);

    return s;
}

void check_commands_object(void)
{
    Suite *s = object_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/commands/object.xml");
    srunner_set_log(sr, "unit/commands/object.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

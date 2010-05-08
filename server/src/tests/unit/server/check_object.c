/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

START_TEST(test_CAN_MERGE)
{
	object *ob1, *ob2;

	ob1 = get_archetype("bolt");
	ob2 = get_archetype("bolt");
	fail_if(CAN_MERGE(ob1, ob2) == 0, "Should be able to merge 2 same objects.");
	ob2->name = add_string("Not same name");
	fail_if(CAN_MERGE(ob1, ob2) == 1, "Should not be able to merge 2 objects with different names.");
	ob2 = get_archetype("bolt");
	ob2->type++;
	fail_if(CAN_MERGE(ob1, ob2) == 1, "Should not be able to merge 2 objects with different types.");
	ob2 = get_archetype("bolt");
	ob1->nrof = (1UL << 31) - 1;
	ob2->nrof = 1;
	fail_if(CAN_MERGE(ob1, ob2) == 1, "Should not be able to merge 2 objects if result nrof goes to 1 << 31 or higher");
}
END_TEST

START_TEST(test_sum_weight)
{
	object *ob1, *ob2, *ob3, *ob4;
	unsigned long sum;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	ob4 = get_archetype("sack");
	ob1->weight = 10;
	ob1->type = CONTAINER;
	/* 40% reduction of weight */
	ob1->weapon_speed = 0.6f;
	ob2->weight = 6;
	ob2->nrof = 10;
	ob3->weight = 7;
	ob4->weight = 8;
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob1);
	insert_ob_in_ob(ob4, ob1);
	sum = sum_weight(ob1);
	fail_if(sum != 45, "Sum of object's inventory should be 45 ((6 * 10 + 7 + 8) * .6) but was %lu.", sum);
}
END_TEST

START_TEST(test_add_weight)
{
	object *ob1, *ob2, *ob3, *ob4;
	unsigned long sum;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	ob4 = get_archetype("sack");
	ob1->weight = 10;
	ob1->type = CONTAINER;
	ob2->type = CONTAINER;
	ob3->type = CONTAINER;
	/* 40% reduction of weight */
	ob1->weapon_speed = 0.6f;
	ob2->weight = 10;
	ob3->weight = 10;
	ob4->weight = 10;
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob2);
	insert_ob_in_ob(ob4, ob3);
	sum = sum_weight(ob1);
	fail_if(sum != 18, "Sum of object's inventory should be 18 (30 * 0.6 + 10) but was %lu.", sum);
	add_weight(ob4, 10);
	fail_if(ob1->carrying != 24, "After call to add_weight, carrying of ob1 should be 24 but was %d.", ob1->carrying);
}
END_TEST

START_TEST(test_sub_weight)
{
	object *ob1, *ob2, *ob3, *ob4;
	unsigned long sum;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	ob4 = get_archetype("sack");
	ob1->weight = 10;
	ob1->type = CONTAINER;
	ob2->type = CONTAINER;
	ob3->type = CONTAINER;
	/* 40% reduction of weight */
	ob1->weapon_speed = 0.6f;
	ob2->weight = 10;
	ob3->weight = 10;
	ob4->weight = 10;
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob2);
	insert_ob_in_ob(ob4, ob3);
	sum = sum_weight(ob1);
	fail_if(sum != 18, "Sum of object's inventory should be 18 (30 * 0.6 + 10) but was %lu.", sum);
	sub_weight(ob4, 10);
	fail_if(ob1->carrying != 12, "After call to sub_weight, carrying of ob1 should be 12 but was %d.", ob1->carrying);
}
END_TEST

START_TEST(test_get_env_recursive)
{
	object *ob1, *ob2, *ob3, *ob4, *result;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	ob4 = get_archetype("sack");
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob2);
	insert_ob_in_ob(ob4, ob3);
	result = get_env_recursive(ob4);
	fail_if(result != ob1, "Getting top level container for ob4(%p) should bring ob1(%p) but brought %p.", ob4, ob1, result);
}
END_TEST

START_TEST(test_is_player_inv)
{
	object *ob1, *ob2, *ob3, *ob4, *result;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	ob4 = get_archetype("sack");
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob2);
	insert_ob_in_ob(ob4, ob3);
	result = is_player_inv(ob4);
	fail_if(result != NULL, "Getting containing player for ob4(%p) should bring NULL but brought %p while not contained in a player.", ob4, result);
	ob1->type = PLAYER;
	result = is_player_inv(ob4);
	fail_if(result != ob1, "Getting containing player for ob4(%p) should bring ob1(%p) but brought %p while ob1 is player.", ob4, ob1, result);
}
END_TEST

START_TEST(test_dump_object)
{
	object *ob1, *ob2, *ob3;
	StringBuffer *sb;
	char *result;

	ob1 = get_archetype("sack");
	ob2 = get_archetype("sack");
	ob3 = get_archetype("sack");
	insert_ob_in_ob(ob2, ob1);
	insert_ob_in_ob(ob3, ob2);
	sb = stringbuffer_new();
	dump_object(ob1, sb);
	result = stringbuffer_finish(sb);
	fail_if(strstr(result, "arch") == 0, "The object dump should contain 'arch' but was %s", result);
	free(result);
}
END_TEST

START_TEST(test_insert_ob_in_map)
{
	mapstruct *map;
	object *first, *second, *third, *floor , *got;

	map = get_empty_map(5, 5);
	fail_if(map == NULL, "get_empty_map() returned NULL.");

	/* First, simple tests for insertion. */
	floor = get_archetype("water_still");
	floor->x = 3;
	floor->y = 3;
	got = insert_ob_in_map(floor, map, NULL, 0);
	fail_if(got != floor, "Water flood shouldn't disappear.");
	fail_if(floor != GET_MAP_OB(map, 3, 3), "Water floor should be first object.");

	first = get_archetype("letter");
	first->x = 3;
	first->y = 3;
	got = insert_ob_in_map(first, map, NULL, 0);
	fail_if(got != first, "Letter shouldn't disappear.");
	fail_if(floor != GET_MAP_OB(map, 3, 3), "Water floor should still be first object.");
	fail_if(floor->above != first, "Letter should be above floor.");

	second = get_archetype("bolt");
	second->nrof = 1;
	second->x = 3;
	second->y = 3;
	got = insert_ob_in_map(second, map, NULL, 0);
	fail_if(got != second, "Bolt shouldn't disappear.");
	fail_if(floor != GET_MAP_OB(map, 3, 3), "Water floor should still be first object.");
	fail_if(floor->above != second, "Bolt should be above floor.");
	fail_if(second->above != first, "Letter should be above bolt.");

	/* Merging tests. */
	third = get_archetype("bolt");
	third->nrof = 1;
	third->x = 3;
	third->y = 3;
	got = insert_ob_in_map(third, map, NULL, 0);
	fail_if(got != third, "Bolt shouldn't disappear.");
	fail_if(OBJECT_FREE(second), "First bolt should have been removed.");
	fail_if(third->nrof != 2, "Second bolt should have nrof 2.");

	second = get_archetype("bolt");
	second->nrof = 1;
	second->x = 3;
	second->y = 3;
	second->value = 1;
	got = insert_ob_in_map(second, map, NULL, 0);
	fail_if(got != second, "Modified bolt shouldn't disappear.");
	fail_if(second->nrof != 1, "Modified bolt should have nrof 1.");
}
END_TEST

START_TEST(test_get_split_ob)
{
	object *first, *second;
	char err[50];

	first = get_archetype("sack");
	first->nrof = 5;

	second = get_split_ob(first, 2, err, sizeof(err));
	fail_if(second == NULL, "Should return an item.");
	fail_if(second->nrof != 2, "2 expected to split.");
	fail_if(first->nrof != 3, "3 should be left.");

	second = get_split_ob(first, 3, err, sizeof(err));
	fail_if(second == NULL, "Should return an item.");

	first = get_split_ob(second, 10, err, sizeof(err));
	fail_if(first != NULL, "Should return NULL.");
	fail_if(second->nrof != 3, "3 should be left.");
}
END_TEST

START_TEST(test_decrease_ob_nr)
{
	object *first, *second;

	first = get_archetype("bolt");
	first->nrof = 5;

	second = decrease_ob_nr(first, 3);
	fail_if(second != first, "Bolt shouldn't be destroyed.");

	second = decrease_ob_nr(first, 2);
	fail_if(second != NULL, "object_decrease_nrof should return NULL");
}
END_TEST

START_TEST(test_insert_ob_in_ob)
{
	object *container, *item;

	item = get_archetype("bolt");
	item->weight = 50;

	container = get_archetype("sack");
	insert_ob_in_ob(item, container);
	fail_if(container->inv != item, "Item not inserted.");
	fail_if(container->carrying != 50, "Container should carry 50 and not %d.", container->carrying);

	remove_ob(item);
	fail_if(container->carrying != 0, "Container should carry 0 and not %d.", container->carrying);

	/* 50% weight reduction. */
	container->weapon_speed = 0.5f;

	insert_ob_in_ob(item, container);
	fail_if(container->inv != item, "Item not inserted.");
	fail_if(container->carrying != 25, "Container should carry 25 and not %d.", container->carrying);
}
END_TEST

static Suite *object_suite()
{
	Suite *s = suite_create("object");
	TCase *tc_core = tcase_create("Core");

	tcase_add_checked_fixture(tc_core, NULL, NULL);

	suite_add_tcase(s, tc_core);
	tcase_add_test(tc_core, test_CAN_MERGE);
	tcase_add_test(tc_core, test_sum_weight);
	tcase_add_test(tc_core, test_add_weight);
	tcase_add_test(tc_core, test_sub_weight);
	tcase_add_test(tc_core, test_get_env_recursive);
	tcase_add_test(tc_core, test_is_player_inv);
	tcase_add_test(tc_core, test_dump_object);
	tcase_add_test(tc_core, test_insert_ob_in_map);
	tcase_add_test(tc_core, test_get_split_ob);
	tcase_add_test(tc_core, test_decrease_ob_nr);
	tcase_add_test(tc_core, test_insert_ob_in_ob);

	return s;
}

void check_server_object()
{
	Suite *s = object_suite();
	SRunner *sr = srunner_create(s);

	srunner_set_xml(sr, "unit/server/object.xml");
	srunner_set_log(sr, "unit/server/object.out");
	srunner_run_all(sr, CK_ENV);
	srunner_ntests_failed(sr);
	srunner_free(sr);
}

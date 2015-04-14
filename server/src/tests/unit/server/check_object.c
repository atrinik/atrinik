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

START_TEST(test_CAN_MERGE)
{
    object *ob1, *ob2;

    ob1 = get_archetype("bolt");
    ob2 = get_archetype("bolt");
    ck_assert(CAN_MERGE(ob1, ob2));
    FREE_AND_COPY_HASH(ob2->name, "Not same name");
    ck_assert(!CAN_MERGE(ob1, ob2));
    object_destroy(ob2);
    ob2 = get_archetype("bolt");
    ob2->type++;
    ck_assert(!CAN_MERGE(ob1, ob2));
    object_destroy(ob2);
    ob2 = get_archetype("bolt");
    ob1->nrof = INT32_MAX;
    ob2->nrof = 1;
    ck_assert(!CAN_MERGE(ob1, ob2));
    object_destroy(ob1);
    object_destroy(ob2);
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
    ck_assert_uint_eq(sum, 45);
    object_destroy(ob1);
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
    ck_assert_uint_eq(sum, 18);
    add_weight(ob4, 10);
    ck_assert_int_eq(ob1->carrying, 24);
    object_destroy(ob1);
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
    ck_assert_uint_eq(sum, 18);
    sub_weight(ob4, 10);
    ck_assert_int_eq(ob1->carrying, 12);
    object_destroy(ob1);
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
    ck_assert_ptr_eq(result, ob1);
    object_destroy(ob1);
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
    ck_assert_ptr_eq(result, NULL);
    ob1->type = PLAYER;
    result = is_player_inv(ob4);
    ck_assert_ptr_eq(result, ob1);
    ob1->type = CONTAINER;
    object_destroy(ob1);
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
    ck_assert(string_startswith(result, "arch"));
    free(result);
    object_destroy(ob1);
}

END_TEST

START_TEST(test_insert_ob_in_map)
{
    mapstruct *map;
    object *first, *second, *third, *floor_ob, *got;

    map = get_empty_map(5, 5);
    ck_assert_ptr_ne(map, NULL);

    /* First, simple tests for insertion. */
    floor_ob = get_archetype("water_still");
    floor_ob->x = 3;
    floor_ob->y = 3;
    got = insert_ob_in_map(floor_ob, map, NULL, 0);
    ck_assert_ptr_eq(floor_ob, got);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));

    first = get_archetype("letter");
    first->x = 3;
    first->y = 3;
    got = insert_ob_in_map(first, map, NULL, 0);
    ck_assert_ptr_eq(got, first);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));
    ck_assert_ptr_eq(floor_ob->above, first);

    second = get_archetype("bolt");
    second->nrof = 1;
    second->x = 3;
    second->y = 3;
    got = insert_ob_in_map(second, map, NULL, 0);
    ck_assert_ptr_eq(got, second);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));
    ck_assert_ptr_eq(floor_ob->above, second);
    ck_assert_ptr_eq(second->above, first);

    /* Merging tests. */
    third = get_archetype("bolt");
    third->nrof = 1;
    third->x = 3;
    third->y = 3;
    got = insert_ob_in_map(third, map, NULL, 0);
    ck_assert_ptr_eq(got, third);
    ck_assert(OBJECT_FREE(second));
    ck_assert_uint_eq(third->nrof, 2);

    second = get_archetype("bolt");
    second->nrof = 1;
    second->x = 3;
    second->y = 3;
    second->value = 1;
    got = insert_ob_in_map(second, map, NULL, 0);
    ck_assert_ptr_eq(got, second);
    ck_assert_uint_eq(second->nrof, 1);
    ck_assert_uint_eq(third->nrof, 2);
}

END_TEST

START_TEST(test_decrease_ob_nr)
{
    object *first, *second;

    first = get_archetype("bolt");
    first->nrof = 5;

    second = decrease_ob_nr(first, 3);
    ck_assert_ptr_eq(second, first);
    ck_assert(!OBJECT_FREE(first));

    second = decrease_ob_nr(first, 2);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));

    first = get_archetype("bolt");
    first->nrof = 5;

    second = decrease_ob_nr(first, 5);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));

    first = get_archetype("bolt");
    first->nrof = 5;

    second = decrease_ob_nr(first, 50);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));
}

END_TEST

START_TEST(test_insert_ob_in_ob)
{
    object *container, *item;

    item = get_archetype("bolt");
    item->weight = 50;

    container = get_archetype("sack");
    insert_ob_in_ob(item, container);
    ck_assert_ptr_eq(container->inv, item);
    ck_assert_int_eq(container->carrying, 50);

    object_remove(item, 0);
    ck_assert_int_eq(container->carrying, 0);

    /* 50% weight reduction. */
    container->weapon_speed = 0.5f;

    insert_ob_in_ob(item, container);
    ck_assert_ptr_eq(container->inv, item);
    ck_assert_int_eq(container->carrying, 25);

    object_destroy(container);
}

END_TEST

START_TEST(test_can_pick)
{
    mapstruct *map;
    object *pl, *ob;

    check_setup_env_pl(&map, &pl);

    ob = get_archetype("sack");
    ck_assert(can_pick(pl, ob));
    ob->weight = 0;
    ck_assert(!can_pick(pl, ob));
    object_destroy(ob);

    ob = get_archetype("sack");
    SET_FLAG(ob, FLAG_NO_PICK);
    ck_assert(!can_pick(pl, ob));
    SET_FLAG(ob, FLAG_UNPAID);
    ck_assert(can_pick(pl, ob));
    object_destroy(ob);

    ob = get_archetype("sack");
    SET_FLAG(ob, FLAG_IS_INVISIBLE);
    ck_assert(!can_pick(pl, ob));
    object_destroy(ob);

    ob = get_archetype("raas");
    ck_assert(!can_pick(pl, ob));
    object_destroy(ob);
}

END_TEST

START_TEST(test_object_create_clone)
{
    object *ob, *clone_ob;

    ob = get_archetype("raas");
    insert_ob_in_ob(get_archetype("sack"), ob);
    clone_ob = object_create_clone(ob);
    ck_assert_str_eq(clone_ob->name, ob->name);
    ck_assert_ptr_ne(clone_ob->inv, NULL);
    ck_assert_str_eq(clone_ob->inv->name, ob->inv->name);

    object_destroy(ob);
    object_destroy(clone_ob);
}

END_TEST

START_TEST(test_was_destroyed)
{
    object *ob, *ob2;
    tag_t ob_tag, ob2_tag;
    mapstruct *m;

    m = get_empty_map(1, 1);
    ck_assert_ptr_ne(m, NULL);

    ob = get_archetype("sack");
    ob_tag = ob->count;
    insert_ob_in_map(ob, m, ob, 0);
    ck_assert(!was_destroyed(ob, ob_tag));
    ob2 = get_archetype("bolt");
    ob2_tag = ob2->count;
    insert_ob_in_ob(ob2, ob);
    ck_assert(!was_destroyed(ob2, ob2_tag));
    object_remove(ob, 0);
    ck_assert(!was_destroyed(ob, ob_tag));
    object_destroy(ob);
    ck_assert(was_destroyed(ob, ob_tag));
    ck_assert(was_destroyed(ob2, ob2_tag));
}

END_TEST

START_TEST(test_load_object_str)
{
    object *ob;

    ob = load_object_str("arch sack\nend\n");
    ck_assert_ptr_ne(ob, NULL);
    ck_assert_str_eq(ob->arch->name, "sack");

    object_destroy(ob);
    ob = load_object_str("arch sack\nname magic sack\nweight 129\nend\n");
    ck_assert_ptr_ne(ob, NULL);
    ck_assert_str_eq(ob->name, "magic sack");
    ck_assert_int_eq(ob->weight, 129);

    object_destroy(ob);
}

END_TEST

START_TEST(test_object_reverse_inventory)
{
    char *cp, *cp2;
    object *ob;
    StringBuffer *sb;

    cp = path_file_contents("unit/test_object_reverse_inventory.arc");
    ob = load_object_str(cp);

    object_reverse_inventory(ob);

    sb = stringbuffer_new();
    dump_object_rec(ob, sb);
    cp2 = stringbuffer_finish(sb);

    ck_assert_str_eq(cp, cp2);

    object_destroy(ob);
    free(cp);
    free(cp2);
}

END_TEST

static Suite *object_suite(void)
{
    Suite *s = suite_create("object");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_CAN_MERGE);
    tcase_add_test(tc_core, test_sum_weight);
    tcase_add_test(tc_core, test_add_weight);
    tcase_add_test(tc_core, test_sub_weight);
    tcase_add_test(tc_core, test_get_env_recursive);
    tcase_add_test(tc_core, test_is_player_inv);
    tcase_add_test(tc_core, test_dump_object);
    tcase_add_test(tc_core, test_insert_ob_in_map);
    tcase_add_test(tc_core, test_decrease_ob_nr);
    tcase_add_test(tc_core, test_insert_ob_in_ob);
    tcase_add_test(tc_core, test_can_pick);
    tcase_add_test(tc_core, test_object_create_clone);
    tcase_add_test(tc_core, test_was_destroyed);
    tcase_add_test(tc_core, test_load_object_str);
    tcase_add_test(tc_core, test_object_reverse_inventory);

    return s;
}

void check_server_object(void)
{
    Suite *s = object_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/object.xml");
    srunner_set_log(sr, "unit/server/object.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

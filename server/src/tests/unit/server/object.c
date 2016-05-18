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
#include <toolkit_string.h>
#include <arch.h>
#include <object.h>
#include <path.h>

START_TEST(test_object_can_merge)
{
    object *ob1, *ob2;

    ob1 = arch_get("bolt");
    ob2 = arch_get("bolt");
    ck_assert(object_can_merge(ob1, ob2));
    FREE_AND_COPY_HASH(ob2->name, "Not same name");
    ck_assert(!object_can_merge(ob1, ob2));
    object_destroy(ob2);
    ob2 = arch_get("bolt");
    ob2->type++;
    ck_assert(!object_can_merge(ob1, ob2));
    object_destroy(ob2);
    ob2 = arch_get("bolt");
    ob1->nrof = INT32_MAX;
    ob2->nrof = 1;
    ck_assert(!object_can_merge(ob1, ob2));
    object_destroy(ob1);
    object_destroy(ob2);
}
END_TEST

START_TEST(test_object_weight_sum)
{
    object *ob1, *ob2, *ob3, *ob4;
    unsigned long sum;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    ob4 = arch_get("sack");
    ob1->weight = 10;
    ob1->type = CONTAINER;
    /* 40% reduction of weight */
    ob1->weapon_speed = 0.6f;
    ob2->weight = 6;
    ob2->nrof = 10;
    ob3->weight = 7;
    ob4->weight = 8;
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob1, 0);
    object_insert_into(ob4, ob1, 0);
    sum = object_weight_sum(ob1);
    ck_assert_uint_eq(sum, 45);
    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_weight_add)
{
    object *ob1, *ob2, *ob3, *ob4;
    unsigned long sum;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    ob4 = arch_get("sack");
    ob1->weight = 10;
    ob1->type = CONTAINER;
    ob2->type = CONTAINER;
    ob3->type = CONTAINER;
    /* 40% reduction of weight */
    ob1->weapon_speed = 0.6f;
    ob2->weight = 10;
    ob3->weight = 10;
    ob4->weight = 10;
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob2, 0);
    object_insert_into(ob4, ob3, 0);
    sum = object_weight_sum(ob1);
    ck_assert_uint_eq(sum, 18);
    object_weight_add(ob4, 10);
    ck_assert_int_eq(ob1->carrying, 24);
    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_weight_sub)
{
    object *ob1, *ob2, *ob3, *ob4;
    unsigned long sum;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    ob4 = arch_get("sack");
    ob1->weight = 10;
    ob1->type = CONTAINER;
    ob2->type = CONTAINER;
    ob3->type = CONTAINER;
    /* 40% reduction of weight */
    ob1->weapon_speed = 0.6f;
    ob2->weight = 10;
    ob3->weight = 10;
    ob4->weight = 10;
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob2, 0);
    object_insert_into(ob4, ob3, 0);
    sum = object_weight_sum(ob1);
    ck_assert_uint_eq(sum, 18);
    object_weight_sub(ob4, 10);
    ck_assert_int_eq(ob1->carrying, 12);
    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_get_env)
{
    object *ob1, *ob2, *ob3, *ob4, *result;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    ob4 = arch_get("sack");
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob2, 0);
    object_insert_into(ob4, ob3, 0);
    result = object_get_env(ob4);
    ck_assert_ptr_eq(result, ob1);
    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_is_in_inventory)
{
    object *ob1, *ob2, *ob3, *ob4;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    ob4 = arch_get("sack");
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob2, 0);
    object_insert_into(ob4, ob3, 0);

    ck_assert(object_is_in_inventory(ob2, ob1));
    ck_assert(object_is_in_inventory(ob3, ob1));
    ck_assert(object_is_in_inventory(ob3, ob2));
    ck_assert(object_is_in_inventory(ob4, ob1));
    ck_assert(object_is_in_inventory(ob4, ob2));
    ck_assert(object_is_in_inventory(ob4, ob3));

    ck_assert(!object_is_in_inventory(ob1, ob1));
    ck_assert(!object_is_in_inventory(ob2, ob2));
    ck_assert(!object_is_in_inventory(ob3, ob3));
    ck_assert(!object_is_in_inventory(ob4, ob4));

    ck_assert(!object_is_in_inventory(ob1, ob2));
    ck_assert(!object_is_in_inventory(ob2, ob3));
    ck_assert(!object_is_in_inventory(ob3, ob4));

    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_dump)
{
    object *ob1, *ob2, *ob3;
    StringBuffer *sb;
    char *result;

    ob1 = arch_get("sack");
    ob2 = arch_get("sack");
    ob3 = arch_get("sack");
    object_insert_into(ob2, ob1, 0);
    object_insert_into(ob3, ob2, 0);
    sb = stringbuffer_new();
    object_dump(ob1, sb);
    result = stringbuffer_finish(sb);
    ck_assert(string_startswith(result, "arch"));
    efree(result);
    object_destroy(ob1);
}
END_TEST

START_TEST(test_object_insert_map)
{
    mapstruct *map;
    object *first, *second, *third, *floor_ob, *got;

    map = get_empty_map(5, 5);
    ck_assert_ptr_ne(map, NULL);

    /* First, simple tests for insertion. */
    floor_ob = arch_get("water_still");
    floor_ob->x = 3;
    floor_ob->y = 3;
    got = object_insert_map(floor_ob, map, NULL, 0);
    ck_assert_ptr_eq(floor_ob, got);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));

    first = arch_get("letter");
    first->x = 3;
    first->y = 3;
    got = object_insert_map(first, map, NULL, 0);
    ck_assert_ptr_eq(got, first);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));
    ck_assert_ptr_eq(floor_ob->above, first);

    second = arch_get("bolt");
    second->nrof = 1;
    second->x = 3;
    second->y = 3;
    got = object_insert_map(second, map, NULL, 0);
    ck_assert_ptr_eq(got, second);
    ck_assert_ptr_eq(floor_ob, GET_MAP_OB(map, 3, 3));
    ck_assert_ptr_eq(floor_ob->above, second);
    ck_assert_ptr_eq(second->above, first);

    /* Merging tests. */
    third = arch_get("bolt");
    third->nrof = 1;
    third->x = 3;
    third->y = 3;
    got = object_insert_map(third, map, NULL, 0);
    ck_assert_ptr_eq(got, third);
    ck_assert(OBJECT_FREE(second));
    ck_assert_uint_eq(third->nrof, 2);

    second = arch_get("bolt");
    second->nrof = 1;
    second->x = 3;
    second->y = 3;
    second->value = 1;
    got = object_insert_map(second, map, NULL, 0);
    ck_assert_ptr_eq(got, second);
    ck_assert_uint_eq(second->nrof, 1);
    ck_assert_uint_eq(third->nrof, 2);
}
END_TEST

START_TEST(test_object_decrease)
{
    object *first, *second;

    first = arch_get("bolt");
    first->nrof = 5;

    second = object_decrease(first, 3);
    ck_assert_ptr_eq(second, first);
    ck_assert(!OBJECT_FREE(first));

    second = object_decrease(first, 2);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));

    first = arch_get("bolt");
    first->nrof = 5;

    second = object_decrease(first, 5);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));

    first = arch_get("bolt");
    first->nrof = 5;

    second = object_decrease(first, 50);
    ck_assert_ptr_eq(second, NULL);
    ck_assert(OBJECT_FREE(first));
}
END_TEST

START_TEST(test_object_insert_into)
{
    object *container, *item;

    item = arch_get("bolt");
    item->weight = 50;

    container = arch_get("sack");
    object_insert_into(item, container, 0);
    ck_assert_ptr_eq(container->inv, item);
    ck_assert_int_eq(container->carrying, 50);

    object_remove(item, 0);
    ck_assert_int_eq(container->carrying, 0);

    /* 50% weight reduction. */
    container->weapon_speed = 0.5f;

    object_insert_into(item, container, 0);
    ck_assert_ptr_eq(container->inv, item);
    ck_assert_int_eq(container->carrying, 25);

    object_destroy(container);
}
END_TEST

START_TEST(test_object_can_pick)
{
    mapstruct *map;
    object *pl, *ob;

    check_setup_env_pl(&map, &pl);

    ob = arch_get("sack");
    ck_assert(object_can_pick(pl, ob));
    ob->weight = 0;
    ck_assert(!object_can_pick(pl, ob));
    object_destroy(ob);

    ob = arch_get("sack");
    SET_FLAG(ob, FLAG_NO_PICK);
    ck_assert(!object_can_pick(pl, ob));
    SET_FLAG(ob, FLAG_UNPAID);
    ck_assert(object_can_pick(pl, ob));
    object_destroy(ob);

    ob = arch_get("sack");
    SET_FLAG(ob, FLAG_IS_INVISIBLE);
    ck_assert(!object_can_pick(pl, ob));
    object_destroy(ob);

    ob = arch_get("raas");
    ck_assert(!object_can_pick(pl, ob));
    object_destroy(ob);
}
END_TEST

START_TEST(test_object_clone)
{
    object *ob, *clone_ob;

    ob = arch_get("raas");
    object_insert_into(arch_get("sack"), ob, 0);
    clone_ob = object_clone(ob);
    ck_assert_str_eq(clone_ob->name, ob->name);
    ck_assert_ptr_ne(clone_ob->inv, NULL);
    ck_assert_str_eq(clone_ob->inv->name, ob->inv->name);

    object_destroy(ob);
    object_destroy(clone_ob);
}
END_TEST

START_TEST(test_object_load_str)
{
    object *ob;

    ob = object_load_str("arch sack\nend\n");
    ck_assert_ptr_ne(ob, NULL);
    ck_assert_str_eq(ob->arch->name, "sack");
    object_destroy(ob);

    ob = object_load_str("arch sack\nname magic sack\nweight 129\nend\n");
    ck_assert_ptr_ne(ob, NULL);
    ck_assert_str_eq(ob->name, "magic sack");
    ck_assert_int_eq(ob->weight, 129);
    object_destroy(ob);

    ob = object_load_str("arch sack\narch sword\narch sword\ntitle of swords\n"
                         "end\nend\nend\n");
    ck_assert_ptr_ne(ob, NULL);
    ck_assert_str_eq(ob->arch->name, "sack");
    ck_assert_ptr_ne(ob->inv, NULL);
    ck_assert_str_eq(ob->inv->arch->name, "sword");
    ck_assert_ptr_eq(ob->inv->title, NULL);
    ck_assert_ptr_ne(ob->inv->inv, NULL);
    ck_assert_str_eq(ob->inv->inv->arch->name, "sword");
    ck_assert_str_eq(ob->inv->inv->title, "of swords");
    object_destroy(ob);
}
END_TEST

START_TEST(test_object_reverse_inventory)
{
    char *cp, *cp2;
    object *ob;
    StringBuffer *sb;

    cp = path_file_contents("src/tests/data/test_object_reverse_inventory.arc");
    ob = object_load_str(cp);

    object_reverse_inventory(ob);

    sb = stringbuffer_new();
    object_dump_rec(ob, sb);
    cp2 = stringbuffer_finish(sb);

    ck_assert_str_eq(cp, cp2);

    object_destroy(ob);
    efree(cp);
    efree(cp2);
}
END_TEST

START_TEST(test_object_create_singularity)
{
    object *obj;

    obj = object_create_singularity("JO3584jke");
    ck_assert_ptr_ne(obj, NULL);
    ck_assert_ptr_ne(obj->name, NULL);
    ck_assert(strstr(obj->name, "JO3584jke") != NULL);
    object_destroy(obj);

    obj = object_create_singularity(NULL);
    ck_assert_ptr_ne(obj, NULL);
    ck_assert_ptr_ne(obj->name, NULL);
    ck_assert(strstr(obj->name, "singularity") != NULL);
    object_destroy(obj);
}
END_TEST

START_TEST(test_OBJECT_DESTROYED)
{
    object *ob, *ob2;
    tag_t ob_tag, ob2_tag;
    mapstruct *m;

    m = get_empty_map(1, 1);
    ck_assert_ptr_ne(m, NULL);

    ob = arch_get("sack");
    ob_tag = ob->count;
    object_insert_map(ob, m, ob, 0);
    ck_assert(!OBJECT_DESTROYED(ob, ob_tag));
    ob2 = arch_get("bolt");
    ob2_tag = ob2->count;
    object_insert_into(ob2, ob, 0);
    ck_assert(!OBJECT_DESTROYED(ob2, ob2_tag));
    object_remove(ob, 0);
    ck_assert(!OBJECT_DESTROYED(ob, ob_tag));
    object_destroy(ob);
    ck_assert(OBJECT_DESTROYED(ob, ob_tag));
    ck_assert(OBJECT_DESTROYED(ob2, ob2_tag));
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("object");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_object_can_merge);
    tcase_add_test(tc_core, test_object_weight_sum);
    tcase_add_test(tc_core, test_object_weight_add);
    tcase_add_test(tc_core, test_object_weight_sub);
    tcase_add_test(tc_core, test_object_get_env);
    tcase_add_test(tc_core, test_object_is_in_inventory);
    tcase_add_test(tc_core, test_object_dump);
    tcase_add_test(tc_core, test_object_insert_map);
    tcase_add_test(tc_core, test_object_decrease);
    tcase_add_test(tc_core, test_object_insert_into);
    tcase_add_test(tc_core, test_object_can_pick);
    tcase_add_test(tc_core, test_object_clone);
    tcase_add_test(tc_core, test_object_load_str);
    tcase_add_test(tc_core, test_object_reverse_inventory);
    tcase_add_test(tc_core, test_object_create_singularity);
    tcase_add_test(tc_core, test_OBJECT_DESTROYED);

    return s;
}

void check_server_object(void)
{
    check_run_suite(suite(), __FILE__);
}

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

START_TEST(test_shop_get_cost)
{
    object *money = arch_get("coppercoin");
    ck_assert_int_eq(1, shop_get_cost(money, COST_BUY));
    ck_assert_int_eq(1, shop_get_cost(money, COST_SELL));
    ck_assert_int_eq(1, shop_get_cost(money, COST_TRUE));
    money->nrof = 72;
    ck_assert_int_eq(72, shop_get_cost(money, COST_BUY));
    ck_assert_int_eq(72, shop_get_cost(money, COST_SELL));
    ck_assert_int_eq(72, shop_get_cost(money, COST_TRUE));
    money->nrof = 0;
    CLEAR_FLAG(money, FLAG_IDENTIFIED);
    ck_assert_int_eq(1, shop_get_cost(money, COST_BUY));
    ck_assert_int_eq(1, shop_get_cost(money, COST_SELL));
    ck_assert_int_eq(1, shop_get_cost(money, COST_TRUE));
    object_destroy(money);

    money = arch_get("silvercoin");
    ck_assert_int_eq(100, shop_get_cost(money, COST_BUY));
    ck_assert_int_eq(100, shop_get_cost(money, COST_SELL));
    ck_assert_int_eq(100, shop_get_cost(money, COST_TRUE));
    money->nrof = 72;
    ck_assert_int_eq(7200, shop_get_cost(money, COST_BUY));
    ck_assert_int_eq(7200, shop_get_cost(money, COST_SELL));
    ck_assert_int_eq(7200, shop_get_cost(money, COST_TRUE));
    object_destroy(money);

    object *sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    ck_assert_int_eq(sword->value, shop_get_cost(sword, COST_BUY));
    ck_assert_int_eq(sword->value * 0.2, shop_get_cost(sword, COST_SELL));
    ck_assert_int_eq(sword->value, shop_get_cost(sword, COST_TRUE));
    sword->nrof = 5;
    ck_assert_int_eq(sword->value * 5, shop_get_cost(sword, COST_BUY));
    ck_assert_int_eq(sword->value * 5 * 0.2, shop_get_cost(sword, COST_SELL));
    ck_assert_int_eq(sword->value * 5, shop_get_cost(sword, COST_TRUE));
    SET_FLAG(sword, FLAG_CURSED);
    ck_assert_int_eq(0, shop_get_cost(sword, COST_BUY));
    ck_assert_int_eq(0, shop_get_cost(sword, COST_SELL));
    ck_assert_int_eq(0, shop_get_cost(sword, COST_TRUE));
    CLEAR_FLAG(sword, FLAG_DAMNED);
    SET_FLAG(sword, FLAG_DAMNED);
    ck_assert_int_eq(0, shop_get_cost(sword, COST_BUY));
    ck_assert_int_eq(0, shop_get_cost(sword, COST_SELL));
    ck_assert_int_eq(0, shop_get_cost(sword, COST_TRUE));
    object_destroy(sword);
}
END_TEST

START_TEST(test_shop_get_cost_string)
{
    ck_assert_str_eq(shop_get_cost_string(0), "nothing");
    ck_assert_str_eq(shop_get_cost_string(1), "1 copper coin");
    ck_assert_str_eq(shop_get_cost_string(2), "2 copper coins");
    ck_assert_str_eq(shop_get_cost_string(100), "1 silver coin");
    ck_assert_str_eq(shop_get_cost_string(501),
            "5 silver coins and 1 copper coin");
    ck_assert_str_eq(shop_get_cost_string(10000), "1 gold coin");
    ck_assert_str_eq(shop_get_cost_string(30000), "3 gold coins");
    ck_assert_str_eq(shop_get_cost_string(6849602841), "68 amber coins, "
            "4 mithril coins, 9 jade coins, 60 gold coins, 28 silver coins "
            "and 41 copper coins");
}
END_TEST

START_TEST(test_shop_get_cost_string_item)
{
    object *sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    sword->value = 0;
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_BUY), "nothing");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_SELL), "nothing");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_TRUE), "nothing");
    sword->value = 1;
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_BUY),
            "1 copper coin");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_SELL),
            "1 copper coin");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_TRUE),
            "1 copper coin");
    sword->value = 11101;
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_BUY),
            "1 gold coin, 11 silver coins and 1 copper coin");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_SELL),
            "22 silver coins and 20 copper coins");
    ck_assert_str_eq(shop_get_cost_string_item(sword, COST_TRUE),
            "1 gold coin, 11 silver coins and 1 copper coin");
    object_destroy(sword);
}
END_TEST

START_TEST(test_shop_get_money)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    object *sack = arch_get("sack");
    sack = object_insert_into(sack, pl, 0);
    object *money = object_insert_into(arch_get("coppercoin"), sack, 0);
    money->nrof = 50;
    money = object_insert_into(arch_get("silvercoin"), sack, 0);
    money->nrof = 3;
    ck_assert_uint_eq(shop_get_money(pl), 350);
    ck_assert_uint_eq(shop_get_money(sack), 350);
    money = object_insert_into(arch_get("goldcoin"), pl, 0);
    money->nrof = 1;
    ck_assert_uint_eq(shop_get_money(pl), 10350);
    ck_assert_uint_eq(shop_get_money(sack), 350);
    money = object_insert_into(arch_get("silvercoin"), pl, 0);
    money->nrof = 30;
    ck_assert_uint_eq(shop_get_money(pl), 13350);
    ck_assert_uint_eq(shop_get_money(sack), 350);
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 800;
    ck_assert_uint_eq(shop_get_money(pl), 14150);
    ck_assert_uint_eq(shop_get_money(sack), 350);
    int64_t value;
    bank_deposit(pl, "50 copper", &value);
    ck_assert_uint_eq(value, 50);
    ck_assert_uint_eq(bank_get_balance(pl), 50);
    ck_assert_uint_eq(shop_get_money(pl), 14150);
}
END_TEST

START_TEST(test_shop_pay)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    ck_assert(shop_pay(pl, 0));
    object *sack = arch_get("sack");
    sack = object_insert_into(sack, pl, 0);
    object *money = object_insert_into(arch_get("coppercoin"), sack, 0);
    money->nrof = 50;
    money = object_insert_into(arch_get("silvercoin"), sack, 0);
    money->nrof = 3;
    money = object_insert_into(arch_get("goldcoin"), sack, 0);
    money->nrof = 15;
    money = object_insert_into(arch_get("goldcoin"), pl, 0);
    money->nrof = 1;
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 800;
    money = object_insert_into(arch_get("ambercoin"), pl, 0);
    money->nrof = 1;
    int64_t value;
    bank_deposit(pl, "500 copper", &value);
    ck_assert_uint_eq(value, 500);
    ck_assert_uint_eq(bank_get_balance(pl), 500);
    int64_t total = 100161150;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert(shop_pay(pl, 0));
    ck_assert_uint_eq(shop_get_money(pl), total);
    int64_t to_pay = 1;
    ck_assert(shop_pay(pl, to_pay));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = 5009;
    ck_assert(shop_pay(pl, to_pay));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert(!shop_pay(pl, 10000000000));
    ck_assert(!shop_pay(pl, 5436565469565));
    to_pay = 596079;
    ck_assert(shop_pay(pl, to_pay));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = 11111;
    ck_assert(shop_pay(pl, to_pay));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = total - 50;
    ck_assert(shop_pay(pl, to_pay));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert(shop_pay(pl, total));
    total = 0;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert(!shop_pay(pl, 43534));
    ck_assert(!shop_pay(pl, 1));
    ck_assert(shop_pay(pl, 0));
}
END_TEST

START_TEST(test_shop_pay_item)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    ck_assert(shop_pay(pl, 0));
    object *sack = arch_get("sack");
    sack = object_insert_into(sack, pl, 0);
    object *sack2 = arch_get("sack");
    sack2 = object_insert_into(sack2, sack, 0);
    object *money = object_insert_into(arch_get("coppercoin"), sack, 0);
    money->nrof = 50;
    money = object_insert_into(arch_get("silvercoin"), sack, 0);
    money->nrof = 3;
    money = object_insert_into(arch_get("goldcoin"), sack, 0);
    money->nrof = 15;
    money = object_insert_into(arch_get("silvercoin"), sack2, 0);
    money->nrof = 38;
    money = object_insert_into(arch_get("goldcoin"), sack2, 0);
    money->nrof = 168;
    money = object_insert_into(arch_get("goldcoin"), pl, 0);
    money->nrof = 1;
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 800;
    money = object_insert_into(arch_get("ambercoin"), pl, 0);
    money->nrof = 1;
    int64_t value;
    bank_deposit(pl, "3 gold 1 silver 18 copper", &value);
    ck_assert_uint_eq(value, 30118);
    ck_assert_uint_eq(bank_get_balance(pl), 30118);
    int64_t total = 101844950;
    ck_assert_uint_eq(shop_get_money(pl), total);
    object *sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    sword->value = 0;
    ck_assert(shop_pay_item(pl, sword));
    ck_assert_uint_eq(shop_get_money(pl), total);
    int64_t to_pay = 1;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = 8402;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    sword->value = 10000000000;
    ck_assert(!shop_pay_item(pl, sword));
    sword->value = 10000000000;
    ck_assert(!shop_pay_item(pl, sword));
    to_pay = 683921;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = 38392;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = total - 50;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= to_pay;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_pay = total;
    sword->value = to_pay;
    ck_assert(shop_pay_item(pl, sword));
    total -= total;
    ck_assert_uint_eq(shop_get_money(pl), total);
    sword->value = 40394;
    ck_assert(!shop_pay_item(pl, sword));
    sword->value = 1;
    ck_assert(!shop_pay_item(pl, sword));
    sword->value = 0;
    ck_assert(shop_pay_item(pl, sword));
    object_destroy(sword);
}
END_TEST

START_TEST(test_shop_pay_items)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    ck_assert(shop_pay_items(pl));
    object *sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    SET_FLAG(sword, FLAG_UNPAID);
    sword->value = 500;
    sword = object_insert_into(sword, pl, 0);
    object *sword2 = arch_get("sword");
    SET_FLAG(sword2, FLAG_IDENTIFIED);
    SET_FLAG(sword2, FLAG_UNPAID);
    sword2->value = 200;
    sword2 = object_insert_into(sword2, pl, 0);
    ck_assert(!shop_pay_items(pl));
    object *money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 38;
    ck_assert(!shop_pay_items(pl));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    ck_assert(QUERY_FLAG(sword2, FLAG_UNPAID));
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 210;
    ck_assert(!shop_pay_items(pl));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    ck_assert(!QUERY_FLAG(sword2, FLAG_UNPAID));
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 710;
    ck_assert(shop_pay_items(pl));
    ck_assert(!QUERY_FLAG(sword, FLAG_UNPAID));
    ck_assert(!QUERY_FLAG(sword2, FLAG_UNPAID));
}
END_TEST

START_TEST(test_shop_sell_item)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    object *sword = arch_get("sword");
    sword->value = 0;
    shop_sell_item(pl, sword);
    ck_assert(QUERY_FLAG(sword, FLAG_IDENTIFIED));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    int64_t total = sword->arch->clone.value * 0.2;
    ck_assert_uint_eq(shop_get_money(pl), total);
    object_destroy(sword);
    sword = arch_get("sword");
    sword->value = 100;
    shop_sell_item(pl, sword);
    ck_assert(QUERY_FLAG(sword, FLAG_IDENTIFIED));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    total += sword->arch->clone.value * 0.2;
    ck_assert_uint_eq(shop_get_money(pl), total);
    object_destroy(sword);
    sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    sword->value = 0;
    shop_sell_item(pl, sword);
    ck_assert(QUERY_FLAG(sword, FLAG_IDENTIFIED));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    ck_assert_uint_eq(shop_get_money(pl), total);
    object_destroy(sword);
    sword = arch_get("sword");
    SET_FLAG(sword, FLAG_IDENTIFIED);
    FREE_AND_COPY_HASH(sword->custom_name, "xxx");
    sword->value = 100;
    shop_sell_item(pl, sword);
    ck_assert(QUERY_FLAG(sword, FLAG_IDENTIFIED));
    ck_assert(QUERY_FLAG(sword, FLAG_UNPAID));
    total += sword->value * 0.2;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_ptr_eq(sword->custom_name, NULL);
    object_destroy(sword);
}
END_TEST

START_TEST(test_shop_insert_coins)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    int64_t total = 0;
    shop_insert_coins(pl, 0);
    ck_assert_uint_eq(shop_get_money(pl), total);
    int64_t to_insert = 1;
    shop_insert_coins(pl, to_insert);
    total += to_insert;
    ck_assert_uint_eq(shop_get_money(pl), total);
    to_insert = 1203940;
    shop_insert_coins(pl, to_insert);
    total += to_insert;
    ck_assert_uint_eq(shop_get_money(pl), total);
    object *sack = arch_get("sack");
    FREE_AND_COPY_HASH(sack->race, "gold");
    sack = object_insert_into(sack, pl, 0);
    object_apply(sack, pl, 0);
    ck_assert(QUERY_FLAG(sack, FLAG_APPLIED));
    to_insert = 1;
    shop_insert_coins(pl, to_insert);
    total += to_insert;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_ptr_ne(sack->inv, NULL);
    ck_assert_str_eq(sack->inv->arch->name, "coppercoin");
    object_apply(sack, pl, 0);
    to_insert = 69302;
    shop_insert_coins(pl, to_insert);
    total += to_insert;
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_ptr_ne(sack->inv, NULL);
    ck_assert_ptr_ne(sack->inv->below, NULL);
    ck_assert_str_eq(sack->inv->below->arch->name, "goldcoin");
    ck_assert_uint_eq(sack->inv->below->nrof, 6);
    object_apply(sack, pl, 0);
    ck_assert(!QUERY_FLAG(sack, FLAG_APPLIED));

    for (int i = 0; i < 10000; i++) {
        shop_insert_coins(pl, 99);
    }

    ck_assert_ptr_ne(GET_MAP_OB(pl->map, pl->x, pl->y), NULL);
    ck_assert_str_eq(GET_MAP_OB(pl->map, pl->x, pl->y)->arch->name,
            "coppercoin");
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("shop");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_shop_get_cost);
    tcase_add_test(tc_core, test_shop_get_cost_string);
    tcase_add_test(tc_core, test_shop_get_cost_string_item);
    tcase_add_test(tc_core, test_shop_get_money);
    tcase_add_test(tc_core, test_shop_pay);
    tcase_add_test(tc_core, test_shop_pay_item);
    tcase_add_test(tc_core, test_shop_pay_items);
    tcase_add_test(tc_core, test_shop_sell_item);
    tcase_add_test(tc_core, test_shop_insert_coins);

    return s;
}

void check_server_shop(void)
{
    check_run_suite(suite(), __FILE__);
}

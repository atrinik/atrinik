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

START_TEST(test_bank_find_info)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    ck_assert_ptr_eq(bank_find_info(pl), NULL);
    object *money = object_insert_into(arch_get("ambercoin"), pl, 0);
    money->nrof = 1;
    int64_t value;
    ck_assert_int_eq(bank_deposit(pl, "1 a", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, money->value);
    ck_assert_uint_eq(bank_get_balance(pl), money->value);
    ck_assert_uint_eq(shop_get_money(pl), money->value);
    ck_assert_ptr_ne(bank_find_info(pl), NULL);
    ck_assert_uint_eq(bank_find_info(pl)->value, money->value);
}
END_TEST

START_TEST(test_bank_deposit)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    int64_t value;
    ck_assert_int_eq(bank_deposit(pl, "", &value), BANK_SYNTAX_ERROR);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 a", &value), BANK_DEPOSIT_AMBER);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 m", &value), BANK_DEPOSIT_MITHRIL);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 j", &value), BANK_DEPOSIT_JADE);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 g", &value), BANK_DEPOSIT_GOLD);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 s", &value), BANK_DEPOSIT_SILVER);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "1 c", &value), BANK_DEPOSIT_COPPER);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_deposit(pl, "all", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), 0);
    object *money = object_insert_into(arch_get("ambercoin"), pl, 0);
    money->nrof = 1;
    ck_assert_int_eq(bank_deposit(pl, "1 a", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, money->value);
    int64_t total = money->value;
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 58;
    ck_assert_int_eq(bank_deposit(pl, "50 copper", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, money->value * 50);
    total += money->value * 50;
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total + money->value * 8);
    ck_assert_uint_eq(money->nrof, 8);
    ck_assert_int_eq(bank_deposit(pl, "9 copper", &value), BANK_DEPOSIT_COPPER);
    ck_assert_uint_eq(value, 0);
    tag_t money_tag = money->count;
    ck_assert_int_eq(bank_deposit(pl, "8 copper", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, money->value * 8);
    total += money->value * 8;
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert(!OBJECT_VALID(money, money_tag));

    money = object_insert_into(arch_get("goldcoin"), pl, 0);
    money->nrof = 5923;
    int64_t val = money->value * money->nrof;
    money = object_insert_into(arch_get("silvercoin"), pl, 0);
    money->nrof = 5091;
    val += money->value * money->nrof;
    money = object_insert_into(arch_get("coppercoin"), pl, 0);
    money->nrof = 5829104;
    val += money->value * money->nrof;
    money = object_insert_into(arch_get("jadecoin"), pl, 0);
    money->nrof = 5938;
    val += money->value * money->nrof;
    total += val;

    ck_assert_int_eq(bank_deposit(pl, "all", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, val);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);

    ck_assert_int_eq(bank_deposit(pl, "all", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
}
END_TEST

START_TEST(test_bank_withdraw)
{
    mapstruct *map;
    object *pl;
    check_setup_env_pl(&map, &pl);
    int64_t value;
    ck_assert_int_eq(bank_withdraw(pl, "", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 a", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 m", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 j", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 g", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 s", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "1 c", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "all", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), 0);
    object *money = object_insert_into(arch_get("ambercoin"), pl, 0);
    money->nrof = 50;
    int64_t val = money->value * money->nrof;
    ck_assert_int_eq(bank_deposit(pl, "all", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, val);
    int64_t total = val;
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "", &value), BANK_SYNTAX_ERROR);
    ck_assert_uint_eq(value, 0);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 a", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 m", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 j", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 g", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 s", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);
    ck_assert_int_eq(bank_withdraw(pl, "10000000 c", &value),
            BANK_WITHDRAW_HIGH);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);

    ck_assert_int_eq(bank_withdraw(pl, "1000000 c", &value),
            BANK_WITHDRAW_OVERWEIGHT);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), total);
    ck_assert_uint_eq(shop_get_money(pl), total);

    int64_t in_bank = total;
    ck_assert_int_eq(bank_withdraw(pl, "1 c 1 s 1 g", &value), BANK_SUCCESS);
    int64_t withdrawn = 10101;
    in_bank -= withdrawn;
    ck_assert_uint_eq(value, withdrawn);
    ck_assert_uint_eq(bank_get_balance(pl), in_bank);
    ck_assert_uint_eq(shop_get_money(pl), total);

    ck_assert_int_eq(bank_withdraw(pl, "5983 copper 83 silver 500 gold",
            &value), BANK_SUCCESS);
    withdrawn = 5014283;
    in_bank -= withdrawn;
    ck_assert_uint_eq(value, withdrawn);
    ck_assert_uint_eq(bank_get_balance(pl), in_bank);
    ck_assert_uint_eq(shop_get_money(pl), total);

    ck_assert_int_eq(bank_withdraw(pl, "all", &value), BANK_SUCCESS);
    ck_assert_uint_eq(value, in_bank);
    ck_assert_uint_eq(bank_get_balance(pl), 0);
    ck_assert_uint_eq(shop_get_money(pl), total);

    ck_assert_int_eq(bank_withdraw(pl, "all", &value), BANK_WITHDRAW_MISSING);
    ck_assert_uint_eq(value, 0);
    ck_assert_uint_eq(bank_get_balance(pl), 0);
    ck_assert_uint_eq(shop_get_money(pl), total);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("bank");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_bank_find_info);
    tcase_add_test(tc_core, test_bank_deposit);
    tcase_add_test(tc_core, test_bank_withdraw);

    return s;
}

void check_server_bank(void)
{
    check_run_suite(suite(), __FILE__);
}

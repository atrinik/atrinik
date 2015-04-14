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

START_TEST(test_PKCS5_PBKDF2_HMAC_SHA2)
{
    unsigned char result[32];
    char hex[64 + 1];

    PKCS5_PBKDF2_HMAC_SHA2((unsigned char *) "Pa$$w0rd", strlen("Pa$$w0rd"),
            (unsigned char *) "xxx", strlen("xxx"), 4096, 32, result);

    ck_assert_int_eq(string_tohex(result, 32, hex, sizeof(hex), false), 64);
    ck_assert_str_eq(hex,
            "1A27DBE11B730C53A42951F40026F148D65708CCF4829BA89F618CF8720BF5FA");
}

END_TEST

static Suite *pbkdf2_suite(void)
{
    Suite *s = suite_create("pbkdf2");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_PKCS5_PBKDF2_HMAC_SHA2);

    return s;
}

void check_server_pbkdf2(void)
{
    Suite *s = pbkdf2_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/pbkdf2.xml");
    srunner_set_log(sr, "unit/server/pbkdf2.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

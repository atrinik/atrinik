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

START_TEST(test_string_replace)
{
    char buf[MAX_BUF], buf2[6];

    /* Check simple replacement */
    string_replace("hello world", "world", "Earth", buf, sizeof(buf));
    fail_unless(strcmp(buf, "hello Earth") == 0, "Failed to replace 'world' with 'Earth' in string.");

    /* Try to replace spaces with nothing */
    string_replace("hello           world", " ", "", buf, sizeof(buf));
    fail_unless(strcmp(buf, "helloworld") == 0, "Failed to replace spaces with nothing.");

    /* Make sure nothing is replaced when replacing "hello" with "world" in an
     * empty string. */
    string_replace("", "hello", "world", buf, sizeof(buf));
    fail_unless(strcmp(buf, "") == 0, "Empty string changed after replacing 'hello' with 'world'.");

    /* Make sure nothing is replaced when replacing "hello" with "world" in a
     * string that doesn't contain "hello". */
    string_replace("hi world", "hello", "world", buf, sizeof(buf));
    fail_unless(strcmp(buf, "hi world") == 0, "String changed after replacing 'hello' with 'world' but 'hello' was not in string.");

    /* Make sure that when both key and replacement are the same, the string
     * remains the same. */
    string_replace("hello world", "hello", "hello", buf, sizeof(buf));
    fail_unless(strcmp(buf, "hello world") == 0, "String changed after replacing 'hello' with 'hello'.");

    /* Make sure that nothing changes when both key and replacement are an
     * empty string. */
    string_replace("hello world", "", "", buf, sizeof(buf));
    fail_unless(strcmp(buf, "hello world") == 0, "String changed when both key and replacement were an empty string.");

    /* Make sure buffer overflow doesn't happen when the buffer is not large
     * enough. */
    string_replace("hello world", "world", "Earth", buf2, sizeof(buf2));
    fail_unless(strcmp(buf2, "hello") == 0, "Replaced string does not match expected output.");

    /* Make sure buffer overflow doesn't happen when the buffer is not large
     * enough, and a replacement occurs prior to reaching the buffer limit. */
    string_replace("hello world", "hello", "", buf2, sizeof(buf2));
    fail_unless(strcmp(buf2, " worl") == 0, "Replaced string does not match expected output.");
}
END_TEST

static Suite *string_suite(void)
{
    Suite *s = suite_create("string");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_string_replace);

    return s;
}

void check_server_string(void)
{
    Suite *s = string_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "unit/server/string.xml");
    srunner_set_log(sr, "unit/server/string.out");
    srunner_run_all(sr, CK_ENV);
    srunner_ntests_failed(sr);
    srunner_free(sr);
}

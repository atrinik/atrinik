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

START_TEST(test_string_replace_char)
{
    char *cp;

    /* Attempt to replace "a", "e" and "o" characters with spaces. */
    cp = strdup("hello world hello");
    string_replace_char(cp, "aeo", ' ');
    fail_unless(strcmp(cp, "h ll  w rld h ll ") == 0, "Replaced string does not match expected output.");
    free(cp);

    /* Attempt to replace any character with space. */
    cp = strdup("hello world");
    string_replace_char(cp, NULL, ' ');
    fail_unless(strcmp(cp, "           ") == 0, "Replaced string does not match expected output.");
    free(cp);

    /* Replace newlines and tabs with spaces. */
    cp = strdup("\thello\n\t\tworld\n");
    string_replace_char(cp, "\n\t", ' ');
    fail_unless(strcmp(cp, " hello   world ") == 0, "Replaced string does not match expected output.");
    free(cp);

    /* Replace forward-slashes with a dollar sign. */
    cp = strdup("/shattered_islands/world_0112");
    string_replace_char(cp, "/", '$');
    fail_unless(strcmp(cp, "$shattered_islands$world_0112") == 0, "Replaced string does not match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_split)
{
    char *cp, *cps[20], *cps2[2];

    /* Attempt to split two words separated by spaces. */
    cp = strdup("hello world");
    fail_unless(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' ') == 2, "Splitting the string didn't return correct number of results.");
    fail_unless(strcmp(cps[0], "hello") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[1], "world") == 0, "Split string doesn't have the correct output.");
    fail_unless(cps[2] == NULL, "Split string doesn't have the correct output.");
    free(cp);

    /* Attempt to split several one-character words. */
    cp = strdup("q w e r t y");
    fail_unless(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' ') == 6, "Splitting the string didn't return correct number of results.");
    fail_unless(strcmp(cps[0], "q") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[1], "w") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[2], "e") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[3], "r") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[4], "t") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps[5], "y") == 0, "Split string doesn't have the correct output.");
    free(cp);

    /* Attempt to split empty string. */
    cp = strdup("");
    fail_unless(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' ') == 0, "Splitting the string didn't return correct number of results.");
    fail_unless(cps[0] == NULL, "Split string doesn't have the correct output.");
    fail_unless(cps[1] == NULL, "Split string doesn't have the correct output.");
    free(cp);

    /* Attempt to split several one-character words, and the result would not
     * fit into the array. */
    cp = strdup("q w e r t y");
    fail_unless(string_split(cp, cps2, sizeof(cps2) / sizeof(*cps2), ' ') == 2, "Splitting the string didn't return correct number of results.");
    fail_unless(strcmp(cps2[0], "q") == 0, "Split string doesn't have the correct output.");
    fail_unless(strcmp(cps2[1], "w e r t y") == 0, "Split string doesn't have the correct output.");
    free(cp);
}
END_TEST

START_TEST(test_string_replace_unprintable_char)
{
    char *cp;

    /* Replace tabs with spaces. */
    cp = strdup("\thello\tworld");
    string_replace_unprintable_chars(cp);
    fail_unless(strcmp(cp, " hello world") == 0, "String doesn't match expected result after replacing unprintable characters.");
    free(cp);

    /* Replace empty string. */
    cp = strdup("");
    string_replace_unprintable_chars(cp);
    fail_unless(strcmp(cp, "") == 0, "String doesn't match expected result after replacing unprintable characters.");
    free(cp);

    /* Replace string that consists of only unprintable characters. */
    cp = strdup("\t\n\n\t\t\t\b\b");
    string_replace_unprintable_chars(cp);
    fail_unless(strcmp(cp, "        ") == 0, "String doesn't match expected result after replacing unprintable characters.");
    free(cp);
}
END_TEST

START_TEST(test_string_format_number_comma)
{
    fail_unless(strcmp(string_format_number_comma(100), "100") == 0, "Formatted string doesn't have correct output.");
    fail_unless(strcmp(string_format_number_comma(1000), "1,000") == 0, "Formatted string doesn't have correct output.");
    fail_unless(strcmp(string_format_number_comma(123456789), "123,456,789") == 0, "Formatted string doesn't have correct output.");
    fail_unless(strcmp(string_format_number_comma(99999999999999999), "99,999,999,999,999,999") == 0, "Formatted string doesn't have correct output.");
    fail_unless(strcmp(string_format_number_comma(UINT64_MAX), "18,446,744,073,709,551,615") == 0, "Formatted string doesn't have correct output.");
}
END_TEST

START_TEST(test_string_toupper)
{
    char *cp;

    cp = strdup("hello world");
    string_toupper(cp);
    fail_unless(strcmp(cp, "HELLO WORLD") == 0, "Transformed string doesn't match expected output.");
    free(cp);

    cp = strdup("Hello");
    string_toupper(cp);
    fail_unless(strcmp(cp, "HELLO") == 0, "Transformed string doesn't match expected output.");
    free(cp);

    cp = strdup("");
    string_toupper(cp);
    fail_unless(strcmp(cp, "") == 0, "Transformed string doesn't match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_tolower)
{
    char *cp;

    cp = strdup("HELLO WORLD");
    string_tolower(cp);
    fail_unless(strcmp(cp, "hello world") == 0, "Transformed string doesn't match expected output.");
    free(cp);

    cp = strdup("hELLO");
    string_tolower(cp);
    fail_unless(strcmp(cp, "hello") == 0, "Transformed string doesn't match expected output.");
    free(cp);

    cp = strdup("");
    string_tolower(cp);
    fail_unless(strcmp(cp, "") == 0, "Transformed string doesn't match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_whitespace_trim)
{
    char *cp;

    cp = strdup("            ");
    string_whitespace_trim(cp);
    fail_unless(strcmp(cp, "") == 0, "Trimmed string doesn't match expected output.");
    free(cp);

    cp = strdup("hello world        \t\t");
    string_whitespace_trim(cp);
    fail_unless(strcmp(cp, "hello world") == 0, "Trimmed string doesn't match expected output.");
    free(cp);

    cp = strdup("           hello world");
    string_whitespace_trim(cp);
    fail_unless(strcmp(cp, "hello world") == 0, "Trimmed string doesn't match expected output.");
    free(cp);

    cp = strdup("\t              hello world   \t   ");
    string_whitespace_trim(cp);
    fail_unless(strcmp(cp, "hello world") == 0, "Trimmed string doesn't match expected output.");
    free(cp);

    cp = strdup("   hello world   ");
    string_whitespace_trim(cp);
    fail_unless(strcmp(cp, "hello world") == 0, "Trimmed string doesn't match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_whitespace_squeeze)
{
    char *cp;

    cp = strdup(" hello world ");
    string_whitespace_squeeze(cp);
    fail_unless(strcmp(cp, " hello world ") == 0, "Squeezed string doesn't match expected output.");
    free(cp);

    cp = strdup(" hello         world ");
    string_whitespace_squeeze(cp);
    fail_unless(strcmp(cp, " hello world ") == 0, "Squeezed string doesn't match expected output.");
    free(cp);

    cp = strdup("      hello    world ");
    string_whitespace_squeeze(cp);
    fail_unless(strcmp(cp, " hello world ") == 0, "Squeezed string doesn't match expected output.");
    free(cp);

    cp = strdup("hello  world     ");
    string_whitespace_squeeze(cp);
    fail_unless(strcmp(cp, "hello world ") == 0, "Squeezed string doesn't match expected output.");
    free(cp);
}
END_TEST

static Suite *string_suite(void)
{
    Suite *s = suite_create("string");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);

    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_string_replace);
    tcase_add_test(tc_core, test_string_replace_char);
    tcase_add_test(tc_core, test_string_split);
    tcase_add_test(tc_core, test_string_replace_unprintable_char);
    tcase_add_test(tc_core, test_string_format_number_comma);
    tcase_add_test(tc_core, test_string_toupper);
    tcase_add_test(tc_core, test_string_tolower);
    tcase_add_test(tc_core, test_string_whitespace_trim);
    tcase_add_test(tc_core, test_string_whitespace_squeeze);

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

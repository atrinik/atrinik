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

START_TEST(test_string_newline_to_literal)
{
    char *cp;

    cp = strdup("hello\\nworld");
    string_newline_to_literal(cp);
    fail_unless(strcmp(cp, "hello\nworld") == 0, "Didn't correctly replace \\n by literal newline character.");
    free(cp);

    cp = strdup("\\n\\n\\n");
    string_newline_to_literal(cp);
    fail_unless(strcmp(cp, "\n\n\n") == 0, "Didn't correctly replace \\n by literal newline character.");
    free(cp);

    cp = strdup("");
    string_newline_to_literal(cp);
    fail_unless(strcmp(cp, "") == 0, "Didn't correctly replace \\n by literal newline character.");
    free(cp);
}
END_TEST

START_TEST(test_string_get_word)
{
    char *cp, word[MAX_BUF];
    size_t pos;

    cp = strdup("hello world");
    pos = 0;
    fail_unless(strcmp(string_get_word(cp, &pos, ' ', word, sizeof(word), 0), "hello") == 0, "Didn't get correct word.");
    fail_unless(strcmp(string_get_word(cp, &pos, ' ', word, sizeof(word), 0), "world") == 0, "Didn't get correct word.");
    fail_unless(string_get_word(cp, &pos, ' ', word, sizeof(word), 0) == NULL, "Didn't get correct word.");
    free(cp);

    cp = strdup("/teleport 'Player Name'");
    pos = 0;
    fail_unless(strcmp(string_get_word(cp, &pos, ' ', word, sizeof(word), 0), "/teleport") == 0, "Didn't get correct word.");
    fail_unless(strcmp(string_get_word(cp, &pos, ' ', word, sizeof(word), '\''), "Player Name") == 0, "Didn't get correct word.");
    fail_unless(string_get_word(cp, &pos, ' ', word, sizeof(word), 0) == NULL, "Didn't get correct word.");
    free(cp);

    cp = strdup("");
    pos = 0;
    fail_unless(string_get_word(cp, &pos, ' ', word, sizeof(word), 0) == NULL, "Didn't get correct word.");
    free(cp);
}
END_TEST

START_TEST(test_string_skip_word)
{
    char *cp;
    size_t pos;

    cp = strdup("hello world");
    pos = 0;
    string_skip_word(cp, &pos, 1);
    fail_unless(strcmp(cp + pos, " world") == 0, "Didn't skip word correctly.");
    string_skip_word(cp, &pos, 1);
    fail_unless(strcmp(cp + pos, "") == 0, "Didn't skip word correctly.");
    free(cp);

    cp = strdup("hello world");
    pos = strlen(cp);
    string_skip_word(cp, &pos, -1);
    fail_unless(strcmp(cp + pos, "world") == 0, "Didn't skip word correctly.");
    string_skip_word(cp, &pos, -1);
    fail_unless(strcmp(cp + pos, "hello world") == 0, "Didn't skip word correctly.");
    string_skip_word(cp, &pos, 1);
    fail_unless(strcmp(cp + pos, " world") == 0, "Didn't skip word correctly.");
    free(cp);
}
END_TEST

START_TEST(test_string_isdigit)
{
    fail_unless(string_isdigit("10") == 1, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("10000000000000") == 1, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("1234567890") == 1, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("0") == 1, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("1") == 1, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("x") == 0, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("hello world") == 0, "string_isdigit() didn't return correct value.");
    fail_unless(string_isdigit("hell0 w0rld") == 0, "string_isdigit() didn't return correct value.");
}
END_TEST

START_TEST(test_string_capitalize)
{
    char *cp;

    cp = strdup("hello world");
    string_capitalize(cp);
    fail_unless(strcmp(cp, "Hello world") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("Hello World");
    string_capitalize(cp);
    fail_unless(strcmp(cp, "Hello world") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("HELLO");
    string_capitalize(cp);
    fail_unless(strcmp(cp, "Hello") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("MiXeD CaSe");
    string_capitalize(cp);
    fail_unless(strcmp(cp, "Mixed case") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("");
    string_capitalize(cp);
    fail_unless(strcmp(cp, "") == 0, "Capitalized string doesn't match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_title)
{
    char *cp;

    cp = strdup("hello world");
    string_title(cp);
    fail_unless(strcmp(cp, "Hello World") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("Hello World");
    string_title(cp);
    fail_unless(strcmp(cp, "Hello World") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("HELLO");
    string_title(cp);
    fail_unless(strcmp(cp, "Hello") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("MiXeD CaSe");
    string_title(cp);
    fail_unless(strcmp(cp, "Mixed Case") == 0, "Capitalized string doesn't match expected output.");
    free(cp);

    cp = strdup("");
    string_title(cp);
    fail_unless(strcmp(cp, "") == 0, "Capitalized string doesn't match expected output.");
    free(cp);
}
END_TEST

START_TEST(test_string_startswith)
{
    fail_unless(string_startswith("hello world", "hello") == 1, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("", "") == 0, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("hello world", "") == 0, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("", "hello") == 0, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("hello world", "hi") == 0, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("hello world", "h") == 1, "string_startswith() didn't return correct value.");
    fail_unless(string_startswith("hello", "hello world") == 0, "string_startswith() didn't return correct value.");
}
END_TEST

START_TEST(test_string_endswith)
{
    fail_unless(string_endswith("hello world", "world") == 1, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("hello world", "d") == 1, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("", "") == 0, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("", "world") == 0, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("hello world", "") == 0, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("world", "hello world") == 0, "string_endswith() didn't return correct value.");
    fail_unless(string_endswith("hello world", "hello world") == 1, "string_endswith() didn't return correct value.");
}
END_TEST

START_TEST(test_string_sub)
{
    char *cp;

    cp = string_sub("hello world", 1, -1);
    fail_unless(strcmp(cp, "ello worl") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 0, 0);
    fail_unless(strcmp(cp, "hello world") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 1, 0);
    fail_unless(strcmp(cp, "ello world") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 0, -1);
    fail_unless(strcmp(cp, "hello worl") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", -1, -1);
    fail_unless(strcmp(cp, "l") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 4, 0);
    fail_unless(strcmp(cp, "o world") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", -5, 0);
    fail_unless(strcmp(cp, "world") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 20, 0);
    fail_unless(strcmp(cp, "") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", -20, -20);
    fail_unless(strcmp(cp, "") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 0, -20);
    fail_unless(strcmp(cp, "") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", -20, 0);
    fail_unless(strcmp(cp, "hello world") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", -500, 2);
    fail_unless(strcmp(cp, "he") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 0, -500);
    fail_unless(strcmp(cp, "") == 0, "string_sub() didn't return correct result.");
    free(cp);

    cp = string_sub("hello world", 5, -500);
    fail_unless(strcmp(cp, "") == 0, "string_sub() didn't return correct result.");
    free(cp);
}
END_TEST

START_TEST(test_string_isempty)
{
    fail_unless(string_isempty(NULL) == 1, "string_isempty() didn't return correct value.");
    fail_unless(string_isempty("") == 1, "string_isempty() didn't return correct value.");
    fail_unless(string_isempty("1") == 0, "string_isempty() didn't return correct value.");
    fail_unless(string_isempty("hello world") == 0, "string_isempty() didn't return correct value.");
    fail_unless(string_isempty(" ") == 0, "string_isempty() didn't return correct value.");
    fail_unless(string_isempty("   ") == 0, "string_isempty() didn't return correct value.");
}
END_TEST

START_TEST(test_string_iswhite)
{
    fail_unless(string_iswhite(NULL) == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("      ") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite(" ") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("\n\n  ") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("\n \n") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("\t\t\t") == 1, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("hello world") == 0, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("h i") == 0, "string_iswhite() didn't return correct value.");
    fail_unless(string_iswhite("\thi\t") == 0, "string_iswhite() didn't return correct value.");
}
END_TEST

START_TEST(test_char_contains)
{
    fail_unless(char_contains('q', "qwerty") == 1, "char_contains() didn't return correct value.");
    fail_unless(char_contains('\n', "hello\nworld") == 1, "char_contains() didn't return correct value.");
    fail_unless(char_contains('\n', "\t") == 0, "char_contains() didn't return correct value.");
    fail_unless(char_contains('Q', "qwerty") == 0, "char_contains() didn't return correct value.");
    fail_unless(char_contains('a', "qwerty") == 0, "char_contains() didn't return correct value.");
    fail_unless(char_contains('\t', "qwerty") == 0, "char_contains() didn't return correct value.");
    fail_unless(char_contains('\n', "qwerty") == 0, "char_contains() didn't return correct value.");
}
END_TEST

START_TEST(test_string_contains)
{
    fail_unless(string_contains("hello world", "qwerty") == 1, "string_contains() didn't return correct value.");
    fail_unless(string_contains("hello world", " ") == 1, "string_contains() didn't return correct value.");
    fail_unless(string_contains("hello world", "\t") == 0, "string_contains() didn't return correct value.");
}
END_TEST

START_TEST(test_string_contains_other)
{
    fail_unless(string_contains_other("Qwerty", "qwerty") == 1, "string_contains_other() didn't return correct value.");
    fail_unless(string_contains_other("qwerty", "qwerty") == 0, "string_contains_other() didn't return correct value.");
    fail_unless(string_contains_other("hello world", "qwerty") == 1, "string_contains_other() didn't return correct value.");
    fail_unless(string_contains_other("     \t\t\n", "\t\n ") == 0, "string_contains_other() didn't return correct value.");
}
END_TEST

START_TEST(test_string_create_char_range)
{
    char *cp;

    cp = string_create_char_range('a', 'z');
    fail_unless(strcmp(cp, "abcdefghijklmnopqrstuvwxyz") == 0, "string_create_char_range() didn't return correct result.");
    free(cp);

    cp = string_create_char_range('A', 'Z');
    fail_unless(strcmp(cp, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == 0, "string_create_char_range() didn't return correct result.");
    free(cp);

    cp = string_create_char_range('0', '9');
    fail_unless(strcmp(cp, "0123456789") == 0, "string_create_char_range() didn't return correct result.");
    free(cp);
}
END_TEST

START_TEST(test_string_join)
{
    char *cp;

    cp = string_join(NULL, "hello", "world", NULL);
    fail_unless(strcmp(cp, "helloworld") == 0, "string_join() didn't return correct result.");
    free(cp);

    cp = string_join(" ", "hello", "world", NULL);
    fail_unless(strcmp(cp, "hello world") == 0, "string_join() didn't return correct result.");
    free(cp);

    cp = string_join(", ", "hello", "world", NULL);
    fail_unless(strcmp(cp, "hello, world") == 0, "string_join() didn't return correct result.");
    free(cp);

    cp = string_join(NULL, "world", NULL);
    fail_unless(strcmp(cp, "world") == 0, "string_join() didn't return correct result.");
    free(cp);

    cp = string_join("\n", "hello", NULL);
    fail_unless(strcmp(cp, "hello") == 0, "string_join() didn't return correct result.");
    free(cp);

    cp = string_join("\n", "hello", "world", "hi", NULL);
    fail_unless(strcmp(cp, "hello\nworld\nhi") == 0, "string_join() didn't return correct result.");
    free(cp);
}
END_TEST

START_TEST(test_string_join_array)
{
    char *cp;
    char *cps[] = {"hello", "world"};
    char *cps2[] = {"hello"};
    char *cps3[] = {"hello", "world", "hi"};
    char *cps4[] = {"hello", NULL, "world"};

    cp = string_join_array(NULL, cps, arraysize(cps));
    fail_unless(strcmp(cp, "helloworld") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array(" ", cps, arraysize(cps));
    fail_unless(strcmp(cp, "hello world") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array(", ", cps, arraysize(cps));
    fail_unless(strcmp(cp, "hello, world") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array(NULL, cps2, arraysize(cps2));
    fail_unless(strcmp(cp, "hello") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array("\n", cps2, arraysize(cps2));
    fail_unless(strcmp(cp, "hello") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array("\n", cps3, arraysize(cps3));
    fail_unless(strcmp(cp, "hello\nworld\nhi") == 0, "string_join_array() didn't return correct result.");
    free(cp);

    cp = string_join_array("\n", cps4, arraysize(cps4));
    fail_unless(strcmp(cp, "hello\nworld") == 0, "string_join_array() didn't return correct result.");
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
    tcase_add_test(tc_core, test_string_newline_to_literal);
    tcase_add_test(tc_core, test_string_get_word);
    tcase_add_test(tc_core, test_string_skip_word);
    tcase_add_test(tc_core, test_string_isdigit);
    tcase_add_test(tc_core, test_string_capitalize);
    tcase_add_test(tc_core, test_string_title);
    tcase_add_test(tc_core, test_string_startswith);
    tcase_add_test(tc_core, test_string_endswith);
    tcase_add_test(tc_core, test_string_sub);
    tcase_add_test(tc_core, test_string_isempty);
    tcase_add_test(tc_core, test_string_iswhite);
    tcase_add_test(tc_core, test_char_contains);
    tcase_add_test(tc_core, test_string_contains);
    tcase_add_test(tc_core, test_string_contains_other);
    tcase_add_test(tc_core, test_string_create_char_range);
    tcase_add_test(tc_core, test_string_join);
    tcase_add_test(tc_core, test_string_join_array);

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

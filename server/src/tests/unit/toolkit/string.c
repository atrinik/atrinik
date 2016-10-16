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
#include <toolkit/string.h>

START_TEST(test_string_replace)
{
    char buf[MAX_BUF], buf2[6];

    /* Check simple replacement */
    string_replace("hello world", "world", "Earth", buf, sizeof(buf));
    ck_assert_str_eq(buf, "hello Earth");

    /* Try to replace spaces with nothing */
    string_replace("hello           world", " ", "", buf, sizeof(buf));
    ck_assert_str_eq(buf, "helloworld");

    /* Make sure nothing is replaced when replacing "hello" with "world" in an
     * empty string. */
    string_replace("", "hello", "world", buf, sizeof(buf));
    ck_assert_str_eq(buf, "");

    /* Make sure nothing is replaced when replacing "hello" with "world" in a
     * string that doesn't contain "hello". */
    string_replace("hi world", "hello", "world", buf, sizeof(buf));
    ck_assert_str_eq(buf, "hi world");

    /* Make sure that when both key and replacement are the same, the string
     * remains the same. */
    string_replace("hello world", "hello", "hello", buf, sizeof(buf));
    ck_assert_str_eq(buf, "hello world");

    /* Make sure that nothing changes when both key and replacement are an
     * empty string. */
    string_replace("hello world", "", "", buf, sizeof(buf));
    ck_assert_str_eq(buf, "hello world");

    /* Make sure buffer overflow doesn't happen when the buffer is not large
     * enough. */
    string_replace("hello world", "world", "Earth", buf2, sizeof(buf2));
    ck_assert_str_eq(buf2, "hello");

    /* Make sure buffer overflow doesn't happen when the buffer is not large
     * enough, and a replacement occurs prior to reaching the buffer limit. */
    string_replace("hello world", "hello", "", buf2, sizeof(buf2));
    ck_assert_str_eq(buf2, " worl");
}

END_TEST

START_TEST(test_string_replace_char)
{
    char *cp;

    /* Attempt to replace "a", "e" and "o" characters with spaces. */
    cp = estrdup("hello world hello");
    string_replace_char(cp, "aeo", ' ');
    ck_assert_str_eq(cp, "h ll  w rld h ll ");
    efree(cp);

    /* Attempt to replace any character with space. */
    cp = estrdup("hello world");
    string_replace_char(cp, NULL, ' ');
    ck_assert_str_eq(cp, "           ");
    efree(cp);

    /* Replace newlines and tabs with spaces. */
    cp = estrdup("\thello\n\t\tworld\n");
    string_replace_char(cp, "\n\t", ' ');
    ck_assert_str_eq(cp, " hello   world ");
    efree(cp);

    /* Replace forward-slashes with a dollar sign. */
    cp = estrdup("/shattered_islands/world_0112");
    string_replace_char(cp, "/", '$');
    ck_assert_str_eq(cp, "$shattered_islands$world_0112");
    efree(cp);
}

END_TEST

START_TEST(test_string_split)
{
    char *cp, *cps[20], *cps2[2];

    /* Attempt to split two words separated by spaces. */
    cp = estrdup("hello world");
    ck_assert_int_eq(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' '), 2);
    ck_assert_str_eq(cps[0], "hello");
    ck_assert_str_eq(cps[1], "world");
    ck_assert_ptr_eq(cps[2], NULL);
    efree(cp);

    /* Attempt to split several one-character words. */
    cp = estrdup("q w e r t y");
    ck_assert_int_eq(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' '), 6);
    ck_assert_str_eq(cps[0], "q");
    ck_assert_str_eq(cps[1], "w");
    ck_assert_str_eq(cps[2], "e");
    ck_assert_str_eq(cps[3], "r");
    ck_assert_str_eq(cps[4], "t");
    ck_assert_str_eq(cps[5], "y");
    efree(cp);

    /* Attempt to split empty string. */
    cp = estrdup("");
    ck_assert_int_eq(string_split(cp, cps, sizeof(cps) / sizeof(*cps), ' '), 0);
    ck_assert_ptr_eq(cps[0], NULL);
    ck_assert_ptr_eq(cps[1], NULL);
    efree(cp);

    /* Attempt to split several one-character words, and the result would not
     * fit into the array. */
    cp = estrdup("q w e r t y");
    ck_assert_int_eq(string_split(cp, cps2, sizeof(cps2) / sizeof(*cps2), ' '),
            2);
    ck_assert_str_eq(cps2[0], "q");
    ck_assert_str_eq(cps2[1], "w e r t y");
    efree(cp);
}

END_TEST

START_TEST(test_string_replace_unprintable_char)
{
    char *cp;

    /* Replace tabs with spaces. */
    cp = estrdup("\thello\tworld");
    string_replace_unprintable_chars(cp);
    ck_assert_str_eq(cp, " hello world");
    efree(cp);

    /* Replace empty string. */
    cp = estrdup("");
    string_replace_unprintable_chars(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);

    /* Replace string that consists of only unprintable characters. */
    cp = estrdup("\t\n\n\t\t\t\b\b");
    string_replace_unprintable_chars(cp);
    ck_assert_str_eq(cp, "        ");
    efree(cp);
}

END_TEST

START_TEST(test_string_format_number_comma)
{
    ck_assert_str_eq(string_format_number_comma(100), "100");
    ck_assert_str_eq(string_format_number_comma(1000), "1,000");
    ck_assert_str_eq(string_format_number_comma(123456789), "123,456,789");
    ck_assert_str_eq(string_format_number_comma(99999999999999999),
            "99,999,999,999,999,999");
    ck_assert_str_eq(string_format_number_comma(UINT64_MAX),
            "18,446,744,073,709,551,615");
}

END_TEST

START_TEST(test_string_toupper)
{
    char *cp;

    cp = estrdup("hello world");
    string_toupper(cp);
    ck_assert_str_eq(cp, "HELLO WORLD");
    efree(cp);

    cp = estrdup("Hello");
    string_toupper(cp);
    ck_assert_str_eq(cp, "HELLO");
    efree(cp);

    cp = estrdup("");
    string_toupper(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_string_tolower)
{
    char *cp;

    cp = estrdup("HELLO WORLD");
    string_tolower(cp);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = estrdup("hELLO");
    string_tolower(cp);
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    cp = estrdup("");
    string_tolower(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_string_whitespace_trim)
{
    char *cp;

    cp = estrdup("            ");
    string_whitespace_trim(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = estrdup("hello world        \t\t");
    string_whitespace_trim(cp);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = estrdup("           hello world");
    string_whitespace_trim(cp);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = estrdup("\t              hello world   \t   ");
    string_whitespace_trim(cp);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = estrdup("   hello world   ");
    string_whitespace_trim(cp);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);
}

END_TEST

START_TEST(test_string_whitespace_squeeze)
{
    char *cp;

    cp = estrdup(" hello world ");
    string_whitespace_squeeze(cp);
    ck_assert_str_eq(cp, " hello world ");
    efree(cp);

    cp = estrdup(" hello         world ");
    string_whitespace_squeeze(cp);
    ck_assert_str_eq(cp, " hello world ");
    efree(cp);

    cp = estrdup("      hello    world ");
    string_whitespace_squeeze(cp);
    ck_assert_str_eq(cp, " hello world ");
    efree(cp);

    cp = estrdup("hello  world     ");
    string_whitespace_squeeze(cp);
    ck_assert_str_eq(cp, "hello world ");
    efree(cp);
}

END_TEST

START_TEST(test_string_newline_to_literal)
{
    char *cp;

    cp = estrdup("hello\\nworld");
    string_newline_to_literal(cp);
    ck_assert_str_eq(cp, "hello\nworld");
    efree(cp);

    cp = estrdup("\\n\\n\\n");
    string_newline_to_literal(cp);
    ck_assert_str_eq(cp, "\n\n\n");
    efree(cp);

    cp = estrdup("");
    string_newline_to_literal(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_string_get_word)
{
    char *cp, word[MAX_BUF];
    size_t pos;

    cp = estrdup("hello world");
    pos = 0;
    ck_assert_str_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            "hello");
    ck_assert_str_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            "world");
    ck_assert_ptr_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            NULL);
    efree(cp);

    cp = estrdup("/teleport 'Player Name'");
    pos = 0;
    ck_assert_str_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            "/teleport");
    ck_assert_str_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), '\''),
            "Player Name");
    ck_assert_ptr_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            NULL);
    efree(cp);

    cp = estrdup("");
    pos = 0;
    ck_assert_ptr_eq(string_get_word(cp, &pos, ' ', word, sizeof(word), 0),
            NULL);
    efree(cp);
}

END_TEST

START_TEST(test_string_skip_word)
{
    char *cp;
    size_t pos;

    cp = estrdup("hello world");
    pos = 0;
    string_skip_word(cp, &pos, 1);
    ck_assert_str_eq(cp + pos, " world");
    string_skip_word(cp, &pos, 1);
    ck_assert_str_eq(cp + pos, "");
    efree(cp);

    cp = estrdup("hello world");
    pos = strlen(cp);
    string_skip_word(cp, &pos, -1);
    ck_assert_str_eq(cp + pos, "world");
    string_skip_word(cp, &pos, -1);
    ck_assert_str_eq(cp + pos, "hello world");
    string_skip_word(cp, &pos, 1);
    ck_assert_str_eq(cp + pos, " world");
    efree(cp);
}

END_TEST

START_TEST(test_string_isdigit)
{
    ck_assert(string_isdigit("10"));
    ck_assert(string_isdigit("10000000000000"));
    ck_assert(string_isdigit("1234567890"));
    ck_assert(string_isdigit("0"));
    ck_assert(string_isdigit("1"));
    ck_assert(!string_isdigit("x"));
    ck_assert(!string_isdigit("hello world"));
    ck_assert(!string_isdigit("hell0 w0rld"));
}

END_TEST

START_TEST(test_string_capitalize)
{
    char *cp;

    cp = estrdup("hello world");
    string_capitalize(cp);
    ck_assert_str_eq(cp, "Hello world");
    efree(cp);

    cp = estrdup("Hello World");
    string_capitalize(cp);
    ck_assert_str_eq(cp, "Hello world");
    efree(cp);

    cp = estrdup("HELLO");
    string_capitalize(cp);
    ck_assert_str_eq(cp, "Hello");
    efree(cp);

    cp = estrdup("MiXeD CaSe");
    string_capitalize(cp);
    ck_assert_str_eq(cp, "Mixed case");
    efree(cp);

    cp = estrdup("");
    string_capitalize(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_string_title)
{
    char *cp;

    cp = estrdup("hello world");
    string_title(cp);
    ck_assert_str_eq(cp, "Hello World");
    efree(cp);

    cp = estrdup("Hello World");
    string_title(cp);
    ck_assert_str_eq(cp, "Hello World");
    efree(cp);

    cp = estrdup("HELLO");
    string_title(cp);
    ck_assert_str_eq(cp, "Hello");
    efree(cp);

    cp = estrdup(" Hello ");
    string_title(cp);
    ck_assert_str_eq(cp, " Hello ");
    efree(cp);

    cp = estrdup("Hello ");
    string_title(cp);
    ck_assert_str_eq(cp, "Hello ");
    efree(cp);

    cp = estrdup("hello ");
    string_title(cp);
    ck_assert_str_eq(cp, "Hello ");
    efree(cp);

    cp = estrdup("MiXeD CaSe");
    string_title(cp);
    ck_assert_str_eq(cp, "Mixed Case");
    efree(cp);

    cp = estrdup("");
    string_title(cp);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = estrdup(" ");
    string_title(cp);
    ck_assert_str_eq(cp, " ");
    efree(cp);

    cp = estrdup(" a");
    string_title(cp);
    ck_assert_str_eq(cp, " A");
    efree(cp);

    cp = estrdup("fOrMaT thIs aS titLe String");
    string_title(cp);
    ck_assert_str_eq(cp, "Format This As Title String");
    efree(cp);

    cp = estrdup("fOrMaT,thIs-aS*titLe;String");
    string_title(cp);
    ck_assert_str_eq(cp, "Format,This-As*Title;String");
    efree(cp);
}

END_TEST

START_TEST(test_string_startswith)
{
    ck_assert(string_startswith("hello world", "hello"));
    ck_assert(!string_startswith("", ""));
    ck_assert(!string_startswith("hello world", ""));
    ck_assert(!string_startswith("", "hello"));
    ck_assert(!string_startswith("hello world", "hi"));
    ck_assert(string_startswith("hello world", "h"));
    ck_assert(!string_startswith("hello", "hello world"));
}

END_TEST

START_TEST(test_string_endswith)
{
    ck_assert(string_endswith("hello world", "world"));
    ck_assert(string_endswith("hello world", "d"));
    ck_assert(!string_endswith("", ""));
    ck_assert(!string_endswith("", "world"));
    ck_assert(!string_endswith("hello world", ""));
    ck_assert(!string_endswith("world", "hello world"));
    ck_assert(string_endswith("hello world", "hello world"));
}

END_TEST

START_TEST(test_string_sub)
{
    char *cp;

    cp = string_sub("hello world", 1, -1);
    ck_assert_str_eq(cp, "ello worl");
    efree(cp);

    cp = string_sub("hello world", 0, 0);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = string_sub("hello world", 1, 0);
    ck_assert_str_eq(cp, "ello world");
    efree(cp);

    cp = string_sub("hello world", 0, -1);
    ck_assert_str_eq(cp, "hello worl");
    efree(cp);

    cp = string_sub("hello world", -1, -1);
    ck_assert_str_eq(cp, "l");
    efree(cp);

    cp = string_sub("hello world", 4, 0);
    ck_assert_str_eq(cp, "o world");
    efree(cp);

    cp = string_sub("hello world", -5, 0);
    ck_assert_str_eq(cp, "world");
    efree(cp);

    cp = string_sub("hello world", 20, 0);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = string_sub("hello world", -20, -20);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = string_sub("hello world", 0, -20);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = string_sub("hello world", -20, 0);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = string_sub("hello world", -500, 2);
    ck_assert_str_eq(cp, "he");
    efree(cp);

    cp = string_sub("hello world", 0, -500);
    ck_assert_str_eq(cp, "");
    efree(cp);

    cp = string_sub("hello world", 5, -500);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_string_isempty)
{
    ck_assert(string_isempty(NULL));
    ck_assert(string_isempty(""));
    ck_assert(!string_isempty("1"));
    ck_assert(!string_isempty("hello world"));
    ck_assert(!string_isempty(" "));
    ck_assert(!string_isempty("   "));
}

END_TEST

START_TEST(test_string_iswhite)
{
    ck_assert(string_iswhite(NULL));
    ck_assert(string_iswhite("      "));
    ck_assert(string_iswhite(" "));
    ck_assert(string_iswhite(""));
    ck_assert(string_iswhite("\n\n  "));
    ck_assert(string_iswhite("\n \n"));
    ck_assert(string_iswhite("\t\t\t"));
    ck_assert(!string_iswhite("hello world"));
    ck_assert(!string_iswhite("h i"));
    ck_assert(!string_iswhite("\thi\t"));
}

END_TEST

START_TEST(test_char_contains)
{
    ck_assert(char_contains('q', "qwerty"));
    ck_assert(char_contains('\n', "hello\nworld"));
    ck_assert(!char_contains('\n', "\t"));
    ck_assert(!char_contains('Q', "qwerty"));
    ck_assert(!char_contains('a', "qwerty"));
    ck_assert(!char_contains('\t', "qwerty"));
    ck_assert(!char_contains('\n', "qwerty"));
}

END_TEST

START_TEST(test_string_contains)
{
    ck_assert(string_contains("hello world", "qwerty"));
    ck_assert(string_contains("hello world", " "));
    ck_assert(!string_contains("hello world", "\t"));
}

END_TEST

START_TEST(test_string_contains_other)
{
    ck_assert(string_contains_other("Qwerty", "qwerty"));
    ck_assert(!string_contains_other("qwerty", "qwerty"));
    ck_assert(string_contains_other("hello world", "qwerty"));
    ck_assert(!string_contains_other("     \t\t\n", "\t\n "));
}

END_TEST

START_TEST(test_string_create_char_range)
{
    char *cp;

    cp = string_create_char_range('a', 'z');
    ck_assert_str_eq(cp, "abcdefghijklmnopqrstuvwxyz");
    efree(cp);

    cp = string_create_char_range('A', 'Z');
    ck_assert_str_eq(cp, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    efree(cp);

    cp = string_create_char_range('0', '9');
    ck_assert_str_eq(cp, "0123456789");
    efree(cp);
}

END_TEST

START_TEST(test_string_join)
{
    char *cp;

    cp = string_join(NULL, "hello", "world", NULL);
    ck_assert_str_eq(cp, "helloworld");
    efree(cp);

    cp = string_join(" ", "hello", "world", NULL);
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = string_join(", ", "hello", "world", NULL);
    ck_assert_str_eq(cp, "hello, world");
    efree(cp);

    cp = string_join(NULL, "world", NULL);
    ck_assert_str_eq(cp, "world");
    efree(cp);

    cp = string_join("\n", "hello", NULL);
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    cp = string_join("\n", "hello", "world", "hi", NULL);
    ck_assert_str_eq(cp, "hello\nworld\nhi");
    efree(cp);
}

END_TEST

START_TEST(test_string_join_array)
{
    char *cp;
    const char *cps[] = {"hello", "world"};
    const char *cps2[] = {"hello"};
    const char *cps3[] = {"hello", "world", "hi"};
    const char *cps4[] = {"hello", NULL, "world"};

    cp = string_join_array(NULL, cps, arraysize(cps));
    ck_assert_str_eq(cp, "helloworld");
    efree(cp);

    cp = string_join_array(" ", cps, arraysize(cps));
    ck_assert_str_eq(cp, "hello world");
    efree(cp);

    cp = string_join_array(", ", cps, arraysize(cps));
    ck_assert_str_eq(cp, "hello, world");
    efree(cp);

    cp = string_join_array(NULL, cps2, arraysize(cps2));
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    cp = string_join_array("\n", cps2, arraysize(cps2));
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    cp = string_join_array("\n", cps3, arraysize(cps3));
    ck_assert_str_eq(cp, "hello\nworld\nhi");
    efree(cp);

    cp = string_join_array("\n", cps4, arraysize(cps4));
    ck_assert_str_eq(cp, "hello\nworld");
    efree(cp);
}

END_TEST

START_TEST(test_string_repeat)
{
    char *cp;

    cp = string_repeat("hello", 5);
    ck_assert_str_eq(cp, "hellohellohellohellohello");
    efree(cp);

    cp = string_repeat("hello", 1);
    ck_assert_str_eq(cp, "hello");
    efree(cp);

    cp = string_repeat("hello", 0);
    ck_assert_str_eq(cp, "");
    efree(cp);
}

END_TEST

START_TEST(test_snprintfcat)
{
    char buf[MAX_BUF], buf2[8];

    snprintf(buf, sizeof(buf), "%s", "hello");
    ck_assert_int_eq(snprintfcat(buf, sizeof(buf), " %s", "world"), 11);
    ck_assert_str_eq(buf, "hello world");

    snprintf(buf, sizeof(buf), "%s %d", "hello", 10);
    ck_assert_int_eq(snprintfcat(buf, sizeof(buf), " %s", "world"), 14);
    ck_assert_str_eq(buf, "hello 10 world");

    snprintf(buf, sizeof(buf), "%s", "hello");
    ck_assert_int_eq(snprintfcat(buf, sizeof(buf), "%s", ""), 5);
    ck_assert_str_eq(buf, "hello");

    snprintf(buf2, sizeof(buf2), "%s", "hello");
    ck_assert_int_eq(snprintfcat(buf2, sizeof(buf2), " %s", "world"), 11);
    ck_assert_str_eq(buf2, "hello w");

    snprintf(buf2, sizeof(buf2), "%s", "testing");
    ck_assert_int_eq(snprintfcat(buf2, sizeof(buf2), "%s", "world"), 12);
    ck_assert_str_eq(buf2, "testing");

    snprintf(buf2, sizeof(buf2), "%s", "testing");
    ck_assert_int_eq(snprintfcat(buf2, sizeof(buf2), " %s", "world"), 13);
    ck_assert_str_eq(buf2, "testing");
}

END_TEST

START_TEST(test_string_tohex)
{
    char buf[MAX_BUF], buf2[5], buf3[6], buf4[7];
    unsigned char cp[] = {0xff, 0x00, 0x03}, cp2[1] = {0x00}, cp3[] = {0x03};

    ck_assert_uint_eq(string_tohex((const unsigned char *) "hello world",
            strlen("hello world"), buf, sizeof(buf), false),
            strlen("68656C6C6F20776F726C64"));
    ck_assert_str_eq(buf, "68656C6C6F20776F726C64");

    ck_assert_uint_eq(string_tohex(cp, arraysize(cp), buf, sizeof(buf), false),
            6);
    ck_assert_str_eq(buf, "FF0003");

    ck_assert_uint_eq(string_tohex(cp, arraysize(cp), buf, sizeof(buf), true),
            8);
    ck_assert_str_eq(buf, "FF:00:03");

    ck_assert_uint_eq(string_tohex(cp2, 0, buf, sizeof(buf), false), 0);
    ck_assert_str_eq(buf, "");

    ck_assert_uint_eq(string_tohex(cp2, 0, buf, sizeof(buf), true), 0);
    ck_assert_str_eq(buf, "");

    ck_assert_uint_eq(string_tohex(cp3, arraysize(cp3), buf, sizeof(buf), true),
            2);
    ck_assert_str_eq(buf, "03");

    /* Test buffer overflows. */
    ck_assert_int_eq(string_tohex(cp, arraysize(cp), buf2, sizeof(buf2), false),
            4);
    ck_assert_str_eq(buf2, "FF00");

    ck_assert_uint_eq(string_tohex(cp, arraysize(cp), buf3, sizeof(buf3),
            false), 4);
    ck_assert_str_eq(buf3, "FF00");

    ck_assert_uint_eq(string_tohex(cp, arraysize(cp), buf4, sizeof(buf4),
            false), 6);
    ck_assert_str_eq(buf4, "FF0003");
}

END_TEST

START_TEST(test_string_fromhex)
{
    unsigned char buf[MAX_BUF], buf2[2];

    ck_assert_uint_eq(string_fromhex("FF03", strlen("FF03"), buf,
            arraysize(buf)), 2);
    ck_assert(buf[0] == 0xFF && buf[1] == 0x03);

    ck_assert_uint_eq(string_fromhex("FF       03", strlen("FF       03"), buf,
            arraysize(buf)), 2);
    ck_assert(buf[0] == 0xFF && buf[1] == 0x03);

    ck_assert_uint_eq(string_fromhex("FF3", strlen("FF3"), buf,
            arraysize(buf)), 1);
    ck_assert(buf[0] == 0xFF);

    ck_assert_uint_eq(string_fromhex("FF0304", strlen("FF0304"), buf2,
            arraysize(buf2)), 2);
    ck_assert(buf[0] == 0xFF && buf[1] == 0x03);
}

END_TEST

START_TEST(test_string_skip_whitespace)
{
    const char *str = "";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "");

    str = "      ";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "");

    str = "hello world";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "hello world");

    str = "    hello world";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "hello world");

    str = " hello world ";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "hello world ");

    str = "";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "");

    str = "\t\thello world";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "hello world");

    str = "\t\nhello world";
    string_skip_whitespace(str);
    ck_assert_str_eq(str, "hello world");
}

END_TEST

START_TEST(test_string_last)
{
    ck_assert_ptr_eq(string_last("hello", ""), NULL);
    ck_assert_ptr_eq(string_last("a", "hello"), NULL);
    ck_assert_str_eq(string_last("hello", "hello"), "hello");
    ck_assert_str_eq(string_last("hello world", "o"), "orld");
    ck_assert_str_eq(string_last("hello world", "e"), "ello world");

    const char *str = "aaaaa";
    ck_assert_str_eq(string_last(str, "aaa"), "aaa");
    ck_assert_ptr_eq(string_last(str, "aaa"), str + 2);
    ck_assert_str_eq(string_last(str, "a"), "a");
    ck_assert_ptr_eq(string_last(str, "a"), str + 4);
}
END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("string");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);

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
    tcase_add_test(tc_core, test_string_repeat);
    tcase_add_test(tc_core, test_snprintfcat);
    tcase_add_test(tc_core, test_string_tohex);
    tcase_add_test(tc_core, test_string_fromhex);
    tcase_add_test(tc_core, test_string_skip_whitespace);
    tcase_add_test(tc_core, test_string_last);

    return s;
}

void check_server_string(void)
{
    check_run_suite(suite(), __FILE__);
}

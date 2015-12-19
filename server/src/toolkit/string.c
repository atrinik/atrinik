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

/**
 * @file
 * String API.
 */

#include <global.h>
#include <stdarg.h>
#include <toolkit_string.h>

TOOLKIT_API(IMPORTS(math), IMPORTS(stringbuffer), IMPORTS(memory));

#ifndef __CPROTO__

TOOLKIT_INIT_FUNC(string)
{
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(string)
{
}
TOOLKIT_DEINIT_FUNC_FINISH

#undef string_sub
#undef string_create_char_range
#undef string_repeat

/**
 * Like strdup(), but performs error checking.
 * @param s
 * String to duplicate.
 * @return
 * Duplicated string, never NULL.
 * @note abort() is called in case 's' is NULL or strdup() fails to duplicate
 * the string (oom).
 */
char *string_estrdup(const char *s MEMORY_DEBUG_PROTO)
{
    char *cp;

    if (s == NULL) {
        LOG(ERROR, "'s' is NULL.");
        abort();
    }

#ifndef NDEBUG
    size_t len;

    len = strlen(s);
    cp = memory_emalloc(sizeof(*cp) * (len + 1), file, line);
#else
    cp = strdup(s);
#endif

    if (cp == NULL) {
        LOG(ERROR, "OOM.");
        abort();
    }

#ifndef NDEBUG
    memcpy(cp, s, sizeof(*cp) * len);
    cp[len] = '\0';
#endif

    return cp;
}

/**
 * Like strndup(), but performs error checking.
 * @param s
 * String to duplicate.
 * @param n
 * At most this many bytes to copy.
 * @return
 * Duplicated string, never NULL.
 * @note abort() is called in case 's' is NULL or strndup() fails to duplicate
 * the string (oom).
 */
char *string_estrndup(const char *s, size_t n MEMORY_DEBUG_PROTO)
{
    char *cp;

    if (s == NULL) {
        LOG(ERROR, "'s' is NULL.");
        abort();
    }

#ifndef NDEBUG
    size_t len;

    len = strnlen(s, n);
    cp = memory_emalloc(sizeof(*cp) * (len + 1), file, line);
#else
    cp = strndup(s, n);
#endif

    if (cp == NULL) {
        LOG(ERROR, "OOM.");
        abort();
    }

#ifndef NDEBUG
    memcpy(cp, s, sizeof(*cp) * len);
    cp[len] = '\0';
#endif

    return cp;
}

/**
 * Replace in string src all occurrences of key by replacement. The resulting
 * string is put into result; at most resultsize characters (including the
 * terminating null character) will be written to result.
 */
void string_replace(const char *src, const char *key, const char *replacement,
        char *result, size_t resultsize)
{
    size_t resultlen, keylen;

    TOOLKIT_PROTECT();

    /* Special case to prevent infinite loop if key == replacement == "" */
    if (strcmp(key, replacement) == 0) {
        snprintf(result, resultsize, "%s", src);
        return;
    }

    keylen = strlen(key);
    resultlen = 0;

    while (*src != '\0' && resultlen + 1 < resultsize) {
        if (strncmp(src, key, keylen) == 0) {
            snprintf(result + resultlen, resultsize - resultlen, "%s",
                    replacement);
            resultlen += strlen(result + resultlen);
            src += keylen;
        } else {
            result[resultlen++] = *src++;
        }
    }

    result[resultlen] = '\0';
}

/**
 * Perform in-place replacement of all characters in 'key'.
 * @param str
 * String to modify.
 * @param key
 * Characters to replace, eg, " \t" to match all spaces and
 * tabs. NULL to match any character.
 * @param replacement
 * What to replace matched characters with.
 */
void string_replace_char(char *str, const char *key, const char replacement)
{
    size_t i;

    TOOLKIT_PROTECT();

    while (*str != '\0') {
        if (key) {
            for (i = 0; key[i] != '\0'; i++) {
                if (key[i] == *str) {
                    *str = replacement;
                    break;
                }
            }
        } else {
            *str = replacement;
        }

        str++;
    }
}

/**
 * Splits a string delimited by passed in sep value into characters into an
 * array of strings.
 * @param str
 * The string to be split; will be modified.
 * @param array
 * The string array; will be filled with pointers into str.
 * @param array_size
 * The number of elements in array; if <code>str</code>
 * contains more fields
 * excess fields are not split but included into the last element.
 * @param sep
 * Separator to use.
 * @return
 * The number of elements found; always less or equal to
 * <code>array_size</code>.
 */
size_t string_split(char *str, char *array[], size_t array_size, char sep)
{
    char *p;
    size_t pos;

    TOOLKIT_PROTECT();

    for (pos = 0; pos < array_size; pos++) {
        array[pos] = NULL;
    }

    if (!str || *str == '\0' || array_size <= 0) {
        return 0;
    }

    pos = 0;
    p = str;

    while (pos < array_size) {
        array[pos++] = p;

        while (*p != '\0' && *p != sep) {
            p++;
        }

        if (pos >= array_size) {
            break;
        }

        if (*p != sep) {
            break;
        }

        *p++ = '\0';
    }

    return pos;
}

/**
 * Replaces any unprintable character in the given buffer with a space.
 * @param buf
 * The buffer to modify.
 */
void string_replace_unprintable_chars(char *buf)
{
    char *p;

    TOOLKIT_PROTECT();

    for (p = buf; *p != '\0'; p++) {
        if (*p < ' ' || *p > '~') {
            *p = ' ';
        }
    }
}

/**
 * Adds thousand separators to a given number.
 * @param num
 * Number.
 * @return
 * Thousands-separated string.
 */
char *string_format_number_comma(uint64_t num)
{
    static char retbuf[4 * (sizeof(uint64_t) * CHAR_BIT + 2) / 3 / 3 + 1];
    char *buf;
    int i = 0;

    TOOLKIT_PROTECT();

    buf = &retbuf[sizeof(retbuf) - 1];
    *buf = '\0';

    do {
        if (i % 3 == 0 && i != 0) {
            *--buf = ',';
        }

        *--buf = '0' + num % 10;
        num /= 10;
        i++;
    }    while (num != 0);

    return buf;
}

/**
 * Transforms a string to uppercase, in-place.
 * @param str
 * String to transform, will be modified.
 */
void string_toupper(char *str)
{
    TOOLKIT_PROTECT();

    while (*str != '\0') {
        *str = toupper(*str);
        str++;
    }
}

/**
 * Transforms a string to lowercase, in-place.
 * @param str
 * String to transform, will be modified.
 */
void string_tolower(char *str)
{
    TOOLKIT_PROTECT();

    while (*str != '\0') {
        *str = tolower(*str);
        str++;
    }
}

/**
 * Trim left and right whitespace in string.
 *
 * @note Does in-place modification.
 * @param str
 * String to trim.
 * @return
 * 'str'.
 */
char *string_whitespace_trim(char *str)
{
    char *cp;
    size_t len;

    TOOLKIT_PROTECT();

    cp = str;
    len = strlen(cp);

    while (isspace(cp[len - 1])) {
        cp[--len] = '\0';
    }

    while (isspace(*cp)) {
        cp++;
        len--;
    }

    memmove(str, cp, len + 1);

    return str;
}

/**
 * Remove extraneous whitespace in a string.
 *
 * @note Does in-place modification.
 * @param str
 * The string.
 * @return
 * 'str'.
 */
char *string_whitespace_squeeze(char *str)
{
    size_t r, w;

    TOOLKIT_PROTECT();

    for (r = 0, w = 0; str[r] != '\0'; r++) {
        if (isspace(str[r])) {
            if (!w || !isspace(str[w - 1])) {
                str[w++] = ' ';
            }
        } else {
            str[w++] = str[r];
        }
    }

    str[w] = '\0';

    return str;
}

/**
 * Replaces "\n" by a newline char.
 *
 * Since we are replacing 2 chars by 1, no overflow should happen.
 * @param str
 * Text to replace into.
 */
void string_newline_to_literal(char *str)
{
    char *next;

    TOOLKIT_PROTECT();

    while ((next = strstr(str, "\\n"))) {
        *next = '\n';
        memmove(next + 1, next + 2, strlen(next) - 1);
    }
}

/**
 * Returns a single word from a string, free from left and right
 * whitespace.
 *
 * Effectively allows looping through all the words in a string.
 * @param str
 * The string.
 * @param[out] pos Position in string.
 * @param delim
 * Delimeter character.
 * @param word
 * Where to store the word.
 * @param wordsize
 * Size of 'word'.
 * @param surround
 * Character that can surround a word, regardless of 'delim'.
 * @return
 * 'word', NULL if 'word' is empty.
 */
const char *string_get_word(const char *str, size_t *pos, char delim,
        char *word, size_t wordsize, int surround)
{
    size_t i;
    uint8_t in_surround;

    TOOLKIT_PROTECT();

    i = 0;
    in_surround = 0;
    str += (*pos);

    while (str && *str != '\0' && *str == delim) {
        str++;
        (*pos)++;
    }

    while (str && *str != '\0' && (*str != delim || in_surround)) {
        if (*str == surround) {
            in_surround = !in_surround;
        } else if (i < wordsize - 1) {
            word[i++] = *str;
        }

        str++;
        (*pos)++;
    }

    while (str && *str != '\0' && *str == delim) {
        str++;
        (*pos)++;
    }

    word[i] = '\0';

    return *word == '\0' ? NULL : word;
}

/**
 * Skips whitespace and the first word in the string.
 * @param str
 * String.
 * @param[out] i Position to adjust.
 * @param dir
 * If 1, skip to the right, if -1, skip to the left.
 */
void string_skip_word(const char *str, size_t *i, int dir)
{
    uint8_t whitespace;

    TOOLKIT_PROTECT();

    whitespace = 1;

    /* Skip whitespace. */
    while (((dir == -1 && *i != 0) || (dir == 1 && str[*i] != '\0'))) {
        if (isspace(str[*i + MIN(0, dir)])) {
            if (!whitespace) {
                break;
            }
        } else if (whitespace) {
            whitespace = 0;
        }

        *i += dir;
    }
}

/**
 * Checks if string is a digit.
 * @return
 * 1 if the string is a digit, 0 otherwise.
 */
int string_isdigit(const char *str)
{
    TOOLKIT_PROTECT();

    if (*str == '-') {
        str++;
    }

    while (*str != '\0') {
        if (!isdigit(*str)) {
            return 0;
        }

        str++;
    }

    return 1;
}

/**
 * Capitalize a string, transforming the starting letter (if any) into
 * its uppercase version, and all following letters into their lowercase
 * versions.
 * @param str
 * String to capitalize.
 */
void string_capitalize(char *str)
{
    TOOLKIT_PROTECT();

    if (!str || *str == '\0') {
        return;
    }

    *str = toupper(*str);
    str++;

    while (*str != '\0') {
        *str = tolower(*str);
        str++;
    }
}

/**
 * Titlecase a string.
 * @param str
 * String to titlecase.
 */
void string_title(char *str)
{
    uint8_t previous_cased;

    TOOLKIT_PROTECT();

    if (!str) {
        return;
    }

    previous_cased = 0;

    while (*str != '\0') {
        if (islower(*str)) {
            if (!previous_cased) {
                *str = toupper(*str);
            }

            previous_cased = 1;
        } else if (isupper(*str)) {
            if (previous_cased) {
                *str = tolower(*str);
            }

            previous_cased = 1;
        } else {
            previous_cased = 0;
        }

        str++;
    }
}

/**
 * Check whether the specified string starts with another string.
 * @param str
 * String to check.
 * @param cmp
 * What to check for.
 * @return
 * 1 if 'str' starts with 'cmp', 0 otherwise.
 */
int string_startswith(const char *str, const char *cmp)
{
    TOOLKIT_PROTECT();

    if (string_isempty(str) || string_isempty(cmp)) {
        return 0;
    }

    if (strncmp(str, cmp, strlen(cmp)) == 0) {
        return 1;
    }

    return 0;
}

/**
 * Check whether the specified string ends with another string.
 * @param str
 * String to check.
 * @param cmp
 * What to check for.
 * @return
 * 1 if 'str' ends with 'cmp', 0 otherwise.
 */
int string_endswith(const char *str, const char *cmp)
{
    ssize_t len;

    TOOLKIT_PROTECT();

    if (string_isempty(str) || string_isempty(cmp)) {
        return 0;
    }

    len = strlen(str) - strlen(cmp);
    str += MAX(0, len);

    if (strcmp(str, cmp) == 0) {
        return 1;
    }

    return 0;
}

/**
 * Construct a substring from a string.
 *
 * 'start' and 'end' can be negative.
 *
 * If 'start' is, eg, -10, the starting index will automatically become
 * (strlen(str) - 10), in other words, 10 characters from the right, and the
 * ending index will become strlen(str), so you can use it to get the last 10
 * characters of a string, for example.
 *
 * If 'end' is, eg, -1, the ending index will automatically become (strlen(str)
 * - 1), in other words, one less character from the right. In this case,
 * 'start' is unmodified.
 *
 * Example:
 * @code
 * string_sub("hello world", 1, -1); --> "ello worl"
 * string_sub("hello world", 4, 0); --> "o world"
 * string_sub("hello world", -5, 0); --> "world"
 * @endcode
 * @param str
 * String to get a substring from.
 * @param start
 * Starting index, eg, 0 for the beginning.
 * @param end
 * Ending index, eg, strlen(end) for the end.
 * @return
 * The created substring; never NULL. Must be freed.
 */
char *string_sub(const char *str, ssize_t start, ssize_t end MEMORY_DEBUG_PROTO)
{
    size_t n, str_len;

    TOOLKIT_PROTECT();

    str_len = strlen(str);

    if (end <= 0) {
        end = str_len + end;

        if (end < 0) {
            end = 0;
        }
    }

    if (start < 0) {
        start = end + start;

        if (start < 0) {
            start = 0;
        }
    }

    if (!(str + start) || end - start < 0) {
        return string_estrdup("" MEMORY_DEBUG_PARAM);
    }

    str += start;
    n = MIN(str_len, (size_t) (end - start));

    return string_estrndup(str, n MEMORY_DEBUG_PARAM);
}

/**
 * Convenience function to check whether a string is empty.
 * @param str
 * String to check.
 * @return
 * 1 if 'str' is either NULL or if it begins with the NUL
 * character.
 */
int string_isempty(const char *str)
{
    TOOLKIT_PROTECT();

    return !str || *str == '\0';
}

/**
 * Checks whether the specified string consists of only whitespace
 * characters.
 * @param str
 * String to check.
 * @return
 * 1 if 'str' consists of purely whitespace character, 0 otherwise.
 */
int string_iswhite(const char *str)
{
    TOOLKIT_PROTECT();

    while (str && *str != '\0') {
        if (!isspace(*str)) {
            return 0;
        }

        str++;
    }

    return 1;
}

/**
 * Check if the specified character equals to any of the characters in
 * 'key'.
 * @param c
 * Character to check.
 * @param key
 * Characters to look for.
 * @return
 * 1 if 'c' equals to any of the character in 'key', 0 otherwise.
 */
int char_contains(const char c, const char *key)
{
    size_t i;

    TOOLKIT_PROTECT();

    for (i = 0; key[i] != '\0'; i++) {
        if (c == key[i]) {
            return 1;
        }
    }

    return 0;
}

/**
 * Check whether the specified string contains any of the characters in 'key'.
 * @param str
 * String to check.
 * @param key
 * Characters to look for.
 * @return
 * 1 if 'str' contains any of the characters in 'key', 0 otherwise.
 */
int string_contains(const char *str, const char *key)
{
    TOOLKIT_PROTECT();

    while (*str != '\0') {
        if (char_contains(*str, key)) {
            return 1;
        }

        str++;
    }

    return 0;
}

/**
 * Check whether the specified string contains any characters other than
 * those specified in 'key'.
 * @param str
 * String to check.
 * @param key
 * Characters to look for.
 * @return
 * 1 if 'str' contains a character that is not in 'key', 0 otherwise.
 */
int string_contains_other(const char *str, const char *key)
{
    TOOLKIT_PROTECT();

    while (*str != '\0') {
        if (!char_contains(*str, key)) {
            return 1;
        }

        str++;
    }

    return 0;
}

/**
 * Create a string containing characters in the specified character range.
 *
 * Example:
 * @code
 * string_create_char_range('a', 'd'); --> "abcd"
 * @endcode
 * @param start
 * Character index start.
 * @param end
 * Character index end.
 * @return
 * The generated string; never NULL. Must be freed.
 */
char *string_create_char_range(char start, char end MEMORY_DEBUG_PROTO)
{
    char *str, c;

    TOOLKIT_PROTECT();

    str = memory_emalloc((end - start + 1) + 1 MEMORY_DEBUG_PARAM);

    for (c = start; c <= end; c++) {
        str[c - start] = c;
    }

    str[c - start] = '\0';

    return str;
}

/**
 * Join all the provided strings into one.
 *
 * Example:
 * @code
 * string_join(", ", "hello", "world", NULL); --> "hello, world"
 * @endcode
 * @param delim
 * Delimeter to use, eg, ", ". Can be NULL.
 * @param ...
 * Strings to join. Must have a terminating NULL entry.
 * @return
 * Joined string; never NULL. Must be freed.
 */
char *string_join(const char *delim, ...)
{
    StringBuffer *sb;
    va_list args;
    const char *str;

    TOOLKIT_PROTECT();

    sb = stringbuffer_new();

    va_start(args, delim);

    while ((str = va_arg(args, const char *))) {
        if (stringbuffer_length(sb) != 0 && delim) {
            stringbuffer_append_string(sb, delim);
        }

        stringbuffer_append_string(sb, str);
    }

    va_end(args);

    return stringbuffer_finish(sb);
}

/**
 * Similar to string_join(), but for an array of string pointers.
 *
 * Example:
 * @code
 * char **strs;
 * size_t strs_num;
 *
 * strs_num = 2;
 * strs = emalloc(sizeof(*strs) * strs_num);
 * strs[0] = estrdup("hello");
 * strs[1] = estrdup("world");
 *
 * string_join_array(", ", strs, strs_num); --> "hello, world"
 * @endcode
 * @param delim
 * Delimeter to use, eg, ", ". Can be NULL.
 * @param array
 * Array of string pointers.
 * @param arraysize
 * Number of entries inside ::array.
 * @return
 * Joined string; never NULL. Must be freed.
 */
char *string_join_array(const char *delim, const char *const *array,
                        size_t arraysize)
{
    StringBuffer *sb;
    size_t i;

    TOOLKIT_PROTECT();

    sb = stringbuffer_new();

    for (i = 0; i < arraysize; i++) {
        if (!array[i]) {
            continue;
        }

        if (stringbuffer_length(sb) != 0 && delim) {
            stringbuffer_append_string(sb, delim);
        }

        stringbuffer_append_string(sb, array[i]);
    }

    return stringbuffer_finish(sb);
}

/**
 * Repeat the specified string X number of times.
 *
 * Example:
 * @code
 * string_repeat("world", 5); --> "worldworldworldworldworld"
 * @endcode
 * @param str
 * String to repeat.
 * @param num
 * How many times to repeat the string.
 * @return
 * Constructed string; never NULL. Must be freed.
 */
char *string_repeat(const char *str, size_t num MEMORY_DEBUG_PROTO)
{
    size_t len, i;
    char *ret;

    TOOLKIT_PROTECT();

    len = strlen(str);
    ret = memory_emalloc(sizeof(char) * (len * num) + 1 MEMORY_DEBUG_PARAM);

    for (i = 0; i < num; i++) {
        /* Cannot overflow; 'ret' has been allocated to hold enough
         * characters. */
        strcpy(ret + (len * i), str);
    }

    ret[len * i] = '\0';

    return ret;
}

/**
 * Like snprintf(), but appends the formatted string to the specified buffer.
 * The buffer must contain a null-byte.
 * @param buf
 * Buffer to append to.
 * @param size
 * Maximum size of the buffer.
 * @param fmt
 * Format specifier.
 * @param ...
 * Format arguments.
 * @return
 * Length of string in the buffer.
 */
size_t snprintfcat(char *buf, size_t size, const char *fmt, ...)
{
    size_t result;
    va_list args;
    size_t len;

    len = strnlen(buf, size);

    va_start(args, fmt);
    result = vsnprintf(buf + len, size - len, fmt, args);
    va_end(args);

    return result + len;
}

/**
 * Converts unsigned char array into a hexadecimal char representation.
 * @param str
 * Unsigned char array.
 * @param len
 * Number of elements in 'str'.
 * @param result
 * Where to store the result.
 * @param resultsize
 * Size of 'result'.
 * @param sep
 * If true, separate each hex with a colon.
 * @return
 * Number of characters written into 'result'.
 */
size_t string_tohex(const unsigned char *str, size_t len, char *result,
        size_t resultsize, bool sep)
{
    size_t i, written, need;

    written = 0;

    for (i = 0; i < len; i++) {
        if (i == len - 1) {
            sep = false;
        }

        need = sep ? 3 : 2;

        if (written + need > resultsize - 1) {
            break;
        }

        sprintf(result + written, "%02X%s", str[i], sep ? ":" : "");
        written += need;
    }

    result[written] = '\0';

    return written;
}

/**
 * Does the reverse of string_tohex(), loading hexadecimal back into unsigned
 * char.
 * @param str
 * String to load from.
 * @param len
 * Length of 'str'.
 * @param result
 * Where to store the result.
 * @param resultsize
 * Number of elements in 'result'.
 * @return
 * How many elements have been filled into 'result'.
 */
size_t string_fromhex(const char *str, size_t len, unsigned char *result,
        size_t resultsize)
{
    size_t i, j;
    unsigned char c, found;

    for (found = 0, i = 0, j = 0, c = 0; i < len && j < resultsize; i++) {
        if ((str[i] >= 'A' && str[i] <= 'F') ||
                (str[i] >= '0' && str[i] <= '9')) {
            c = (c << 4) |
                    ((str[i] >= 'A') ? (str[i] - 'A' + 10) : (str[i] - '0'));
            found++;
        }

        if (found == 2) {
            found = 0;
            result[j++] = c;
            c = 0;
        }
    }

    return j;
}

/**
 * Find the last occurrence of 'needle' in 'haystack'.
 * @param haystack
 * Where to search in.
 * @param needle
 * What to search.
 * @return
 * Substring or NULL if not found.
 */
char *string_last(const char *haystack, const char *needle)
{
    size_t len_haystack, len_needle;

    TOOLKIT_PROTECT();

    HARD_ASSERT(haystack != NULL);
    HARD_ASSERT(needle != NULL);

    if (*needle == '\0') {
        return NULL;
    }

    len_haystack = strlen(haystack);
    len_needle = strlen(needle);

    if (len_needle > len_haystack) {
        return NULL;
    }

    char *cp = (char *) haystack + len_haystack - len_needle;

    do {
        if (*cp == *needle) {
            if (strncmp(cp, needle, len_needle) == 0) {
                return cp;
            }
        }
    } while (cp-- != haystack);

    return NULL;
}

#endif

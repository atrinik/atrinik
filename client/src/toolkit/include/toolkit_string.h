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
 * Toolkit string API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_STRING_H
#define	TOOLKIT_STRING_H

/* Map the error-checking string duplicating functions into the toolkit
 * variants. This is done for convenience, and because the functions can't be
 * defined as they could conflict with functions from other libraries. */
#ifndef NDEBUG
#define estrdup(_s) string_estrdup(_s, __FILE__, __LINE__)
#define estrndup(_s, _n) string_estrndup(_s, _n, __FILE__, __LINE__)
#else
#define estrdup(_s) string_estrdup(_s)
#define estrndup(_s, _n) string_estrndup(_s, _n)
#endif

/**
 * Skip whitespace in the specified string.
 *
 * @param str
 * The string. Cannot be NULL.
 */
#define string_skip_whitespace(str)     \
do {                                    \
    HARD_ASSERT(str != NULL);           \
    while (isspace(*(str))) {           \
        (str)++;                        \
    }                                   \
} while (0)

/**
 * Strip the trailing newline in the specified string, if any.
 *
 * @param str
 * The string. Cannot be NULL.
 */
#define string_strip_newline(str)                       \
do {                                                    \
    HARD_ASSERT(str != NULL);                           \
    char *CONCAT(end, __LINE__) = strchr(str, '\n');    \
    if (CONCAT(end, __LINE__) != NULL) {                \
        *CONCAT(end, __LINE__) = '\0';                  \
    }                                                   \
} while (0)

/* Prototypes */

void toolkit_string_init(void);
void toolkit_string_deinit(void);
char *string_estrdup(const char *s MEMORY_DEBUG_PROTO);
char *string_estrndup(const char *s, size_t n MEMORY_DEBUG_PROTO);
void string_replace(const char *src, const char *key, const char *replacement,
        char *result, size_t resultsize);
void string_replace_char(char *str, const char *key, const char replacement);
size_t string_split(char *str, char *array[], size_t array_size, char sep);
void string_replace_unprintable_chars(char *buf);
char *string_format_number_comma(uint64_t num);
void string_toupper(char *str);
void string_tolower(char *str);
char *string_whitespace_trim(char *str);
char *string_whitespace_squeeze(char *str);
void string_newline_to_literal(char *str);
const char *string_get_word(const char *str, size_t *pos, char delim,
        char *word, size_t wordsize, int surround);
void string_skip_word(const char *str, size_t *i, int dir);
int string_isdigit(const char *str);
void string_capitalize(char *str);
void string_title(char *str);
int string_startswith(const char *str, const char *cmp);
int string_endswith(const char *str, const char *cmp);
char *string_sub(const char *str, ssize_t start,
        ssize_t end MEMORY_DEBUG_PROTO);
int string_isempty(const char *str);
int string_iswhite(const char *str);
int char_contains(const char c, const char *key);
int string_contains(const char *str, const char *key);
int string_contains_other(const char *str, const char *key);
char *string_create_char_range(char start, char end MEMORY_DEBUG_PROTO);
char *string_join(const char *delim, ...);
char *string_join_array(const char *delim, const char *const *array, size_t arraysize);
char *string_repeat(const char *str, size_t num MEMORY_DEBUG_PROTO);
size_t snprintfcat(char *buf, size_t size, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
size_t string_tohex(const unsigned char *str, size_t len, char *result,
        size_t resultsize, bool sep);
size_t string_fromhex(const char *str, size_t len, unsigned char *result,
        size_t resultsize);
char *string_last(const char *haystack, const char *needle);

#ifndef NDEBUG
#define string_sub(_str, _start, _end) \
    string_sub(_str, _start, _end MEMORY_DEBUG_INFO)
#define string_create_char_range(_start, _end) \
    string_create_char_range(_start, _end MEMORY_DEBUG_INFO)
#define string_repeat(_str, _num) \
    string_repeat(_str, _num MEMORY_DEBUG_INFO)
#endif

#endif

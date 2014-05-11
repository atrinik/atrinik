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

/**
 * @file
 * Pattern match a string, parsing some of the common RE-metacharacters.
 *
 * This code is public domain, but I would appreciate to hear of
 * improvements or even the fact that you use it in your program.
 *
 * Deliberate BUGS:
 *    - These tokens are not supported: | ( )
 *    - You will get the longest expansion of the _first_ string which
 *    matches the RE, not the longest string which would be the proper
 *    behavior for a RE-matcher.
 *
 * Author: Kjetil T. Homme (kjetilho@ifi.uio.no) May 1993 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include <re-cmp.h>
#include <ctype.h>
#include <string.h>

const char *re_cmp(const char *, const char *);
static int re_cmp_step(const char *, const char *, int, int);
static void re_init(void);
static int re_match_token(unsigned char, selection *);
static const char *re_get_token(selection *, const char *);
#ifdef DEBUG2
static void re_dump_sel(selection *);
#endif

static int re_init_done = 0;
static selection *re_token[RE_TOKEN_MAX];
static const char *re_substr[RE_TOKEN_MAX];
static unsigned int re_token_depth;

/**
 * Get regular expression match.
 * @param str String that will be matched against the regexp.
 * @param regexp Regular expression.
 * @return
 * - No match or error in regexp.
 * - Pointer to beginning of matching string */
const char *re_cmp(const char *str, const char *regexp)
{
    const char *next_regexp;
    int once = 0;
    int matched = 0;

    if (re_init_done == 0) {
        re_init();
    }

#ifdef SAFE_CHECKS

    if (regexp == NULL || str == NULL) {
        return NULL;
    }
#endif

    if (*regexp == '^') {
        once = 1;
        ++regexp;
    }

    if (*regexp == '\0') {
        /* // or /^/ matches any string */
        return str;
    }

    next_regexp = re_get_token(re_token[0], regexp);
    re_token_depth = 0;
    re_substr[0] = next_regexp;

    while (*str != '\0' && !(matched = re_match_token(*str, re_token[0]))) {
        /* If '^' was present and we didn't get a match above, return
         * NULL now so things like '^hi$' won't match on 'thi'. */
        if (once) {
            return NULL;
        }

        str++;
    }

    if (matched && *next_regexp == 0) {
        return str;
    }

    /* Apologies for the nearly duplicated code below, hopefully it
     * speeds things up. */
    if (once) {
        switch (re_token[0]->repeat) {
            case rep_once:

                if (matched == 0) {
                    return NULL;
                }

                break;

            case rep_once_or_more:

                if (matched == 0) {
                    return NULL;
                }

                if (re_cmp_step(str + 1, regexp, 0, 1)) {
                    return str;
                }

                break;

            case rep_null_or_once:

                if (matched == 0) {
                    return re_cmp_step(str, next_regexp, 1, 0) ? str : NULL;
                }

                break;

            case rep_null_or_more:

                if (matched) {
                    if (re_cmp_step(str + 1, regexp, 0, 1)) {
                        return str;
                    }
                }
                else {
                    return re_cmp_step(str, next_regexp, 1, 0) ? str : NULL;
                }

                break;
        }

        return re_cmp_step(str + 1, next_regexp, 1, 0) ? str : NULL;
    }

    if (matched) {
        switch (re_token[0]->repeat) {
            case rep_once:
            case rep_null_or_once:
                break;

            case rep_once_or_more:
            case rep_null_or_more:

                if (re_cmp_step(str + 1, regexp, 0, 1)) {
                    return str;
                }

                break;
        }

        /* The logic here is that re_match_token only sees if the one
         * letter matches. Thus, if the regex is like '@match dragon', and
         * the the user enters anything with 'd', re_match_token returns
         * true, but they really need to match the entire regexp, which
         * re_cmp_step will do. However, what happens is that there can
         * be a case where the string being match is something like
         * 'where is dragon'. In this case, the re_match_token matches
         * that first 'd', but the re_cmp_step below, fails because the
         * next character ('a') doesn't match the 'r'. So we call re_cmp
         * with the string after the first 'a', so that it should hopefully
         * match up properly. */
        if (re_cmp_step(str + 1, next_regexp, 1, 0)) {
            return str;
        }
        else if (*(str + 1) != 0) {
            return re_cmp(str + 1, regexp);
        }
    }

    return NULL;
}

/**
 * Tries to match a string with a regexp.
 * @param str String to match.
 * @param regexp Pattern.
 * @param slot Number of the token which under consideration.
 * @param matches How many times the token has matched.
 * @return 1 if match, 0 otherwise. */
static int re_cmp_step(const char *str, const char *regexp, int slot, int matches)
{
    const char *next_regexp;
    int matched;

    if (*regexp == '\0') {
        /* When we reach the end of the regexp, the match is a success */
        return 1;
    }

    /* This chunk of code makes sure that the regexp-tokenising happens
     * only once. We only tokenise as much as we need. */
    if ((unsigned int) slot > re_token_depth) {
        re_token_depth = slot;

        if (re_token[slot] == NULL) {
            re_token[slot] = malloc(sizeof(selection));
        }

        next_regexp = re_get_token(re_token[slot], regexp);

        if (next_regexp == NULL) {
            /* Syntax error, what else can we do? */
            return 0;
        }

        re_substr[slot] = next_regexp;
    }
    else {
        next_regexp = re_substr[slot];
    }

    matched = re_match_token(*str, re_token[slot]);

    if (matched) {
        ++matches;
    }

    if (*str == '\0') {
        return (*next_regexp == '\0' || re_token[slot]->type == sel_end) && matched;
    }

    switch (re_token[slot]->repeat) {
        case rep_once:

            /* (matches == 1) => (matched == 1) */
            if (matches == 1) {
                return re_cmp_step(str + 1, next_regexp, slot + 1, 0);
            }

            return 0;

        case rep_once_or_more:

            /* (matched == 1) => (matches >= 1) */
            if (matched) {
                /* First check if the current token repeats more */
                if (re_cmp_step(str + 1, regexp, slot, matches)) {
                    return 1;
                }

                return re_cmp_step(str + 1, next_regexp, slot + 1, 0);
            }

            return 0;

        case rep_null_or_once:

            /* We must go on to the next token, but should we advance str? */
            if (matches == 0) {
                return re_cmp_step(str, next_regexp, slot + 1, 0);
            }
            else if (matches == 1) {
                return re_cmp_step(str + 1, next_regexp, slot + 1, 0);
            }

            /* Not reached */
            return 0;

        case rep_null_or_more:

            if (matched) {
                /* Look for further repeats, advance str */
                if (re_cmp_step(str + 1, regexp, slot, matches)) {
                    return 1;
                }

                return re_cmp_step(str, next_regexp, slot + 1, 0);
            }

            return re_cmp_step(str, next_regexp, slot + 1, 0);
    }

    return 0;
}

/**
 * Init the regular expression structures. */
static void re_init(void)
{
    int i;

    re_token[0] = malloc(sizeof(selection));

    for (i = 1; i < RE_TOKEN_MAX; i++) {
        re_token[i] = NULL;
    }

    re_init_done = 1;
}

/**
 * Tests if a char matches a token.
 * @param c Char to test.
 * @param sel Token to test.
 * @return 1 if matches, 0 otherwise. */
static int re_match_token(unsigned char c, selection *sel)
{
    switch (sel->type) {
        case sel_any:
            return 1;

        case sel_end:
            return (c == '\0');

        case sel_single:
            return (tolower(c) == tolower(sel->u.single));

        case sel_range:
            return (c >= sel->u.range.low && c <= sel->u.range.high);

        case sel_array:
            return (sel->u.array[c]);

        case sel_not_single:
            return (tolower(c) != tolower(sel->u.single));

        case sel_not_range:
            return (c < sel->u.range.low && c > sel->u.range.high);
    }

    return 0;
}

/**
 * Get the first regular expression token found in regexp in sel.
 * @param[out] sel Where to store the token.
 * @param regexp Regular expression.
 * @return
 * - NULL: syntax error
 * - Pointer to first character past token. */
static const char *re_get_token(selection *sel, const char *regexp)
{
#ifdef SAFE_CHECKS
#   define exit_if_null if (*regexp == '\0') return NULL
#else
#   define exit_if_null
#endif

    int quoted = 0;
    unsigned char looking_at;

#ifdef SAFE_CHECKS

    if (sel == NULL || regexp == NULL || *regexp == '\0') {
        return NULL;
    }
#endif

    do
    {
        looking_at = *regexp++;

        switch (looking_at) {
            case '$':

                if (quoted) {
                    quoted = 0;
                    sel->type = sel_single;
                    sel->u.single = looking_at;
                }
                else {
                    sel->type = sel_end;
                }

                break;

            case '.':

                if (quoted) {
                    quoted = 0;
                    sel->type = sel_single;
                    sel->u.single = looking_at;
                }
                else {
                    sel->type = sel_any;
                }

                break;

            case '[':

                /* The fun stuff... perhaps a little obfuscated since I
                 * don't trust the compiler to analyze liveness. */
                if (quoted) {
                    quoted = 0;
                    sel->type = sel_single;
                    sel->u.single = looking_at;
                }
                else {
                    int neg = 0;
                    unsigned char first, last = 0;

                    exit_if_null;
                    looking_at = *regexp++;

                    if (looking_at == '^') {
                        neg = 1;
                        exit_if_null;
                        looking_at = *regexp++;
                    }

                    first = looking_at;
                    exit_if_null;

                    looking_at = *regexp++;

                    if (looking_at == ']') {
                        /* On the form [q] or [^q] */
                        sel->type = neg ? sel_not_single : sel_single;
                        sel->u.single = first;
                        break;
                    }
                    else if (looking_at == '-') {
                        exit_if_null;
                        last = *regexp++;

                        if (last == ']') {
                            /* On the form [A-] or [^A-]. Checking for
                             * [,-] and making it a range is probably not
                             * worth it :-) */
                            sel->type = sel_array;
                            memset(sel->u.array, neg, sizeof(sel->u.array));
                            sel->u.array[first] = sel->u.array['-'] = !neg;
                            break;
                        }
                        else {
                            exit_if_null;
                            looking_at = *regexp++;

                            if (looking_at == ']') {
                                /* On the form [A-G] or [^A-G]. Note that [G-A]
                                 * is a syntax error. Fair enough, I think. */
#ifdef SAFE_CHECKS

                                if (first > last) {
                                    return NULL;
                                }
#endif
                                sel->type = neg ? sel_not_range : sel_range;
                                sel->u.range.low = first;
                                sel->u.range.high = last;
                                break;
                            }
                        }
                    }

                    {
                        /* The datastructure can only represent a RE this
                         * complex with an array. */
                        int i;
                        unsigned char previous;

                        sel->type = sel_array;
                        memset(sel->u.array, neg, sizeof(sel->u.array));

                        if (last) {
                            /* It starts with a range */
#ifdef SAFE_CHECKS

                            if (first > last) {
                                return NULL;
                            }
#endif
                            for (i = first; i < last + 1; i++) {
                                sel->u.array[i] = !neg;
                            }
                        }
                        else {
                            /* It begins with a "random" character */
                            sel->u.array[first] = !neg;
                        }

                        sel->u.array[looking_at] = !neg;

                        exit_if_null;
                        previous = looking_at;
                        looking_at = *regexp++;

                        /* Add more characters to the array until we reach
                         * ]. Quoting doesn't and shouldn't work in here.
                         * ("]" should be put first, and "-" last if they
                         * are needed inside this construct.)
                         * Look for ranges as we go along. */
                        while (looking_at != ']') {
                            if (looking_at == '-') {
                                exit_if_null;
                                looking_at = *regexp++;

                                if (looking_at != ']') {
#ifdef SAFE_CHECKS

                                    if (previous > looking_at) {
                                        return NULL;
                                    }
#endif
                                    for (i = previous + 1; i < looking_at; i++) {
                                        /* previous has already been set and
                                         * looking_at is set below. */
                                        sel->u.array[i] = !neg;
                                    }

                                    exit_if_null;
                                }
                                else {
                                    sel->u.array['-'] = !neg;
                                    break;
                                }
                            }

                            sel->u.array[looking_at] = !neg;
                            previous = looking_at;
                            exit_if_null;
                            looking_at = *regexp++;
                        }
                    }
                }

                break;

            case '\\':

                if (quoted) {
                    quoted = 0;
                    sel->type = sel_single;
                    sel->u.single = looking_at;
                }
                else {
                    quoted = 1;
                }

                break;

            default:
                quoted = 0;
                sel->type = sel_single;
                sel->u.single = looking_at;
                break;
        }
    }
    while (quoted);

    if (*regexp == '*') {
        sel->repeat = rep_null_or_more;
        ++regexp;
    }
    else if (*regexp == '?') {
        sel->repeat = rep_null_or_once;
        ++regexp;
    }
    else if (*regexp == '+') {
        sel->repeat = rep_once_or_more;
        ++regexp;
    }
    else {
        sel->repeat = rep_once;
    }

    return regexp;
}

#ifdef DEBUG2

/**
 * Dumps specified selection to stdout.
 * @param sel Token to dump. */
static void re_dump_sel(selection *sel)
{
    switch (sel->type) {
        case sel_any:
            printf(".");
            break;

        case sel_end:
            printf("$");
            break;

        case sel_single:
            printf("<%c>", sel->u.single);
            break;

        case sel_range:
            printf("[%c-%c]", sel->u.range.low, sel->u.range.high);
            break;

        case sel_array:
        {
            int i;

            printf("[");

            for (i = 0; i < UCHAR_MAX; i++) {
                if (sel->u.array[i]) {
                    printf("%c", i);
                }
            }

            printf("]");
            break;
        }

        case sel_not_single:
            printf("[^%c]", sel->u.single);
            break;

        case sel_not_range:
            printf("[^%c-%c]", sel->u.range.low, sel->u.range.high);
            break;

        default:
            printf("<UNKNOWN TOKEN!>");
            break;
    }

    switch (sel->repeat) {
        case rep_once:
            break;

        case rep_null_or_once:
            printf("?");
            break;

        case rep_null_or_more:
            printf("*");
            break;

        case rep_once_or_more:
            printf("+");
            break;

        default:
            printf("<UNKNOWN REP-TOKEN!>");
            break;
    }
}

int main(int argc, char *argv[])
{
    char *re, *m;
    selection sel;

    re = re_get_token(&sel, argv[1]);

    printf("'%s' -> '%s'\n", argv[1], re);
    re_dump_sel(&sel);
    printf("\n");
    m = re_cmp(argv[2], argv[1]);

    if (m) {
        printf("MATCH! -> '%s'\n", m);
    }

    return 0;
}
#endif

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
 * Shared-strings defines. */

#ifndef SHSTR_H
#define SHSTR_H

/**
 * Used to differentiate shared strings from normal strings. */
typedef const char shstr;

/**
 * The size of the shared strings hashtable. */
#define TABLESIZE 8266

/*
 * This will make the shared string interface more secure by checking for
 * valid string before manipulating them, but also somewhat slower.
 *
 * Should only be used for debugging purposes. */
/*#define SECURE_SHSTR_HASH*/

/**
 * This specifies how many characters the hashing routine should look at.
 * You may actually save CPU by increasing this number if the typical string
 * is large. */
#ifndef MAXSTRING
#define MAXSTRING 20
#endif

/**
 * In the unlikely occurrence that 16383 references to a string are too
 * few, you can modify the below type to something bigger.
 * (The top bit of "refcount" is used to signify that "u.array" points
 * at the array entry.) */
#define REFCOUNT_TYPE long

/* The offsetof macro is part of ANSI C, but many compilers lack it, for
 * example "gcc -ansi" */
#if !defined(offsetof)
#define offsetof(type, member) (int)&(((type *)0)->member)
#endif

/**
 * SS(string) will return the address of the shared_string struct which
 * contains "string". */
#define SS(x) ((shared_string *) ((x) - offsetof(shared_string, string)))

#define SS_DUMP_TABLE   1
#define SS_DUMP_TOTALS  2

#define TOPBIT ((unsigned REFCOUNT_TYPE) 1 << (sizeof(REFCOUNT_TYPE) * CHAR_BIT - 1))

#define PADDING ((2 * sizeof(long) - sizeof(REFCOUNT_TYPE)) % sizeof(long)) + 1

/**
 * One actual shared string. */
typedef struct _shared_string {

    union {
        struct _shared_string **array;

        struct _shared_string *previous;
    } u;

    /** Next shared string. */
    struct _shared_string *next;

    /**
     * The top bit of "refcount" is used to signify that "u.array" points
     * at the array entry. */
    unsigned REFCOUNT_TYPE refcount;

    /**
     * Padding will be unused memory, since we can't know how large the
     * padding when allocating memory. We assume here that sizeof(long)
     * is a good boundary. */
    char string[PADDING];
} shared_string;

/**
 * Used to link together shared strings.
 *
 * @sa SHSTR_LIST_xxx
 */
typedef struct shstr_list {
    struct shstr_list *next; ///< Next shared string.
    shstr *value; ///< The actual shared string.
} shstr_list_t;

/**
 * @defgroup SHSTR_LIST_xxx Shared string list manipulation macros
 * Macros used for manipulation of shared string lists, such as prepending,
 * clearing, looping, etc.
 * @author Alex Tokar
 *@{*/

/**
 * Prepends the specified string value to the shared string list.
 *
 * The value has its shstr refcount increased automatically.
 *
 * Example:
 * <pre>
 * shstr_list_t *list = NULL;
 * SHSTR_LIST_PREPEND(list, "hello world");
 * </pre>
 * @param list_ Pointer to a shared string list that will be prepended to.
 * @param value_ String to add to the list.
 */
#define SHSTR_LIST_PREPEND(list_, value_) \
    do {                                                                       \
        shstr_list_t *shstr_list = emalloc(sizeof(*shstr_list));               \
        shstr_list->next = NULL;                                               \
        shstr_list->value = add_string(value_);                                \
        LL_PREPEND(list_, shstr_list);                                         \
    } while (0)

/**
 * Frees the entire shared string list, setting the list pointer to NULL
 * afterwards.
 *
 * Example:
 * <pre>
 * SHSTR_LIST_CLEAR(list);
 * </pre>
 * @param list_ Pointer to a shared string list that will be cleared.
 */
#define SHSTR_LIST_CLEAR(list_)                                                \
    do {                                                                       \
        shstr_list_t *shstr_list, *shstr_list_tmp;                             \
        LL_FOREACH_SAFE(list_, shstr_list, shstr_list_tmp) {                   \
            free_string_shared(shstr_list->value);                             \
            efree(shstr_list);                                                 \
        }                                                                      \
        list_ = NULL;                                                          \
    } while (0)

/**
 * Begin iterating a shared string list.
 *
 * Example:
 * <pre>
 * SHSTR_LIST_FOR_PREPARE(list, value) {
 *     printf("%s\n", value);
 * } SHSTR_LIST_FOR_FINISH();
 * </pre>
 * @param list_ Pointer to a shared string list that will be iterated.
 * @param it_ A variable name that will hold the shared string values;
 * a new variable will be declared
 */
#define SHSTR_LIST_FOR_PREPARE(list_, it_)                                     \
    do {                                                                       \
        shstr_list_t *shstr_list, *shstr_list_tmp;                             \
        LL_FOREACH_SAFE(list_, shstr_list, shstr_list_tmp) {                   \
            shstr *it_ = shstr_list->value;

/**
 * Finishes iteration started by #SHSTR_LIST_FOR_PREPARE.
 */
#define SHSTR_LIST_FOR_FINISH()                                                \
        }                                                                      \
    } while (0)

/*@}*/

#endif

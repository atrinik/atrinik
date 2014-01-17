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

#ifdef SS_STATISTICS
/** Used to collect statistics on string manipulation. */
static struct statistics
{
    int calls;
    int hashed;
    int strcmps;
    int search;
    int linked;
} add_stats, add_ref_stats, free_stats, find_stats, hash_stats;

#define GATHER(n) (++n)
#else
#define GATHER(n)
#endif

#define TOPBIT ((unsigned REFCOUNT_TYPE) 1 << (sizeof(REFCOUNT_TYPE) * CHAR_BIT - 1))

#define PADDING ((2 * sizeof(long) - sizeof(REFCOUNT_TYPE)) % sizeof(long)) + 1

/**
 * One actual shared string. */
typedef struct _shared_string
{
    union
    {
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

#endif

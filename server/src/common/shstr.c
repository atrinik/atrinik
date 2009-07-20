/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

/* shstr.c
 *
 * This is a simple shared strings package with a simple interface.
 *
 *     char *find_string(const char *str)
 *     char *add_string(const char *str)
 *	char *add_refcount(char *str)
 *	void free_string_shared(char *str)
 *	char *ss_dump_table(int what)
 *	void ss_dump_statistics()
 *
 * Author: Kjetil T. Homme, Oslo 1992.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>

#if defined (__sun__) && defined (StupidSunHeaders)
#include <sys/time.h>
#include "sunos.h"
#endif

#include "shstr.h"

#ifndef WIN32
#include <autoconf.h>
#endif
#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

#ifdef SS_STATISTICS
static struct statistics {
    int calls;
    int hashed;
    int strcmps;
    int search;
    int linked;
} add_stats, add_ref_stats, free_stats, find_stats, hash_stats;
#define GATHER(n) (++n)
#else /* !SS_STATISTICS */
#define GATHER(n)
#endif /* SS_STATISTICS */

/* define this will make the hash table more secure but
 * also somewhat slower - if no problems happens after
 * some testings (no bug messages from this module)
 * then this define should be disabled
 */
/*#define SECURE_SHSTR_HASH*/

/* small hack to insert LOG here to without the whole other externs */
#include "logger.h"
extern void LOG(LogLevel logLevel, char *format, ...);

static shared_string *hash_table[TABLESIZE];

/* Initialises the hash-table used by the shared string library. */
void init_hash_table()
{
    /* A static object should be zeroed out always */
#if !defined(__STDC__)
    (void) memset((void *)hash_table, 0, TABLESIZE * sizeof(shared_string *));
#endif
}

/* Hashing-function used by the shared string library. */
static int hashstr(const char *str)
{
    unsigned long hash = 0;
    int i = 0, rot = 0;
    const char *p;

    GATHER(hash_stats.calls);

    for (p = str; i < MAXSTRING && *p; p++, i++)
	{
		hash ^= (unsigned long) *p << rot;
		rot += 2;

		if (rot >= ((int) sizeof(long) - (int) sizeof(char)) * CHAR_BIT)
	    	rot = 0;
    }

    return (hash % TABLESIZE);
}

/* Allocates and initialises a new shared_string structure, containing
 * the string str. */
static shared_string *new_shared_string(const char *str)
{
    shared_string *ss;

    /* Allocate room for a struct which can hold str. Note
     * that some bytes for the string are already allocated in the
     * shared_string struct. */
    ss = (shared_string *) malloc(sizeof(shared_string) - PADDING + strlen(str) + 1);
    ss->u.previous = NULL;
    ss->next = NULL;
    ss->refcount = 1;
	/*LOG(llevDebug,"SS: >%s< #%d - new\n",str,ss->refcount& ~TOPBIT);*/
    strcpy(ss->string, str);

    return ss;
}

/* Description:
 *      This will add 'str' to the hash table. If there's no entry for this
 *      string, a copy will be allocated, and a pointer to that is returned.
 * Return values:
 *      - pointer to string identical to str */
const char *add_string(const char *str)
{
    shared_string *ss;
    int ind;

    GATHER(add_stats.calls);

    /* Should really core dump here, since functions should not be calling
     * add_string with a null parameter.  But this will prevent a few
     * core dumps. */
    if (str == NULL)
	{
		LOG(llevBug, "BUG: add_string(): try to add null string to hash table\n");
		return NULL;
	}
    ind = hashstr(str);
    ss = hash_table[ind];

    /* Is there an entry for that hash?
     */
    if (ss)
	{
		/* Simple case first: See if the first pointer matches. */
		if (str != ss->string)
		{
			GATHER(add_stats.strcmps);
			if (strcmp(ss->string, str))
			{
				/* Apparantly, a string with the same hash value has this
				 * slot. We must see in the list if "str" has been
				 * registered earlier. */
				while (ss->next)
				{
					GATHER(add_stats.search);
					ss = ss->next;
					if (ss->string != str)
					{
						GATHER(add_stats.strcmps);
						if (strcmp(ss->string, str))
						{
							/* This wasn't the right string... */
							continue;
						}
					}

					/* We found an entry for this string. Fix the
					 * refcount and exit. */
					GATHER(add_stats.linked);
					++(ss->refcount);
					/*LOG(llevDebug,"SS: >%s< #%d add-s\n", ss->string,ss->refcount& ~TOPBIT);*/

					return ss->string;
				}
				/* There are no occurences of this string in the hash table. */
				{
					shared_string *new_ss;

					GATHER(add_stats.linked);
					new_ss = new_shared_string(str);
					ss->next = new_ss;
					new_ss->u.previous = ss;
					return new_ss->string;
				}
	    	}

	   		/* Fall through. */
		}

		GATHER(add_stats.hashed);
		++(ss->refcount);
		/*LOG(llevDebug, "SS: >%s< #%d add-s\n", ss->string, ss->refcount & ~TOPBIT);*/
		return ss->string;
    }
	else
	{
		/* The string isn't registered, and the slot is empty. */
		GATHER(add_stats.hashed);
		hash_table[ind] = new_shared_string(str);

		/* One bit in refcount is used to keep track of the union. */
		hash_table[ind]->refcount |= TOPBIT;
		hash_table[ind]->u.array = &(hash_table[ind]);

		return hash_table[ind]->string;
    }
}

/* Description:
 *      This will return the refcount of the string str, which *must*
 *      have been returned from a previous add_string().
 * Return values:
 *      - length */
int query_refcount(const char *str)
{
    return SS(str)->refcount& ~TOPBIT;
}

/* Description:
 *      This will see if str is in the hash table, and return the address
 *      of that string if it exists.
 * Return values:
 *      - pointer to identical string or NULL */
const char *find_string(const char *str)
{
    shared_string *ss;
    int ind;

    GATHER(find_stats.calls);

    ind = hashstr(str);
    ss = hash_table[ind];

    /* Is there an entry for that hash? */
    if (ss)
	{
		/* Simple case first: Is the first string the right one? */
		GATHER(find_stats.strcmps);
		if (!strcmp(ss->string, str))
		{
	    	GATHER(find_stats.hashed);
	    	return ss->string;
		}
		else
		{
	    	/* Recurse through the linked list, if there's one. */
	    	while (ss->next)
			{
				GATHER(find_stats.search);
				GATHER(find_stats.strcmps);
				ss = ss->next;
				if (!strcmp(ss->string, str))
				{
		    		GATHER(find_stats.linked);
		    		return ss->string;
				}
	    	}
	    	/* No match. Fall through. */
		}
    }
    return NULL;
}

/* This function should be declared
 * const char *add_refcount(const char *)
 * Unfortunately, that would require changing a lot of structs */

/* Description:
 *      This will increase the refcount of the string str, which *must*
 *      have been returned from a previous add_string().
 * Return values:
 *      - str */
const char *add_refcount(const char *str)
{
#ifdef SECURE_SHSTR_HASH
	char *tmp_str = find_string(str);

	if (!str || str != tmp_str)
	{
		LOG(llevBug, "BUG: add_refcount(shared_string)(): tried to free a invalid string! >%s<\n", str ? str : ">NULL<");
		return NULL;
	}
#endif

    GATHER(add_ref_stats.calls);
    ++(SS(str)->refcount);
	/*LOG(llevDebug, "SS: >%s< #%d addref\n", str, SS(str)->refcount& ~TOPBIT);*/
    return str;
}

/* Description:
 *     This will reduce the refcount, and if it has reached 0, str will
 *     be freed.
 * Return values:
 *     None */
void free_string_shared(const char *str)
{
    shared_string *ss;

	/* We check str is in the hash table - if not, something
	 * is VERY wrong here. We can also be sure, that SS(str) is
	 * a safe operation - except we really messed something
	 * strange up inside the hash table. This will check for
	 * free, wrong or non SS() objects. */
#ifdef SECURE_SHSTR_HASH
	const char *tmp_str = find_string(str);

	if (!str || str != tmp_str)
	{
		LOG(llevBug, "BUG: free_string_shared(): tried to free a invalid string! >%s<\n", str ? str : ">NULL<");
		return;
	}
#endif

    GATHER(free_stats.calls);

    ss = SS(str);
	--ss->refcount;
    if ((ss->refcount & ~TOPBIT) == 0)
	{
		/* Remove this entry. */
		if (ss->refcount & TOPBIT)
		{
			/* We must put a new value into the hash_table[]. */
			if (ss->next)
			{
				/*LOG(llevDebug, "SS: >%s< #%d remove_a!\n", str, ss->refcount& ~TOPBIT);*/
				*(ss->u.array) = ss->next;
				ss->next->u.array = ss->u.array;
				ss->next->refcount |= TOPBIT;
			}
			else
			{
				/*LOG(llevDebug, "SS: >%s< #%d remove_b!\n", str, ss->refcount& ~TOPBIT);*/
				*(ss->u.array) = NULL;
			}
			free(ss);
		}
		else
		{
			/*LOG(llevDebug, "SS: >%s< #%d remove_c!\n", str, ss->refcount& ~TOPBIT);*/
			/* Relink and free this struct. */
			if (ss->next)
				ss->next->u.previous = ss->u.previous;

			ss->u.previous->next = ss->next;
			free(ss);
		}
    }
#if 0
	else
		LOG(llevDebug, "SS: >%s< #%d dec\n", str, ss->refcount& ~TOPBIT);
#endif
}

#ifdef SS_STATISTICS

extern char errmsg[];

/* Description:
 *      The routines will gather statistics if SS_STATISTICS is defined.
 *      A call to this function will cause the statistics to be dumped
 *      into an external string errmsg, which must be large.
 * Return values:
 *      None */
void ss_dump_statistics()
{
    static char line[80];

    sprintf(errmsg, "%-13s %6s %6s %6s %6s %6s\n", "", "calls", "hashed", "strcmp", "search", "linked");
    sprintf(line, "%-13s %6d %6d %6d %6d %6d\n", "add_string:", add_stats.calls, add_stats.hashed, add_stats.strcmps, add_stats.search, add_stats.linked);
    strcat(errmsg, line);
    sprintf(line, "%-13s %6d\n", "add_refcount:", add_ref_stats.calls);
    strcat(errmsg, line);
    sprintf(line, "%-13s %6d\n", "free_string:", free_stats.calls);
    strcat(errmsg, line);
    sprintf(line, "%-13s %6d %6d %6d %6d %6d\n", "find_string:", find_stats.calls, find_stats.hashed, find_stats.strcmps, find_stats.search, find_stats.linked);
    strcat(errmsg, line);
    sprintf(line, "%-13s %6d\n", "hashstr:", hash_stats.calls);
    strcat(errmsg, line);
}
#endif /* SS_STATISTICS */

/* Description:
 *      If (what & SS_DUMP_TABLE) dump the contents of the hash table to
 *      stderr. If (what & SS_DUMP_TOTALS) return a string which
 *      says how many entries etc. there are in the table.
 * Return values:
 *      - a string or NULL */
char *ss_dump_table(int what)
{
    static char totals[80];
    int entries = 0, refs = 0, links = 0;
    int i;

    for (i = 0; i < TABLESIZE; i++)
	{
		shared_string *ss;
		if ((ss = hash_table[i]) != NULL)
		{
			++entries;
			refs += (ss->refcount & ~TOPBIT);

			/* Can't use stderr any longer, need to include global.h and
			 * if (what & SS_DUMP_TABLE)
			 * use logfile. */
			LOG(llevSystem, "%4d -- %4d refs '%s' %c\n", i, (ss->refcount & ~TOPBIT), ss->string, (ss->refcount & TOPBIT ? ' ' : '#'));
			while (ss->next)
			{
				ss = ss->next;
				++links;
				refs += (ss->refcount & ~TOPBIT);
				if (what & SS_DUMP_TABLE)
					LOG(llevSystem, "     -- %4d refs '%s' %c\n", (ss->refcount & ~TOPBIT), ss->string, (ss->refcount & TOPBIT ? '*' : ' '));
			}
		}
    }

    if (what & SS_DUMP_TOTALS)
	{
		sprintf(totals, "\n%d entries, %d refs, %d links.", entries, refs, links);
        return totals;
    }
    return NULL;
}

/* to find memory leak! */
void ss_test_table(void)
{
	shared_string *ss;
    int entries = 0;
    int i;
	char c;

    for (i = 0; i < TABLESIZE; i++)
	{
		if ((ss = hash_table[i]) != NULL)
		{
			++entries;
			c = ss->string[0];
		}
	}
}

/* buf_overflow() - we don't want to exceed the buffer size of
 * buf1 by adding on buf2! Returns true if overflow will occur. */
int buf_overflow(const char *buf1, const char *buf2, int bufsize)
{
    int len1 = 0, len2 = 0;

    if (buf1)
		len1 = strlen (buf1);

    if (buf2)
		len2 = strlen (buf2);

    if ((len1 + len2) >= bufsize)
		return 1;

    return 0;
}

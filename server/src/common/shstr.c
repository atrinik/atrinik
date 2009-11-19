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

/**
 * @file
 * This is a simple shared strings package with a simple interface.
 * @author Kjetil T. Homme, Oslo 1992. */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <global.h>

#if defined (__sun__) && defined (StupidSunHeaders)
#include <sys/time.h>
#include "sunos.h"
#endif

#define SS_STATISTICS
#include "shstr.h"

#ifndef WIN32
#include <autoconf.h>
#endif
#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif

/** Hash table to store our string. */
static shared_string *hash_table[TABLESIZE];

/**
 * Initializes the hash-table used by the shared string library. */
void init_hash_table()
{
	/* A static object should be zeroed out always */
#if !defined(__STDC__)
	(void) memset((void *) hash_table, 0, TABLESIZE * sizeof(shared_string *));
#endif
}

/**
 * Hashing function used by the shared string library.
 * @param str String to hash.
 * @return Hash of string, suitable for use in ::hash_table. */
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
		{
			rot = 0;
		}
	}

	return (hash % TABLESIZE);
}

/**
 * Allocates and initializes a new shared_string structure, containing
 * the string str.
 * @param str String to store.
 * @return Sharing structure. */
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

/**
 * This will add 'str' to the hash table. If there's no entry for this
 * string, a copy will be allocated, and a pointer to that is returned.
 * @param str String to share.
 * @return Pointer to string identical to str, but shared. */
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
		LOG(llevBug, "BUG: add_string(): Tried to add NULL string to hash table\n");
		return NULL;
	}

	ind = hashstr(str);
	ss = hash_table[ind];

	/* Is there an entry for that hash? */
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

/**
 * This will increase the refcount of the string str.
 * @param str String which <b>must</b> have been returned from a previous
 * add_string().
 * @return str. */
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

	return str;
}

/**
 * This will return the refcount of the string str.
 * @param str String which <b>must</b> have been returned from a previous
 * add_string().
 * @return Refcount of the string. */
int query_refcount(const char *str)
{
	return SS(str)->refcount & ~TOPBIT;
}

/**
 * Searches a string in the shared strings.
 * @param str String to search for.
 * @return Pointer to identical string or NULL. */
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

/**
 * This will reduce the refcount, and if it has reached 0, str will be
 * freed.
 * @param str String to release, which <b>must</b> have been returned
 * from a previous add_string(). */
void free_string_shared(const char *str)
{
	shared_string *ss;

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
				*(ss->u.array) = ss->next;
				ss->next->u.array = ss->u.array;
				ss->next->refcount |= TOPBIT;
			}
			else
			{
				*(ss->u.array) = NULL;
			}

			free(ss);
		}
		else
		{
			/* Relink and free this struct. */
			if (ss->next)
			{
				ss->next->u.previous = ss->u.previous;
			}

			ss->u.previous->next = ss->next;
			free(ss);
		}
	}
}

#ifdef SS_STATISTICS

/**
 * A call to this function will cause the statistics to be dumped into
 * specified buffer.
 *
 * The routines will gather statistics if SS_STATISTICS is defined.
 * @param buf Buffer which will contain dumped information.
 * @param size Buffer's size. */
void ss_dump_statistics(char *buf, size_t size)
{
	static char line[MAX_BUF];

    snprintf(buf, size, "%-13s %6s %6s %6s %6s %6s\n", "", "calls", "hashed", "strcmp", "search", "linked");
    snprintf(line, sizeof(line), "%-13s %6d %6d %6d %6d %6d\n", "add_string:", add_stats.calls, add_stats.hashed, add_stats.strcmps, add_stats.search, add_stats.linked);
    snprintf(buf + strlen(buf), size - strlen(buf), "%s", line);
    snprintf(line, sizeof(line), "%-13s %6d\n", "add_refcount:", add_ref_stats.calls);
    snprintf(buf + strlen(buf), size - strlen(buf), "%s", line);
    snprintf(line, sizeof(line), "%-13s %6d\n", "free_string:", free_stats.calls);
    snprintf(buf + strlen(buf), size - strlen(buf), "%s", line);
    snprintf(line, sizeof(line), "%-13s %6d %6d %6d %6d %6d\n", "find_string:", find_stats.calls, find_stats.hashed, find_stats.strcmps, find_stats.search, find_stats.linked);
    snprintf(buf + strlen(buf), size - strlen(buf), "%s", line);
    snprintf(line, sizeof(line), "%-13s %6d\n", "hashstr:", hash_stats.calls);
    snprintf(buf + strlen(buf), size - strlen(buf), "%s", line);
}
#endif

/**
 * Dump the contents of the shared string tables.
 * @param what Combination of flags:
 * - ::SS_DUMP_TABLE: Dump the contents of the hash table to log.
 * - ::SS_DUMP_TOTALS: Return a string which says how many entries etc.
 *   there are in the table.
 * @param buf Buffer that will contain total information
 * if (what & SS_DUMP_TABLE). Left untouched else.
 * @param size Buffer's size. */
void ss_dump_table(int what, char *buf, size_t size)
{
	int entries = 0, refs = 0, links = 0, i;

	for (i = 0; i < TABLESIZE; i++)
	{
		shared_string *ss;

		if ((ss = hash_table[i]) != NULL)
		{
			++entries;
			refs += (ss->refcount & ~TOPBIT);

			LOG(llevSystem, "%4d -- %4d refs '%s' %c\n", i, (ss->refcount & ~TOPBIT), ss->string, (ss->refcount & TOPBIT ? ' ' : '#'));

			while (ss->next)
			{
				ss = ss->next;
				++links;
				refs += (ss->refcount & ~TOPBIT);

				if (what & SS_DUMP_TABLE)
				{
					LOG(llevSystem, "     -- %4d refs '%s' %c\n", (ss->refcount & ~TOPBIT), ss->string, (ss->refcount & TOPBIT ? '*' : ' '));
				}
			}
		}
	}

	if (what & SS_DUMP_TOTALS)
	{
		snprintf(buf, size, "\n%d entries, %d refs, %d links.", entries, refs, links);
	}
}

/**
 * We don't want to exceed the buffer size of buf1 by adding on buf2!
 * @param buf1
 * @param buf2
 * Buffers we plan on concatening. Can be NULL.
 * @param bufsize Size of buf1. Can be 0.
 * @return 1 if overflow will occur, 0 otherwise. */
int buf_overflow(const char *buf1, const char *buf2, size_t bufsize)
{
	size_t len1 = 0, len2 = 0;

	if (buf1)
	{
		len1 = strlen(buf1);
	}

	if (buf2)
	{
		len2 = strlen(buf2);
	}

	if ((len1 + len2) >= bufsize)
	{
		return 1;
	}

	return 0;
}

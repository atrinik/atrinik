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
 * This is a simple shared strings package with a simple interface.
 * @author Kjetil T. Homme, Oslo 1992. */

#include <global.h>

/** Hash table to store our strings. */
static shared_string *hash_table[TABLESIZE];

/**
 * Initialize the shstr API.
 * @internal */
void toolkit_shstr_init(void)
{
	TOOLKIT_INIT_FUNC_START(shstr)
	{
		memset((void *) hash_table, 0, TABLESIZE * sizeof(shared_string *));
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the shstr API.
 * @internal */
void toolkit_shstr_deinit(void)
{
}

/**
 * Hashing function used by the shared string library.
 * @param str String to hash.
 * @return Hash of string, suitable for use in ::hash_table. */
static unsigned long hashstr(const char *str)
{
	unsigned long hash = 0;
	int i = 0;
	unsigned int rot = 0;
	const char *p;

	GATHER(hash_stats.calls);

	for (p = str; i < MAXSTRING && *p; p++, i++)
	{
		hash ^= (unsigned long) *p << rot;
		rot += 2;

		if (rot >= (sizeof(unsigned long) - sizeof(char)) * CHAR_BIT)
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
	size_t n = strlen(str);

	/* Allocate room for a struct which can hold str. Note
	 * that some bytes for the string are already allocated in the
	 * shared_string struct. */
	ss = (shared_string *) malloc(sizeof(shared_string) - PADDING + n + 1);

	if (!ss)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	ss->u.previous = NULL;
	ss->next = NULL;
	ss->refcount = 1;
	memcpy(ss->string, str, n);
	ss->string[n] = '\0';

	return ss;
}

/**
 * This will add 'str' to the hash table. If there's no entry for this
 * string, a copy will be allocated, and a pointer to that is returned.
 * @param str String to share.
 * @return Pointer to string identical to str, but shared. */
shstr *add_string(const char *str)
{
	shared_string *ss;
	unsigned long ind;

	GATHER(add_stats.calls);

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
				/* Apparently, a string with the same hash value has this
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

					return ss->string;
				}

				/* There are no occurrences of this string in the hash table. */
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
	/* The string isn't registered, and the slot is empty. */
	else
	{
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
shstr *add_refcount(shstr *str)
{
	GATHER(add_ref_stats.calls);
	++(SS(str)->refcount);

	return str;
}

/**
 * This will return the refcount of the string str.
 * @param str String which <b>must</b> have been returned from a previous
 * add_string().
 * @return Refcount of the string. */
int query_refcount(shstr *str)
{
	return SS(str)->refcount & ~TOPBIT;
}

/**
 * Searches a string in the shared strings.
 * @param str String to search for.
 * @return Pointer to identical string or NULL. */
shstr *find_string(const char *str)
{
	shared_string *ss;
	unsigned long ind;

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
void free_string_shared(shstr *str)
{
	shared_string *ss;

	GATHER(free_stats.calls);
	ss = SS(str);

	if ((--ss->refcount & ~TOPBIT) == 0)
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

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Implements generic caching of any pointer identified by a unique key.
 *
 * API usage:
 *
 * @code
char *str = strdup_local("hello world");
cache_struct *res;

// CACHE_FLAG_AUTOFREE will automatically free the pointer.
cache_add("cache_test", str, CACHE_FLAG_AUTOFREE);

// The cache code stores the keys as shared strings, so you must attempt
// to find the string first.
res = cache_find(find_string("cache_test"));

// Since it was added before, it should not be NULL here.
if (res)
{
	// The cache uses void pointers, so you need to cast the returned pointer
	// back to what it was.
	printf("Found cache entry:\n%s\n", (char *) res->ptr);
}

// Remove the cache entry: after this call, 'str' is pointing to freed memory,
// since CACHE_FLAG_AUTOFREE was set.
cache_remove(find_string("cache_test"));
 * @endcode */

#include <global.h>

/** Array of the cached entries. */
static cache_struct *cache = NULL;
/** Number of entries in ::cache. */
static size_t num_cache = 0;

/**
 * Comparison function for binary search in cache_find(). */
static int cache_compare(const void *one, const void *two)
{
	cache_struct *one_cache = (cache_struct *) one;
	cache_struct *two_cache = (cache_struct *) two;

	if (one == NULL)
	{
		return -1;
	}
	else if (two == NULL)
	{
		return 1;
	}

	if (one_cache->key < two_cache->key)
	{
		return -1;
	}
	else if (one_cache->key > two_cache->key)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Attempt to find a cache entry, identified by 'key'.
 * @param key Cache entry to find.
 * @return Pointer to the cache entry, NULL if there is no such entry. */
cache_struct *cache_find(shstr *key)
{
	cache_struct bkey;

	/* Sanity. */
	if (!cache || !num_cache || !key)
	{
		return NULL;
	}

	/* Attempt to find the cache entry. */
	bkey.key = key;
	return bsearch((void *) &bkey, cache, num_cache, sizeof(cache_struct), cache_compare);
}

/**
 * Add an entry to the cache.
 * @param key Unique identified for the cache entry.
 * @param ptr Pointer to store; must not be freed.
 * @param flags A combination of @ref CACHE_FLAG_xxx.
 * @return 1 on success, 0 on failure (NULL ptr, or cache entry with name
 * 'key' already exists). */
int cache_add(const char *key, void *ptr, uint32 flags)
{
	size_t i, ii;
	shstr *sh_key = add_string(key);

	/* Sanity. */
	if (!ptr || cache_find(sh_key))
	{
		return 0;
	}

	/* Increase the array's size. */
	cache = realloc(cache, sizeof(cache_struct) * (num_cache + 1));

	/* Now, insert the cache into the correct spot in the array. */
	for (i = 0; i < num_cache; i++)
	{
		if (cache[i].key > sh_key)
		{
			break;
		}
	}

	/* If this is not the special case of insertion at the last point, then shift everything. */
	for (ii = num_cache; ii > i; ii--)
	{
		cache[ii] = cache[ii - 1];
		/* Increase the ID, as it's getting moved upwards. */
		cache[ii].id++;
	}

	/* Store the values. */
	cache[i].key = sh_key;
	cache[i].ptr = ptr;
	cache[i].flags = flags;
	cache[i].id = i;
	num_cache++;

	return 1;
}

/**
 * Remove a cache entry identified by 'key'.
 * @param key What cache entry to remove.
 * @return 1 on success, 0 on failure (cache entry not found). */
int cache_remove(shstr *key)
{
	cache_struct *entry = cache_find(key);
	size_t i;

	if (!entry)
	{
		return 0;
	}

	/* The entry wants global events, so send one about it being removed. */
	if (entry->flags & CACHE_FLAG_GEVENT)
	{
		trigger_global_event(GEVENT_CACHE_REMOVED, entry->ptr, (uint32 *) &entry->flags);
	}

	/* Does it want to be freed automatically? */
	if (entry->flags & CACHE_FLAG_AUTOFREE)
	{
		free(entry->ptr);
	}

	/* Shift the entries. */
	for (i = entry->id + 1; i < num_cache; i++)
	{
		cache[i - 1] = cache[i];
		/* Moving downwards, decrease ID. */
		cache[i - 1].id--;
	}

	/* Decrease the array's size. */
	cache = realloc(cache, sizeof(cache_struct) * (num_cache - 1));
	num_cache--;

	return 1;
}

/**
 * Remove all cache entries. */
void cache_remove_all()
{
	/* Keep removing until there's nothing left. */
	while (num_cache)
	{
		cache_remove(cache[0].key);
	}
}

/**
 * Remove all cache entries identified by (a combination of) 'flags'.
 * @param flags One or a combination of @ref CACHE_FLAG_xxx. */
void cache_remove_by_flags(uint32 flags)
{
	size_t i;

	/* Search for matching entries, and remove them. */
	for (i = 0; i < num_cache; i++)
	{
		if (cache[i].flags & flags)
		{
			cache_remove(cache[i].key);
			i--;
		}
	}
}

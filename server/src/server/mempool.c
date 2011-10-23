/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Basic pooling memory management system.
 *
 * The mempool system never frees memory back to the system, but is
 * extremely efficient when it comes to allocating and returning pool
 * chunks.
 *
 * Always use the get_poolchunk() and return_poolchunk() functions for
 * getting and returning memory chunks. expand_mempool() is used
 * internally.
 *
 * Be careful if you want to use the internal chunk or pool data, its
 * semantics and format might change in the future.
 *
 *
 * The Life Cycle of an Object:
 *
 * - <b>expand_mempool()</b>: Allocated from system memory and put into
 *   the freelist of the object pool.
 * - <b>get_object()</b>: Removed from freelist & put into removedlist (
 *   since it is not inserted anywhere yet).
 * - <b>insert_ob_in_(map/ob)()</b>: Filled with data and inserted into
 *   (any) environment
 * - <b>...</b> end of timestep
 * - <b>object_gc()</b>: Removed from removedlist, but not freed (since it sits in
 *   an env).
 * - <b>...</b>
 * - <b>remove_ob()</b>: Removed from environment
 * - Sits in removedlist until the end of this server timestep
 * - <b>...</b> end of timestep
 * - <b>object_gc()</b>: Freed and moved to freelist
 *
 * attrsets are freed and given back to their respective pools too. */

#include <global.h>

#ifdef MEMPOOL_OBJECT_TRACKING
/* for debugging only! */
static struct mempool_chunk *used_object_list = NULL;
static uint32 chunk_tracking_id = 1;
#define MEMPOOL_OBJECT_FLAG_FREE 1
#define MEMPOOL_OBJECT_FLAG_USED 2
#endif

/**
 * The removedlist is not ended by NULL, but by a pointer to the end_marker.
 *
 * Only used as an end marker for the lists */
struct mempool_chunk end_marker;

int nrof_mempools = 0;
struct mempool *mempools[MAX_NROF_MEMPOOLS];

#ifdef MEMPOOL_TRACKING
struct mempool *pool_puddle;
#endif

struct mempool *pool_object, *pool_objectlink, *pool_player, *pool_bans, *pool_parties;

/**
 * Return the exponent exp needed to round n up to the nearest power of two, so that
 * (1 << exp) >= n and (1 << (exp -1)) \< n */
uint32 nearest_pow_two_exp(uint32 n)
{
	static const uint32 exp_lookup[65] =
	{
		0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
	};
	uint32 i;

	if (n <= 64)
	{
		return exp_lookup[n];
	}

	for (i = 7; (uint32) (1 << i) < n; i++)
	{
	}

	return i;
}

/**
 * A tiny little function to set up the constructors/destructors to
 * functions that may reside outside the library.
 * @param pool Pool.
 * @param constructor Constructor function.
 * @param destructor Destructor function. */
void setup_poolfunctions(struct mempool *pool, chunk_constructor constructor, chunk_destructor destructor)
{
	pool->constructor = constructor;
	pool->destructor = destructor;
}

/**
 * Create a memory pool.
 * @param description
 * @param expand
 * @param size
 * @param flags
 * @param initialisator
 * @param deinitialisator
 * @param constructor
 * @param destructor
 * @return The created memory pool. */
struct mempool *create_mempool(const char *description, uint32 expand, uint32 size, uint32 flags, chunk_initialisator initialisator, chunk_deinitialisator deinitialisator, chunk_constructor constructor, chunk_destructor destructor)
{
	int i;
	struct mempool *pool;

	if (nrof_mempools >= MAX_NROF_MEMPOOLS)
	{
		LOG(llevError, "Too many memory pools registered. Please increase the MAX_NROF_MEMPOOLS constant in mempools.h\n");
	}

	pool = calloc(1, sizeof(struct mempool));

	mempools[nrof_mempools] = pool;

	pool->chunk_description = description;
	pool->expand_size = expand;
	pool->chunksize = size;
	pool->flags = flags;
	pool->initialisator = initialisator;
	pool->deinitialisator = deinitialisator;
	pool->constructor = constructor;
	pool->destructor = destructor;

#if MEMORY_DEBUG
	pool->flags |= MEMPOOL_BYPASS_POOLS;
#endif

	for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++)
	{
		pool->freelist[i] = &end_marker;
		pool->nrof_free[i] = 0;
		pool->nrof_allocated[i] = 0;
	}

#ifdef MEMPOOL_TRACKING
	pool->first_puddle_info = NULL;
#endif

	nrof_mempools++;

	return pool;
}

/**
 * Initialize the mempools lists and related data structures. */
void init_mempools(void)
{
#ifdef MEMPOOL_TRACKING
	pool_puddle = create_mempool("puddles", 10, sizeof(struct puddle_info), MEMPOOL_ALLOW_FREEING, NULL, NULL, NULL, NULL);
#endif
	pool_object = create_mempool("objects", OBJECT_EXPAND, sizeof(object), 0, NULL, NULL, (chunk_constructor) initialize_object, (chunk_destructor) destroy_object);
	pool_player = create_mempool("players", 25, sizeof(player), MEMPOOL_BYPASS_POOLS, NULL, NULL, NULL, NULL);
	pool_objectlink = create_mempool("object links", 500, sizeof(objectlink), 0, NULL, NULL, NULL, NULL);
	pool_bans = create_mempool("bans", 25, sizeof(_ban_struct), 0, NULL, NULL, NULL, NULL);
	pool_parties = create_mempool("parties", 25, sizeof(party_struct), 0, NULL, NULL, NULL, NULL);

	/* Initialize end-of-list pointers and a few other values*/
	removed_objects = &end_marker;

	/* Set up container for "loose" objects */
	initialize_object(&void_container);
	void_container.type = VOID_CONTAINER;
	FREE_AND_COPY_HASH(void_container.name, "<void container>");
}

/**
 * Free a mempool.
 * @param pool The mempool to free. */
static void free_mempool(struct mempool *pool)
{
	free(pool);
}

/**
 * Free all the mempools previously initialized by init_mempools(). */
void free_mempools(void)
{
	LOG(llevDebug, "Freeing memory pools.\n");
#ifdef MEMPOOL_TRACKING
	free_mempool(pool_puddle);
#endif
	free_mempool(pool_object);
	free_mempool(pool_player);
	free_mempool(pool_objectlink);
	free_mempool(pool_bans);
	free_mempool(pool_parties);

	FREE_AND_CLEAR_HASH2(void_container.name);
}

/**
 * Expands the memory pool with MEMPOOL_EXPAND new chunks. All new chunks
 * are put into the pool's freelist for future use.
 * expand_mempool is only meant to be used from get_poolchunk().
 * @param pool Pool to expand.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8) */
static void expand_mempool(struct mempool *pool, uint32 arraysize_exp)
{
	uint32 i;
	struct mempool_chunk *first, *ptr;
	int chunksize_real, nrof_arrays;

	if (pool->nrof_free[arraysize_exp] > 0)
	{
		LOG(llevBug, "expand_mempool() called with chunks still available in pool\n");
	}

	nrof_arrays = pool->expand_size >> arraysize_exp;

	if (nrof_arrays == 0)
	{
		LOG(llevDebug, "expand_mempool() called with too big array size for its expand_size\n");
		nrof_arrays = 1;
	}

	chunksize_real = sizeof(struct mempool_chunk) + (pool->chunksize << arraysize_exp);
	first = (struct mempool_chunk *) calloc(1, nrof_arrays * chunksize_real);

	if (first == NULL)
	{
		LOG(llevError, "expand_mempool(): Out of memory.\n");
	}

	pool->freelist[arraysize_exp] = first;
	pool->nrof_allocated[arraysize_exp] += nrof_arrays;
	pool->nrof_free[arraysize_exp] = nrof_arrays;

	/* Set up the linked list */
	ptr = first;

	for (i = 0; (int) i < nrof_arrays - 1; i++)
	{
#ifdef DEBUG_MEMPOOL_OBJECT_TRACKING
		ptr->obj_next = ptr->obj_prev = 0;
		ptr->pool = pool;
		/* This is a real, unique object ID. Allows tracking beyond
		 * get/free objects */
		ptr->id = chunk_tracking_id++;
		ptr->flags |= MEMPOOL_OBJECT_FLAG_FREE;
#endif
		if (pool->initialisator)
		{
			pool->initialisator(MEM_USERDATA(ptr));
		}

		ptr = ptr->next = (struct mempool_chunk *) (((char *) ptr) + chunksize_real);
	}

	/* And the last element */
	ptr->next = &end_marker;

	if (pool->initialisator)
	{
		pool->initialisator(MEM_USERDATA(ptr));
	}

#ifdef DEBUG_MEMPOOL_OBJECT_TRACKING
	ptr->obj_next = ptr->obj_prev = 0; /* secure */
	ptr->pool = pool;
	/* This is a real, unique object ID. Allows tracking beyond get/free
	 * objects */
	ptr->id = chunk_tracking_id++;
	ptr->flags |= MEMPOOL_OBJECT_FLAG_FREE;
#endif

#ifdef MEMPOOL_TRACKING
	/* Track the allocation of puddles */
	{
		struct puddle_info *p = get_poolchunk(pool_puddle);
		p->first_chunk = first;
		p->next = pool->first_puddle_info;
		pool->first_puddle_info = p;
	}
#endif
}

/**
 * Get a chunk from the selected pool. The pool will be expanded if
 * necessary.
 * @param pool
 * @param arraysize_exp
 * @return  */
void *get_poolchunk_array_real(struct mempool *pool, uint32 arraysize_exp)
{
	struct mempool_chunk *new_obj;

	if (pool->flags & MEMPOOL_BYPASS_POOLS)
	{
		new_obj = calloc(1, sizeof(struct mempool_chunk) + (pool->chunksize << arraysize_exp));
		pool->nrof_allocated[arraysize_exp]++;
	}
	else
	{
		if (pool->nrof_free[arraysize_exp] == 0)
		{
			expand_mempool(pool, arraysize_exp);
		}

		new_obj = pool->freelist[arraysize_exp];
		pool->freelist[arraysize_exp] = new_obj->next;
		pool->nrof_free[arraysize_exp]--;
	}

	new_obj->next = NULL;

	if (pool->constructor)
	{
		pool->constructor(MEM_USERDATA(new_obj));
	}

#ifdef DEBUG_MEMPOOL_OBJECT_TRACKING
	if (new_obj->obj_prev || new_obj->obj_next)
	{
		LOG(llevDebug, "get_poolchunk_array_real() object >%d< is in used_object list!!\n", new_obj->id);
	}

	/* Put it in front of the used object list */
	new_obj->obj_next = used_object_list;

	if (new_obj->obj_next)
	{
		new_obj->obj_next->obj_prev = new_obj;
	}

	used_object_list = new_obj;
	new_obj->flags &= ~MEMPOOL_OBJECT_FLAG_FREE;
	new_obj->flags |= MEMPOOL_OBJECT_FLAG_USED;
#endif

	return MEM_USERDATA(new_obj);
}

/**
 * Return a chunk to the selected pool. Don't return memory to the wrong
 * pool!
 *
 * Returned memory will be reused, so be careful about those stale pointers
 * @param data
 * @param arraysize_exp
 * @param pool  */
void return_poolchunk_array_real(void *data, uint32 arraysize_exp, struct mempool *pool)
{
	struct mempool_chunk *old = MEM_POOLDATA(data);

	if (CHUNK_FREE(data))
	{
		LOG(llevBug, "return_poolchunk_array_real() on already free chunk (pool \"%s\")\n", pool->chunk_description);
		return;
	}

#ifdef DEBUG_MEMPOOL_OBJECT_TRACKING
	if (old->obj_next)
	{
		old->obj_next->obj_prev = old->obj_prev;
	}

	if (old->obj_prev)
	{
		old->obj_prev->obj_next = old->obj_next;
	}
	else
	{
		used_object_list = old->obj_next;
	}

	old->obj_next = old->obj_prev = 0;
	old->flags &= ~MEMPOOL_OBJECT_FLAG_USED;
	old->flags |= MEMPOOL_OBJECT_FLAG_FREE;
#endif

	if (pool->destructor)
	{
		pool->destructor(data);
	}

	if (pool->flags & MEMPOOL_BYPASS_POOLS)
	{
		if (pool->deinitialisator)
		{
			pool->deinitialisator(MEM_USERDATA(old));
		}

		free(old);
		pool->nrof_allocated[arraysize_exp]--;
	}
	else
	{
		old->next = pool->freelist[arraysize_exp];
		pool->freelist[arraysize_exp] = old;
		pool->nrof_free[arraysize_exp]++;
	}
}

/**
 * Gather mempool statistics and write details to the log and the given
 * player.
 * @param op To send detailed info to (optional).
 * @param[out] sum_used Total number of bytes actively in use from
 * mempools.
 * @param[out] sum_alloc Total number of bytes allocated by the mempool
 * system. */
void dump_mempool_statistics(object *op, int *sum_used, int *sum_alloc)
{
	int j, k;
	char buf[MAX_BUF];

	for (j = 0; j < nrof_mempools; j++)
	{
		for (k = 0; k < MEMPOOL_NROF_FREELISTS; k++)
		{
			if (mempools[j]->nrof_allocated[k] > 0)
			{
				int ob_used = mempools[j]->nrof_allocated[k] - mempools[j]->  nrof_free[k], ob_free = mempools[j]->nrof_free[k];
				int mem_used = ob_used * ((mempools[j]->chunksize << k) + sizeof(struct mempool_chunk));
				int mem_free = ob_free * ((mempools[j]->chunksize << k) + sizeof(struct mempool_chunk));

				snprintf(buf, sizeof(buf), "%4d used (%4d free) %s[%3d]: %d (%d)", ob_used, ob_free, mempools[j]->chunk_description, 1 << k, mem_used, mem_free);

				if (op)
				{
					draw_info(COLOR_WHITE, op, buf);
				}

				LOG(llevSystem, "%s\n", buf);

				if (sum_used)
				{
					*sum_used += mem_used;
				}

				if (sum_alloc)
				{
					*sum_alloc += mem_used + mem_free;
				}
			}
		}
	}
}

#ifdef DEBUG_MEMPOOL_OBJECT_TRACKING

/**
 * This is time consuming DEBUG only function. Mainly, it checks the
 * different memory parts and controls they are was they are - if a
 * object claims it's in an inventory we check the inventory - same for
 * map. If we have detached but not deleted a object - we will find it
 * here. */
void check_use_object_list(void)
{
	struct mempool_chunk *chunk;

	for (chunk = used_object_list; chunk; chunk = chunk->obj_next)
	{
#ifdef MEMPOOL_TRACKING
		/* ignore for now */
		if (chunk->pool == pool_puddle)
		{
		}
		else
#endif
		if (chunk->pool == pool_object)
		{
			object *tmp2, *tmp = MEM_USERDATA(chunk);

			if (QUERY_FLAG(tmp, FLAG_REMOVED))
			{
				LOG(llevDebug, "check_use_object_list(): object >%s< (%d) has removed flag set!\n", query_name(tmp), chunk->id);
			}

			/* We are on a map */
			if (tmp->map)
			{
				if (tmp->map->in_memory != MAP_IN_MEMORY)
				{
					LOG(llevDebug, "check_use_object_list(): object >%s< (%d) has invalid map! >%d<!\n", query_name(tmp), tmp->map->name ? tmp->map->name : "NONE", chunk->id);
				}
				else
				{
					for (tmp2 = GET_MAP_OB(tmp->map, tmp->x, tmp->y); tmp2; tmp2 = tmp2->above)
					{
						if (tmp2 == tmp)
						{
							goto goto_object_found;
						}
					}

					LOG(llevDebug, "check_use_object_list(): object >%s< (%d) has invalid map! >%d<!\n", query_name(tmp), tmp->map->name ? tmp->map->name : "NONE", chunk->id);
				}
			}
			else if (tmp->env)
			{
				/* Object claims to be here... Let's check it IS here */
				for (tmp2 = tmp->env->inv; tmp2; tmp2 = tmp2->below)
				{
					if (tmp2 == tmp)
					{
						goto goto_object_found;
					}
				}

				LOG(llevDebug, "check_use_object_list(): object >%s< (%d) has invalid env >%d<!\n", query_name(tmp), query_name(tmp->env), chunk->id);
			}
			/* Where are we? */
			else
			{
				LOG(llevDebug, "check_use_object_list(): object >%s< (%d) has no env/map\n", query_name(tmp), chunk->id);
			}
		}
		else if (chunk->pool == pool_player)
		{
			player *tmp = MEM_USERDATA(chunk);
		}
		else
		{
			LOG(llevDebug, "check_use_object_list(): wrong pool ID! (%s - %d)", chunk->pool->chunk_description, chunk->id);
		}

		goto_object_found:;
	}
}
#endif

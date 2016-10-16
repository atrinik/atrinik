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
 * Memory pools API.
 *
 * The mempool system never frees memory back to the system, but is extremely
 * efficient when it comes to allocating and returning pool chunks.
 *
 * Always use the get_poolchunk() and return_poolchunk() functions for getting
 * and returning memory chunks. expand_mempool() is used internally.
 *
 * Be careful if you want to use the internal chunk or pool data, its semantics
 * and format might change in the future.
 */

#include "mempool.h"
#include "string.h"

static void mempool_free(mempool_struct *pool);

/**
 * The removedlist is not ended by NULL, but by a pointer to the end_marker.
 *
 * Only used as an end marker for the lists.
 */
static mempool_chunk_struct end_marker;

/**
 * Pool for ::mempool_puddle_struct.
 */
static mempool_struct *pool_puddle;

/**
 * All the registered pools.
 */
static mempool_struct **pools;

static size_t pools_num; ///< Number of ::pools.

/**
 * If true, the API is being deinitialized.
 */
static bool deiniting;

TOOLKIT_API(DEPENDS(math), DEPENDS(memory), DEPENDS(logger), DEPENDS(string),
            DEPENDS(stringbuffer));

TOOLKIT_INIT_FUNC(mempool)
{
    pools = NULL;
    pools_num = 0;
    deiniting = false;

    pool_puddle = mempool_create("puddles", 10,
            sizeof(mempool_puddle_struct), MEMPOOL_ALLOW_FREEING,
            NULL, NULL, NULL, NULL);
}
TOOLKIT_INIT_FUNC_FINISH

TOOLKIT_DEINIT_FUNC(mempool)
{
    size_t i;

    deiniting = true;

    /* The first memory pool ever created is the puddles pool, so
     * avoid freeing it until all the other ones have been freed. */
    for (i = 1; i < pools_num; i++) {
        mempool_free(pools[i]);
    }

    mempool_free(pool_puddle);

    efree(pools);
}
TOOLKIT_DEINIT_FUNC_FINISH

/* Comparison function for sort_linked_list() */
static int
sort_puddle_by_nrof_free (void *a, void *b, void *args)
{
    mempool_puddle_struct *puddle_a, *puddle_b;

    puddle_a = a;
    puddle_b = b;

    if (puddle_a->nrof_free < puddle_b->nrof_free) {
        return -1;
    } else if (puddle_a->nrof_free > puddle_b->nrof_free) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Go through the freelists and free puddles with no used chunks.
 *
 * @return
 * Number of freed puddles.
 */
static size_t
mempool_free_puddles (mempool_struct *pool)
{
    size_t chunksize_real, nrof_arrays, i, j, freed;
    mempool_chunk_struct *last_free, *chunk;
    mempool_puddle_struct *puddle, *next_puddle;

    HARD_ASSERT(pool != NULL);

    if (pool->flags & MEMPOOL_BYPASS_POOLS) {
        return 0;
    }

    freed = 0;

    for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++) {
        chunksize_real = sizeof(mempool_chunk_struct) + (pool->chunksize << i);
        nrof_arrays = pool->expand_size >> i;

        /* Free empty puddles and setup puddle-local freelists */
        for (puddle = pool->puddlelist[i], pool->puddlelist[i] = NULL;
                puddle != NULL; puddle = next_puddle) {
            next_puddle = puddle->next;

            /* Count free chunks in puddle, and set up a local freelist */
            puddle->first_free = puddle->last_free = NULL;
            puddle->nrof_free = 0;

            for (j = 0; j < nrof_arrays; j++) {
                chunk = (mempool_chunk_struct *)
                        (((char *) puddle->first_chunk) + chunksize_real * j);

                /* Find free chunks. */
                if (CHUNK_FREE(MEM_USERDATA(chunk))) {
                    if (puddle->nrof_free == 0) {
                        puddle->first_free = chunk;
                        puddle->last_free = chunk;
                        chunk->next = NULL;
                    } else {
                        chunk->next = puddle->first_free;
                        puddle->first_free = chunk;
                    }

                    puddle->nrof_free++;
                }
            }

            /* Can we actually free this puddle? */
            if (puddle->nrof_free == nrof_arrays ||
                    (deiniting && pool == pool_puddle)) {
                /* Yup. Forget about it. */
                efree(puddle->first_chunk);

                if (!deiniting || pool != pool_puddle) {
                    mempool_return(pool_puddle, puddle);
                }

                pool->nrof_free[i] -= nrof_arrays;
                pool->nrof_allocated[i] -= nrof_arrays;
                freed++;
            } else {
                /* Nope, keep this puddle: put it back into the tracking list */
                puddle->next = pool->puddlelist[i];
                pool->puddlelist[i] = puddle;
            }
        }

        /* Sort the puddles by amount of free chunks. It will let us set up the
         * freelist so that the chunks from the fullest puddles are used first.
         * This should (hopefully) help us free some of the lesser-used puddles
         * earlier. */
        pool->puddlelist[i] = sort_linked_list(pool->puddlelist[i], 0,
                sort_puddle_by_nrof_free, NULL, NULL, NULL);

        /* Finally: restore the global freelist */
        pool->freelist[i] = &end_marker;
        last_free = &end_marker;

        for (puddle = pool->puddlelist[i]; puddle != NULL;
                puddle = puddle->next) {
            if (puddle->nrof_free > 0) {
                if (pool->freelist[i] == &end_marker) {
                    pool->freelist[i] = puddle->first_free;
                } else {
                    last_free->next = puddle->first_free;
                }

                puddle->last_free->next = &end_marker;
                last_free = puddle->last_free;
            }
        }
    }

    return freed;
}

mempool_struct *
mempool_create (const char           *description,
                size_t                expand,
                size_t                size,
                uint32_t              flags,
                chunk_initialisator   initialisator,
                chunk_deinitialisator deinitialisator,
                chunk_constructor     constructor,
                chunk_destructor      destructor)
{
    size_t i;
    mempool_struct *pool;

    TOOLKIT_PROTECT();

    pool = ecalloc(1, sizeof(*pool));

    pool->chunk_description = description;
    pool->expand_size = expand;
    pool->chunksize = size;
    pool->flags = flags;
    pool->initialisator = initialisator;
    pool->deinitialisator = deinitialisator;
    pool->constructor = constructor;
    pool->destructor = destructor;

    for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++) {
        pool->freelist[i] = &end_marker;
        pool->nrof_free[i] = 0;
        pool->nrof_allocated[i] = 0;
    }

    pools = erealloc(pools, sizeof(*pools) * (pools_num + 1));
    pools[pools_num] = pool;
    pools_num++;

    return pool;
}

/**
 * Construct debug information about leaked chunks in the specified pool.
 *
 * @param pool
 * Memory pool.
 * @param sb
 * StringBuffer instance to store the information in.
 */
static void
mempool_leak_info(mempool_struct *pool, StringBuffer *sb)
{
    size_t chunksize_real, nrof_arrays, i, j;
    char buf[HUGE_BUF];
    mempool_puddle_struct *puddle;
    mempool_chunk_struct *chunk;
    void *data;

    HARD_ASSERT(pool != NULL);
    HARD_ASSERT(sb != NULL);

#ifndef NDEBUG
    if (pool->debugger == NULL) {
#else
    {
#endif
        snprintf(VS(buf), "no debug information available");
    }

    for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++) {
        chunksize_real = sizeof(mempool_chunk_struct) + (pool->chunksize << i);
        nrof_arrays = pool->expand_size >> i;

        for (puddle = pool->puddlelist[i]; puddle != NULL;
                puddle = puddle->next) {
            for (j = 0; j < nrof_arrays; j++) {
                chunk = (mempool_chunk_struct *)
                        (((char *) puddle->first_chunk) + chunksize_real * j);
                data = MEM_USERDATA(chunk);

                /* Find free chunks. */
                if (CHUNK_FREE(data)) {
                    continue;
                }

                if (!deiniting) {
#ifndef NDEBUG
                    if (pool->validator == NULL || pool->validator(data)) {
                        continue;
                    }
#else
                    continue;
#endif
                } else if (pool == pool_puddle) {
                    continue;
                }

#ifndef NDEBUG
                if (pool->debugger != NULL) {
                    pool->debugger(data, VS(buf));
                }
#endif

                stringbuffer_append_printf(sb, "Chunk %p (%p) in pool %s has "
                        "not been freed: %s\n", chunk, data,
                        pool->chunk_description, buf);
            }
        }
    }
}

/**
 * Free a mempool.
 *
 * @param pool
 * The mempool to free.
 */
static void
mempool_free (mempool_struct *pool)
{
    StringBuffer *sb;
    char *info, *cp;

    TOOLKIT_PROTECT();

    HARD_ASSERT(pool != NULL);

    sb = stringbuffer_new();
    mempool_leak_info(pool, sb);
    info = stringbuffer_finish(sb);

    cp = strtok(info, "\n");

    while (cp != NULL) {
        LOG(ERROR, "%s", cp);
        cp = strtok(NULL, "\n");
    }

    efree(info);

    mempool_free_puddles(pool);

    efree(pool);
}

void
mempool_set_debugger (mempool_struct *pool, chunk_debugger debugger)
{
#ifndef NDEBUG
    pool->debugger = debugger;
#endif
}

void
mempool_set_validator (mempool_struct *pool, chunk_validator validator)
{
#ifndef NDEBUG
    pool->validator = validator;
#endif
}

void
mempool_stats (const char *name, char *buf, size_t size)
{
    size_t i, j, allocated;

    HARD_ASSERT(buf != NULL);
    HARD_ASSERT(size != 0);

    snprintfcat(buf, size, "\n=== MEMPOOL ===");

    if (string_isempty(name)) {
        snprintfcat(buf, size, "\nRegistered pools: %"PRIu64,
                (uint64_t) pools_num);
    }

    for (i = 0; i < pools_num; i++) {
        if (!string_isempty(name) &&
                strcasecmp(pools[i]->chunk_description, name) != 0) {
            continue;
        }

        snprintfcat(buf, size,
                "\n\nMemory pool: %s"
                "\n - Expand size: %"PRIu64
                "\n - Chunk size: %"PRIu64,
                pools[i]->chunk_description, (uint64_t) pools[i]->expand_size,
                (uint64_t) pools[i]->chunksize);

        allocated = 0;

        for (j = 0; j < MEMPOOL_NROF_FREELISTS; j++) {
            if (pools[i]->nrof_allocated[j] == 0) {
                continue;
            }

            snprintfcat(buf, size, "\nFreelist #%"PRIu64":", (uint64_t) j);
            snprintfcat(buf, size, " allocated: %s",
                    string_format_number_comma(pools[i]->nrof_allocated[j]));
            snprintfcat(buf, size, " free: %s",
                    string_format_number_comma(pools[i]->nrof_free[j]));

            allocated += pools[i]->nrof_allocated[j] *
                    ((pools[i]->chunksize << j) + sizeof(mempool_chunk_struct));
        }

        snprintfcat(buf, size, "\nCalls:");
        snprintfcat(buf, size, " %s expansions",
                string_format_number_comma(pools[i]->calls_expand));
        snprintfcat(buf, size, " %s gets",
                string_format_number_comma(pools[i]->calls_get));
        snprintfcat(buf, size, " %s returns",
                string_format_number_comma(pools[i]->calls_return));
        snprintfcat(buf, size, "\nTotal allocated memory: %s bytes",
                string_format_number_comma(allocated));

        if (!string_isempty(name)) {
            break;
        }
    }

    if (!string_isempty(name) && i == pools_num) {
        snprintfcat(buf, size, "\nNo such pool: %s", name);
    }

    snprintfcat(buf, size, "\n");
}

mempool_struct *
mempool_find (const char *name)
{
    size_t i;

    HARD_ASSERT(name != NULL);

    for (i = 0; i < pools_num; i++) {
        if (strcasecmp(pools[i]->chunk_description, name) == 0) {
            return pools[i];
        }
    }

    return NULL;
}

/**
 * Expands the memory pool based on its settings. All new chunks are put into
 * the pool's freelist for future use.
 *
 * @param pool
 * Pool to expand.
 * @param arraysize_exp
 * The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 */
static void
mempool_expand (mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *first, *chunk;
    size_t chunksize_real, nrof_arrays, i;
    mempool_puddle_struct *p;

    HARD_ASSERT(pool != NULL);
    HARD_ASSERT(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    HARD_ASSERT(pool->nrof_free[arraysize_exp] == 0);

    pool->calls_expand++;

    nrof_arrays = pool->expand_size >> arraysize_exp;

    if (nrof_arrays == 0) {
        LOG(ERROR, "Called with too large array size exponent: %"PRIu64,
                (uint64_t) arraysize_exp);
        nrof_arrays = 1;
    }

    chunksize_real = sizeof(mempool_chunk_struct) +
            (pool->chunksize << arraysize_exp);
    first = ecalloc(1, nrof_arrays * chunksize_real);

    pool->freelist[arraysize_exp] = first;
    pool->nrof_allocated[arraysize_exp] += nrof_arrays;
    pool->nrof_free[arraysize_exp] = nrof_arrays;

    /* Set up the linked list */
    chunk = first;

    for (i = 0; i < nrof_arrays; i++) {
        if (pool->initialisator) {
            pool->initialisator(MEM_USERDATA(chunk));
        }

        if (i < nrof_arrays - 1) {
            chunk = chunk->next = (mempool_chunk_struct *) (((char *) chunk) +
                    chunksize_real);
        } else {
            chunk->next = &end_marker;
        }
    }

    if (pool != pool_puddle || 1) {
        p = mempool_get(pool_puddle);
        p->first_chunk = first;
        p->next = pool->puddlelist[arraysize_exp];
        pool->puddlelist[arraysize_exp] = p;
    }
}

void *
mempool_get_chunk (mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *new_obj;

    TOOLKIT_PROTECT();

    HARD_ASSERT(pool != NULL);
    HARD_ASSERT(arraysize_exp < MEMPOOL_NROF_FREELISTS);

    pool->calls_get++;

    if (pool->flags & MEMPOOL_BYPASS_POOLS) {
        new_obj = ecalloc(1, sizeof(mempool_chunk_struct) +
                (pool->chunksize << arraysize_exp));
        pool->nrof_allocated[arraysize_exp]++;
    } else {
        if (pool->nrof_free[arraysize_exp] == 0) {
            mempool_expand(pool, arraysize_exp);
        } else {
            memset(MEM_USERDATA(pool->freelist[arraysize_exp]), 0,
                    pool->chunksize << arraysize_exp);
        }

        new_obj = pool->freelist[arraysize_exp];
        pool->freelist[arraysize_exp] = new_obj->next;
        pool->nrof_free[arraysize_exp]--;
    }

    new_obj->next = NULL;

    if (pool->constructor) {
        pool->constructor(MEM_USERDATA(new_obj));
    }

    return MEM_USERDATA(new_obj);
}

void
mempool_return_chunk (mempool_struct *pool,
                      size_t          arraysize_exp,
                      void           *data)
{
    mempool_chunk_struct *chunk;

    TOOLKIT_PROTECT();

    HARD_ASSERT(pool != NULL);
    HARD_ASSERT(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    HARD_ASSERT(data != NULL);

    pool->calls_return++;

    chunk = MEM_POOLDATA(data);

    if (CHUNK_FREE(data)) {
        LOG(ERROR, "Chunk %p in pool '%s' has already been freed.", chunk,
                pool->chunk_description);
        abort();
    }

    if (pool->destructor) {
        pool->destructor(data);
    }

    if (pool->flags & MEMPOOL_BYPASS_POOLS) {
        if (pool->deinitialisator) {
            pool->deinitialisator(MEM_USERDATA(chunk));
        }

        efree(chunk);
        pool->nrof_allocated[arraysize_exp]--;
    } else {
        chunk->next = pool->freelist[arraysize_exp];
        pool->freelist[arraysize_exp] = chunk;
        pool->nrof_free[arraysize_exp]++;
    }
}

size_t
mempool_reclaim (mempool_struct *pool)
{
    HARD_ASSERT(pool != NULL);

    if (!(pool->flags & MEMPOOL_ALLOW_FREEING)) {
        return 0;
    }

    return mempool_free_puddles(pool);
}

void
mempool_leak_info_all (StringBuffer *sb)
{
    size_t i;

    HARD_ASSERT(sb != NULL);

    for (i = 1; i < pools_num; i++) {
        mempool_leak_info(pools[i], sb);
    }
}

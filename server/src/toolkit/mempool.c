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

#include <global.h>

#define API_NAME mempool ///< Name of the API.
static uint8 did_init = 0; ///< Whether the API has been initialized.

/**
 * The removedlist is not ended by NULL, but by a pointer to the end_marker.
 *
 * Only used as an end marker for the lists.
 */
static mempool_chunk_struct end_marker;

/**
 * Pool for ::mempool_puddle_struct.
 */
mempool_struct *pool_puddle;

/**
 * Initialize the mempool API.
 * @internal */
void toolkit_mempool_init(void)
{

    TOOLKIT_INIT_FUNC_START(mempool)
    {
        toolkit_import(math);
        toolkit_import(memory);
        toolkit_import(logger);
        toolkit_import(string);

        pool_puddle = mempool_create("puddles", 10,
                sizeof(mempool_puddle_struct), MEMPOOL_ALLOW_FREEING,
                NULL, NULL, NULL, NULL);
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the mempool API.
 * @internal */
void toolkit_mempool_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(mempool)
    {
        mempool_free(pool_puddle);
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/* Comparison function for sort_linked_list() */
static int sort_puddle_by_nrof_free(void *a, void *b, void *args)
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
 * @return Number of freed puddles.
 */
static size_t mempool_free_puddles(mempool_struct *pool)
{
    size_t chunksize_real, nrof_arrays, i, j, freed;
    mempool_chunk_struct *last_free, *chunk;
    mempool_puddle_struct *puddle, *next_puddle;

    assert(pool != NULL);

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
            if (puddle->nrof_free == nrof_arrays) {
                /* Yup. Forget about it. */
                free(puddle->first_chunk);
                mempool_return(pool_puddle, puddle);
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

/**
 * Create a memory pool.
 * @param description @copydoc mempool_struct::chunk_description
 * @param expand @copydoc mempool_struct::expand_size
 * @param size @copydoc mempool_struct::chunk_size
 * @param flags @copydoc mempool_struct::flags
 * @param initialisator @copydoc mempool_struct::initialisator
 * @param deinitialisator @copydoc mempool_struct::deinitialisator
 * @param constructor @copydoc mempool_struct::constructor
 * @param destructor @copydoc mempool_struct::destructor
 * @return The created memory pool.
 */
mempool_struct *mempool_create(const char *description, size_t expand,
        size_t size, uint32 flags, chunk_initialisator initialisator,
        chunk_deinitialisator deinitialisator, chunk_constructor constructor,
        chunk_destructor destructor)
{
    size_t i;
    mempool_struct *pool;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

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

    return pool;
}

/**
 * Free a mempool.
 * @param pool The mempool to free.
 */
void mempool_free(mempool_struct *pool)
{
    size_t chunksize_real, nrof_arrays, i, j;
    char buf[HUGE_BUF];
    mempool_puddle_struct *puddle;
    mempool_chunk_struct *chunk;
    void *data;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    assert(pool != NULL);

    if (pool->debugger == NULL) {
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

                if (pool->debugger != NULL) {
                    pool->debugger(data, VS(buf));
                }

                log(LOG(ERROR), "Chunk %p (%p) in pool %s has not been freed: "
                        "%s", chunk, data, pool->chunk_description, buf);
            }
        }
    }

    mempool_free_puddles(pool);

    efree(pool);
}

/**
 * Set the mempool's debugging function.
 * @param pool Memory pool.
 * @param debugger Debugging function to use.
 */
void mempool_set_debugger(mempool_struct *pool, chunk_debugger debugger)
{
#ifndef NDEBUG
    pool->debugger = debugger;
#endif
}

/**
 * Acquire detailed statistics about the specified memory pool.
 * @param pool Memory pool.
 * @param[out] buf Buffer to use for writing.
 * @param size Size of 'buf'.
 */
void mempool_stats(mempool_struct *pool, char *buf, size_t size)
{
    size_t i, allocated;

    assert(pool != NULL);
    assert(buf != NULL);
    assert(size != 0);

    snprintf(buf, size,
            "\nMemory pool: %s"
            "\n - Expand size: %"FMT64U
            "\n - Chunk size: %"FMT64U,
            pool->chunk_description, (uint64) pool->expand_size,
            (uint64) pool->chunksize);

    allocated = 0;

    for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++) {
        if (pool->nrof_allocated[i] == 0) {
            continue;
        }

        snprintfcat(buf, size, "\nFreelist #%"FMT64U":", (uint64) i);
        snprintfcat(buf, size, " allocated: %s",
                string_format_number_comma(pool->nrof_allocated[i]));
        snprintfcat(buf, size, " free: %s",
                string_format_number_comma(pool->nrof_free[i]));

        allocated += pool->nrof_allocated[i] * ((pool->chunksize << i) +
                sizeof(mempool_chunk_struct));
    }

    snprintfcat(buf, size, "\nCalls:");
    snprintfcat(buf, size, " %s expansions",
            string_format_number_comma(pool->calls_expand));
    snprintfcat(buf, size, " %s gets",
            string_format_number_comma(pool->calls_get));
    snprintfcat(buf, size, " %s returns",
            string_format_number_comma(pool->calls_return));
    snprintfcat(buf, size, "\nTotal allocated memory: %s bytes",
            string_format_number_comma(allocated));
}

/**
 * Expands the memory pool based on its settings. All new chunks are put into
 * the pool's freelist for future use.
 * @param pool Pool to expand.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 */
static void mempool_expand(mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *first, *chunk;
    size_t chunksize_real, nrof_arrays, i;
    mempool_puddle_struct *p;

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    assert(pool->nrof_free[arraysize_exp] == 0);

    pool->calls_expand++;

    nrof_arrays = pool->expand_size >> arraysize_exp;

    if (nrof_arrays == 0) {
        log(LOG(ERROR), "Called with too large array size exponent: %"FMT64U,
                (uint64) arraysize_exp);
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

    if (pool != pool_puddle) {
        p = mempool_get(pool_puddle);
        p->first_chunk = first;
        p->next = pool->puddlelist[arraysize_exp];
        pool->puddlelist[arraysize_exp] = p;
    }
}

/**
 * Get a chunk from the selected pool. The pool will be expanded if
 * necessary.
 * @param pool Pool to get the chunk from.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 * @return Acquired memory chunk, guaranteed to be zero-filled.
 */
void *mempool_get_chunk(mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *new_obj;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);

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

/**
 * Return a chunk to the selected pool.
 * @warning Don't ever return memory to the wrong pool.
 * @warning Returned memory will be reused, so be careful about stale pointers.
 * @param pool Memory pool to return to.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 * @param data Data to return.
 */
void mempool_return_chunk(mempool_struct *pool, size_t arraysize_exp,
        void *data)
{
    mempool_chunk_struct *chunk;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    assert(data != NULL);

    pool->calls_return++;

    chunk = MEM_POOLDATA(data);

    if (CHUNK_FREE(data)) {
        log(LOG(ERROR), "Chunk %p in pool '%s' has already been freed.", chunk,
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

/**
 * Attempt to reclaim no longer used memory allocated by the specified pool.
 * @param pool
 * @return Number of reclaimed puddles.
 */
size_t mempool_reclaim(mempool_struct *pool)
{
    assert(pool != NULL);

    if (!(pool->flags & MEMPOOL_ALLOW_FREEING)) {
        return 0;
    }

    return mempool_free_puddles(pool);
}

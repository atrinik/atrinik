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
 * Used by nearest_pow_two_exp() for a fast lookup.
 */
static const size_t exp_lookup[65] = {
    0, 0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
};

/**
 * Initialize the mempool API.
 * @internal */
void toolkit_mempool_init(void)
{

    TOOLKIT_INIT_FUNC_START(mempool)
    {
        toolkit_import(memory);
        toolkit_import(string);
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
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Return the exponent exp needed to round n up to the nearest power of two, so
 * that (1 << exp) >= n and (1 << (exp - 1)) \< n
 */
size_t nearest_pow_two_exp(size_t n)
{
    size_t i;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (n <= 64) {
        return exp_lookup[n];
    }

    for (i = 7; (1U << i) < n; i++) {
    }

    return i;
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

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

#ifndef NDEBUG
    {
        size_t i;
        void *data;
        char buf[HUGE_BUF];

        if (pool->debugger == NULL) {
            snprintf(VS(buf), "no debug information available");
        }

        for (i = 0; i < pool->chunks_num; i++) {
            data = MEM_USERDATA(pool->chunks[i]);

            if (CHUNK_FREE(data)) {
                continue;
            }

            if (pool->debugger != NULL) {
                pool->debugger(data, VS(buf));
            }

            log(LOG(ERROR), "Chunk #%"FMT64U" %p (%p) in pool %s has not been "
                    "freed: %s", (uint64) i, pool->chunks[i], data,
                    pool->chunk_description, buf);
        }
    }
#endif

    efree(pool);
}

void mempool_set_debugger(mempool_struct *pool, chunk_debugger debugger)
{
#ifndef NDEBUG
    pool->debugger = debugger;
#endif
}

void mempool_stats(mempool_struct *pool, char *buf, size_t size)
{
    size_t i;

    assert(pool != NULL);
    assert(buf != NULL);
    assert(size != 0);

    snprintf(buf, size,
            "\nMemory pool: %s"
            "\n - Expand size: %"FMT64U
            "\n - Chunk size: %"FMT64U,
            pool->chunk_description, (uint64) pool->expand_size,
            (uint64) pool->chunksize);

    for (i = 0; i < MEMPOOL_NROF_FREELISTS; i++) {
        if (pool->nrof_allocated[i] == 0 && pool->nrof_free[i] == 0) {
            continue;
        }

        snprintfcat(buf, size, "\nFreelist #%"FMT64U":", (uint64) i);
        snprintfcat(buf, size, " allocated: %s",
                string_format_number_comma(pool->nrof_allocated[i]));
        snprintfcat(buf, size, " free: %s",
                string_format_number_comma(pool->nrof_free[i]));
    }
}

/**
 * Expands the memory pool based on its settings. All new chunks are put into
 * the pool's freelist for future use.
 * @param pool Pool to expand.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 */
static void expand_mempool(mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *first, *chunk;
    size_t chunksize_real, nrof_arrays, i;

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    assert(pool->nrof_free[arraysize_exp] == 0);

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
#ifndef NDEBUG
        if (!(pool->flags & MEMPOOL_BYPASS_POOLS)) {
            pool->chunks = erealloc(pool->chunks, sizeof(*pool->chunks) *
                    (pool->chunks_num + 1));
            pool->chunks[pool->chunks_num] = chunk;
            pool->chunks_num++;
        }
#endif

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
}

/**
 * Get a chunk from the selected pool. The pool will be expanded if
 * necessary.
 * @param pool Pool to get the chunk from.
 * @param arraysize_exp The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 * @return Acquired memory chunk, guaranteed to be zero-filled.
 */
void *get_poolchunk_array_real(mempool_struct *pool, size_t arraysize_exp)
{
    mempool_chunk_struct *new_obj;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);

    if (pool->flags & MEMPOOL_BYPASS_POOLS) {
        new_obj = ecalloc(1, sizeof(mempool_chunk_struct) +
                (pool->chunksize << arraysize_exp));
        pool->nrof_allocated[arraysize_exp]++;
    } else {
        if (pool->nrof_free[arraysize_exp] == 0) {
            expand_mempool(pool, arraysize_exp);
        } else {
            memset(MEM_USERDATA(pool->freelist[arraysize_exp]), 0,
                    pool->chunksize);
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
void return_poolchunk_array_real(mempool_struct *pool, size_t arraysize_exp,
        void *data)
{
    mempool_chunk_struct *chunk;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    assert(pool != NULL);
    assert(arraysize_exp < MEMPOOL_NROF_FREELISTS);
    assert(data != NULL);

    chunk = MEM_POOLDATA(data);

    if (CHUNK_FREE(data)) {
        log(LOG(ERROR), "Chunk %p in pool '%s' has already been freed.", chunk,
                pool->chunk_description);
        return;
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

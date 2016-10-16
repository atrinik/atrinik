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
 * Memory pools API header file.
 */

#ifndef TOOLKIT_MEMPOOL_H
#define TOOLKIT_MEMPOOL_H

#include "toolkit.h"
#include "stringbuffer_dec.h"

/**
 * Minimalistic memory management data for a single chunk of memory.
 *
 * It is up to the application to keep track of which pool it belongs
 * to.
 */
typedef struct mempool_chunk_struct {
    /* This struct must always be padded for longword alignment of the data
     * coming behind it.
     *
     * Not a problem as long as we only keep a single pointer here, but be
     * careful when adding more data. */

    /**
     * Used for the free list and the limbo list. NULL if this memory chunk has
     * been allocated and is in use.
     */
    struct mempool_chunk_struct *next;
} mempool_chunk_struct;

/**
 * Structure used to keep track of allocated puddles.
 */
typedef struct mempool_puddle_struct {
    struct mempool_puddle_struct *next; ///< Next puddle.

    mempool_chunk_struct *first_chunk; ///< First mempool chunk.

    mempool_chunk_struct *first_free; ///< Used for freeing memory.
    mempool_chunk_struct *last_free; ///< Used for freeing memory.
    uint32_t nrof_free; ///< Number of free chunks in this puddle.
} mempool_puddle_struct;

/**
 * Optional initialisator to be called when expanding.
 */
typedef void (*chunk_initialisator)(void *ptr);
/**
 * Optional deinitialisator to be called when freeing.
 */
typedef void (*chunk_deinitialisator)(void *ptr);
/**
 * Optional constructor to be called when getting chunks.
 */
typedef void (*chunk_constructor)(void *ptr);
/**
 * Optional destructor to be called when returning chunks.
 */
typedef void (*chunk_destructor)(void *ptr);
/**
 * Optional debugger to be called when debugging chunks.
 */
typedef void (*chunk_debugger)(void *ptr, char *buf, size_t size);
/**
 * Validator to be used when detecting leaks.
 */
typedef bool (*chunk_validator)(void *ptr);

/* Definitions used for array handling */
#define MEMPOOL_NROF_FREELISTS 8

/** Data for a single memory pool */
typedef struct mempool_struct {
    /**
     * Description of chunks. Mostly for debugging.
     */
    const char *chunk_description;

    /**
     * How many chunks to allocate at each expansion.
     */
    size_t expand_size;

    /**
     * Size of chunks, excluding sizeof(mempool_chunk) and padding.
     */
    size_t chunksize;

    /**
     * Special handling flags. See @ref mempool_flags
     */
    uint32_t flags;

    /**
     * First free chunk.
     */
    mempool_chunk_struct *freelist[MEMPOOL_NROF_FREELISTS];

    /**
     * Number of free.
     */
    size_t nrof_free[MEMPOOL_NROF_FREELISTS];

    /**
     * Number of allocated.
     */
    size_t nrof_allocated[MEMPOOL_NROF_FREELISTS];

    /**
     * List of puddles used for chunk tracking.
     */
    mempool_puddle_struct *puddlelist[MEMPOOL_NROF_FREELISTS];

    uint64_t calls_expand; ///< Number of calls to expand the pool.
    uint64_t calls_get; ///< Number of calls to getting a chunk from the pool.
    uint64_t calls_return; ///< Number of calls to returning a chunk to the pool.

    chunk_initialisator initialisator; ///< @copydoc chunk_initialisator
    chunk_deinitialisator deinitialisator; ///< @copydoc chunk_deinitialisator
    chunk_constructor constructor; ///< @copydoc chunk_constructor
    chunk_destructor destructor; ///< @copydoc chunk_destructor

#ifndef NDEBUG
    chunk_debugger debugger; ///< @copydoc chunk_debugger
    chunk_validator validator; ///< @copydoc chunk_validator
#endif
} mempool_struct;

/**
 * Get the memory management struct for a chunk of memory
 */
#define MEM_POOLDATA(ptr) (((mempool_chunk_struct *)(ptr)) - 1)
/**
 * Get the actual user data area from a mempool reference
 */
#define MEM_USERDATA(ptr) ((void *)(((mempool_chunk_struct *)(ptr)) + 1))
/**
 * Check that a chunk of memory is in the free (or removed for objects) list
 */
#define CHUNK_FREE(ptr) (MEM_POOLDATA(ptr)->next != NULL)

/**
 * @defgroup mempool_flags Mempool flags
 * Mempool flags.
 *@{*/

/** Don't use pooling, but only malloc/free instead */
#define MEMPOOL_BYPASS_POOLS  1
/** Allow puddles from this pool to be freed. */
#define MEMPOOL_ALLOW_FREEING 2
/*@}*/

#define mempool_get(_pool) mempool_get_chunk((_pool), 0)
#define mempool_get_array(_pool, _arraysize) \
    mempool_get_chunk((_pool), nearest_pow_two_exp(_arraysize))

#define mempool_return(_pool, _data) mempool_return_chunk((_pool), 0, (_data))
#define mempool_return_array(_pool, _arraysize, _data) \
    mempool_return_chunk((_pool), nearest_pow_two_exp(_arraysize), (_data))

/* Prototypes */

TOOLKIT_FUNCS_DECLARE(mempool);

/**
 * Create a memory pool.
 *
 * @param description
 * Text description of the memory pool. Used for debugging.
 * @param expand
 * How many chunks to allocate at each expansion.
 * @param size
 * Size of each individual chunk.
 * @param flags
 * A combination of @ref mempool_flags "memory pool flags".
 * @param initialisator
 * Function to be called when expanding.
 * @param deinitialisator
 * Function to be called when freeing.
 * @param constructor
 * Function to be called when getting chunks.
 * @param destructor
 * Function to be called when returning chunks.
 * @return
 * The created memory pool.
 */
extern mempool_struct *
mempool_create(const char           *description,
               size_t                expand,
               size_t                size,
               uint32_t              flags,
               chunk_initialisator   initialisator,
               chunk_deinitialisator deinitialisator,
               chunk_constructor     constructor,
               chunk_destructor      destructor);

/**
 * Set the mempool's debugging function.
 *
 * @param pool
 * Memory pool.
 * @param debugger
 * Debugging function to use.
 */
extern void
mempool_set_debugger(mempool_struct *pool, chunk_debugger debugger);

/**
 * Set the mempool's validator function.
 *
 * @param pool
 * Memory pool.
 * @param validator
 * Validator function to use.
 */
extern void
mempool_set_validator(mempool_struct *pool, chunk_validator validator);

/**
 * Acquire detailed statistics about the specified memory pool.
 *
 * @param name
 * Name of the memory pool, empty or NULL for all.
 * @param[out] buf Buffer to use for writing. Must end with a NUL.
 * @param size
 * Size of 'buf'.
 */
extern void
mempool_stats(const char *name, char *buf, size_t size);

/**
 * Attempt to find the specified memory pool identified by its name.
 *
 * @param name
 * Name of the memory pool to find.
 * @return
 * Memory pool if found, NULL otherwise.
 */
extern mempool_struct *
mempool_find(const char *name);

/**
 * Get a chunk from the selected pool. The pool will be expanded if
 * necessary.
 *
 * @param pool
 * Pool to get the chunk from.
 * @param arraysize_exp
 * The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 * @return
 * Acquired memory chunk, guaranteed to be zero-filled.
 */
extern void *
mempool_get_chunk(mempool_struct *pool, size_t arraysize_exp);

/**
 * Return a chunk to the selected pool.
 *
 * @param pool
 * Memory pool to return to.
 * @param arraysize_exp
 * The exponent for the array size, for example 3
 * for arrays of length 8 (2^3 = 8)
 * @param data
 * Data to return.
 * @warning Don't ever return memory to the wrong pool.
 * @warning Returned memory will be reused, so be careful about stale pointers.
 */
extern void
mempool_return_chunk(mempool_struct *pool,
                     size_t          arraysize_exp,
                     void           *data);

/**
 * Attempt to reclaim no longer used memory allocated by the specified pool.
 *
 * @param pool
 * Memory pool.
 * @return
 * Number of reclaimed puddles.
 */
extern size_t
mempool_reclaim(mempool_struct *pool);

/**
 * Gather leak information from all the registered pools into the specified
 * StringBuffer instance.
 *
 * @param sb
 * StringBuffer instance to use.
 */
extern void
mempool_leak_info_all(StringBuffer *sb);

#endif

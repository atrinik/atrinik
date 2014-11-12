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
 * Memory pools API header file. */

#ifndef MEMPOOL_H
#define MEMPOOL_H

/**
 * Minimalistic memory management data for a single chunk of memory.
 *
 * It is up to the application to keep track of which pool it belongs
 * to. */
typedef struct mempool_chunk_struct {
    /* This struct must always be padded for longword alignment of the data
     * coming behind it.
     * Not a problem as long as we only keep a single pointer here, but be
     * careful
     * if adding more data. */

    /**
     * Used for the free list and the limbo list. NULL if this
     * memory chunk has been allocated and is in use */
    struct mempool_chunk_struct *next;
} mempool_chunk_struct;

/** Optional initialisator to be called when expanding. */
typedef void (*chunk_initialisator)(void *ptr);
/** Optional deinitialisator to be called when freeing. */
typedef void (*chunk_deinitialisator)(void *ptr);
/** Optional constructor to be called when getting chunks. */
typedef void (*chunk_constructor)(void *ptr);
/** Optional destructor to be called when returning chunks. */
typedef void (*chunk_destructor)(void *ptr);

/* Definitions used for array handling */
#define MEMPOOL_NROF_FREELISTS 8
/* = 256 if NROF_FREELISTS == 8 */
#define MEMPOOL_MAX_ARRAYSIZE (1 << MEMPOOL_NROF_FREELISTS)

/** Data for a single memory pool */
typedef struct mempool_struct {
    /** Mutex protecting the mempool's data. */
    pthread_mutex_t mutex;

    /** Description of chunks. Mostly for debugging */
    const char *chunk_description;

    /** How many chunks to allocate at each expansion */
    uint32 expand_size;

    /** size of chunks, excluding sizeof(mempool_chunk) and padding */
    uint32 chunksize;

    /** Special handling flags. See definitions below */
    uint32 flags;

    /** Optional initialisator to be called when expanding */
    chunk_initialisator initialisator;

    /** Optional deinitialisator to be called when freeing */
    chunk_deinitialisator deinitialisator;

    /** Optional constructor to be called when getting chunks */
    chunk_constructor constructor;

    /** Optional destructor to be called when returning chunks */
    chunk_destructor destructor;

    /** First free chunk */
    mempool_chunk_struct *freelist[MEMPOOL_NROF_FREELISTS];

    /** Number of free. */
    uint32 nrof_free[MEMPOOL_NROF_FREELISTS];

    /** Number of allocated. */
    uint32 nrof_allocated[MEMPOOL_NROF_FREELISTS];
} mempool_struct;

/** Get the memory management struct for a chunk of memory */
#define MEM_POOLDATA(ptr) (((mempool_chunk_struct *)(ptr)) - 1)
/** Get the actual user data area from a mempool reference */
#define MEM_USERDATA(ptr) ((void *)(((mempool_chunk_struct *)(ptr)) + 1))
/** Check that a chunk of memory is in the free (or removed for objects) list */
#define CHUNK_FREE(ptr) (MEM_POOLDATA(ptr)->next != NULL)

/**
 * @defgroup mempool_flags Mempool flags
 * Mempool flags.
 *@{*/

/** Allow puddles from this pool to be freed */
#define MEMPOOL_ALLOW_FREEING 1
/** Don't use pooling, but only malloc/free instead */
#define MEMPOOL_BYPASS_POOLS  2
/*@}*/

#define get_poolchunk(_pool_) get_poolchunk_array_real((_pool_), 0)
#define get_poolarray(_pool_, _arraysize_) get_poolchunk_array_real((_pool_), nearest_pow_two_exp(_arraysize_))

#define return_poolchunk(_data_, _pool_) return_poolchunk_array_real((_data_), 0, (_pool_))
#define return_poolarray(_data_, _arraysize_, _pool_) return_poolchunk_array_real((_data_), nearest_pow_two_exp(_arraysize_), (_pool_))

#endif

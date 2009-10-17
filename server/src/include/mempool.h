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
 * Memory pooling definitions. */

#ifndef MEMPOOL_H
#define MEMPOOL_H

/** Enable tracking/freeing of mempools? */
#define MEMPOOL_TRACKING

/**
 * Enable a global list of *all* objects we have allocated. We can browse
 * them to control and debug them.
 * @warning Enabling this feature will slow down the server
 * by an <b>HUGE</b> amount and should only be done in real debug
 * runs. */
/*#define MEMPOOL_OBJECT_TRACKING*/

/**
 * Minimalistic memory management data for a single chunk of memory.
 *
 * It is (currently) up to the application to keep track of which pool
 * it belongs to. */
struct mempool_chunk
{
	/* This struct must always be padded for longword alignment of the data coming behind it.
	 * Not a problem as long as we only keep a single pointer here, but be careful
	 * if adding more data. */

	/**
	 * Used for the free list and the limbo list. NULL if this
	 * memory chunk has been allocated and is in use */
	struct mempool_chunk *next;

#ifdef MEMPOOL_OBJECT_TRACKING
	/** Previous mempool object */
	struct mempool_chunk *obj_prev;

	/** Next mempool object */
	struct mempool_chunk *obj_next;

	/** Mempool flags */
	uint32 flags;

	/** To what mpool is this memory part related? */
	uint32 pool_id;

	/** The REAL unique ID number */
	uint32 id;
#endif
};

/** Optional constructor to be called when expanding */
typedef void (* chunk_constructor) (void *ptr);

/** Optional destructor to be called when freeing */
typedef void (* chunk_destructor) (void *ptr);

/** Data for a single memory pool */
struct mempool
{
	/** First free chunk */
	struct mempool_chunk *first_free;

	/** How many chunks to allocate at each expansion */
	uint32 expand_size;

	/** Size of chunks, excluding sizeof(mempool_chunk) and padding */
	uint32 chunksize;

	/** Number of used */
	uint32 nrof_used;

	/** Number of free */
	uint32 nrof_free;

	/** Optional constructor to be called when getting chunks */
	chunk_constructor constructor;

	/** Optional destructor to be called when returning chunks */
	chunk_destructor destructor;

	/** Description of chunks. Mostly for debugging */
	char *chunk_description;

	/** Spacial handling flags. See definitions below */
	uint32 flags;

	/** First puddle info */
	struct puddle_info *first_puddle_info;
};

#ifdef MEMPOOL_TRACKING
/** Mempool information structure */
struct puddle_info
{
	/** Next puddle info */
	struct puddle_info *next;

	/** First chunk */
	struct mempool_chunk *first_chunk;

	/**
	 * Local freelist only for this puddle. Temporary used when freeing
	 * memory */
	struct mempool_chunk *first_free, *last_free;

	/** Number of free */
	uint32 nrof_free;
};
#endif

typedef enum
{
#ifdef MEMPOOL_TRACKING
	POOL_PUDDLE,
#endif
	POOL_OBJECT,
	POOL_PLAYER,
	NROF_MEMPOOLS
} mempool_id;

/* Get the memory management struct for a chunk of memory */
#define MEM_POOLDATA(ptr) (((struct mempool_chunk *)(ptr)) - 1)

/* Get the actual user data area from a mempool reference */
#define MEM_USERDATA(ptr) ((void *)(((struct mempool_chunk *)(ptr)) + 1))

/* Check that a chunk of memory is in the free (or removed for objects) list */
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

extern struct mempool mempools[];

extern struct mempool_chunk end_marker;

#endif

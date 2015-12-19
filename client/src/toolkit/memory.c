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
 * Memory API.
 *
 * @author Alex Tokar
 */

#ifndef __CPROTO__

#include <global.h>

#ifndef NDEBUG
#ifdef HAVE_VALGRIND_H
#   include <valgrind/valgrind.h>
#else
#define RUNNING_ON_VALGRIND (0)
#endif

/**
 * Whether to ensure that pointers passed to memory_efree() have actually
 * been allocated by this memory API.
 * @warning This *will* be EXTREMELY taxing.
 */
#define CHECK_CHUNKS 0

/**
 * Magic value stored before each allocated memory block.
 */
#define CHUNK_BEFORE_VAL 0x539A4C3F659C5B2EULL
/**
 * Magic value stored after each allocated memory block.
 */
#define CHUNK_AFTER_VAL  0x659C5B2E539A4C3FULL

typedef struct memory_chunk {
    struct memory_chunk *next; ///< Next memory chunk.
    struct memory_chunk *prev; ///< Previous memory chunk.

    size_t size; ///< Number of the bytes in 'data'.
    char *file; ///< File the memory chunk was allocated in.
    uint32_t line; ///< Line number the chunk was allocated in.

    uint64_t before; ///< Value before the data to check for underruns.
    char data[1]; ///< The allocated data; ie, *the* memory block.
} memory_chunk_t;

/**
 * Calculates size of a single memory chunk, taking memory block size into
 * consideration.
 *
 * @param _n
 *
 * The memory block size.
 */
#define MEM_CHUNK_SIZE(_n) \
    (sizeof(memory_chunk_t) - 1 + (_n) + sizeof(uint64_t))
/**
 * Acquire a pointer to the memory block part of the specified memory chunk.
 *
 * @param _chunk
 *
 * The memory chunk.
 */
#define MEM_DATA(_chunk) ((void *) &((_chunk)->data[0]))
/**
 * Acquire a memory chunk pointer from the specified memory block pointer. If
 * the pointer was not returned by this API, the dragons will come after you.
 *
 * @param _ptr
 *
 * Memory block pointer.
 */
#define MEM_CHUNK(_ptr) \
    ((memory_chunk_t *) ((char *) _ptr - offsetof(memory_chunk_t, data[0])))

/**
 * List of all the allocated memory chunks.
 */
static memory_chunk_t *memory_chunks;
/**
 * Lock for ::memory_chunks.
 */
static pthread_mutex_t memory_chunks_mutex;
/**
 * List of all the freed chunks. This is periodically reclaimed, and allows
 * for checking the pointers for modifications after free().
 */
static memory_chunk_t *memory_chunks_freed;
/**
 * Lock for ::memory_chunks_freed.
 */
static pthread_mutex_t memory_chunks_freed_mutex;
/**
 * Number of currently allocated memory chunks.
 */
static ssize_t memory_chunks_num;
/**
 * Maximum number of chunks ever allocated at any one time.
 */
static ssize_t memory_chunks_num_max;
/**
 * Number of currently allocated bytes (not counting memory chunk metadata).
 */
static ssize_t memory_chunks_allocated;
/**
 * Maximum number of bytes ever allocated at any one time.
 */
static ssize_t memory_chunks_allocated_max;

static const char *
chunk_get_str(memory_chunk_t *chunk);
static void
chunks_free(void);

#else
#define _malloc(_size, _file, _line) malloc(_size)
#define _free(_ptr, _file, _line) free(_ptr)
#define _calloc(_nmemb, _size, _file, _line) calloc(_nmemb, _size)
#define _realloc(_ptr, _size, _file, _line) realloc(_ptr, _size)
#endif

TOOLKIT_API(DEPENDS(logger));

/**
 * Initialize the memory API.
 */
TOOLKIT_INIT_FUNC(memory)
{
#ifndef NDEBUG
    pthread_mutex_init(&memory_chunks_mutex, NULL);
    pthread_mutex_init(&memory_chunks_freed_mutex, NULL);
    memory_chunks = memory_chunks_freed = NULL;
    memory_chunks_num = 0;
    memory_chunks_num_max = 0;
    memory_chunks_allocated = 0;
    memory_chunks_allocated_max = 0;
#endif
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the memory API.
 */
TOOLKIT_DEINIT_FUNC(memory)
{
#ifndef NDEBUG
    chunks_free();
    pthread_mutex_destroy(&memory_chunks_mutex);
    pthread_mutex_destroy(&memory_chunks_freed_mutex);

    memory_chunk_t *chunk;
    DL_FOREACH(memory_chunks, chunk) {
        LOG(ERROR, "Unfreed pointer: %s", chunk_get_str(chunk));
    }

    LOG(INFO, "Maximum number of bytes allocated: %" PRIu64,
            (uint64_t) memory_chunks_allocated_max);

    LOG(INFO, "Maximum number of pointers allocated: %" PRIu64,
            (uint64_t) memory_chunks_num_max);

    if (memory_chunks_num != 0) {
        LOG(ERROR, "Number of pointers still allocated: %" PRIu64,
                (uint64_t) memory_chunks_num);
    }

    if (memory_chunks_allocated != 0) {
        LOG(ERROR, "Number of bytes still allocated: %" PRIu64,
                (uint64_t) memory_chunks_allocated);
    }
#endif
}
TOOLKIT_DEINIT_FUNC_FINISH

#ifndef NDEBUG

/**
 * Create a string representation of the specified memory chunk.
 *
 * @param chunk
 *
 * The memory chunk.
 * @return
 * String
 * representation of the memory chunk as a static-storage string.
 */
static const char *
chunk_get_str (memory_chunk_t *chunk)
{
    static char buf[HUGE_BUF];
    snprintf(VS(buf), "Chunk %p, pointer %p (%" PRIu64 " bytes) allocated in "
             "%s:%u", chunk, MEM_DATA(chunk), (uint64_t) chunk->size,
             chunk->file, chunk->line);
    return buf;
}

/**
 * Check the specified memory chunk for overruns/underruns.
 *
 * @param chunk
 *
 * The memory chunk.
 */
static void
chunk_check (memory_chunk_t *chunk)
{
    /* Check for underrun */
    if (unlikely(chunk->before != CHUNK_BEFORE_VAL)) {
        log_error("Pointer underrun detected: %s", chunk_get_str(chunk));
        abort();
    }

    /* Check for overrun */
    if (unlikely(*(uint64_t *) (&chunk->data[chunk->size]) !=
                 CHUNK_AFTER_VAL)) {
        log_error("Pointer overrun detected: %s", chunk_get_str(chunk));
        abort();
    }
}

/**
 * Check the specified pointer; if it's a pointer that was allocated through
 * this API as a memory chunk, it's checked using chunk_check() for overruns
 * and underruns and returned; otherwise NULL is returned.
 *
 * @param ptr
 *
 * The pointer to check.
 * @return
 *
 * Memory chunk the pointer belongs to; NULL if it's not a memory
 * chunk pointer.
 */
static memory_chunk_t *
chunk_checkptr (void *ptr)
{
    memory_chunk_t *chunk;
    DL_FOREACH(memory_chunks, chunk) {
        if (ptr >= (void *) &chunk->data[0] &&
            ptr < (void *) ((char *) chunk + MEM_CHUNK_SIZE(chunk->size))) {
            break;
        }
    }

    if (unlikely(chunk == NULL)) {
        return NULL;
    }

    chunk_check(chunk);
    return chunk;
}

/**
 * Check if the specified memory chunk has been freed. This is done by comparing
 * all the allocated bytes to 0x7A.
 *
 * @param chunk
 *
 * The chunk to check.
 */
static void
chunk_freed_check (memory_chunk_t *chunk)
{
    chunk_check(chunk);

    /* When running under Valgrind, the bytes are not set to 0x7A. */
    if (RUNNING_ON_VALGRIND) {
        return;
    }

    if (unlikely(chunk->size == 0)) {
        return;
    }

    if (unlikely(chunk->data[0] != 0x7A ||
                 memcmp(chunk->data, chunk->data + 1, chunk->size - 1) != 0)) {
        log_error("Freed pointer has been modified: %s", chunk_get_str(chunk));
        abort();
    }
}

/**
 * Check the specified pointer; if it's a pointer that was allocated through
 * this API as a memory chunk, it's checked using chunk_freed_check() for
 * modifications after freeing and returned; otherwise NULL is returned.
 *
 * @param ptr
 *
 * The pointer to check.
 * @return
 *
 * Memory chunk the pointer belongs to; NULL if it's not a memory
 * chunk pointer.
 */
static memory_chunk_t *
chunk_freed_checkptr (void *ptr)
{
    memory_chunk_t *chunk;
    DL_FOREACH(memory_chunks_freed, chunk) {
        if (ptr >= (void *) &chunk->data[0] &&
            ptr < (void *) ((char *) chunk + MEM_CHUNK_SIZE(chunk->size))) {
            break;
        }
    }

    if (unlikely(chunk == NULL)) {
        return NULL;
    }

    chunk_freed_check(chunk);
    return chunk;
}

/**
 * Free all the memory chunks on the free list.
 */
static void
chunks_free (void)
{
    pthread_mutex_lock(&memory_chunks_freed_mutex);

    memory_chunk_t *chunk, *tmp;
    DL_FOREACH_SAFE(memory_chunks_freed, chunk, tmp) {
        /* Check if it was modified. */
        chunk_freed_check(chunk);
        DL_DELETE(memory_chunks_freed, chunk);
        free(chunk->file);
        free(chunk);
    }

    pthread_mutex_unlock(&memory_chunks_freed_mutex);
}

/**
 * Check all memory chunks for errors; allocated ones for overruns/underruns
 * and freed ones for modifications.
 */
static void
chunk_check_all (void)
{
    /* Check allocated chunks */
    pthread_mutex_lock(&memory_chunks_mutex);
    memory_chunk_t *chunk;
    DL_FOREACH(memory_chunks, chunk) {
        chunk_check(chunk);
    }
    pthread_mutex_unlock(&memory_chunks_mutex);

    /* Check freed chunks */
    pthread_mutex_lock(&memory_chunks_freed_mutex);
    DL_FOREACH(memory_chunks_freed, chunk) {
        chunk_freed_check(chunk);
    }
    pthread_mutex_unlock(&memory_chunks_freed_mutex);
}

/**
 * Wrapper around malloc(); creates a memory chunk structure which holds
 * metadata about the chunk, plus the requested number of bytes.
 *
 * @param size
 *
 * Number of bytes to allocate.
 * @param file
 *
 * File the allocation is being done from.
 * @param line
 *
 * Line the allocation is being done from.
 * @return
 *
 * Pointer to an allocated memory block; never NULL.
 */
static void *
_malloc (size_t size, const char *file, uint32_t line)
{
    HARD_ASSERT(file != NULL);

    memory_chunk_t *chunk = malloc(MEM_CHUNK_SIZE(size));
    if (chunk == NULL) {
        LOG(ERROR, "OOM (size: %" PRIu64 ", file: %s:%" PRIu32 ").",
            (uint64_t) MEM_CHUNK_SIZE(size), file, line);
        abort();
    }

    /* Reclaim freed chunks */
    chunks_free();

    chunk->size = size;
    chunk->file = strdup(file);
    chunk->line = line;
    chunk->before = CHUNK_BEFORE_VAL;
    chunk->next = NULL;
    chunk->prev = NULL;
    *(uint64_t *) (&chunk->data[chunk->size]) = CHUNK_AFTER_VAL;

    pthread_mutex_lock(&memory_chunks_mutex);

    DL_APPEND(memory_chunks, chunk);

    /* Store some statistics. */
    memory_chunks_num++;
    if (memory_chunks_num > memory_chunks_num_max) {
        memory_chunks_num_max = memory_chunks_num;
    }

    memory_chunks_allocated += size;
    if (memory_chunks_allocated > memory_chunks_allocated_max) {
        memory_chunks_allocated_max = memory_chunks_allocated;
    }

    pthread_mutex_unlock(&memory_chunks_mutex);

    /*
     * When not running under Valgrind (so that it can still detect invalid
     * reads), set all of the allocated bytes to 0xEE to increase the chances
     * of instant errors if one tries to read from it without initializing it
     * first, instead of relying on garbage data.
     */
    if (!RUNNING_ON_VALGRIND) {
        memset(MEM_DATA(chunk), 0xEE, size);
    }

    return MEM_DATA(chunk);
}

/**
 * Wrapper around free(); frees the memory chunk associated with the specified
 * pointer. It is a fatal error if the pointer is not the one returned by a
 * previous call to _malloc().
 *
 * @param ptr
 *
 * Pointer to free.
 * @param file
 *
 * File freeing is done from.
 * @param line
 *
 * Line freeing is done from.
 */
static void
_free (void *ptr, const char *file, uint32_t line)
{
    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&memory_chunks_mutex);

    if (unlikely(memory_chunks_num <= 0)) {
        log_error("More frees than allocs (%" PRId64 "), free called from: "
                  "%s:%u", (int64_t) memory_chunks_num, file, line);
        abort();
    }

    /*
     * This is a costly operation, so don't do it unless CHECK_CHUNKS is
     * defined and assume the passed pointer is good. If it's not, the
     * underrun/overrun checks are likely to catch the error anyway.
     */
#if CHECK_CHUNKS
    memory_chunk_t *chunk = chunk_checkptr(ptr);

    if (chunk == NULL) {
        log_error("Invalid pointer detected: %p, free called from: %s:%u", ptr,
                  file, line);
        abort();
    }
#else
    memory_chunk_t *chunk = MEM_CHUNK(ptr);
#endif

    /* Check for underrun */
    if (unlikely(chunk->before != CHUNK_BEFORE_VAL)) {
        log_error("Pointer underrun detected: %s, free called from: %s:%u",
                  chunk_get_str(chunk), file, line);
        abort();
    }

    /* Check for overrun */
    if (unlikely(*(uint64_t *) (&chunk->data[chunk->size]) !=
                 CHUNK_AFTER_VAL)) {
        log_error("Pointer overrun detected: %s, free called from: %s:%u",
                  chunk_get_str(chunk), file, line);
        abort();
    }

    memory_chunks_allocated -= chunk->size;

    if (unlikely(memory_chunks_allocated < 0)) {
        log_error("Freed more bytes than what should be possible: %s, now "
                  "allocated: %" PRId64 ", free called from: %s:%u",
                  chunk_get_str(chunk), (int64_t) memory_chunks_allocated,
                  file, line);
        abort();
    }

    DL_DELETE(memory_chunks, chunk);
    memory_chunks_num--;

    pthread_mutex_unlock(&memory_chunks_mutex);

    /*
     * Set the bytes to 0x7A to track modifications when not running under
     * Valgrind.
     */
    if (!RUNNING_ON_VALGRIND) {
        memset(MEM_DATA(chunk), 0x7A, chunk->size);
    }

    chunk->next = chunk->prev = NULL;

    pthread_mutex_lock(&memory_chunks_freed_mutex);
    DL_APPEND(memory_chunks_freed, chunk);
    pthread_mutex_unlock(&memory_chunks_freed_mutex);
}

/**
 * Wrapper around calloc(); allocates 'nmemb' number of elements of size 'size'
 * and zero-initializes them.
 *
 * @param nmemb
 *
 * Number of elements.
 * @param size
 *
 * Size of each element.
 * @param file
 *
 * File allocation is being done from.
 * @param line
 *
 * Line allocation is being done from.
 * @return
 *
 * Pointer to an allocated zero-initialized memory block; never NULL.
 */
static void *
_calloc (size_t nmemb, size_t size, const char *file, uint32_t line)
{
    void *ptr;

    ptr = _malloc(size * nmemb, file, line);
    memset(ptr, 0, size * nmemb);

    return ptr;
}

/**
 * Wrapper around realloc(); resize the specified memory block. Added memory
 * (if any) is not initialized.
 *
 * @param ptr
 *
 * Memory block that is being resized.
 * @param size
 *
 * New size for the memory block.
 * @param file
 *
 * File reallocation was called from.
 * @param line
 *
 * Line reallocation was called from.
 * @return
 *
 * Resized memory block; NULL in case size was zero.
 */
static void *
_realloc (void *ptr, size_t size, const char *file, uint32_t line)
{
    void *new_ptr;
    if (size == 0) {
        new_ptr = NULL;
    } else {
        new_ptr = _malloc(size, file, line);
    }

    /* Copy the data over to the new pointer and free the old one. */
    if (ptr != NULL) {
        if (new_ptr != NULL) {
            memory_chunk_t *chunk = MEM_CHUNK(ptr);

            if (chunk->size < size) {
                size = chunk->size;
            }

            memcpy(new_ptr, ptr, size);
        }

        _free(ptr, file, line);
    }

    return new_ptr;
}
#endif

/*
 * Check all the currently allocated memory chunks.
 */
void
memory_check_all (void)
{
    TOOLKIT_PROTECT();

#ifndef NDEBUG
    chunks_free();
    chunk_check_all();
#endif
}

/**
 * Check the specified memory block.
 *
 * @param ptr
 *
 * Pointer to the memory block.
 * @return
 *
 * True on success, false on failure (memory checking API is disabled or the
 * pointer was not allocated by this API).
 */
bool
memory_check (void *ptr)
{
    HARD_ASSERT(ptr != NULL);

    TOOLKIT_PROTECT();

#ifndef NDEBUG
    memory_chunk_t *chunk;

    pthread_mutex_lock(&memory_chunks_mutex);
    chunk = chunk_checkptr(ptr);
    pthread_mutex_unlock(&memory_chunks_mutex);

    if (chunk == NULL) {
        return false;
    }

    return true;
#else
    return false;
#endif
}

/**
 * Acquire status of the specified memory block.
 *
 * @param ptr
 *
 * Pointer to the memory block; must have been allocated through this API.
 * @param status
 *
 * Where to store the status.
 * @return
 *
 * True on success (status contains a valid value), false otherwise.
 */
bool
memory_get_status (void *ptr, memory_status_t *status)
{
    HARD_ASSERT(ptr != NULL);
    HARD_ASSERT(status != NULL);

    TOOLKIT_PROTECT();

#ifndef NDEBUG
    memory_chunk_t *chunk;

    pthread_mutex_lock(&memory_chunks_mutex);
    chunk = chunk_checkptr(ptr);
    pthread_mutex_unlock(&memory_chunks_mutex);

    if (chunk == NULL) {
        pthread_mutex_lock(&memory_chunks_freed_mutex);
        chunk = chunk_freed_checkptr(ptr);
        pthread_mutex_unlock(&memory_chunks_freed_mutex);

        if (chunk == NULL) {
            log_error("Could not find pointer %p", ptr);
            abort();
        }

        chunk = MEM_CHUNK(ptr);
        chunk_freed_check(chunk);
        *status = MEMORY_STATUS_FREE;
    } else {
        *status = MEMORY_STATUS_OK;
    }

    return true;
#else
    return false;
#endif
}

/**
 * Acquire the allocation size of the specified memory block.
 *
 * @param ptr
 *
 * Pointer to the memory block.
 * @param size
 *
 * Where to store the size of the memory block.
 * @return
 *
 * True on success (size contains a valid value), false otherwise (the pointer
 * was not returned by this API or the API is disabled).
 */
bool
memory_get_size (void *ptr, size_t *size)
{
    HARD_ASSERT(ptr != NULL);
    HARD_ASSERT(size != NULL);

    TOOLKIT_PROTECT();

#ifndef NDEBUG
    memory_chunk_t *chunk;

    pthread_mutex_lock(&memory_chunks_mutex);
    chunk = chunk_checkptr(ptr);

    if (chunk == NULL) {
        pthread_mutex_unlock(&memory_chunks_mutex);
        return false;
    }

    *size = chunk->size;
    pthread_mutex_unlock(&memory_chunks_mutex);

    return true;
#else
    return false;
#endif
}

/**
 * Check for memory leaks; essentially, any memory chunk that is still
 * allocated when this function is called is considered a memory leak, so this
 * should be called after all the de-initialization is done (eg, from unit
 * tests).
 *
 * @param verbose
 *
 * If true, print all the chunks that are still allocated.
 * @return
 *
 * Number of allocated chunks.
 */
size_t
memory_check_leak (bool verbose)
{
#ifndef NDEBUG
    if (_did_init_) {
        pthread_mutex_lock(&memory_chunks_mutex);
    }

    memory_chunk_t *chunk;
    size_t num = 0;
    DL_FOREACH(memory_chunks, chunk) {
        if (verbose) {
            LOG(ERROR, "Unfreed pointer: %s", chunk_get_str(chunk));
        }

        num++;
    }

    if (_did_init_) {
        pthread_mutex_unlock(&memory_chunks_mutex);
    }

    return num;
#else
    return 0;
#endif
}

/**
 * Like malloc(), but performs error checking.
 *
 * @param size
 *
 * Number of bytes to allocate.
 * @return
 *
 * Allocated pointer, never NULL.
 * @note
 * Will abort() in case the pointer can't be allocated.
 */
void *
memory_emalloc (size_t size MEMORY_DEBUG_PROTO)
{
    TOOLKIT_PROTECT();

    void *ptr = _malloc(size, file, line);
    if (ptr == NULL) {
        LOG(ERROR, "OOM (size: %"PRIu64").", (uint64_t) size);
        abort();
    }

    return ptr;
}

/**
 * Like free(), but performs error checking.
 *
 * @param ptr
 *
 * Pointer to free.
 * @note
 * Will abort() in case the pointer is NULL.
 */
void
memory_efree (void *ptr MEMORY_DEBUG_PROTO)
{
    TOOLKIT_PROTECT();

    SOFT_ASSERT(ptr != NULL, "Freeing NULL pointer.");
    _free(ptr, file, line);
}

/**
 * Like calloc(), but performs error checking.
 *
 * @param nmemb
 *
 * Number of elements.
 * @param size
 *
 * Number of bytes.
 * @return
 *
 * Allocated pointer, never NULL.
 * @note
 * Will abort() in case the pointer can't be allocated.
 */
void *memory_ecalloc(size_t nmemb, size_t size MEMORY_DEBUG_PROTO)
{
    TOOLKIT_PROTECT();

    void *ptr = _calloc(nmemb, size, file, line);
    if (ptr == NULL) {
        LOG(ERROR, "OOM (nmemb: %"PRIu64", size: %"PRIu64").",
                (uint64_t) nmemb, (uint64_t) size);
        abort();
    }

    return ptr;
}

/**
 * Like realloc(), but performs error checking.
 *
 * @param ptr
 *
 * Pointer to resize.
 * @param size
 *
 * New number of bytes.
 * @return
 *
 * Resized pointer, never NULL.
 * @note
 * Will abort() in case the pointer can't be resized.
 */
void *
memory_erealloc (void *ptr, size_t size MEMORY_DEBUG_PROTO)
{
    TOOLKIT_PROTECT();

    void *newptr = _realloc(ptr, size, file, line);
    if (newptr == NULL && size != 0) {
        LOG(ERROR, "OOM (ptr: %p, size: %"PRIu64".", ptr,
                (uint64_t) size);
        abort();
    }

    return newptr;
}

/**
 * Like realloc(), but if more bytes are being allocated, they get set to
 * 0 using memset().
 *
 * @param ptr
 *
 * Original pointer.
 * @param old_size
 *
 * Size of the pointer.
 * @param new_size
 *
 * New size the pointer should have.
 * @return
 *
 * Resized pointer, NULL on failure.
 */
void *
memory_reallocz (void *ptr, size_t old_size, size_t new_size MEMORY_DEBUG_PROTO)
{
    TOOLKIT_PROTECT();

#ifndef NDEBUG
    void *new_ptr = memory_erealloc(ptr, new_size, file, line);
#else
    void *new_ptr = realloc(ptr, new_size);
#endif

    if (new_ptr && new_size > old_size) {
        memset(((char *) new_ptr) + old_size, 0, new_size - old_size);
    }

    return new_ptr;
}

#endif

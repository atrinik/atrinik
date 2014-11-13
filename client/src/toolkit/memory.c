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
 * Memory API. */

#include <global.h>

/**
 * Name of the API. */
#define API_NAME memory

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * Initialize the memory API.
 * @internal */
void toolkit_memory_init(void)
{

    TOOLKIT_INIT_FUNC_START(memory)
    {
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the memory API.
 * @internal */
void toolkit_memory_deinit(void)
{

    TOOLKIT_DEINIT_FUNC_START(memory)
    {
    }
    TOOLKIT_DEINIT_FUNC_END()
}

/**
 * Like malloc(), but performs error checking.
 * @param size Number of bytes to allocate.
 * @return Allocated pointer, never NULL.
 * @note Will abort() in case the pointer can't be allocated.
 */
void *memory_emalloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);

    if (ptr == NULL) {
        logger_print(LOG(ERROR), "OOM (size: %"FMT64U").", (uint64_t) size);
        abort();
    }

    return ptr;
}

/**
 * Like free(), but performs error checking.
 * @param ptr Pointer to free.
 * @note Will abort() in case the pointer is NULL.
 */
void memory_efree(void *ptr)
{
    if (ptr == NULL) {
        logger_print(LOG(ERROR), "Freeing NULL pointer.");
        abort();
    }

    free(ptr);
}

/**
 * Like calloc(), but performs error checking.
 * @param nmemb Elements.
 * @param size Number of bytes.
 * @return Allocated pointer, never NULL.
 * @note Will abort() in case the pointer can't be allocated.
 */
void *memory_ecalloc(size_t nmemb, size_t size)
{
    void *ptr;

    ptr = calloc(nmemb, size);

    if (ptr == NULL) {
        logger_print(LOG(ERROR), "OOM (nmemb: %"FMT64U", size: %"FMT64U").",
                (uint64_t) nmemb, (uint64_t) size);
        abort();
    }

    return ptr;
}

/**
 * Like realloc(), but performs error checking.
 * @param ptr Pointer to resize.
 * @param size New number of bytes.
 * @return Resized pointer, never NULL.
 * @note Will abort() in case the pointer can't be resized.
 */
void *memory_erealloc(void *ptr, size_t size)
{
    void *newptr;

    newptr = realloc(ptr, size);

    if (newptr == NULL && size != 0) {
        logger_print(LOG(ERROR), "OOM (ptr: %p, size: %"FMT64U".", ptr,
                (uint64_t) size);
        abort();
    }

    return newptr;
}

/**
 * Like realloc(), but if more bytes are being allocated, they get set to
 * 0 using memset().
 * @param ptr Original pointer.
 * @param old_size Size of the pointer.
 * @param new_size New size the pointer should have.
 * @return Resized pointer, NULL on failure. */
void *memory_reallocz(void *ptr, size_t old_size, size_t new_size)
{
    void *new_ptr;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    new_ptr = erealloc(ptr, new_size);

    if (new_ptr && new_size > old_size) {
        memset(((char *) new_ptr) + old_size, 0, new_size - old_size);
    }

    return new_ptr;
}

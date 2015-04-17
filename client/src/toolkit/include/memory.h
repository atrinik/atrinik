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
 * Memory API header file. */

#ifndef MEMORY_H
#define MEMORY_H

/* Map the error-checking memory allocating/freeing functions into the toolkit
 * variants. This is done for convenience, and because the functions can't be
 * defined as emalloc as they will conflict with functions from other libraries
 * such as libcheck. */
#ifndef NDEBUG
#define emalloc(_size) memory_emalloc(_size, __FILE__, __LINE__)
#define efree(_ptr) memory_efree(_ptr, __FILE__, __LINE__)
#define ecalloc(_nmemb, _size) memory_ecalloc(_nmemb, _size, __FILE__, __LINE__)
#define erealloc(_ptr, _size) memory_erealloc(_ptr, _size, __FILE__, __LINE__)
#define ereallocz(_ptr, _old_size, _new_size) \
    memory_reallocz(_ptr, _old_size, _new_size, __FILE__, __LINE__)
#else
#define emalloc(_size) memory_emalloc(_size)
#define efree(_ptr) memory_efree(_ptr)
#define ecalloc(_nmemb, _size) memory_ecalloc(_nmemb, _size)
#define erealloc(_ptr, _size) memory_erealloc(_ptr, _size)
#define ereallocz(_ptr, _old_size, _new_size) \
    memory_reallocz(_ptr, _old_size, _new_size)
#endif

typedef enum memory_status {
    MEMORY_STATUS_DISABLED = -1,
    MEMORY_STATUS_OK = 0,
    MEMORY_STATUS_FREE = 1
} memory_status_t;

/* Prototypes */

void toolkit_memory_init(void);
void toolkit_memory_deinit(void);
void memory_check_all(void);
bool memory_check(void *ptr);
bool memory_get_status(void *ptr, memory_status_t *status);
bool memory_get_size(void *ptr, size_t *size);

#ifndef NDEBUG
void *memory_emalloc(size_t size, const char *file, uint32_t line);
void memory_efree(void *ptr, const char *file, uint32_t line);
void *memory_ecalloc(size_t nmemb, size_t size, const char *file,
        uint32_t line);
void *memory_erealloc(void *ptr, size_t size, const char *file, uint32_t line);
void *memory_reallocz(void *ptr, size_t old_size, size_t new_size,
        const char *file, uint32_t line);
#else
void *memory_emalloc(size_t size);
void memory_efree(void *ptr);
void *memory_ecalloc(size_t nmemb, size_t size);
void *memory_erealloc(void *ptr, size_t size);
void *memory_reallocz(void *ptr, size_t old_size, size_t new_size);
#endif

#endif

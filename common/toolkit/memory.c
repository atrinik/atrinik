/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Fail-fast allocation helpers.
 */

#ifndef __CPROTO__

#include "memory.h"

static void allocation_failure(void) {
    fputs("fatal: memory allocation failed\n", stderr);
    abort();
}

static size_t checked_allocation_size(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        allocation_failure();
    }

    return nmemb * size;
}

void *xmalloc(size_t size) {
    void *ptr;

    ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        allocation_failure();
    }

    return ptr;
}

void *xmallocarray(size_t nmemb, size_t size) {
    return xmalloc(checked_allocation_size(nmemb, size));
}

void *xcalloc(size_t nmemb, size_t size) {
    void *ptr;

    checked_allocation_size(nmemb, size);
    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }

    ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        allocation_failure();
    }

    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *new_ptr;

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    new_ptr = realloc(ptr, size);
    if (new_ptr == NULL) {
        allocation_failure();
    }

    return new_ptr;
}

void *xreallocarray(void *ptr, size_t nmemb, size_t size) {
    return xrealloc(ptr, checked_allocation_size(nmemb, size));
}

char *xstrdup(const char *str) {
    size_t length;
    char *copy;

    if (str == NULL) {
        fputs("fatal: cannot duplicate a null string\n", stderr);
        abort();
    }

    length = strlen(str) + 1;
    copy = xmalloc(length);
    memcpy(copy, str, length);

    return copy;
}

char *xstrndup(const char *str, size_t max_length) {
    size_t length;
    char *copy;

    if (str == NULL) {
        fputs("fatal: cannot duplicate a null string\n", stderr);
        abort();
    }

    length = strnlen(str, max_length);
    copy = xmalloc(length + 1);
    memcpy(copy, str, length);
    copy[length] = '\0';

    return copy;
}

void xfree(void *ptr) {
    free(ptr);
}

#endif

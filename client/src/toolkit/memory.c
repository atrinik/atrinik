/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Like realloc(), but if more bytes are being allocated, they get set to
 * 0 using memset().
 * @param ptr Original pointer.
 * @param old_size Size of the pointer.
 * @param new_size New size the pointer should have.
 * @return Resized pointer, NULL on failure. */
void *memory_reallocz(void *ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = realloc(ptr, new_size);

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (new_ptr && new_size > old_size) {
        memset(((char *) new_ptr) + old_size, 0, new_size - old_size);
    }

    return new_ptr;
}

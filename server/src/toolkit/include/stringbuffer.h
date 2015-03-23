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
 * Header file for stringbuffer.c. */

#ifndef STRINGBUFFER_H
#define STRINGBUFFER_H

/**
 * The string buffer state. */
typedef struct StringBuffer_struct {
    /**
     * The string buffer. The first ::pos bytes contain the collected
     * string. Its size is at least ::bytes. */
    char *buf;

    /**
     * The current length of ::buf. The invariant <code>pos \< size</code>
     * always holds; this means there is always enough room to attach a
     * trailing NUL character. */
    size_t pos;

    /**
     * The allocation size of ::buf. */
    size_t size;
} StringBuffer;

#endif

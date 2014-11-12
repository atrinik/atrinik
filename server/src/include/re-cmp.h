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
 * Datastructures for representing a subset of regular expressions.
 *
 * Author: Kjetil T. Homme (kjetilho@ifi.uio.no) May 1993 */

/* Regexp's with syntax errors will core dump if
 * this is undefined. */
#define SAFE_CHECKS

/*
 * Max amount of tokens in a regexp.
 * Each token uses ~264 bytes. They are allocated
 * as needed, but never de-allocated.
 * E.g. [A-Za-z0-9_] counts as one token, so 64
 * should be plenty for most purposes. */
#define RE_TOKEN_MAX 64

typedef enum {
    /* corresponds to e.g. . */
    sel_any,

    /* "           $ */
    sel_end,

    /* "           q */
    sel_single,

    /* "           [A-F] */
    sel_range,

    /* "           [AF-RqO-T] */
    sel_array,

    /* "           [^f] */
    sel_not_single,

    /* "           [^A-F] */
    sel_not_range
} selection_type;

typedef enum {
    /* corresponds to no meta-char */
    rep_once,

    /* "       + */
    rep_once_or_more,

    /* "       ? */
    rep_null_or_once,

    /* "       * */
    rep_null_or_more
} repetetion_type;

typedef struct {
    selection_type type;

    union {
        unsigned char single;

        struct {
            unsigned char low, high;
        } range;

        int array[UCHAR_MAX];
    } u;

    repetetion_type repeat;
} selection;

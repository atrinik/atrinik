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
 * Book related definitions (type BOOK). */

#ifndef BOOK_H
#define BOOK_H

/**
 * Maximum message buf size for books.
 * @note Note that the book messages are stored in the msg buf, which is
 * limited by 'HUGE_BUF' in the loader. */
#define BOOK_BUF ((HUGE_BUF / 2) - 10)

#define MSGTYPE_MSGFILE 0
#define MSGTYPE_MONSTER 1
#define MSGTYPE_ARTIFACT 2
#define MSGTYPE_SPELLPATH 3
#define MSGTYPE_NUM 3

#endif

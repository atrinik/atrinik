/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Book related definitions (type BOOK). */

/**
 * Message buf size. */
#define BOOK_BUF 800

/** If little books aren't getting enough text generated, enlarge this */
#define BASE_BOOK_BUF 250

/**
 * Book buffer size. We shouldn't let little books/scrolls have more
 * information than big, weighty tomes! So let's base the 'natural'
 * book message buffer size on its weight. But never let a book
 * message buffer exceed the maximum size (BOOK_BUF) */
#define BOOKSIZE(xyz) \
	BASE_BOOK_BUF + ((xyz)->weight / 10) > BOOK_BUF ? BOOK_BUF : BASE_BOOK_BUF + ((xyz)->weight / 10);

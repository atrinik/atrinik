/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Book GUI header. */

#ifndef BOOK_H
#define BOOK_H

/** Maximum width of the book content. */
#define BOOK_CONTENT_WIDTH (Bitmaps[BITMAP_BOOK]->bitmap->w - 90)
/** Maximum height of the book content. */
#define BOOK_CONTENT_HEIGHT (Bitmaps[BITMAP_BOOK]->bitmap->h - 80)

/** X position of the book. */
#define BOOK_BACKGROUND_X (ScreenSurface->w / 2 - Bitmaps[BITMAP_BOOK]->bitmap->w / 2)
/** Y position of the book. */
#define BOOK_BACKGROUND_Y (ScreenSurface->h / 2 - Bitmaps[BITMAP_BOOK]->bitmap->h / 2)

#endif

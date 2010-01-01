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
 * Book GUI related structures. */

#ifndef BOOK_H
#define BOOK_H

/** Maximum lines on a page of book. */
#define BOOK_PAGE_LINES 18
/** Maximum characters on a line. */
#define BOOK_LINES_CHAR 256

/** Book data structure */
typedef struct __global_book_data
{
	/** X position */
	int x;

	/** Y position */
	int y;

	/** X length */
	int xlen;

	/** Y length */
	int ylen;
}_global_book_data;

/** Single line in the book GUI */
typedef struct gui_book_line
{
	/** Line mode */
	int mode;

	/** Line color */
	int color;

	/** Line */
	char line[BOOK_LINES_CHAR + 1];
} _gui_book_line;

/** Book GUI structure */
typedef struct gui_book_struct
{
	/** Mode */
	int mode;

	/** Number of pages */
	int pages;

	/** Current page */
	int page_show;

	/** First page structure */
	struct gui_book_page *start;

	/** Name of the book */
	char name[256];
} _gui_book_struct;

/** Single page in the book GUI */
typedef struct gui_book_page
{
	/** Next page in the structure */
	struct gui_book_page *next;

	/** Line in this page */
	_gui_book_line *line[BOOK_PAGE_LINES];
} _gui_book_page;

extern _global_book_data global_book_data;

extern _gui_book_struct *book_gui_load(char *data, int len);
extern void book_gui_show();
extern void book_gui_handle_mouse(int x, int y);

#endif

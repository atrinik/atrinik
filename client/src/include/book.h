/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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

#if !defined(__BOOK_H)
#define __BOOK_H

#define BOOK_PAGE_LINES 16
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

/** Book GUI */
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

/** Book GUI page structure */
typedef struct gui_book_page
{
	/** Next page in the structure */
	struct gui_book_page *next;

	/** Line in this page */
	_gui_book_line *line[BOOK_PAGE_LINES];
} _gui_book_page;

extern _global_book_data global_book_data;

extern _gui_book_struct *load_book_interface(char *data, int len);
extern void show_book();
extern void book_clear();
extern void gui_book_handle_mouse(int x, int y);

#endif

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
* the Free Software Foundation; either version 3 of the License, or     *
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

typedef struct __global_book_data
{
	int x;
	int y;
	int xlen;
	int ylen;
}_global_book_data;

typedef struct gui_book_line
{
	int mode;
	int color;
	char line[BOOK_LINES_CHAR+1];
} _gui_book_line;

typedef struct gui_book_struct
{
	int mode;
	int pages;
	int page_show;
	struct gui_book_page *start;
	char name[256];
} _gui_book_struct;


typedef struct gui_book_page
{
	struct gui_book_page *next;
	_gui_book_line *line[BOOK_PAGE_LINES];
} _gui_book_page;

extern _global_book_data global_book_data;

extern _gui_book_struct *load_book_interface(char *data, int len);
extern void show_book();
extern void book_clear();

#endif

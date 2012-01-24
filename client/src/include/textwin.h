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
 * Text window header file. */

#ifndef TEXTWIN_H
#define TEXTWIN_H

/** Custom attributes for text window widgets. */
typedef struct textwin_struct
{
	/** The text in the text window. */
	char *entries;

	/** Length of the entries. */
	size_t entries_size;

	/** Font used. */
	int font;

	/** Scroll offset. */
	uint32 scroll_offset;

	/** Number of lines. */
	uint32 num_lines;

	/** The scrollbar. */
	scrollbar_struct scrollbar;

	/** Whether there is anything in selection_start yet. */
	uint8 selection_started;

	/** Start of selection. */
	sint64 selection_start;

	/** End of selection. */
	sint64 selection_end;
} textwin_struct;

/**
 * @defgroup TEXTWIN_TEXT_xxx Textwin text coordinates
 * Coordinates used for the text in text window widgets.
 *@{*/
/** Text starting X position. */
#define TEXTWIN_TEXT_STARTX(_widget) (3)
/** Text starting Y position. */
#define TEXTWIN_TEXT_STARTY(_widget) (1)
/** Maximum width of the text in the widget. */
#define TEXTWIN_TEXT_WIDTH(_widget) ((_widget)->wd - scrollbar_get_width(&TEXTWIN((_widget))->scrollbar) - (TEXTWIN_TEXT_STARTX((_widget)) * 2))
/** Maximum height of the text in the widget. */
#define TEXTWIN_TEXT_HEIGHT(_widget) ((_widget)->ht - (TEXTWIN_TEXT_STARTY((_widget)) * 2))
/*@}*/

/** Get the maximum number of visible rows. */
#define TEXTWIN_ROWS_VISIBLE(widget) (TEXTWIN_TEXT_HEIGHT((widget)) / FONT_HEIGHT(TEXTWIN((widget))->font))
/** Get the base flags depending on the text window. */
#define TEXTWIN_TEXT_FLAGS(widget) ((widget)->WidgetTypeID == MSGWIN_ID ? TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_NO_FONT_CHANGE : TEXT_WORD_WRAP)

#endif

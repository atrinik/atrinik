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
 * Text window header file. */

#ifndef TEXTWIN_H
#define TEXTWIN_H

/* Events */
enum
{
	TW_CHECK_BUT_DOWN,
	TW_CHECK_BUT_UP,
	TW_CHECK_MOVE
};

/* Highlights */
enum
{
	TW_HL_NONE,
	TW_HL_UP,
	TW_ABOVE,
	TW_HL_SLIDER,
	TW_UNDER,
	TW_HL_DOWN
};

/* Flags */
enum
{
	TW_SCROLL = 0x01,
	TW_RESIZE = 0x02
};

/** Custom attributes for text window widgets. */
struct _textwin
{
	/** startpos of the window */
	int x, y;

	/** number or printed textlines */
	int size;

	/** scroll offset */
	int scroll;

	/** height of the scrollbar-slider  */
	int slider_h;

	/** start pos of the scrollbar-slider */
	int slider_y;

	/** which part to highlight */
	int highlight;

	/** The text in the text window. */
	char *entries;

	/** Number of lines. */
	size_t num_entries;

	/** Length of the entries. */
	size_t entries_size;

	/** Font used. */
	int font;
};

/** Get the maximum number of visible rows. */
#define TEXTWIN_ROWS_VISIBLE(widget) ((widget)->ht / FONT_HEIGHT(TEXTWIN((widget))->font))
/** Get the base flags depending on the text window. */
#define TEXTWIN_TEXT_FLAGS(widget) ((widget)->WidgetTypeID == MSGWIN_ID ? TEXT_WORD_WRAP | TEXT_MARKUP : TEXT_WORD_WRAP)

int textwin_flags;

#endif

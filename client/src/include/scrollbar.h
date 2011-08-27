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
 * Scrollbar header file. */

#ifndef SCROLLBAR_H
#define SCROLLBAR_H

typedef struct scrollbar_element
{
	int x;

	int y;

	int w;

	int h;

	uint8 highlight;

	void (*render_func)(SDL_Surface *surface, SDL_Rect *box, struct scrollbar_element *element);
} scrollbar_element;

#define SCROLL_DIRECTION_NONE 0
#define SCROLL_DIRECTION_UP 1
#define SCROLL_DIRECTION_DOWN 2

/**
 * Holds scrollbar information. */
typedef struct scrollbar_struct
{
	/** Pointer to the scroll offset. */
	uint32 *scroll_offset;

	/** Pointer to number of lines. */
	uint32 *num_lines;

	/** Maximum number of lines. */
	uint32 max_lines;

	uint8 *redraw;

	SDL_Surface *surface;

	int x;

	int y;

	int px;

	int py;

	int old_slider_pos;

	uint8 dragging;

	uint32 click_ticks;

	uint32 click_repeat_ticks;

	int scroll_direction;

	scrollbar_element background;

	scrollbar_element arrow_up;

	scrollbar_element arrow_down;

	scrollbar_element slider;
} scrollbar_struct;

#define SLIDER_HEIGHT_FULL(_scrollbar) ((_scrollbar)->background.h - ((_scrollbar)->background.w + 1) * 2)
#define SLIDER_YPOS_START(_scrollbar) ((_scrollbar)->background.w + 1)

#endif

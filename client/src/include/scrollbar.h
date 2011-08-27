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

/**
 * Holds scrollbar information. */
typedef struct scrollbar_struct
{
	/** Pointer to the scroll offset. */
	uint32 *scroll;

	/** Pointer to number of lines. */
	uint32 *num_lines;

	/** Pointer to maximum number of lines. */
	uint32 *max_lines;

	SDL_Surface *surface;

	int x;

	int y;

	int px;

	int py;

	scrollbar_element background;

	scrollbar_element arrow_up;

	scrollbar_element arrow_down;

	scrollbar_element slider;
} scrollbar_struct;

#endif

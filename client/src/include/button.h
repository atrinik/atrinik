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
 * Button header file. */

#ifndef BUTTON_H
#define BUTTON_H

/** Determine whether the x,y position is over the specified button. */
#define BUTTON_MOUSE_OVER(button, mx, my, sprite) ((mx) >= (button)->x && (mx) < (button)->x + (sprite)->bitmap->w && (my) >= (button)->y && (my) < (button)->y + (sprite)->bitmap->h)

/** Button structure. */
typedef struct button_struct
{
	/** X position. */
	int x;

	/** Y position. */
	int y;

	/** Bitmap to normally use for the button. */
	int bitmap;

	/**
	 * Bitmap to use if the mouse is over the button, -1 to use regular
	 * one. */
	int bitmap_over;

	/**
	 * Bitmap to use if the button is being pressed, -1 to use regular
	 * one. */
	int bitmap_pressed;

	/** Font used for the text. */
	int font;

	/** Text flags. */
	uint64 flags;

	/** Color of the text. */
	SDL_Color color;

	/** Color of the text's shadow. */
	SDL_Color color_shadow;

	/** Color of the text if the mouse is over the button. */
	SDL_Color color_over;

	/** Color of the text's shadow if the mouse is over the button. */
	SDL_Color color_over_shadow;

	/** 1 if the mouse is over the button. */
	int mouse_over;

	/** 1 if the button is being pressed. */
	int pressed;
} button_struct;

#endif

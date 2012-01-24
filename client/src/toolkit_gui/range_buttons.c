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
 * Range buttons API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Last clicked ticks to prevent single button click from triggering many
 * actions at once. */
static uint32 ticks = 0;

/**
 * Show the range buttons.
 * @param x X position.
 * @param y Y position.
 * @param val Pointer to integer to update when one of the range buttons
 * is clicked.
 * @param advance How much to advance 'val' by.
 * @return 1 if one of the buttons was clicked, 0 otherwise. */
int range_buttons_show(int x, int y, int *val, int advance)
{
	int state, mx, my;

	/* Get state of the mouse and the x/y. */
	state = SDL_GetMouseState(&mx, &my);

	/* Show the two range buttons. */
	sprite_blt(Bitmaps[BITMAP_RANGE_BUTTONS_OFF], x, y, NULL, NULL);

	/* Check the Y position. */
	if (my > y && my < y + Bitmaps[BITMAP_RANGE_BUTTONS_OFF]->bitmap->h && state == SDL_BUTTON(SDL_BUTTON_LEFT) && (!ticks || SDL_GetTicks() - ticks > 125))
	{
		/* If the left range button was clicked, decrease the value. */
		if (mx > x && mx < x + Bitmaps[BITMAP_RANGE_BUTTONS_LEFT]->bitmap->w)
		{
			sprite_blt(Bitmaps[BITMAP_RANGE_BUTTONS_LEFT], x, y, NULL, NULL);
			*val -= advance;
			ticks = SDL_GetTicks();
			return 1;
		}
		/* Otherwise increase it. */
		else if (mx > x + Bitmaps[BITMAP_RANGE_BUTTONS_LEFT]->bitmap->w && mx < x + Bitmaps[BITMAP_RANGE_BUTTONS_LEFT]->bitmap->w + Bitmaps[BITMAP_RANGE_BUTTONS_LEFT]->bitmap->w)
		{
			sprite_blt(Bitmaps[BITMAP_RANGE_BUTTONS_RIGHT], x + Bitmaps[BITMAP_RANGE_BUTTONS_LEFT]->bitmap->w, y, NULL, NULL);
			*val += advance;
			ticks = SDL_GetTicks();
			return 1;
		}
	}

	return 0;
}

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
 * The scroll buttons API.
 *
 * Scroll buttons are basically simplified scrollbars which are often
 * more user friendly than scrollbars. They allow the user to keep
 * holding them in order to scroll, instead of having to drag a
 * scrollbar, which is often imprecise.
 *
 * Scroll buttons consist of 4 buttons:
 *
 * - ^x: Scrolls a custom number of lines up per click (often the whole
 *       visible page).
 * - ^: Scrolls one line up per click.
 * - v: Scrolls one line down per click.
 * - vX: Scrolls a custom number of lines down per click (often the whole
 *       visible page). */

#include <include.h>

/**
 * Show one scroll button.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param pos Pointer to integer storing the current position.
 * @param max_pos Maximum possible value for 'pos'.
 * @param advance How many rows to advance when using the custom-scroll
 * buttons.
 * @param box Positions for the box. */
static void scroll_buttons_show_one(SDL_Surface *surface, int x, int y, int *pos, int max_pos, int advance, SDL_Rect *box)
{
	int state, mx, my;

	/* Draw the button's borders. */
	draw_frame(surface, box->x - 1, box->y - 1, box->w + 1, box->h + 1);

	state = SDL_GetMouseState(&mx, &my);

	/* Visual feedback so the user knows whether there is anything more
	 * further up/down so they don't have to bother clicking the buttons
	 * to find out. */
	if (*pos + MAX(-1, MIN(1, advance)) < 0 || *pos + MAX(-1, MIN(1, advance)) > max_pos)
	{
		SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 123, 115, 115));
	}
	/* Mouse over the button? */
	else if (mx > x && mx < x + box->w && my > y && my < y + box->h)
	{
		/* Clicked? */
		if (state == SDL_BUTTON(SDL_BUTTON_LEFT))
		{
			/* Advance the position pointer. */
			*pos += advance;
			SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 119, 106, 77));
		}
		else
		{
			SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 147, 133, 103));
		}
	}
	else
	{
		SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 181, 165, 132));
	}
}

/**
 * Show scroll buttons.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param pos Pointer to integer storing the current position.
 * @param max_pos Maximum possible value for 'pos'.
 * @param advance How many rows to advance when using the custom-scroll
 * buttons.
 * @param box Box containing coordinates, usually same as x/y. */
void scroll_buttons_show(SDL_Surface *surface, int x, int y, int *pos, int max_pos, int advance, SDL_Rect *box)
{
	_BLTFX bltfx;

	bltfx.surface = surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;

	box->h = 20;
	box->w = 20;

	/* Show each of the buttons, color them depending on the mouse's
	 * status, and show the arrow's bitmap. */
	scroll_buttons_show_one(surface, x, y, pos, max_pos, -advance, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_UP2], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_UP2]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_UP2]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, -1, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_UP], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_UP]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_UP]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, 1, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_DOWN], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_DOWN]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_DOWN]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, advance, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_DOWN2], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_DOWN2]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_DOWN2]->bitmap->h / 2, NULL, &bltfx);

	/* Check out of bounds values. */
	if (*pos > max_pos)
	{
		*pos = max_pos;
	}

	if (*pos < 0)
	{
		*pos = 0;
	}
}

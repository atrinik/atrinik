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
 * Button API. */

#include <include.h>

/**
 * Show a button.
 * @param bitmap_id Bitmap ID to use for the button.
 * @param bitmap_id_over Bitmap ID to use for the button when the mouse
 * is over the button. -1 to use 'bitmap_id'.
 * @param bitmap_id_clicked Bitmap ID to use for the button when left
 * mouse button is down. -1 to use 'bitmap_id'.
 * @param x X position of the button.
 * @param y Y position of the button.
 * @param text Text to display in the middle of the button. Can be NULL
 * for no text.
 * @param font Font to use for the text. One of @ref FONT_xxx.
 * @param color Color to use for the text.
 * @param color_shadow Shadow color of the text.
 * @param color_over Color to use when the mouse is over the button. -1
 * to use 'color'.
 * @param color_over_shadow Shadow color to use when the mouse is over
 * the button. -1 to use 'color_shadow'.
 * @return 1 if left mouse button is being held over the button, 0
 * otherwise. */
int button_show(int bitmap_id, int bitmap_id_over, int bitmap_id_clicked, int x, int y, const char *text, int font, SDL_Color color, SDL_Color color_shadow, SDL_Color color_over, SDL_Color color_over_shadow)
{
	_Sprite *sprite = Bitmaps[bitmap_id];
	int mx, my, ret = 0, state;
	SDL_Color use_color = color, use_color_shadow = color_shadow;

	/* Get state of the mouse and the x/y. */
	state = SDL_GetMouseState(&mx, &my);

	/* Is the mouse inside the button, and also not on top of button's
	 * transparent pixel? */
	if (mx > x && mx < x + sprite->bitmap->w && my > y && my < y + sprite->bitmap->h)
	{
		/* Change color. */
		use_color = color_over;
		use_color_shadow = color_over_shadow;

		/* Left button clicked? */
		if (state == SDL_BUTTON(SDL_BUTTON_LEFT))
		{
			/* Change bitmap. */
			if (bitmap_id_clicked != -1)
			{
				sprite = Bitmaps[bitmap_id_clicked];
			}

			ret = 1;
		}
		else
		{
			if (bitmap_id_over != -1)
			{
				sprite = Bitmaps[bitmap_id_over];
			}
		}
	}

	/* Draw the bitmap. */
	sprite_blt(sprite, x, y, NULL, NULL);

	/* If text was passed, draw it as well. */
	if (text)
	{
		string_blt_shadow(ScreenSurface, font, text, x + sprite->bitmap->w / 2 - string_get_width(font, text, 0) / 2, y + sprite->bitmap->h / 2 - FONT_HEIGHT(font) / 2, use_color, use_color_shadow, 0, NULL);
	}

	return ret;
}

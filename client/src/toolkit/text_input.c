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
 * Text input API. */

#include <include.h>

/**
 * Calculate X offset for centering a text input bitmap.
 * @return The offset. */
int text_input_center_offset()
{
	return Bitmaps[BITMAP_LOGIN_INP]->bitmap->w / 2;
}

/**
 * Draw text input's background (the bitmap).
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param bitmap Bitmap to use. */
void text_input_draw_background(SDL_Surface *surface, int x, int y, int bitmap)
{
	_BLTFX bltfx;

	bltfx.surface = surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;
	sprite_blt(Bitmaps[bitmap], x, y, NULL, &bltfx);
}

/**
 * Draw text input's text.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param font Font to use.
 * @param text Text to draw.
 * @param color Color to use.
 * @param flags Text @ref TEXT_xxx "flags".
 * @param bitmap Bitmap to use.
 * @param box Contains coordinates to use and maximum string width. */
void text_input_draw_text(SDL_Surface *surface, int x, int y, int font, const char *text, SDL_Color color, int flags, int bitmap, SDL_Rect *box)
{
	if (!box)
	{
		SDL_Rect box2;

		box2.x = 0;
		box2.y = 0;
		box2.w = Bitmaps[bitmap]->bitmap->w;
		box2.h = Bitmaps[bitmap]->bitmap->h;
		box = &box2;
	}

	x += 6 + box->x;
	y += box->h / 2 - FONT_HEIGHT(font) / 2 + box->y;
	box->x = 0;
	box->y = 0;

	string_blt(surface, font, text, x, y, color, flags, box);
}

/**
 * Show text input.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param font Font to use.
 * @param text Text to draw.
 * @param color Color to use.
 * @param flags Text @ref TEXT_xxx "flags".
 * @param bitmap Bitmap to use.
 * @param box Contains coordinates to use and maximum string width. */
void text_input_show(SDL_Surface *surface, int x, int y, int font, const char *text, SDL_Color color, int flags, int bitmap, SDL_Rect *box)
{
	char buf[MAX_BUF];

	/* Need to adjust the text by the cursor's position? */
	if (CurrentCursorPos)
	{
		SDL_Rect box2;
		size_t pos = CurrentCursorPos;
		const char *cp = text;

		box2.w = 0;

		/* Figure out the width by going backwards. */
		while (pos > 0)
		{
			blt_character(&font, font, NULL, &box2, cp + pos, NULL, NULL, 0, NULL);
			pos--;

			/* Reached the maximum yet? */
			if (box2.w > Bitmaps[bitmap]->bitmap->w - 26 - (box ? box->x * 2 : 0))
			{
				break;
			}
		}

		/* Adjust the text position if necessary. */
		if (pos)
		{
			text += pos;
		}
	}

	/* Draw the background. */
	text_input_draw_background(surface, x, y, bitmap);
	snprintf(buf, sizeof(buf), "%s_", text);
	/* Draw the text. */
	text_input_draw_text(surface, x, y, font, buf, color, flags, bitmap, box);
}

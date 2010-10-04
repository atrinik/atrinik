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
 * Text drawing API. */

#include <include.h>

/**
 * Switch font's color.
 * @param surface Surface. If NULL, will not do anything.
 * @param font Font to switch color for.
 * @param color The new color. */
static void bitmap_set_color(SDL_Surface *surface, _Font *font, int color)
{
	SDL_Color col;

	if (!surface)
	{
		return;
	}

	col.r = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].r;
	col.g = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].g;
	col.b = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].b;
	SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &col, 1, 1);
}

/**
 * Draw one character on the screen or parse markup (if applicable).
 * @param font Font to use.
 * @param surface Surface to draw on. If NULL, there is no drawing done,
 * but the return value is still calculated along with dest->w.
 * @param dest Destination, will have width (and x, if surface wasn't
 * NULL) updated.
 * @param cp String we are working on, cp[0] is the character to draw.
 * @param color Color to use.
 * @param flags Flags as passed to string_blt().
 * @return How many characters to jump. Usually 1, but can be more in
 * case of markup tags that need to be jumped over, since they are not
 * actually drawn. */
static int blt_character(_Font *font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, int color, int flags)
{
	SDL_Rect src;
	int width;
	char c = *cp;

	/* Doing markup? */
	if (!(flags & TEXT_NO_MARKUP))
	{
		/* Color tag: <c=r,g,b> */
		if (!strncmp(cp, "<c=", 3))
		{
			if (surface)
			{
				SDL_Color col;

				/* Parse the r,g,b colors. */
				if (sscanf(cp, "<c=%d,%d,%d>", (int *) &col.r, (int *) &col.g, (int *) &col.b) != 3)
				{
					return 3;
				}

				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &col, 1, 1);
			}

			/* Get the position of the ending '>'. */
			return strcspn(cp, ">") + 1;
		}
		/* End of color tag. */
		else if (!strncmp(cp, "</c>", 4))
		{
			bitmap_set_color(surface, font, color);
			return 4;
		}
		/* Convenience tag to make string green. */
		else if (!strncmp(cp, "<green>", 7))
		{
			if (surface)
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &(SDL_Color) {0, 255, 0, 0}, 1, 1);
			}

			return 7;
		}
		else if (!strncmp(cp, "</green>", 8))
		{
			bitmap_set_color(surface, font, color);
			return 8;
		}
		/* Convenience tag to make string yellow. */
		else if (!strncmp(cp, "<yellow>", 8))
		{
			if (surface)
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &(SDL_Color) {255, 255, 0, 0}, 1, 1);
			}

			return 8;
		}
		else if (!strncmp(cp, "</yellow>", 9))
		{
			bitmap_set_color(surface, font, color);
			return 9;
		}
		/* Convenience tag to make string red. */
		else if (!strncmp(cp, "<red>", 5))
		{
			if (surface)
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &(SDL_Color) {255, 0, 0, 0}, 1, 1);
			}

			return 5;
		}
		else if (!strncmp(cp, "</red>", 6))
		{
			bitmap_set_color(surface, font, color);
			return 6;
		}
		/* Convenience tag to make string blue. */
		else if (!strncmp(cp, "<blue>", 6))
		{
			if (surface)
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &(SDL_Color) {0, 0, 255, 0}, 1, 1);
			}

			return 6;
		}
		else if (!strncmp(cp, "</blue>", 7))
		{
			bitmap_set_color(surface, font, color);
			return 7;
		}
	}

	/* Draw the character (unless it's a space, since there's no point in
	 * drawing whitespace). */
	if (surface && c != ' ')
	{
		src.x = font->c[(int) c].x;
		src.y = font->c[(int) c].y;
		src.w = font->c[(int) c].w;
		src.h = font->c[(int) c].h;
		SDL_BlitSurface(font->sprite->bitmap, &src, surface, dest);
	}

	/* Get the character's width. */
	width = CHAR_WIDTH(c, font);

	/* Update the x/w of the destination with the character's width. */
	if (surface)
	{
		dest->x += width;
	}

	dest->w += width;
	return 1;
}

/**
 * Draw a string on the specified surface.
 * @param surface Surface to draw on.
 * @param font Font to use.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color Color to use.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void string_blt(SDL_Surface *surface, _Font *font, const char *text, int x, int y, int color, int flags, SDL_Rect *box)
{
	const char *cp = text;
	SDL_Rect dest;
	int pos = 0, last_space = 0, is_lf, ret;

	/* Store the x/y. */
	dest.x = x;
	dest.y = y;
	dest.w = 0;

	/* Set the font's color. */
	bitmap_set_color(surface, font, color);

	while (cp[pos] != '\0')
	{
		/* Have we gone over the height limit yet? */
		if (box && box->h && dest.y + 12 - y > box->h)
		{
			return;
		}

		is_lf = cp[pos] == '\n';

		/* Is this a newline, or word wrap was set and we are over
		 * maximum width? */
		if (is_lf || (flags & TEXT_WORD_WRAP && box && box->w && dest.w > box->w))
		{
			/* Store the last space. */
			if (is_lf || last_space == 0)
			{
				last_space = pos;
			}

			/* Draw characters until we have reached the cut point (last_space). */
			while (*cp != '\0' && last_space > 0)
			{
				ret = blt_character(font, surface, &dest, cp, color, flags);
				cp += ret;
				last_space -= ret;
			}

			/* Update the Y position. */
			dest.y += 12;

			/* Jump over the newline, if any. */
			if (is_lf)
			{
				cp++;
			}

			/* Strip leading spaces. */
			while (*cp != '\0' && *cp == ' ')
			{
				cp++;
			}

			/* Update the coordinates. */
			last_space = pos = 0;
			dest.w = 0;
			dest.x = x;
		}
		else
		{
			/* Store last space position. */
			if (cp[pos] == ' ')
			{
				last_space = pos;
			}

			/* Do not do any drawing, just calculate how many characters
			 * to jump and the width. */
			pos += blt_character(font, NULL, &dest, cp + pos, color, flags);
		}
	}

	/* Draw leftover characters. */
	while (*cp != '\0')
	{
		cp += blt_character(font, surface, &dest, cp, color, flags);
	}
}

/**
 * Draw a string with a shadow.
 * @param surface Surface to draw on.
 * @param font Font to use.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color Color to use.
 * @param color_shadow Color to use for the shadow.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void string_blt_shadow(SDL_Surface *surface, _Font *font, const char *text, int x, int y, int color, int color_shadow, int flags, SDL_Rect *box)
{
	string_blt(surface, font, text, x + 1, y - 1, color_shadow, flags, box);
	string_blt(surface, font, text, x, y - 2, color, flags, box);
}

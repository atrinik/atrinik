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
 * Header file for text drawing API. */

#ifndef TEXT_H
#define TEXT_H

/** One font. */
typedef struct font_struct
{
	/** The font's path. */
	const char *path;

	/** Size of the font. */
	size_t size;

	/** The actual font used by SDL_ttf. */
	TTF_Font *font;

	/** Maximum line height. */
	int height;
} font_struct;

/**
 * @anchor FONT_xxx
 * The font IDs. */
enum
{
	/** Sans, 10px, good for general drawing. */
	FONT_SANS10,
	/** Serif, 10px. */
	FONT_SERIF10,
	/** Arial, 10px. */
	FONT_ARIAL10,

	/** Number of the fonts. */
	FONTS_MAX
};

/**
 * @defgroup TEXT_xxx Text flags
 * Various text flags for controlling behavior of string_blt().
 *@{*/
/** Parse markup, otherwise it will be rendered as normal text. */
#define TEXT_MARKUP 1
/** Wrap words, otherwise only newlines. */
#define TEXT_WORD_WRAP 2
/** Render the text in solid mode (faster, but worse looking). */
#define TEXT_SOLID 4
/*@}*/

/**
 * Convenience macro to construct SDL_Color array from one of the
 * COLOR_xxx constants.
 * @param color Color.
 * @return SDL_Color array containing the rgb values. */
#define COLOR_SIMPLE(color) ((SDL_Color) {Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].r, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].g, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].b, 0})
/** Get font's maximum height. */
#define FONT_HEIGHT(font) (fonts[font].height)

font_struct fonts[FONTS_MAX];

#endif

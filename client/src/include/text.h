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
	/** Sans, 7px. */
	FONT_SANS7,
	/** Sans, 8px. */
	FONT_SANS8,
	/** Sans, 9px. */
	FONT_SANS9,
	/** Sans, 10px. */
	FONT_SANS10,
	/** Sans, 11px. */
	FONT_SANS11,
	/** Sans, 12px. */
	FONT_SANS12,
	/** Sans, 13px. */
	FONT_SANS13,
	/** Sans, 14px. */
	FONT_SANS14,
	/** Sans, 15px. */
	FONT_SANS15,
	/** Sans, 16px. */
	FONT_SANS16,
	/** Sans, 18px. */
	FONT_SANS18,
	/** Sans, 20px. */
	FONT_SANS20,
	/** Serif, 8px. */
	FONT_SERIF8,
	/** Serif, 10px. */
	FONT_SERIF10,
	/** Serif, 12px. */
	FONT_SERIF12,
	/** Serif, 14px. */
	FONT_SERIF14,
	/** Serif, 16px. */
	FONT_SERIF16,
	/** Serif, 18px. */
	FONT_SERIF18,
	/** Serif, 20px. */
	FONT_SERIF20,
	/** Mono, 8px. */
	FONT_MONO8,
	/** Mono, 9px. */
	FONT_MONO9,
	/** Mono, 10px. */
	FONT_MONO10,
	/** Mono, 12px. */
	FONT_MONO12,
	/** Mono, 14px. */
	FONT_MONO14,
	/** Mono, 16px. */
	FONT_MONO16,
	/** Mono, 18px. */
	FONT_MONO18,
	/** Mono, 20px. */
	FONT_MONO20,
	/** Arial, 8px. */
	FONT_ARIAL8,
	/** Arial, 10px, good for general drawing (looks the same across systems). */
	FONT_ARIAL10,
	/** Arial, 11px. */
	FONT_ARIAL11,
	/** Arial, 12px. */
	FONT_ARIAL12,
	/** Arial, 13px. */
	FONT_ARIAL13,
	/** Arial, 14px. */
	FONT_ARIAL14,
	/** Arial, 15px. */
	FONT_ARIAL15,
	/** Arial, 16px. */
	FONT_ARIAL16,
	/** Arial, 18px. */
	FONT_ARIAL18,
	/** Arial, 20px. */
	FONT_ARIAL20,

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
/** Align the text to center of box's width. */
#define TEXT_ALIGN_CENTER 8
/**
 * Instead of quitting drawing when maximum height passed was reached,
 * continue going on, but without doing any more drawing, and store the
 * final height in box->h (where the initial height limit came from).
 *
 * If this flag is passed, you can also specify skipping # of rows in
 * the box's 'y'. */
#define TEXT_HEIGHT 16
/**
 * Vertically center the text to that of the passed box's height. Note
 * that this will NOT take font tag changing into account. */
#define TEXT_VALIGN_CENTER 32
/**
 * Do not allow color changing using markup. */
#define TEXT_NO_COLOR_CHANGE 64
/**
 * Show a black outline around the text (can be changed to different
 * color using \<o\> markup. */
#define TEXT_OUTLINE 128
/**
 * Store number of lines in box->h. */
#define TEXT_LINES_CALC 256
/**
 * Skip first box->y lines. */
#define TEXT_LINES_SKIP 512
/**
 * Do not allow font changing using markup. */
#define TEXT_NO_FONT_CHANGE 1024
/**
 * Like @ref TEXT_WORD_WRAP, but will stop drawing when the characters
 * width would be more than box->w. */
#define TEXT_WIDTH 2048
/*@}*/

/**
 * Convenience macro to construct SDL_Color array from one of the
 * COLOR_xxx constants.
 * @param color Color.
 * @return SDL_Color array containing the rgb values. */
#define COLOR_SIMPLE(color) ((SDL_Color) {Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].r, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].g, Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color].b, 0})
/** Get font's maximum height. */
#define FONT_HEIGHT(font) (fonts[(font)].height)

font_struct fonts[FONTS_MAX];

#endif

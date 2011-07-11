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
 * Tooltip API. */

#include <global.h>

/** Tooltip's text. */
static char tooltip_text[HUGE_BUF];
/** Font of the tooltip text. */
static int tooltip_font;
/** Tooltip's X position. */
static int tooltip_x = -1;
/** Tooltip's Y position. */
static int tooltip_y = -1;

/**
 * Creates a new tooltip. This must be called every frame in order for
 * the tooltip to remain shown.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param font Font to use, one of @ref FONT_xxx.
 * @param text The text to show in the tooltip. */
void tooltip_create(int mx, int my, int font, const char *text)
{
	tooltip_font = font;
	tooltip_x = mx;
	tooltip_y = my;
	strncpy(tooltip_text, text, sizeof(tooltip_text) - 1);
	tooltip_text[sizeof(tooltip_text) - 1] = '\0';
}

/**
 * Actually show the tooltip. */
void tooltip_show()
{
	SDL_Rect box;

	/* No tooltip to show. */
	if (tooltip_x == -1 || tooltip_y == -1)
	{
		return;
	}

	/* Generate the tooltip's background. */
	box.x = tooltip_x + 9;
	box.y = tooltip_y + 17;
	box.w = string_get_width(tooltip_font, tooltip_text, 0) + 6;
	box.h = FONT_HEIGHT(tooltip_font) + 1;

	/* Push the tooltip to the left if it would go beyond maximum screen
	 * size. */
	if (box.x + box.w >= ScreenSurface->w)
	{
		box.x -= (box.x + box.w + 1) - ScreenSurface->w;
	}

	SDL_FillRect(ScreenSurface, &box, -1);
	string_blt(ScreenSurface, tooltip_font, tooltip_text, box.x + 3, box.y - 1, COLOR_BLACK, 0, NULL);

	/* Set stored x/y back to -1, so the next frame the tooltip isn't
	 * shown again, unless tooltip_create() gets called. */
	tooltip_x = -1;
	tooltip_y = -1;
}

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
 * Implements progress indicators.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Create progress dots indicator.
 * @param progress Where to store indicator data. */
void progress_dots_create(progress_dots *progress)
{
	progress->ticks = SDL_GetTicks();
	progress->dot = 0;
	progress->done = 0;
}

/**
 * Show the progress dots.
 * @param progress The progress dots indicator data.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position. */
void progress_dots_show(progress_dots *progress, SDL_Surface *surface, int x, int y)
{
	uint8 i;
	SDL_Surface *texture;

	for (i = 0; i < PROGRESS_DOTS_NUM; i++)
	{
		texture = TEXTURE_CLIENT(progress->dot == i || progress->done ? "loading_on" : "loading_off");
		surface_show(surface, x + (texture->w + PROGRESS_DOTS_SPACING) * i, y, NULL, texture);
	}

	/* Progress the lit dot. */
	if (!progress->done && SDL_GetTicks() - progress->ticks > PROGRESS_DOTS_TICKS)
	{
		progress->ticks = SDL_GetTicks();
		progress->dot++;

		/* More than maximum, back to the first one. */
		if (progress->dot >= PROGRESS_DOTS_NUM)
		{
			progress->dot = 0;
		}
	}
}

/**
 * Calculate the width of progress dots.
 * @param progress The progress dots indicator data.
 * @return The width. */
int progress_dots_width(progress_dots *progress)
{
	(void) progress;

	return (TEXTURE_CLIENT("loading_on")->w + PROGRESS_DOTS_SPACING) * PROGRESS_DOTS_NUM - PROGRESS_DOTS_SPACING;
}

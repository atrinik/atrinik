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

#include <global.h>

void progress_dots_create(progress_dots *progress)
{
	progress->ticks = SDL_GetTicks();
	progress->dot = 0;
	progress->done = 0;
}

void progress_dots_show(progress_dots *progress, SDL_Surface *surface, int x, int y)
{
	uint8 i;
	_BLTFX bltfx;

	bltfx.surface = surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;

	for (i = 0; i < PROGRESS_DOTS_NUM; i++)
	{
		sprite_blt(Bitmaps[progress->dot == i || progress->done ? BITMAP_LOADING_ON : BITMAP_LOADING_OFF], x + (Bitmaps[BITMAP_LOADING_ON]->bitmap->w + 2) * i, y, NULL, &bltfx);
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

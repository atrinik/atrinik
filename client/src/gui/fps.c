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
 *  */

#include <include.h>

/**
 * Show the frames per second widget.
 * @param x X position.
 * @param y Y position. */
void widget_show_fps(widgetdata *widget)
{
	char buf[MAX_BUF];

	if (!options.show_frame || !FrameCount)
	{
		return;
	}

	snprintf(buf, sizeof(buf), "fps %d (%d) (%d %d) %s%s%s%s%s%s%s%s%s%s %d %d", ((LastTick - tmpGameTick) / FrameCount) ? 1000 / ((LastTick - tmpGameTick) / FrameCount) : 0, (LastTick - tmpGameTick) / FrameCount, GameStatus, cpl.input_mode, ScreenSurface->flags & SDL_FULLSCREEN ? "F" : "", ScreenSurface->flags & SDL_HWSURFACE ? "H" : "S", ScreenSurface->flags & SDL_HWACCEL ? "A" : "", ScreenSurface->flags & SDL_DOUBLEBUF ? "D" : "", ScreenSurface->flags & SDL_ASYNCBLIT ? "a" : "", ScreenSurface->flags & SDL_ANYFORMAT ? "f" : "", ScreenSurface->flags & SDL_HWPALETTE ? "P" : "", options.rleaccel_flag ? "R" : "", options.force_redraw ? "r" : "", options.use_rect ? "u" : "", options.used_video_bpp, options.real_video_bpp);

	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1, widget->y1, COLOR_DEFAULT, NULL, NULL);
}

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
 * Handles fps widget.
 *
 * @author Alex Tokar */

#include <global.h>

static Uint32 fps_lasttime;
static Uint32 fps_current;
/** Number of frames drawn. */
static Uint32 fps_frames;

/**
 * Initialize variables used by fps widget. */
void fps_init(void)
{
	fps_lasttime = SDL_GetTicks();
	fps_current = fps_frames = 0;
}

/**
 * Called at the end of each frame to calculate the current fps (if
 * applicable). */
void fps_do(void)
{
	fps_frames++;

	if (fps_lasttime < SDL_GetTicks() - 1000)
	{
		fps_lasttime = SDL_GetTicks();
		fps_current = fps_frames;
		fps_frames = 0;
	}
}

/**
 * Show the fps widget
 * @param widget Widget. */
void widget_show_fps(widgetdata *widget)
{
	char buf[MAX_BUF];

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_CLIENT("fps"));

	snprintf(buf, sizeof(buf), "%d", fps_current);
	string_show(ScreenSurface, FONT_ARIAL11, "fps:", widget->x1 + 5, widget->y1 + 4, COLOR_WHITE, 0, NULL);
	string_show(ScreenSurface, FONT_ARIAL11, buf, widget->x1 + widget->wd - 5 - string_get_width(FONT_ARIAL11, buf, 0), widget->y1 + 4, COLOR_WHITE, 0, NULL);
}

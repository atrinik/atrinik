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
 * Implements FPS type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * FPS widget data. */
typedef struct widget_fps_struct
{
	/**
	 * Last time the FPS was calculated. */
	uint32 lasttime;

	/**
	 * Current FPS. */
	uint32 current;

	/**
	 * Number of frames drawn since last calculation. */
	uint32 frames;
} widget_fps_struct;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box;
	widget_fps_struct *tmp;
	char buf[MAX_BUF];

	tmp = (widget_fps_struct *) widget->subwidget;
	tmp->frames++;

	if (tmp->lasttime < SDL_GetTicks() - 1000)
	{
		tmp->lasttime = SDL_GetTicks();
		tmp->current = tmp->frames;
		tmp->frames = 0;
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);

	snprintf(buf, sizeof(buf), "%d", tmp->current);
	text_show(ScreenSurface, FONT_ARIAL11, "fps:", widget->x + 5, widget->y + 4, COLOR_WHITE, 0, NULL);
	text_show(ScreenSurface, FONT_ARIAL11, buf, widget->x + widget->w - 5 - text_get_width(FONT_ARIAL11, buf, 0), widget->y + 4, COLOR_WHITE, 0, NULL);
}

/**
 * Initialize one FPS widget. */
void widget_fps_init(widgetdata *widget)
{
	widget_fps_struct *tmp;

	widget->draw_func = widget_draw;

	widget->subwidget = tmp = calloc(1, sizeof(*tmp));
	tmp->lasttime = SDL_GetTicks();
}

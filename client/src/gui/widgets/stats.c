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
 * Implements stats type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	double temp;
	SDL_Rect box;
	int tmp;
	SDL_Surface *texture;

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("main_stats");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("main_stats"));

		/* Health */
		text_show(widget->surface, FONT_ARIAL10, "HP", 58, 10, COLOR_WHITE, 0, NULL);
		snprintf(buf, sizeof(buf), "%d/%d", cpl.stats.hp, cpl.stats.maxhp);
		text_truncate_overflow(FONT_ARIAL10, buf, 90);
		text_show(widget->surface, FONT_ARIAL10, buf, 160 - text_get_width(FONT_ARIAL10, buf, 0), 10, COLOR_GREEN, 0, NULL);

		/* Mana */
		text_show(widget->surface, FONT_ARIAL10, "Mana", 58, 34, COLOR_WHITE, 0, NULL);
		snprintf(buf, sizeof(buf), "%d/%d", cpl.stats.sp, cpl.stats.maxsp);
		text_truncate_overflow(FONT_ARIAL10, buf, 75);
		text_show(widget->surface, FONT_ARIAL10, buf, 160 - text_get_width(FONT_ARIAL10, buf, 0), 34, COLOR_GREEN, 0, NULL);

		/* Food */
		text_show(widget->surface, FONT_ARIAL10, "Food", 58, 83, COLOR_WHITE, 0, NULL);
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);

	/* Health bar */
	if (cpl.stats.maxhp)
	{
		texture = TEXTURE_CLIENT("hp");
		tmp = cpl.stats.hp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxhp;
		box.x = 0;
		box.y = 0;
		box.h = texture->h;
		box.w = (int) (texture->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > texture->w)
		{
			box.w = texture->w;
		}

		surface_show(ScreenSurface, widget->x + 57, widget->y + 23, NULL, TEXTURE_CLIENT("hp_back"));
		surface_show(ScreenSurface, widget->x + 57, widget->y + 23, &box, texture);
	}

	/* Mana bar */
	if (cpl.stats.maxsp)
	{
		texture = TEXTURE_CLIENT("sp");
		tmp = cpl.stats.sp;

		if (tmp < 0)
		{
			tmp = 0;
		}

		temp = (double) tmp / (double) cpl.stats.maxsp;
		box.x = 0;
		box.y = 0;
		box.h = texture->h;
		box.w = (int) (texture->w * temp);

		if (tmp && !box.w)
		{
			box.w = 1;
		}

		if (box.w > texture->w)
		{
			box.w = texture->w;
		}

		surface_show(ScreenSurface, widget->x + 57, widget->y + 47, NULL, TEXTURE_CLIENT("sp_back"));
		surface_show(ScreenSurface, widget->x + 57, widget->y + 47, &box, texture);
	}

	texture = TEXTURE_CLIENT("food");
	/* Food bar */
	tmp = cpl.stats.food;

	if (tmp < 0)
	{
		tmp = 0;
	}

	temp = (double) tmp / 1000;
	box.x = 0;
	box.y = 0;
	box.h = texture->h;
	box.w = (int) (texture->w * temp);

	if (tmp && !box.w)
	{
		box.w = 1;
	}

	if (box.w > texture->w)
	{
		box.w = texture->w;
	}

	surface_show(ScreenSurface, widget->x + 87, widget->y + 88, NULL, TEXTURE_CLIENT("food_back"));
	surface_show(ScreenSurface, widget->x + 87, widget->y + 88, &box, texture);
}

/**
 * Initialize one stats widget. */
void widget_stats_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

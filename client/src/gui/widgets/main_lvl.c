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
 * Implements main level type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box;

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("main_level_bg");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("main_level_bg"));

		text_show(widget->surface, FONT_ARIAL10, "Level / Exp", 5, 5, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		snprintf(buf, sizeof(buf), "<b>%d</b>", cpl.stats.level);
		text_show(widget->surface, FONT_SERIF14, buf, widget->w - 4 - text_get_width(FONT_SERIF14, buf, TEXT_MARKUP), 4, cpl.stats.level == s_settings->max_level ? COLOR_HGOLD : COLOR_WHITE, TEXT_MARKUP, NULL);

		text_show_format(widget->surface, FONT_ARIAL10, 5, 20, COLOR_WHITE, 0, NULL, "%"FMT64, cpl.stats.exp);

		player_draw_exp_progress(widget->surface, 4, 35, cpl.stats.exp, cpl.stats.level);
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/**
 * Initialize one main level widget. */
void widget_main_lvl_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

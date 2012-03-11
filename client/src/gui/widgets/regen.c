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
 * Implements regen type widgets.
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

		texture = TEXTURE_CLIENT("regen_bg");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		char buf[MAX_BUF];

		widget->redraw = 0;

		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("regen_bg"));

		text_show(widget->surface, FONT_SANS8, "R", 4, 1, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		text_show(widget->surface, FONT_SANS8, "e", 4, 7, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		text_show(widget->surface, FONT_SANS8, "g", 4, 13, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		text_show(widget->surface, FONT_SANS8, "e", 4, 21, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		text_show(widget->surface, FONT_SANS8, "n", 4, 27, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		/* Health */
		text_show(widget->surface, FONT_ARIAL10, "HP:", 13, 3, COLOR_HGOLD, 0, NULL);
		snprintf(buf, sizeof(buf), "%2.1f/s", cpl.gen_hp);
		string_truncate_overflow(FONT_ARIAL10, buf, 45);
		text_show(widget->surface, FONT_ARIAL10, buf, widget->w - 5 - text_get_width(FONT_ARIAL10, buf, 0), 3, COLOR_WHITE, 0, NULL);

		/* Mana */
		text_show(widget->surface, FONT_ARIAL10, "Mana:", 13, 13, COLOR_HGOLD, 0, NULL);
		snprintf(buf, sizeof(buf), "%2.1f/s", cpl.gen_sp);
		string_truncate_overflow(FONT_ARIAL10, buf, 45);
		text_show(widget->surface, FONT_ARIAL10, buf, widget->w - 5 - text_get_width(FONT_ARIAL10, buf, 0), 13, COLOR_WHITE, 0, NULL);
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/**
 * Initialize one regen widget. */
void widget_regen_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

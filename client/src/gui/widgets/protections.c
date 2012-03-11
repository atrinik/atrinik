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
 * Implements protections type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box;
	size_t i;
	int x = 5, y = 2, mx, my;

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("resist_bg");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("resist_bg"));
		text_show(widget->surface, FONT_SERIF10, "Protection Table", x, y, COLOR_HGOLD, TEXT_OUTLINE, NULL);
	}

	SDL_GetMouseState(&mx, &my);

	for (i = 0; i < sizeof(cpl.stats.protection) / sizeof(*cpl.stats.protection); i++)
	{
		if (!(i % 5))
		{
			y += 15;
			x = 5;
		}

		if (widget->redraw)
		{
			const char *color;

			/* Figure out color for the protection value. */
			if (!cpl.stats.protection[i])
			{
				color = COLOR_GRAY;
			}
			else if (cpl.stats.protection[i] < 0)
			{
				color = COLOR_RED;
			}
			else if (cpl.stats.protection[i] >= 100)
			{
				color = COLOR_ORANGE;
			}
			else
			{
				color = COLOR_WHITE;
			}

			text_show_format(widget->surface, FONT_MONO9, x, y + 1, color, TEXT_MARKUP, NULL, "<c=#d4d553>%s</c>%s %02d", s_settings->protection_letters[i], s_settings->protection_letters[i][1] == '\0' ? " " : "", cpl.stats.protection[i]);
		}

		/* Show a tooltip with the protection's full name. */
		if (mx >= widget->x + x && mx < widget->x + x + 38 && my >= widget->y + y && my < widget->y + y + 15)
		{
			tooltip_create(mx, my, FONT_ARIAL10, s_settings->protection_full[i]);
		}

		x += 38;
	}

	widget->redraw = 0;
	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/**
 * Initialize one protections widget. */
void widget_protections_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

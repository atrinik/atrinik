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
 * Implements skill experience type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box;
	static uint32 action_tick = 0;

	/* Pre-emptively tick down the skill delay timer */
	if (cpl.action_timer > 0)
	{
		if (LastTick - action_tick > 125)
		{
			cpl.action_timer -= (float) (LastTick - action_tick) / 1000.0f;

			if (cpl.action_timer < 0)
			{
				cpl.action_timer = 0;
			}

			action_tick = LastTick;
			WIDGET_REDRAW(widget);
		}
	}
	else
	{
		action_tick = LastTick;
	}

	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("skill_exp_bg");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	if (widget->redraw)
	{
		widget->redraw = 0;

		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("skill_exp_bg"));

		text_show(widget->surface, FONT_ARIAL10, "Used", 4, 0, COLOR_HGOLD, TEXT_OUTLINE, NULL);
		text_show(widget->surface, FONT_ARIAL10, "Skill", 5, 9, COLOR_HGOLD, TEXT_OUTLINE, NULL);

		text_show_format(widget->surface, FONT_ARIAL10, 40, 0, COLOR_WHITE, 0, NULL, "%1.2f sec", cpl.action_timer);
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/**
 * Initialize one skill experience widget. */
void widget_skill_exp_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

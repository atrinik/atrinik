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
 * Implements the protections table widget. */

#include <include.h>

/**
 * Show the protection table widget.
 * @param widget The widget. */
void widget_show_resist(widgetdata *widget)
{
	SDL_Rect box;
	size_t i;
	int x = 0, y = 2, mx, my;

	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_RESIST_BG]->bitmap, Bitmaps[BITMAP_RESIST_BG]->bitmap->format, Bitmaps[BITMAP_RESIST_BG]->bitmap->flags);
	}

	if (widget->redraw)
	{
		_BLTFX bltfx;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_RESIST_BG], 0, 0, NULL, &bltfx);
		string_blt(widget->widgetSF, FONT_SANS7, s_settings->text[SERVER_TEXT_PROTECTION_GROUPS], 5, 1, COLOR_SIMPLE(COLOR_HGOLD), TEXT_MARKUP | TEXT_OUTLINE, NULL);
	}

	SDL_GetMouseState(&mx, &my);

	for (i = 0; i < sizeof(cpl.stats.protection) / sizeof(*cpl.stats.protection); i++)
	{
		if (!(i % 5))
		{
			y += 15;
			x = 43;
		}

		if (widget->redraw)
		{
			string_blt(widget->widgetSF, FONT_SANS8, s_settings->protection_letters[i], x + 2 - string_get_width(FONT_SANS8, s_settings->protection_letters[i], 0) / 2, y + 2, COLOR_SIMPLE(COLOR_HGOLD), 0, NULL);
			string_blt_format(widget->widgetSF, FONT_MONO8, x + 10, y + 2, COLOR_SIMPLE(cpl.stats.protection[i] ? (cpl.stats.protection[i] < 0 ? COLOR_RED : (cpl.stats.protection[i] >= 100 ? COLOR_ORANGE : COLOR_WHITE)) : COLOR_GREY), 0, NULL, "%02d", cpl.stats.protection[i]);
		}

		/* Show a tooltip with the protection's full name. */
		if (mx > widget->x1 + x && mx < widget->x1 + x + 25 && my > widget->y1 + y && my < widget->y1 + y + 15)
		{
			tooltip_create(mx, my, FONT_ARIAL10, s_settings->protection_full[i]);
		}

		x += 30;
	}

	widget->redraw = 0;
	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

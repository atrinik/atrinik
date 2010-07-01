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

/** Few letters representation of a protection ID */
static char *protections[20] =
{
	"I", 	"S", 	"C", 	"P", 	"W",
	"F",	"C",	"E",	"P",	"A",
	"M",	"Mi",	"B",	"P",	"F",
	"N",	"Ch",	"D",	"Sp",	"Co"
};

/**
 * Show the protection table widget.
 * @param x X position of the widget
 * @param y Y position of the widget */
void widget_show_resist(widgetdata *widget)
{
	char buf[12];
	SDL_Rect box;
	_BLTFX bltfx;
	int protectionID, protection_x = 0, protection_y = 2;

	if (!widget->widgetSF)
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_RESIST_BG]->bitmap, Bitmaps[BITMAP_RESIST_BG]->bitmap->format, Bitmaps[BITMAP_RESIST_BG]->bitmap->flags);

	if (widget->redraw)
	{
		widget->redraw = 0;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_RESIST_BG], 0, 0, NULL, &bltfx);

		StringBlt(widget->widgetSF, &Font6x3Out, "Protection Table", 5, 1, COLOR_HGOLD, NULL, NULL);

		/* This is a dynamic protection table, unlike the old one.
		 * It reduces the code by a considerable amount. */
		for (protectionID = 0; protectionID < (int) sizeof(cpl.stats.protection) / 2; protectionID++)
		{
			/* We have 4 groups of protections. That means we
			 * will need 4 lines to output them all. Adjust
			 * the x and y for it. */
			if (protectionID == 0 || protectionID == 5 || protectionID == 10 || protectionID == 15)
			{
				protection_y += 15;
				protection_x = 43;
			}

			/* Switch the protection ID, so we can output the groups. */
			switch (protectionID)
			{
					/* Physical */
				case 0:
					StringBlt(widget->widgetSF, &Font6x3Out, "Physical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Elemental */
				case 5:
					StringBlt(widget->widgetSF, &Font6x3Out, "Elemental", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Magical */
				case 10:
					StringBlt(widget->widgetSF, &Font6x3Out, "Magical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Spherical */
				case 15:
					StringBlt(widget->widgetSF, &Font6x3Out, "Spherical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;
			}

			/* Output the protection few letters name from the table 'protections'. */
			StringBlt(widget->widgetSF, &SystemFont, protections[protectionID], protection_x + 2 - (int) strlen(protections[protectionID]) * 2, protection_y, COLOR_HGOLD, NULL, NULL);

			/* Now output the protection value. No protection will be drawn gray,
			 * some protection will be white, immunity (100%) will be orange, and
			 * negative will be red. */
			snprintf(buf, sizeof(buf), "%02d", cpl.stats.protection[protectionID]);
			StringBlt(widget->widgetSF, &SystemFont, buf, protection_x + 10, protection_y, cpl.stats.protection[protectionID] ? (cpl.stats.protection[protectionID] < 0 ? COLOR_RED : (cpl.stats.protection[protectionID] >= 100 ? COLOR_ORANGE : COLOR_WHITE)) : COLOR_GREY, NULL, NULL);

			protection_x += 30;
		}
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);
}

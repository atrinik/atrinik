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
 * Implements player doll type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Player doll item positions.
 *
 * Used to determine where to put item sprites on the player doll. */
static int player_doll_positions[PLAYER_EQUIP_MAX][2] =
{
	{22, 6},
	{22, 44},
	{22, 82},
	{22, 120},
	{22, 158},

	{62, 6},
	{62, 44},
	{62, 82},
	{62, 120},
	{62, 158},

	{102, 6},
	{102, 44},
	{102, 82},
	{102, 120},
	{102, 158}
};

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	char *tooltip_text;
	int i, xpos, ypos, mx, my;
	SDL_Surface *texture_slot_border;

	tooltip_text = NULL;
	widget->redraw++;

	text_show(widget->surface, FONT_SANS12, "<b>Ranged</b>", 20, 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	text_show(widget->surface, FONT_ARIAL10, "DMG", 9, 205, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 40, 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_dam);
	text_show(widget->surface, FONT_ARIAL10, "WC", 10, 215, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 40, 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_wc);
	text_show(widget->surface, FONT_ARIAL10, "WS", 10, 225, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 40, 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.ranged_ws / 1000.0);

	text_show(widget->surface, FONT_SANS12, "<b>Melee</b>", 155, 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	text_show(widget->surface, FONT_ARIAL10, "DMG", 139, 205, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 170, 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.dam);
	text_show(widget->surface, FONT_ARIAL10, "WC", 140, 215, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 170, 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.wc);
	text_show(widget->surface, FONT_ARIAL10, "WS", 140, 225, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 170, 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.weapon_speed);

	text_show(widget->surface, FONT_ARIAL10, "Speed", 92, 193, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 93, 205, COLOR_WHITE, 0, NULL, "%3.2f", (float) cpl.stats.speed / FLOAT_MULTF);
	text_show(widget->surface, FONT_ARIAL10, "AC", 92, 215, COLOR_HGOLD, 0, NULL);
	text_show_format(widget->surface, FONT_MONO10, 92, 225, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ac);

	texture_slot_border = TEXTURE_CLIENT("player_doll_slot_border");

	for (i = 0; i < PLAYER_EQUIP_MAX; i++)
	{
		rectangle_create(widget->surface, player_doll_positions[i][0], player_doll_positions[i][1], texture_slot_border->w, texture_slot_border->h, PLAYER_DOLL_SLOT_COLOR);
	}

	surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("player_doll"));

	SDL_GetMouseState(&mx, &my);

	for (i = 0; i < PLAYER_EQUIP_MAX; i++)
	{
		surface_show(widget->surface, player_doll_positions[i][0], player_doll_positions[i][1], NULL, texture_slot_border);

		if (!cpl.equipment[i])
		{
			continue;
		}

		xpos = player_doll_positions[i][0] + 2;
		ypos = player_doll_positions[i][1] + 2;

		object_show_centered(widget->surface, cpl.equipment[i], xpos, ypos);

		/* Prepare item name tooltip */
		if (mx - widget->x > xpos && mx - widget->x <= xpos + INVENTORY_ICON_SIZE && my - widget->y > ypos && my - widget->y <= ypos + INVENTORY_ICON_SIZE)
		{
			tooltip_text = cpl.equipment[i]->s_name;
		}
	}

	/* Draw item name tooltip */
	if (tooltip_text)
	{
		tooltip_create(mx, my, FONT_ARIAL10, tooltip_text);
	}
}

void widget_playerdoll_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
}

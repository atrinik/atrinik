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
static int player_doll_positions[PLAYER_DOLL_MAX][2] =
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

void player_doll_update_items(void)
{
	object *tmp;
	int i, ring_num;

	memset(&cpl.player_doll, 0, sizeof(cpl.player_doll));

	ring_num = 0;

	for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
	{
		if (!(tmp->flags & CS_FLAG_APPLIED) && !(tmp->flags & CS_FLAG_IS_READY))
		{
			continue;
		}

		if (tmp->flags & CS_FLAG_IS_READY)
		{
			i = PLAYER_DOLL_AMMO;
		}
		else if (tmp->itype == TYPE_AMULET)
		{
			i = PLAYER_DOLL_AMULET;
		}
		else if (tmp->itype == TYPE_WEAPON)
		{
			i = PLAYER_DOLL_WEAPON;
		}
		else if (tmp->itype == TYPE_GLOVES)
		{
			i = PLAYER_DOLL_GAUNTLETS;
		}
		else if (tmp->itype == TYPE_RING && ring_num == 0)
		{
			i = PLAYER_DOLL_RING_RIGHT;
			ring_num++;
		}
		else if (tmp->itype == TYPE_HELMET)
		{
			i = PLAYER_DOLL_HELM;
		}
		else if (tmp->itype == TYPE_ARMOUR)
		{
			i = PLAYER_DOLL_ARMOUR;
		}
		else if (tmp->itype == TYPE_GIRDLE)
		{
			i = PLAYER_DOLL_BELT;
		}
		else if (tmp->itype == TYPE_GREAVES)
		{
			i = PLAYER_DOLL_GREAVES;
		}
		else if (tmp->itype == TYPE_BOOTS)
		{
			i = PLAYER_DOLL_BOOTS;
		}
		else if (tmp->itype == TYPE_CLOAK)
		{
			i = PLAYER_DOLL_CLOAK;
		}
		else if (tmp->itype == TYPE_BRACERS)
		{
			i = PLAYER_DOLL_BRACERS;
		}
		else if (tmp->itype == TYPE_SHIELD)
		{
			i = PLAYER_DOLL_SHIELD;
		}
		else if (tmp->itype == TYPE_LIGHT_APPLY)
		{
			i = PLAYER_DOLL_LIGHT;
		}
		else if (tmp->itype == TYPE_RING && ring_num == 1)
		{
			i = PLAYER_DOLL_RING_LEFT;
			ring_num++;
		}
		else
		{
			continue;
		}

		cpl.player_doll[i] = tmp;
	}
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	char *tooltip_text;
	int i, xpos, ypos, mx, my;
	SDL_Surface *texture_slot_border;

	tooltip_text = NULL;

	surface_show(ScreenSurface, widget->x, widget->y, NULL, TEXTURE_CLIENT("player_doll_bg"));

	text_show(ScreenSurface, FONT_SANS12, "<b>Ranged</b>", widget->x + 20, widget->y + 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	text_show(ScreenSurface, FONT_ARIAL10, "DMG", widget->x + 9, widget->y + 205, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 40, widget->y + 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_dam);
	text_show(ScreenSurface, FONT_ARIAL10, "WC", widget->x + 10, widget->y + 215, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 40, widget->y + 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ranged_wc);
	text_show(ScreenSurface, FONT_ARIAL10, "WS", widget->x + 10, widget->y + 225, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 40, widget->y + 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.ranged_ws / 1000.0);

	text_show(ScreenSurface, FONT_SANS12, "<b>Melee</b>", widget->x + 155, widget->y + 188, COLOR_HGOLD, TEXT_MARKUP, NULL);
	text_show(ScreenSurface, FONT_ARIAL10, "DMG", widget->x + 139, widget->y + 205, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 170, widget->y + 205, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.dam);
	text_show(ScreenSurface, FONT_ARIAL10, "WC", widget->x + 140, widget->y + 215, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 170, widget->y + 215, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.wc);
	text_show(ScreenSurface, FONT_ARIAL10, "WS", widget->x + 140, widget->y + 225, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 170, widget->y + 225, COLOR_WHITE, 0, NULL, "%3.2fs", cpl.stats.weapon_speed);

	text_show(ScreenSurface, FONT_ARIAL10, "Speed", widget->x + 92, widget->y + 193, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 93, widget->y + 205, COLOR_WHITE, 0, NULL, "%3.2f", (float) cpl.stats.speed / FLOAT_MULTF);
	text_show(ScreenSurface, FONT_ARIAL10, "AC", widget->x + 92, widget->y + 215, COLOR_HGOLD, 0, NULL);
	text_show_format(ScreenSurface, FONT_MONO10, widget->x + 92, widget->y + 225, COLOR_WHITE, 0, NULL, "%02d", cpl.stats.ac);

	texture_slot_border = TEXTURE_CLIENT("player_doll_slot_border");

	for (i = 0; i < PLAYER_DOLL_MAX; i++)
	{
		rectangle_create(ScreenSurface, widget->x + player_doll_positions[i][0], widget->y + player_doll_positions[i][1], texture_slot_border->w, texture_slot_border->h, PLAYER_DOLL_SLOT_COLOR);
	}

	surface_show(ScreenSurface, widget->x, widget->y, NULL, TEXTURE_CLIENT("player_doll"));

	SDL_GetMouseState(&mx, &my);

	for (i = 0; i < PLAYER_DOLL_MAX; i++)
	{
		surface_show(ScreenSurface, widget->x + player_doll_positions[i][0], widget->y + player_doll_positions[i][1], NULL, texture_slot_border);

		if (!cpl.player_doll[i])
		{
			continue;
		}

		xpos = widget->x + player_doll_positions[i][0] + 2;
		ypos = widget->y + player_doll_positions[i][1] + 2;

		object_show_centered(cpl.player_doll[i], xpos, ypos);

		/* Prepare item name tooltip */
		if (mx > xpos && mx <= xpos + INVENTORY_ICON_SIZE && my > ypos && my <= widget->y + ypos + INVENTORY_ICON_SIZE)
		{
			tooltip_text = cpl.player_doll[i]->s_name;
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

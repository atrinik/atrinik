/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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

#include <global.h>

/** Default icon length */
#define ICONDEFLEN 32

/**
 * Mouse event on range widget.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @param MEvent Mouse event type (MOUSE_DOWN, MOUSE_UP) */
void widget_range_event(widgetdata *widget, int x, int y, SDL_Event event, int MEvent)
{
	if (x > widget->x1 + 5 && x < widget->x1 + 38 && y >= widget->y1 + 3 && y <= widget->y1 + 33)
	{
		if (MEvent == MOUSE_DOWN)
		{
			if (event.button.button == SDL_BUTTON_LEFT)
			{
				keybind_process_command("?RANGE");
			}
			/* Mousewheel up */
			else if (event.button.button == 4)
			{
				keybind_process_command("?RANGE");
			}
		}
	}
}

/**
 * Show range widget.
 * @param widget The widget. */
void widget_show_range(widgetdata *widget)
{
	char buf[MAX_BUF];
	object *tmp;

	fire_mode_tab[FIRE_MODE_BOW].item = FIRE_ITEM_NO;
	fire_mode_tab[FIRE_MODE_WAND].item = FIRE_ITEM_NO;

	for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
	{
		if (tmp->flags & F_APPLIED)
		{
			if (tmp->itype == TYPE_BOW)
			{
				fire_mode_tab[FIRE_MODE_BOW].item = tmp->tag;
			}
			else if (tmp->itype == TYPE_WAND || tmp->itype == TYPE_ROD || tmp->itype == TYPE_HORN)
			{
				fire_mode_tab[FIRE_MODE_WAND].item = tmp->tag;
			}
		}
	}

	sprite_blt(Bitmaps[BITMAP_RANGE], widget->x1 - 2, widget->y1, NULL, NULL);

	switch (RangeFireMode)
	{
		case FIRE_MODE_BOW:
			if (fire_mode_tab[FIRE_MODE_BOW].item != FIRE_ITEM_NO)
			{
				tmp = object_find_object(cpl.ob, fire_mode_tab[RangeFireMode].item);

				object_blit_centered(tmp, widget->x1 + 3, widget->y1 + 2);
				string_blt(ScreenSurface, FONT_SANS10, tmp->s_name, widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);

				if (fire_mode_tab[FIRE_MODE_BOW].amun != FIRE_ITEM_NO && (tmp = object_find_object(cpl.ob, fire_mode_tab[FIRE_MODE_BOW].amun)))
				{
					if (tmp->itype == TYPE_ARROW)
					{
						snprintf(buf, sizeof(buf), "%s (%d)", tmp->s_name, tmp->nrof);
					}
					else
					{
						snprintf(buf, sizeof(buf), "%s (%4.3f kg)", tmp->s_name, tmp->weight);
					}

					object_blit_centered(tmp, widget->x1 + 43, widget->y1 + 2);
					string_blt(ScreenSurface, FONT_SANS10, buf, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
				}
			}
			else
			{
				string_blt(ScreenSurface, FONT_SANS10, "no range weapon applied", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_RANGE_MARKER], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
			break;

		/* Wands, staves, rods and horns */
		case FIRE_MODE_WAND:
			if (fire_mode_tab[FIRE_MODE_WAND].item != FIRE_ITEM_NO && (tmp = object_find_object(cpl.ob, fire_mode_tab[FIRE_MODE_WAND].item)))
			{
				string_blt(ScreenSurface, FONT_SANS10, tmp->s_name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				object_blit_centered(tmp, widget->x1 + 43, widget->y1 + 2);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				string_blt(ScreenSurface, FONT_SANS10, "nothing applied", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}

			string_blt(ScreenSurface, FONT_SANS10, "use range tool", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);
			break;

		/* These are client only, no server signal needed */
		case FIRE_MODE_SKILL:
			if (fire_mode_tab[FIRE_MODE_SKILL].skill)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				blit_face(fire_mode_tab[FIRE_MODE_SKILL].skill->icon, widget->x1 + 43, widget->y1 + 2);
				string_blt(ScreenSurface, FONT_SANS10, fire_mode_tab[FIRE_MODE_SKILL].skill->name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				string_blt(ScreenSurface, FONT_SANS10, "no skill selected", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}

			string_blt(ScreenSurface, FONT_SANS10, "use skill", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);
			break;

		case FIRE_MODE_SPELL:
			if (fire_mode_tab[FIRE_MODE_SPELL].spell)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				blit_face(fire_mode_tab[FIRE_MODE_SPELL].spell->icon, widget->x1 + 43, widget->y1 + 2);
				string_blt(ScreenSurface, FONT_SANS10, fire_mode_tab[FIRE_MODE_SPELL].spell->name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				string_blt(ScreenSurface, FONT_SANS10, "no spell selected", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}

			string_blt(ScreenSurface, FONT_SANS10, "cast spell", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);
			break;

		case FIRE_MODE_THROW:
			if (fire_mode_tab[FIRE_MODE_THROW].item != FIRE_ITEM_NO && (tmp = object_find_object(cpl.ob, fire_mode_tab[FIRE_MODE_THROW].item)))
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				object_blit_centered(tmp, widget->x1 + 43, widget->y1 + 2);

				if (tmp->nrof > 1)
				{
					if (tmp->nrof > 9999)
					{
						snprintf(buf, sizeof(buf), "many");
					}
					else
					{
						snprintf(buf, sizeof(buf), "%d", tmp->nrof);
					}

					string_blt(ScreenSurface, FONT_SANS7, buf, widget->x1 + 43 + ICONDEFLEN / 2 - string_get_width(FONT_SANS7, buf, TEXT_OUTLINE) / 2, widget->y1 + 2 + ICONDEFLEN - FONT_HEIGHT(FONT_SANS7), COLOR_WHITE, TEXT_OUTLINE, NULL);
				}

				string_blt(ScreenSurface, FONT_SANS10, tmp->s_name, widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW_NO], widget->x1 + 3, widget->y1 + 2, NULL, NULL);
				string_blt(ScreenSurface, FONT_SANS10, "no item ready", widget->x1 + 3, widget->y1 + 46, COLOR_WHITE, 0, NULL);
			}

			string_blt(ScreenSurface, FONT_SANS10, "throw item", widget->x1 + 3, widget->y1 + 35, COLOR_WHITE, 0, NULL);
			break;
	}
}

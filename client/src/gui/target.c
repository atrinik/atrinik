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
 *  */

#include <global.h>

/**
 * Handle mouse events for target widget.
 * @param x Mouse X position
 * @param y Mouse Y position */
void widget_event_target(widgetdata *widget, int x, int y)
{
	/* Combat modes */
	if (y > widget->y1 + 3 && y < widget->y1 + 38 && x > widget->x1 + 3 && x < widget->x1 + 30)
		keybind_process_command("?COMBAT");

	/* Talk button */
	if (y > widget->y1 + 7 && y < widget->y1 + 25 && x > widget->x1 + 223 && x < widget->x1 + 259)
	{
		if (cpl.target_code)
		{
			keybind_process_command("?HELLO");
		}
	}
}

/**
 * Show target widget.
 * @param x X position of the target
 * @param y Y position of the target */
void widget_show_target(widgetdata *widget)
{
	char *ptr = NULL;
	SDL_Rect box;
	double temp;
	int hp_tmp;

	sprite_blt(Bitmaps[BITMAP_TARGET_BG], widget->x1, widget->y1, NULL, NULL);

	sprite_blt(Bitmaps[cpl.target_mode ? BITMAP_TARGET_ATTACK : BITMAP_TARGET_NORMAL], widget->x1 + 5, widget->y1 + 4, NULL, NULL);

	sprite_blt(Bitmaps[BITMAP_TARGET_HP_B], widget->x1 + 4, widget->y1 + 24, NULL, NULL);

	hp_tmp = (int) cpl.target_hp;

	/* Redirect target_hp to our hp - server doesn't send it
	 * because we should know our hp exactly */
	if (cpl.target_code == 0)
		hp_tmp = (int)(((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100.0f);

	if (cpl.target_code == 0)
	{
		if (cpl.target_mode)
			ptr = "target self (hold attack)";
		else
			ptr = "target self";
	}
	else if (cpl.target_code == 1)
	{
		if (cpl.target_mode)
			ptr = "target and attack enemy";
		else
			ptr = "target enemy";
	}
	else if (cpl.target_code == 2)
	{
		if (cpl.target_mode)
			ptr = "target friend (hold attack)";
		else
			ptr = "target friend";
	}

	if (cpl.target_code)
	{
		sprite_blt(Bitmaps[BITMAP_TARGET_TALK], widget->x1 + 223, widget->y1 + 7, NULL, NULL);
	}

	if (setting_get_int(OPT_CAT_GENERAL, OPT_TARGET_SELF) || cpl.target_code != 0)
	{
		if (hp_tmp)
		{
			temp = (double) hp_tmp * 0.01;
			box.x = 0;
			box.y = 0;
			box.h = Bitmaps[BITMAP_TARGET_HP]->bitmap->h;
			box.w = (int) (Bitmaps[BITMAP_TARGET_HP]->bitmap->w * temp);

			if (!box.w)
			{
				box.w = 1;
			}

			if (box.w > Bitmaps[BITMAP_TARGET_HP]->bitmap->w)
			{
				box.w = Bitmaps[BITMAP_TARGET_HP]->bitmap->w;
			}

			sprite_blt(Bitmaps[BITMAP_TARGET_HP], widget->x1 + 5, widget->y1 + 25, &box, NULL);
		}

		if (ptr && cpl.target_name[0] != '\0')
		{
			/* Draw the name of the target */
			string_blt(ScreenSurface, FONT_ARIAL10, cpl.target_name, widget->x1 + 35, widget->y1 + 2, cpl.target_color, 0, NULL);

			/* Either draw HP remaining percent and description... */
			if (hp_tmp > 0)
			{
				char hp_text[MAX_BUF];
				const char *hp_color;

				snprintf(hp_text, sizeof(hp_text), "HP: %d%%", hp_tmp);

				if (hp_tmp > 90)
				{
					hp_color = COLOR_GREEN;
				}
				else if (hp_tmp > 75)
				{
					hp_color = COLOR_DGOLD;
				}
				else if (hp_tmp > 50)
				{
					hp_color = COLOR_HGOLD;
				}
				else if (hp_tmp > 25)
				{
					hp_color = COLOR_ORANGE;
				}
				else if (hp_tmp > 10)
				{
					hp_color = COLOR_YELLOW;
				}
				else
				{
					hp_color = COLOR_RED;
				}

				string_blt(ScreenSurface, FONT_ARIAL10, hp_text, widget->x1 + 35, widget->y1 + 14, hp_color, 0, NULL);
				string_blt(ScreenSurface, FONT_ARIAL10, ptr, widget->x1 + 85, widget->y1 + 14, cpl.target_color, 0, NULL);
			}
			/* Or draw just the description */
			else
			{
				string_blt(ScreenSurface, FONT_ARIAL10, ptr, widget->x1 + 35, widget->y1 + 14, cpl.target_color, 0, NULL);
			}
		}
	}
}

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

#define TXT_START_LEVEL 370
#define TXT_START_EXP   400

/** The skill groups. */
static const char *skill_tab[] =
{
	"Agility",
	"Mental",
	"Magic",
	"Person",
	"Physique",
	"Wisdom",
	"Misc",
	NULL
};

/**
 * Show the skill list window. */
void show_skilllist()
{
	SDL_Rect box;
	char buf[256];
	int x, y, i;
	int mx, my, mb;
	static int active = 0, dblclk = 0;
	static Uint32 Ticks = 0;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x= Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y= Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_SKILL], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_SKILL]->bitmap->w / 2, y + 14, NULL, NULL);
	add_close_button(x, y, MENU_SKILL);

	/* Tabs */
	draw_tabs(skill_tab, &skill_list_set.group_nr, "Skill Group", x + 8, y + 70);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select group                  ~%c%c~ to select skill                    ~RETURN~ for use", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

	/* Headline */
	StringBlt(ScreenSurface, &SystemFont, "Name", x + TXT_START_NAME + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Name", x + TXT_START_NAME, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Level", x + TXT_START_LEVEL + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Level", x + TXT_START_LEVEL, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Experience", x + TXT_START_EXP + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Experience", x + TXT_START_EXP, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);

	/* Print skill entries */
	if (!(mb & SDL_BUTTON(SDL_BUTTON_LEFT)))
	{
		active = 0;
	}

	if (mx > x + TXT_START_NAME && mx < x + TXT_START_NAME + 327 && my > y + TXT_Y_START && my < y + 12 + TXT_Y_START + DIALOG_LIST_ENTRY * 12)
	{
		if (!mb)
		{
			if (dblclk == 1)
			{
				dblclk = 2;
			}

			if (dblclk == 3)
			{
				dblclk = 0;

				if (skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].flag == LIST_ENTRY_KNOWN)
				{
					check_menu_keys(MENU_SKILL, SDLK_RETURN);
				}
			}
		}
		else
		{
			if (dblclk == 0)
			{
				dblclk = 1;
				Ticks = SDL_GetTicks();
			}

			if (dblclk == 2)
			{
				dblclk = 3;

				if (SDL_GetTicks() - Ticks > 300)
				{
					dblclk = 0;
				}
			}

			/* mb was pressed in the selection field */
			if (mb_clicked)
			{
				active = 1;
			}

			if (active && skill_list_set.entry_nr != (my - y - 12 - TXT_Y_START) / 12)
			{
				skill_list_set.entry_nr = (my - y - 12 - TXT_Y_START) / 12;

				dblclk = 0;
			}
		}
	}

	for (i = 0; i < DIALOG_LIST_ENTRY; i++)
	{
		y += 12;
		box.y += 12;

		if (i != skill_list_set.entry_nr)
		{
			if (i & 1)
			{
				SDL_FillRect(ScreenSurface, &box, sdl_gray2);
			}
			else
			{
				SDL_FillRect(ScreenSurface, &box, sdl_gray1);
			}
		}
		else
		{
			SDL_FillRect(ScreenSurface, &box, sdl_blue1);
		}

		if (skill_list[skill_list_set.group_nr].entry[i].flag == LIST_ENTRY_KNOWN)
		{
			StringBlt(ScreenSurface, &SystemFont, skill_list[skill_list_set.group_nr].entry[i].name, x + TXT_START_NAME, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (skill_list[skill_list_set.group_nr].entry[i].exp == -1)
			{
				strcpy(buf, "**");
			}
			else
			{
				sprintf(buf, "%d", skill_list[skill_list_set.group_nr].entry[i].exp_level);
			}

			StringBlt(ScreenSurface, &SystemFont, buf, x + TXT_START_LEVEL, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (skill_list[skill_list_set.group_nr].entry[i].exp == -1)
			{
				strcpy(buf, "**");
			}
			else if (skill_list[skill_list_set.group_nr].entry[i].exp == -2)
			{
				strcpy(buf, "**");
			}
			else
			{
				sprintf(buf, "%"FMT64, skill_list[skill_list_set.group_nr].entry[i].exp);
			}

			StringBlt(ScreenSurface, &SystemFont, buf, x + TXT_START_EXP, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
		}
	}

	x += 160;
	y += 120;

	/* Print skill description */
	if (skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].flag >= LIST_ENTRY_KNOWN)
	{
		/* Selected */
		if ((mb & SDL_BUTTON(SDL_BUTTON_LEFT)) && mx > x - 40 && mx < x - 10 && my > y + 10 && my < y + 43)
		{
			check_menu_keys(MENU_SKILL, SDLK_RETURN);
		}

		sprite_blt(skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].icon, x - 42, y + 10, NULL, NULL);

		/* Print textblock */
		for (i = 0; i <= 3; i++)
		{
			StringBlt(ScreenSurface, &SystemFont, &skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].desc[i][0], x - 2, y + 1, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, &skill_list[skill_list_set.group_nr].entry[skill_list_set.entry_nr].desc[i][0], x - 3, y, COLOR_WHITE, NULL, NULL);
			y += 13;
		}
	}

	if (!mb)
	{
		active_button = -1;
	}
}

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

#define X_COL1 136
#define X_COL2 290
#define X_COL3 430

/**
 * Show the keybinds window. */
void show_keybind()
{
	SDL_Rect box;
	char buf[256];
	int x, x2, y, y2, i;
	int mx, my, mb;
	int numButton =0;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_KEYBIND], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_KEYBIND]->bitmap->w / 2, y + 17, NULL, NULL);
	add_close_button(x, y, MENU_KEYBIND);

	sprintf(buf, "~SHIFT~ + ~%c%c~ to select group         ~%c%c~ to select macro          ~RETURN~ to change/create", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 125, y + 410, COLOR_WHITE, NULL, NULL);

	/* Draw group tabs */
	i = 0;
	x2 = x + 8;
	y2 = y + 70;

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_START], x2, y2 - 10, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Group", x2 + 15, y2 + 4, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Group", x2 + 14, y2 + 3, COLOR_WHITE, NULL, NULL);

	y2 += 17;
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);
	y2 += 17;

	while (i < BINDKEY_LIST_MAX && bindkey_list[i].name[0])
	{
		sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x2, y2, NULL, NULL);

		if (i == bindkey_list_set.group_nr)
		{
			sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_SEL], x2, y2, NULL, NULL);
		}

		StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 25, y2 + 4, COLOR_BLACK, NULL, NULL);

		if (mx > x2 && mx < x2 + 100 && my > y2 && my < y2 + 17)
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 24, y2 + 3, COLOR_HGOLD, NULL, NULL);

			if (mb && bindkey_list_set.group_nr != i)
			{
				bindkey_list_set.group_nr = i;
			}
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[i].name, x2 + 24, y2 + 3, COLOR_WHITE, NULL, NULL);
		}

		y2 += 17;
		i++;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_STOP], x2, y2, NULL, NULL);

	/* Headline */
	StringBlt(ScreenSurface, &SystemFont, "Macro", x + X_COL1 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Macro", x + X_COL1, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Key", x + X_COL2 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Key", x + X_COL2, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Repeat", x+X_COL3 + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "~R~epeat", x+X_COL3, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	/* Save button */
	if (add_button(x + 25, y + 454, numButton++, BITMAP_DIALOG_BUTTON_UP, "Done", "~D~one"))
	{
		check_menu_keys(MENU_KEYBIND, SDLK_d);
	}

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);
	y2 = y;

	/* Print keybind entries */
	for (i = 0; i <OPTWIN_MAX_OPT; i++)
	{
		y += 12;
		box.y += 12;

		if (mb && mx > x + TXT_START_NAME && mx < x + TXT_START_NAME + 327 && my > y + TXT_Y_START && my < y + 12 + TXT_Y_START)
		{
			/* Cancel edit */
			if (mb & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				InputStringEscFlag = 1;
				keybind_status = KEYBIND_STATUS_NO;
			}

			bindkey_list_set.entry_nr = i;

			if ((mb & SDL_BUTTON(SDL_BUTTON_RIGHT)) && mb_clicked && mx > x + X_COL1)
			{
				/* Macro name + key value */
				if (mx < x + X_COL3)
				{
					check_menu_keys(MENU_KEYBIND, SDLK_RETURN);
				}
				/* Key repeat */
				else
				{
					check_menu_keys(MENU_KEYBIND, SDLK_r);
				}

				mb_clicked = 0;
			}
		}

		if (i != bindkey_list_set.entry_nr)
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

		if (bindkey_list[bindkey_list_set.group_nr].entry[i].text[0])
		{
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[bindkey_list_set.group_nr].entry[i].text, x + X_COL1, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, bindkey_list[bindkey_list_set.group_nr].entry[i].keyname, x + X_COL2, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);

			if (bindkey_list[bindkey_list_set.group_nr].entry[i].repeatflag)
			{
				StringBlt(ScreenSurface, &SystemFont, "on", x + X_COL3, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "off", x + X_COL3, y + TXT_Y_START, COLOR_WHITE, NULL, NULL);
			}
		}
	}

	y2 += 13 + TXT_Y_START + bindkey_list_set.entry_nr * 12;
	box.y = y2;

	if (keybind_status == KEYBIND_STATUS_EDIT)
	{
		box.w = X_COL2 - X_COL1;
		SDL_FillRect(ScreenSurface, &box, 0);
		StringBlt(ScreenSurface, &SystemFont, show_input_string(InputString, &SystemFont, box.w), x + X_COL1, y2, COLOR_WHITE, NULL, NULL);
	}
	else if (keybind_status == KEYBIND_STATUS_EDITKEY)
	{
		box.x += X_COL2 - X_COL1;
		box.w = X_COL3 - X_COL2;

		SDL_FillRect(ScreenSurface, &box, 0);
		StringBlt(ScreenSurface, &SystemFont, "Press a key!", x + X_COL2, y2, COLOR_WHITE, NULL, NULL);
	}

	if (!mb)
	{
		active_button = -1;
	}
}

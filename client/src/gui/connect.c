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

/**
 * Show login window. */
void show_login_server()
{
	SDL_Rect box;
	char buf[MAX_BUF];
	int x, y, i;
	int mx, my, mb, t, progress;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = 25;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGO270], x + 20, y + 85, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_LOGIN], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_LOGIN]->bitmap->w / 2, y + 17, NULL, NULL);

	t = x + 275;
	x += 170;
	y += 100;
	draw_frame(ScreenSurface, x - 3, y - 3, 211, 168);
	box.x = x - 2;
	box.y = y - 2;
	box.w = 210;
	box.h = 17;

	StringBlt(ScreenSurface, &SystemFont, "Server", t + 1 - 21, y - 35, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Server", t - 21 , y - 36, COLOR_WHITE, NULL, NULL);

	t -= get_string_pixel_length(selected_server->name, &BigFont) / 2;
	StringBlt(ScreenSurface, &BigFont, selected_server->name, t + 1, y - 21, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &BigFont, selected_server->name, t, y - 22, COLOR_HGOLD, NULL, NULL);

	SDL_FillRect(ScreenSurface, &box, sdl_gray3);
	box.y = y + 15;
	box.h = 150;
	SDL_FillRect(ScreenSurface, &box, sdl_gray4);

	StringBlt(ScreenSurface, &SystemFont, "- UPDATING FILES- ", x + 58, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "- UPDATING FILES -", x + 57, y, COLOR_WHITE, NULL, NULL);

	/* Update progress bar of requested files */
	sprite_blt(Bitmaps[BITMAP_PROGRESS_BACK], x, y + (168 - Bitmaps[BITMAP_PROGRESS_BACK]->bitmap->h - 5), NULL, NULL);

	progress = MIN(100, 8);
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_PROGRESS]->bitmap->h;
	box.w = (int) ((float) Bitmaps[BITMAP_PROGRESS]->bitmap->w / 100 * progress);
	sprite_blt(Bitmaps[BITMAP_PROGRESS], x, y + (168 - Bitmaps[BITMAP_PROGRESS]->bitmap->h - 5), &box, NULL);

	/* Login user part */
	if (GameStatus == GAME_STATUS_REQUEST_FILES || !file_updates_finished())
	{
		return;
	}

	StringBlt(ScreenSurface, &SystemFont, "done.", x + 2, y + 116, COLOR_WHITE, NULL, NULL);
	y += 180;

	StringBlt(ScreenSurface, &SystemFont, "Enter your name", x, y, COLOR_HGOLD, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 15, NULL, NULL);

	if (GameStatus == GAME_STATUS_NAME)
	{
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));
		buf[0] = toupper(buf[0]);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 17, COLOR_WHITE, NULL, NULL);
	}
	else
	{
		cpl.name[0] = toupper(cpl.name[0]);
		StringBlt(ScreenSurface, &SystemFont, cpl.name, x + 2, y + 17, COLOR_WHITE, NULL, NULL);
	}

	StringBlt(ScreenSurface, &SystemFont, "Enter your password", x + 2, y + 40, COLOR_HGOLD, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 55, NULL, NULL);

	if (GameStatus == GAME_STATUS_PSWD)
	{
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));

		for (i = 0; i < CurrentCursorPos; i++)
		{
			buf[i] = '*';
		}

		for (i = CurrentCursorPos + 1; i < (int) strlen(InputString) + 1; i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 57, COLOR_WHITE, NULL, NULL);
	}
	else
	{
		for (i = 0; i < (int) strlen(cpl.password); i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 57, COLOR_WHITE, NULL, NULL);
	}

	if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		StringBlt(ScreenSurface, &SystemFont, "New Character: Verify Password", x + 2, y + 80, COLOR_HGOLD, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_LOGIN_INP], x - 2, y + 95, NULL, NULL);
		strcpy(buf, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_LOGIN_INP]->bitmap->w - 16));

		for (i = 0; i < (int) strlen(InputString); i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';
		StringBlt(ScreenSurface, &SystemFont, buf, x + 2, y + 97, COLOR_WHITE, NULL, NULL);
	}

	y += 160;
	StringBlt(ScreenSurface, &SystemFont, "To start playing enter your character ~name~ and ~password~.", x - 10, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "To start playing enter your character ~name~ and ~password~.", x - 11, y, COLOR_WHITE, NULL, NULL);

	y += 12;
	StringBlt(ScreenSurface, &SystemFont, "You will be asked to ~verify~ the password for new characters.", x - 10, y + 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "You will be asked to ~verify~ the password for new characters.", x - 11, y, COLOR_WHITE, NULL, NULL);
}

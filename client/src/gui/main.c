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
 * Show metaserver window. */
void show_meta_server()
{
	int x, y;
	char buf[1024];
	SDL_Rect rec_name, rec_desc, box;
	int mx, my, mb;
	int i, count = server_get_count();
	server_struct *node;

	mb = SDL_GetMouseState(&mx, &my);

	/* Background */
	x = 25;
	y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;

	sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_LOGO270], x + 20, y + 85, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_SERVER], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_SERVER]->bitmap->w / 2, y + 15, NULL, NULL);

	rec_name.w = 272;
	rec_desc.w = 325;

	box.x = x + 133;
	box.y = y + TXT_Y_START + 1;
	box.w = 329;
	box.h = 12;

	/* Frame for selection field */
	draw_frame(box.x - 1, box.y + 11, box.w + 1, 313);

	/* Frame for scrollbar */
	draw_frame(box.x + box.w + 4, box.y + 11, 10, 313);

	/* Show scrollbar, and adjust its position by yoff */
	blt_window_slider(Bitmaps[BITMAP_SLIDER_LONG], count - 18, 8, dialog_yoff, -1, box.x + box.w + 5, box.y + 12);

	if (ms_connecting(-1))
	{
		StringBlt(ScreenSurface, &SystemFont, "Connecting to metaserver, please wait...", x + TXT_START_NAME + 81, y + TXT_Y_START - 13, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Connecting to metaserver, please wait...", x + TXT_START_NAME + 80, y + TXT_Y_START - 14, COLOR_HGOLD, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "Select a server.", x + TXT_START_NAME + 126, y + TXT_Y_START - 13, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Select a server.", x + TXT_START_NAME + 125, y + TXT_Y_START - 14, COLOR_GREEN, NULL, NULL);
	}

	/* we should prepare for this the SystemFontOut */
	StringBlt(ScreenSurface, &SystemFont, "Servers", x + TXT_START_NAME + 1, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Servers", x + TXT_START_NAME, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Port", x + 380, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Port", x + 379, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Players", x + 416, y + TXT_Y_START - 1, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Players", x + 415, y + TXT_Y_START - 2, COLOR_WHITE, NULL, NULL);

	sprintf(buf, "Use cursors ~%c%c~ to select server                                  press ~RETURN~ to connect", ASCII_UP, ASCII_DOWN);
	StringBlt(ScreenSurface, &SystemFont, buf, x + 140, y + 410, COLOR_WHITE, NULL, NULL);

	for (i = 0; i < OPTWIN_MAX_OPT; i++)
	{
		box.y += 12;

		if (i & 1)
		{
			SDL_FillRect(ScreenSurface, &box, sdl_gray2);
		}
		else
		{
			SDL_FillRect(ScreenSurface, &box, sdl_gray1);
		}
	}

	for (i = 0; i < count; i++)
	{
		if (i < dialog_yoff)
		{
			continue;
		}
		/* Never more than maximum */
		else if (i - dialog_yoff >= DIALOG_LIST_ENTRY)
		{
			break;
		}

		node = server_get_id(i);

		if (i == server_sel)
		{
			int tmp_y = 0, width = 0;
			char tmpbuf[MAX_BUF], *cp;

			snprintf(buf, sizeof(buf), "version %s", node->version);

			StringBlt(ScreenSurface, &SystemFont, buf, x + 160, y + 433, COLOR_BLACK, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont, buf, x + 159, y + 432, COLOR_WHITE, NULL, NULL);

			strncpy(tmpbuf, node->desc, sizeof(tmpbuf));
			cp = strtok(tmpbuf, " ");

			/* Loop through spaces */
			while (cp)
			{
				int len = get_string_pixel_length(cp, &SystemFont) + SystemFont.c[' '].w + SystemFont.char_offset;

				/* Do we need to adjust for the next line? */
				if (width + len > MAX_MS_DESC_LINE)
				{
					width = 0;
					tmp_y += 12;

					/* We hit the max */
					if (tmp_y >= MAX_MS_DESC_Y)
					{
						break;
					}
				}

				StringBlt(ScreenSurface, &SystemFont, cp, x + 160 + width, y + 446 + tmp_y, COLOR_BLACK, &rec_desc, NULL);
				StringBlt(ScreenSurface, &SystemFont, cp, x + 159 + width, y + 445 + tmp_y, COLOR_HGOLD, &rec_desc, NULL);
				width += len;

				cp = strtok(NULL, " ");
			}

			box.y = y + TXT_Y_START + 13 + (i - dialog_yoff) * 12;
			SDL_FillRect(ScreenSurface, &box, sdl_blue1);
		}

		StringBlt(ScreenSurface, &SystemFont, node->name, x + 137, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, &rec_name, NULL);

		sprintf(buf, "%d", node->port);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 380, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, NULL, NULL);

		if (node->player >= 0)
		{
			sprintf(buf, "%d", node->player);
		}
		else
		{
			strcpy(buf, "-");
		}

		StringBlt(ScreenSurface, &SystemFont, buf, x + 416, y + 94 + (i - dialog_yoff) * 12, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Called on mouse event in metaserver dialog.
 * @param e SDL event. */
void metaserver_mouse(SDL_Event *e)
{
	/* Mousewheel up/down */
	if (e->button.button == 4 || e->button.button == 5)
	{
		int count = server_get_count();

		/* Scroll down... */
		if (e->button.button == 5)
		{
			if (server_sel < count - 1)
			{
				server_sel++;
			}

			if (server_sel >= DIALOG_LIST_ENTRY + dialog_yoff)
			{
				dialog_yoff++;
			}
		}
		/* .. or up */
		else
		{
			if (server_sel)
			{
				server_sel--;
			}

			if (dialog_yoff > server_sel)
			{
				dialog_yoff--;
			}
		}

		/* Sanity checks for going out of bounds. */
		if (dialog_yoff < 0 || count < DIALOG_LIST_ENTRY)
		{
			dialog_yoff = 0;
		}
		else if (dialog_yoff >= count - DIALOG_LIST_ENTRY)
		{
			dialog_yoff = count - DIALOG_LIST_ENTRY;
		}

		if (server_sel < 0)
		{
			server_sel = 0;
		}
		else if (server_sel >= count)
		{
			server_sel = count;
		}
	}
}

/* Metaserver menu key */
int key_meta_menu(SDL_KeyboardEvent *key)
{
	if (key->type == SDL_KEYDOWN)
	{
		int count = server_get_count();

		switch (key->keysym.sym)
		{
			case SDLK_UP:
				if (server_sel)
				{
					server_sel--;

					if (dialog_yoff > server_sel)
						dialog_yoff--;
				}

				break;

			case SDLK_DOWN:
				if (server_sel < count - 1)
				{
					server_sel++;

					if (server_sel >= DIALOG_LIST_ENTRY + dialog_yoff)
						dialog_yoff++;
				}

				break;

			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				selected_server = server_get_id(server_sel);

				if (selected_server)
				{
					GameStatus = GAME_STATUS_STARTCONNECT;
				}

				break;

			case SDLK_ESCAPE:
				return 1;

			case SDLK_PAGEUP:
				if (server_sel)
					server_sel -= DIALOG_LIST_ENTRY;

				dialog_yoff -= DIALOG_LIST_ENTRY;

				break;

			case SDLK_PAGEDOWN:
				if (server_sel < count - DIALOG_LIST_ENTRY)
					server_sel += DIALOG_LIST_ENTRY;

				dialog_yoff += DIALOG_LIST_ENTRY;

				break;

			default:
				break;
		}

		/* Sanity checks for going out of bounds */
		if (dialog_yoff < 0 || count < DIALOG_LIST_ENTRY)
			dialog_yoff = 0;
		else if (dialog_yoff >= count - DIALOG_LIST_ENTRY)
			dialog_yoff = count - DIALOG_LIST_ENTRY;

		if (server_sel < 0)
			server_sel = 0;
		else if (server_sel >= count)
			server_sel = count;
	}

	return 0;
}

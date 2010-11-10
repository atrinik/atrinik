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
 * Implements most client dialogs (GUIs). */

#include <include.h>

int active_button = -1;

/**
 * Draw a single frame.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param w Width of the frame.
 * @param h Height of the frame. */
void draw_frame(SDL_Surface *surface, int x, int y, int w, int h)
{
	SDL_Rect box;

	box.x = x;
	box.y = y;
	box.h = h;
	box.w = 1;
	SDL_FillRect(surface, &box, surface == ScreenSurface ? sdl_gray4 : SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
	box.x = x + w;
	box.h++;
	SDL_FillRect(surface, &box, surface == ScreenSurface ? sdl_gray3 : SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
	box.x = x;
	box.y+= h;
	box.w = w;
	box.h = 1;
	SDL_FillRect(surface, &box, surface == ScreenSurface ? sdl_gray4 : SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
	box.x++;
	box.y = y;
	SDL_FillRect(surface, &box, surface == ScreenSurface ? sdl_gray3 : SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
}

/**
 * Create a border around the specified coordinates.
 * @param surface Surface to use.
 * @param x X start of the border.
 * @param y Y start of the border.
 * @param w Maximum border width.
 * @param h Maximum border height.
 * @param color Color to use for the border.
 * @param size Border's size. */
void border_create(SDL_Surface *surface, int x, int y, int w, int h, int color, int size)
{
	SDL_Rect box;

	/* Left border. */
	box.x = x;
	box.y = y;
	box.h = h;
	box.w = size;
	SDL_FillRect(surface, &box, color);

	/* Right border. */
	box.x = x + w - size;
	SDL_FillRect(surface, &box, color);

	/* Top border. */
	box.x = x + size;
	box.y = y;
	box.w = w - size * 2;
	box.h = size;
	SDL_FillRect(surface, &box, color);

	/* Bottom border. */
	box.y = y + h - size;
	SDL_FillRect(surface, &box, color);
}

/**
 * Add a close button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param menu Menu the button is on. */
void add_close_button(int x, int y, int menu)
{
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	StringBlt(ScreenSurface, &SystemFont, "X", x + 463, y + 28, COLOR_BLACK, NULL, NULL);

	if (mx >x + 459 && mx < x + 469 && my > y + 27 && my < y + 39)
	{
		StringBlt(ScreenSurface, &SystemFont, "X", x + 462, y + 27, COLOR_HGOLD, NULL, NULL);

		if (mb && mb_clicked)
		{
			check_menu_keys(menu, SDLK_ESCAPE);
		}
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, "X", x + 462, y + 27, COLOR_WHITE, NULL, NULL);
	}
}

/**
 * Add a button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param id Button ID.
 * @param gfxNr ID of the button bitmap to draw.
 * @param text Text to draw.
 * @param text_h Text for keyboard user.
 * @return 1 if the button was pressed, 0 otherwise. */
int add_button(int x, int y, int id, int gfxNr, char *text, char *text_h)
{
	char *text_sel;
	int ret = 0, mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

	if (text_h)
	{
		text_sel = text_h;
	}
	else
	{
		text_sel = text;
	}

	sprite_blt(Bitmaps[gfxNr], x, y, NULL, NULL);

	if (mx >x && my >y && mx < x + Bitmaps[gfxNr]->bitmap->w && my < y + Bitmaps[gfxNr]->bitmap->h)
	{
		if (mb && mb_clicked && active_button < 0)
		{
			active_button = id;
		}

		if (active_button == id)
		{
			sprite_blt(Bitmaps[gfxNr + 1], x, y++, NULL, NULL);

			if (!mb)
			{
				ret = 1;
			}
		}

		StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_HGOLD, NULL, NULL);
	}
	else
	{
		StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_WHITE, NULL, NULL);
	}

	return ret;
}

/**
 * Add a group button and handle mouse events over it.
 * @param x X position.
 * @param y Y position.
 * @param id Button ID.
 * @param gfxNr ID of the button bitmap to draw.
 * @param text Text to draw.
 * @param text_h Text for keyboard user.
 * @return 1 if the button was pressed, 0 otherwise. */
int add_gr_button(int x, int y, int id, int gfxNr, const char *text, const char *text_h)
{
	const char *text_sel;
	int ret = 0;
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	/* use text_h (=highlighted char for keyboard user) if available. */
	if (text_h)
		text_sel = text_h;
	else
		text_sel = text;

	if (id)
		sprite_blt(Bitmaps[++gfxNr], x, y++, NULL, NULL);
	else
		sprite_blt(Bitmaps[gfxNr], x, y, NULL, NULL);

	StringBlt(ScreenSurface, &SystemFont, text, x + 11, y + 2, COLOR_BLACK, NULL, NULL);

	if (mx >x && my >y && mx < x + Bitmaps[gfxNr]->bitmap->w && my < y + Bitmaps[gfxNr]->bitmap->h)
	{
		if (mb && mb_clicked)
			ret = 1;

		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_HGOLD, NULL, NULL);
	}
	else
		StringBlt(ScreenSurface, &SystemFont, text_sel, x + 10, y + 1, COLOR_WHITE, NULL, NULL);

	return ret;
}

/**
 * Add offset to an integer value.
 * @param value Value to add to.
 * @param type @ref value_type "Type" of the value.
 * @param offset What to add to value.
 * @param min Minimum value.
 * @param max Maximum value. */
void add_value(void *value, int type, int offset, int min, int max)
{
	int old_value = 0;

	switch (type)
	{
		case VAL_INT:
			old_value = *((int *) value);
			*((int *) value) += offset;

			if (*((int *) value) > max)
			{
				*((int *) value) = max;
			}

			if (*((int *) value) < min)
			{
				*((int *) value) = min;
			}

			break;

		case VAL_U32:
			old_value = *((uint32 *) value);
			*((uint32 *) value) += offset;

			if (*((uint32 *) value) > (uint32) max)
			{
				*((uint32 *) value) = (uint32) max;
			}

			if (*((uint32 *) value) < (uint32) min)
			{
				*((uint32 *) value) = (uint32) min;
			}

			break;

		default:
			break;
	}

	/* Changed map X/Y, send a setup command to tell the server about the
	 * change. */
	if ((&options.map_size_x == value && options.map_size_x != old_value) || (&options.map_size_y == value && options.map_size_y != old_value))
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "setup mapsize %dx%d", options.map_size_x, options.map_size_y);
		cs_write_string(buf, strlen(buf));
	}
}

/**
 * Draw tabs on the left side of a window.
 * @param tabs The tabs to draw.
 * @param[out] act_tab Currently active tab.
 * @param head_text Header text, above the tabs.
 * @param x X position.
 * @param y Y position. */
void draw_tabs(const char *tabs[], int *act_tab, const char *head_text, int x, int y)
{
	int i = -1;
	int mx, my, mb;
	static int active = 0;

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_START], x, y - 10, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, head_text, x + 15, y + 4, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, head_text, x + 14, y + 3, COLOR_WHITE, NULL, NULL);
	y += 17;
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	y += 17;

	while (tabs[++i])
	{
		sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);

		if (i == *act_tab)
		{
			sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_SEL], x, y, NULL, NULL);
		}

		StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 25, y + 4, COLOR_BLACK, NULL, NULL);

		if (mx > x && mx < x + 100 && my > y && my < y + 17)
		{
			StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 24, y + 3, COLOR_HGOLD, NULL, NULL);

			if (mb && mb_clicked)
			{
				active = 1;
			}

			if (active)
			{
				*act_tab = i;
			}
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, tabs[i], x + 24, y + 3, COLOR_WHITE, NULL, NULL);
		}

		y += 17;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_STOP], x, y, NULL, NULL);

	if (!mb)
	{
		active = 0;
	}
}

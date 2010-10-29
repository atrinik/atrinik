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
 * Book GUI related code. */

#include <include.h>

/** The book's content. */
static char *book_content = NULL;
/** Name of the book. */
static char book_name[HUGE_BUF];
/** Number of lines in the book. */
static int book_lines = 0;
/** Number of lines at the end. */
static int book_scroll_lines = 0;
/** Lines scrolled. */
static int book_scroll = 0;

/**
 * Change the book's displayed name.
 * @param name The name to change to.
 * @param len Length of the name. */
void book_name_change(const char *name, size_t len)
{
	len = MIN(sizeof(book_name) - 1, len);
	strncpy(book_name, name, len);
	book_name[len] = '\0';
}

/**
 * Load the book interface.
 * @param data Book's content.
 * @param len Length of 'data'. */
void book_load(const char *data, int len)
{
	SDL_Rect box;
	int pos;

	/* Nothing to do. */
	if (!data || !len)
	{
		return;
	}

	/* Free old book data and reset the values. */
	if (book_content)
	{
		free(book_content);
		book_lines = 0;
		book_scroll_lines = 0;
		book_scroll = 0;
	}

	/* Store the data. */
	book_content = strdup(data);
	book_name_change("Book", 4);

	/* Strip trailing newlines. */
	for (pos = len - 1; pos >= 0; pos--)
	{
		if (book_content[pos] != '\n')
		{
			break;
		}

		book_content[pos] = '\0';
	}

	/* No data... */
	if (book_content[0] == '\0')
	{
		return;
	}

	/* Calculate the line numbers. */
	box.w = BOOK_CONTENT_WIDTH;
	box.h = BOOK_CONTENT_HEIGHT;
	string_blt(NULL, FONT_ARIAL11, book_content, 30, 50, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
	book_lines = box.h;
	book_scroll_lines = box.y;

	/* The book menu is now ready to be shown. */
	cpl.menustatus = MENU_BOOK;
}

/**
 * Actually show the book interface. */
void book_show()
{
	SDL_Rect box;
	int x, y;

	/* Draw the background. */
	x = BOOK_BACKGROUND_X;
	y = BOOK_BACKGROUND_Y;
	sprite_blt(Bitmaps[BITMAP_BOOK], x, y, NULL, NULL);

	/* Draw the book name. */
	box.w = Bitmaps[BITMAP_BOOK]->bitmap->w - 60;
	box.h = 0;
	string_blt(ScreenSurface, FONT_SERIF16, book_name, x + 30, y + 30, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);

	/* Draw the content. */
	box.w = BOOK_CONTENT_WIDTH;
	box.h = BOOK_CONTENT_HEIGHT;
	box.y = book_scroll;
	string_blt(ScreenSurface, FONT_ARIAL11, book_content, x + 30, y + 50, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);

	/* Show scroll buttons. */
	box.x = x + Bitmaps[BITMAP_BOOK]->bitmap->w - 50;
	box.y = y + Bitmaps[BITMAP_BOOK]->bitmap->h / 2 - 55;
	scroll_buttons_show(ScreenSurface, box.x, box.y, &book_scroll, book_lines - book_scroll_lines, book_scroll_lines, &box);

	/* Show close button. */
	if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, box.x, y + 30, "X", FONT_ARIAL10, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK)))
	{
		cpl.menustatus = MENU_NO;
		map_udate_flag = 2;
		reset_keys();
	}
}

/**
 * Handle a key.
 * @param key The key. */
void book_handle_key(SDLKey key)
{
	/* Scrolling. */
	if (key == SDLK_DOWN)
	{
		book_scroll++;
	}
	else if (key == SDLK_UP)
	{
		book_scroll--;
	}
	else if (key == SDLK_PAGEDOWN)
	{
		book_scroll += book_scroll_lines;
	}
	else if (key == SDLK_PAGEUP)
	{
		book_scroll -= book_scroll_lines;
	}

	/* Make sure the new scroll value is within range. */
	if (book_scroll < 0)
	{
		book_scroll = 0;
	}
	else if (book_scroll > book_lines - book_scroll_lines)
	{
		book_scroll = book_lines - book_scroll_lines;
	}
}

/**
 * Handle book events, such as mouse wheel scrolling.
 * @param event The event. */
void book_handle_event(SDL_Event *event)
{
	/* Mouse event and the mouse is inside the book. */
	if (event->type == SDL_MOUSEBUTTONDOWN && event->motion.x > BOOK_BACKGROUND_X && event->motion.x < BOOK_BACKGROUND_X + Bitmaps[BITMAP_BOOK]->bitmap->w && event->motion.y > BOOK_BACKGROUND_Y && event->motion.y < BOOK_BACKGROUND_Y + Bitmaps[BITMAP_BOOK]->bitmap->h)
	{
		/* Scroll the book. */
		if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			book_handle_key(SDLK_PAGEDOWN);
		}
		else if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			book_handle_key(SDLK_PAGEUP);
		}
	}
}

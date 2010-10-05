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
 * Servers list, logging to a server, creating new character, etc. */

#include <include.h>

/** How often to blink the eyes in ticks. */
#define EYES_BLINK_TIME (10 * 1000)
/** How long the eyes remain 'closed' (not drawn). */
#define EYES_BLINK_DELAY (200)

/**
 * Last server count to see when to re-create the servers list. Since the
 * metaserver code uses threading so the whole program doesn't lock up,
 * we need to do it like this. */
static size_t last_server_count = 0;

/** Data buffer used when downloading news from the site. */
static curl_data *news_data = NULL;

/** Last time the eyes blinked. */
static uint32 eyes_blink_ticks = 0;
/** Whether to draw the eyes. */
static uint8 eyes_draw = 1;

/**
 * Handle enter key being pressed in the servers list.
 * @param list The servers list. */
static void list_handle_enter(list_struct *list)
{
	/* Get selected server. */
	selected_server = server_get_id(list->row_selected - 1);

	/* Valid server, start connecting. */
	if (selected_server)
	{
		GameStatus = GAME_STATUS_STARTCONNECT;
	}
}

/**
 * Show the main GUI after starting the client -- servers list, chat box,
 * connecting to server, etc.
 * @todo Game news right of the servers list acquired by connecting to
 * the site with cURL. */
void show_meta_server()
{
	int x, y;
	list_struct *list;
	size_t server_count;
	server_struct *node;
	char buf[MAX_BUF];

	x = 5;
	y = Screensize->y - Bitmaps[BITMAP_SERVERS_BG]->bitmap->h - 5;

	/* Background */
	sprite_blt(Bitmaps[BITMAP_INTRO], 0, 0, NULL, NULL);

	/* Remove the servers list after successfully connecting to the
	 * server.
	 * TODO: use popups instead, that gray out the screen behind. */
	if (GameStatus > GAME_STATUS_STARTCONNECT)
	{
		list_remove_all();
		return;
	}

	sprite_blt(Bitmaps[BITMAP_SERVERS_BG], x, y, NULL, NULL);

	list = list_exists(LIST_SERVERS);
	server_count = server_get_count();

	/* List doesn't exist or the count changed? Create new list. */
	if (!list || last_server_count != server_count)
	{
		size_t i;

		/* Remove it if it exists already. */
		if (list)
		{
			list_remove(list);
		}

		/* Create the servers list. */
		list = list_create(LIST_SERVERS, x + 13, y + 8, 132, 3, 8);
 		list->handle_enter_func = list_handle_enter;
		list_set_column(list, 0, 295, 7, "Server", -1);
		list_set_column(list, 1, 50, 9, "Port", 1);
		list_set_column(list, 2, 50, 7, "Players", 1);

		/* Add the servers to the list. */
		for (i = 0; i < server_count; i++)
		{
			node = server_get_id(i);

			list_add(list, i, 0, node->name);
			snprintf(buf, sizeof(buf), "%d", node->port);
			list_add(list, i, 1, buf);

			if (node->player >= 0)
			{
				snprintf(buf, sizeof(buf), "%d", node->player);
			}
			else
			{
				strcpy(buf, "-");
			}

			list_add(list, i, 2, buf);
		}

		/* Update the focus if we re-created the list, since it's no
		 * longer the first one in the list. */
		if (last_server_count != server_count)
		{
			list_set_focus(list);
		}

		/* Store the new count. */
		last_server_count = server_count;
	}

	/* Actually draw the list. */
	list_show(list);
	node = server_get_id(list->row_selected - 1);

	/* Do we have any selected server? If so, show its version and
	 * description. */
	if (node)
	{
		SDL_Rect box;

		snprintf(buf, sizeof(buf), "Version: %s", node->version);
		string_blt_shadow(ScreenSurface, &SystemFont, buf, x + 13, y + 185, COLOR_HGOLD, COLOR_BLACK, 0, NULL);

		box.w = 410;
		box.h = 48;
		string_blt(ScreenSurface, &SystemFont, node->desc, x + 13, y + 197, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	/* Show whether we are connecting to the metaserver or not. */
	if (ms_connecting(-1))
	{
		string_blt_shadow(ScreenSurface, &SystemFont, "Connecting to metaserver, please wait...", x + 128, y + 8, COLOR_HGOLD, COLOR_BLACK, 0, NULL);
	}
	else
	{
		string_blt_shadow(ScreenSurface, &SystemFont, "Select a server.", x + 230, y + 8, COLOR_GREEN, COLOR_BLACK, 0, NULL);
	}

	sprite_blt(Bitmaps[BITMAP_SERVERS_BG_OVER], x, y, NULL, NULL);

	x += Bitmaps[BITMAP_SERVERS_BG_OVER]->bitmap->w + 10;
	sprite_blt(Bitmaps[BITMAP_NEWS_BG], x, y, NULL, NULL);

	list = list_exists(LIST_NEWS);

	/* No list yet, make one and start downloading the data. */
	if (!list)
	{
		/* Start downloading. */
		news_data = curl_download_start("http://www.atrinik.org/client_news.php");

		list = list_create(LIST_NEWS, x + 13, y + 10, 216, 1, 8);
//		list->handle_enter_func = list_handle_enter;
		list_set_column(list, 0, 150, 7, NULL, -1);
	}

	/* Download in progress? */
	if (news_data)
	{
		/* Get the status. */
		int ret = curl_download_finished(news_data);

		/* Finished downloading, parse the data. */
		if (ret == 1)
		{
			char *mem = strdup(news_data->memory), *cp;
			size_t i = 0;

			cp = strtok(mem, "\n");

			while (cp)
			{
				list_add(list, i, 0, cp);
				i++;
				cp = strtok(NULL, "\n");
			}

			free(mem);
		}

		/* Finished downloading or there was an error: clean up in either
		 * case. */
		if (ret != 0)
		{
			curl_data_free(news_data);
			news_data = NULL;
		}
	}

	/* Show the news list. */
	list_show(list);

	/* Calculate whether to show the eyes or not. Blinks every
	 * EYES_BLINK_TIME ticks, then waits EYES_BLINK_DELAY ticks until
	 * showing the eyes again. */
	if (SDL_GetTicks() - eyes_blink_ticks >= (eyes_draw ? EYES_BLINK_TIME : EYES_BLINK_DELAY))
	{
		eyes_blink_ticks = SDL_GetTicks();
		eyes_draw = !eyes_draw;
	}

	if (eyes_draw)
	{
		sprite_blt(Bitmaps[BITMAP_EYES], Bitmaps[BITMAP_INTRO]->bitmap->w - 90, 310, NULL, NULL);
	}
}

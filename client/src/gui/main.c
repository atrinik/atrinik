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
 * Draw the news popup.
 * @param popup Popup. */
static void news_popup_draw_func(popup_struct *popup)
{
	/* Got the news yet? */
	if (popup->buf)
	{
		SDL_Rect box;

		/* Show the news. */
		box.h = 350;
		box.w = 320;
 		string_blt(popup->surface, FONT_SERIF12, popup->buf, 120, 70, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP, &box);
		return;
	}
	/* Haven't started downloading yet. */
	else if (!popup->custom_data)
	{
		list_struct *list = list_exists(LIST_NEWS);
		char url[MAX_BUF], *id;
		CURL *curl;

		/* Shouldn't happen... */
		if (!list)
		{
			popup_destroy_visible();
			return;
		}

		/* Initialize cURL, escape the selected row's text and construct
		 * the url to use for downloading. */
		curl = curl_easy_init();
		id = curl_easy_escape(curl, list->text[list->row_selected - 1][0], 0);
		snprintf(url, sizeof(url), "http://www.atrinik.org/client_news.php?news=%s", id);
		curl_free(id);
		curl_easy_cleanup(curl);

		/* Start downloading. */
		popup->custom_data = curl_download_start(url);
	}
	/* Downloading. */
	else
	{
		/* Check if we finished yet. */
		int ret = curl_download_finished(popup->custom_data);

		/* Yes, we finished, store the string we got. */
		if (ret == 1)
		{
			popup->buf = strdup(((curl_data *) popup->custom_data)->memory);
		}

		/* Free the cURL data if we finished. */
		if (ret != 0)
		{
			curl_data_free(popup->custom_data);
			popup->custom_data = NULL;
		}
	}

	/* Haven't downloaded the text yet, inform the user. */
	string_blt(popup->surface, FONT_SERIF12, "Downloading news, please wait...", 120, 70, COLOR_SIMPLE(COLOR_WHITE), 0, NULL);
}

/** @copydoc popup_struct::event_func */
static int news_popup_event_func(popup_struct *popup, SDL_Event *event)
{
	/* Escape was pressed? */
	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)
	{
		/* Free the cURL data, if any. */
		if (popup->custom_data)
		{
			curl_data_free(popup->custom_data);
			popup->custom_data = NULL;
		}
	}

	return -1;
}

/** @copydoc popup_struct::event_func */
/** @todo finish */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
	(void) popup;

	if (event->type == SDL_KEYDOWN)
	{
		switch (event->key.keysym.sym)
		{
			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				GameStatus = GAME_STATUS_STARTCONNECT;
				popup_destroy_visible();
				return 1;

			default:
				break;
		}
	}

	return -1;
}

/**
 * Handle enter key being pressed in the servers list.
 * @param list The servers list. */
static void list_handle_enter(list_struct *list)
{
	/* Servers list? */
	if (list->id == LIST_SERVERS)
	{
		/* Get selected server. */
		selected_server = server_get_id(list->row_selected - 1);

		/* Valid server, start connecting. */
		if (selected_server)
		{
			popup_struct *popup = popup_create(BITMAP_DIALOG_BG);

// 			popup->draw_func = popup_draw_func;
 			popup->event_func = popup_event_func;
		}
	}
	else if (list->id == LIST_NEWS)
	{
		if (list->text && list->text[list->row_selected - 1])
		{
			popup_struct *popup = popup_create(BITMAP_DIALOG_BG);

			popup->draw_func = news_popup_draw_func;
			popup->event_func = news_popup_event_func;
		}
	}
}

/**
 * Show the main GUI after starting the client -- servers list, chat box,
 * connecting to server, etc. */
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
	 * server. */
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
		list = list_create(LIST_SERVERS, x + 13, y + 8, 11, 3, 8);
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
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, buf, x + 13, y + 185, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), 0, NULL);

		box.w = 410;
		box.h = 48;
		string_blt(ScreenSurface, FONT_ARIAL10, node->desc, x + 13, y + 197, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	/* Show whether we are connecting to the metaserver or not. */
	if (ms_connecting(-1))
	{
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, "Connecting to metaserver, please wait...", x + 105, y + 8, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), 0, NULL);
	}
	else
	{
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, "Select a server.", x + 226, y + 8, COLOR_SIMPLE(COLOR_GREEN), COLOR_SIMPLE(COLOR_BLACK), 0, NULL);
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

		list = list_create(LIST_NEWS, x + 13, y + 10, 18, 1, 8);
		list->handle_enter_func = list_handle_enter;
		list_set_column(list, 0, 150, 7, NULL, -1);
		list_set_font(list, FONT_ARIAL10);
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

	/* Show the play button. */
	if (button_show(BITMAP_DIALOG_BUTTON_UP, -1, BITMAP_DIALOG_BUTTON_DOWN, 479, y + 10, "Play", FONT_ARIAL10, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK)))
	{
		list_handle_enter(list);
	}
}

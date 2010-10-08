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
		list_struct *list = list_exists(LIST_NEWS);
		uint32 visible_lines, lines;

		box.w = popup->surface->w;
		/* Show the news title. */
 		string_blt(popup->surface, FONT_SERIF12, list ? list->text[list->row_selected - 1][0] : "???", 0, 10, COLOR_SIMPLE(COLOR_HGOLD), TEXT_ALIGN_CENTER, &box);

		box.h = 260;
		box.w = 455;
		/* Skip rows we scrolled past. */
		box.y = popup->i;
		/* Calculate number of visible rows. */
		visible_lines = box.h / FONT_HEIGHT(FONT_SERIF12);
		/* Show the news. */
 		string_blt(popup->surface, FONT_SERIF12, popup->buf, 10, 30, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_HEIGHT, &box);
		/* NUmber of lines in the string. */
		lines = box.h / FONT_HEIGHT(FONT_SERIF12);

		box.x = Bitmaps[popup->bitmap_id]->bitmap->w - 30;
		box.y = Bitmaps[popup->bitmap_id]->bitmap->h / 2 - 50;
		/* Show scroll buttons. */
		scroll_buttons_show(popup->surface, Screensize->x / 2 - Bitmaps[popup->bitmap_id]->bitmap->w / 2 + box.x, Screensize->y / 2 - Bitmaps[popup->bitmap_id]->bitmap->h / 2 + box.y, (int *) &popup->i, lines - visible_lines + 1, visible_lines, &box);
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
	string_blt(popup->surface, FONT_SERIF12, "Downloading news, please wait...", 10, 10, COLOR_SIMPLE(COLOR_WHITE), TEXT_ALIGN_CENTER, NULL);
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

/**
 * Draw the server connection/character creation popup.
 * @param popup Popup. */
static void popup_draw_func(popup_struct *popup)
{
	_BLTFX bltfx;
	int progress;
	SDL_Rect box;
	char buf[MAX_BUF];
	int x, y;

	/* Waiting to log in. */
	if (GameStatus == GAME_STATUS_WAITFORPLAY)
	{
		box.w = Bitmaps[popup->bitmap_id]->bitmap->w;
		box.h = Bitmaps[popup->bitmap_id]->bitmap->h;
		string_blt_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return;
	}
	/* Playing now, so destroy this popup and remove any lists. */
	else if (GameStatus == GAME_STATUS_PLAY)
	{
		popup_destroy_visible();
		list_remove_all();
		return;
	}

	bltfx.surface = popup->surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;

	/* Update progress bar of requested files */
	sprite_blt(Bitmaps[BITMAP_PROGRESS_BACK], Bitmaps[popup->bitmap_id]->bitmap->w / 2 - Bitmaps[BITMAP_PROGRESS_BACK]->bitmap->w / 2, 30, NULL, &bltfx);

	progress = MIN(100, request_file_chain * 8);
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_PROGRESS]->bitmap->h;
	box.w = (int) ((float) Bitmaps[BITMAP_PROGRESS]->bitmap->w / 100 * progress);
	sprite_blt(Bitmaps[BITMAP_PROGRESS], Bitmaps[popup->bitmap_id]->bitmap->w / 2 - Bitmaps[BITMAP_PROGRESS]->bitmap->w / 2, 30, &box, &bltfx);

	/* Show that we are connecting to the server. */
	box.w = Bitmaps[popup->bitmap_id]->bitmap->w;
	string_blt_shadow(popup->surface, FONT_SERIF12, "Connecting to server, please wait...", 0, 10, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER, &box);

	/* Downloading the files, or updates haven't finished yet? */
	if (GameStatus <= GAME_STATUS_REQUEST_FILES || !file_updates_finished())
	{
		return;
	}

	/* Middle of the screen for the text inputs. */
	x = Bitmaps[popup->bitmap_id]->bitmap->w / 2 - text_input_center_offset();
	y = 75;

	/* Player name. */
	if (GameStatus == GAME_STATUS_NAME)
	{
		string_blt(popup->surface, FONT_ARIAL10, "Enter your name", 0, 55, COLOR_SIMPLE(COLOR_HGOLD), TEXT_ALIGN_CENTER, &box);
		InputString[0] = toupper(InputString[0]);
		text_input_show(popup->surface, x, y, FONT_ARIAL10, InputString, COLOR_SIMPLE(COLOR_WHITE), 0);
	}
	else
	{
		cpl.name[0] = toupper(cpl.name[0]);
		text_input_draw_background(popup->surface, x, y);
		text_input_draw_text(popup->surface, x, y, FONT_ARIAL10, cpl.name, COLOR_SIMPLE(COLOR_WHITE), 0);
	}

	y += 35;

	/* Player password. */
	if (GameStatus == GAME_STATUS_PSWD || cpl.password[0])
	{
		char *cp;

		strncpy(buf, GameStatus == GAME_STATUS_PSWD ? InputString : cpl.password, sizeof(buf) - 1);

		for (cp = buf; *cp; cp++)
		{
			*cp = '*';
		}

		if (GameStatus == GAME_STATUS_PSWD)
		{
			string_blt(popup->surface, FONT_ARIAL10, "Enter your password", 0, 95, COLOR_SIMPLE(COLOR_HGOLD), TEXT_ALIGN_CENTER, &box);
			text_input_show(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_SIMPLE(COLOR_WHITE), 0);
		}
		else
		{
			text_input_draw_background(popup->surface, x, y);
			text_input_draw_text(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_SIMPLE(COLOR_WHITE), 0);
		}
	}

	y += 35;

	/* Verify password for character creation. */
	if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		char *cp;

		strncpy(buf, InputString, sizeof(buf) - 1);

		for (cp = buf; *cp; cp++)
		{
			*cp = '*';
		}

		string_blt(popup->surface, FONT_ARIAL10, "New Character: Verify Password", 0, 130, COLOR_SIMPLE(COLOR_HGOLD), TEXT_ALIGN_CENTER, &box);
		text_input_show(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_SIMPLE(COLOR_WHITE), 0);
	}
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
	(void) popup;

	if (event->type == SDL_KEYDOWN)
	{
		switch (event->key.keysym.sym)
		{
			/* ESC, go back to the servers list part and destroy the
			 * popup. */
			case SDLK_ESCAPE:
				GameStatus = GAME_STATUS_START;
				popup_destroy_visible();
				return 1;

			/* Anything else. */
			default:
				/* Try to handle text input. */
				if (key_string_event(&event->key))
				{
					return 1;
				}

				/* Ignore. */
				return 0;
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
			popup_struct *popup = popup_create(BITMAP_POPUP);

 			popup->draw_func = popup_draw_func;
 			popup->event_func = popup_event_func;
			GameStatus = GAME_STATUS_STARTCONNECT;
		}
	}
	else if (list->id == LIST_NEWS)
	{
		if (list->text && list->text[list->row_selected - 1])
		{
			popup_struct *popup = popup_create(BITMAP_POPUP);

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
	SDL_Rect box;

	/* Active popup, no need to do anything. */
	if (popup_get_visible() && !popup_overlay_need_update(popup_get_visible()))
	{
		return;
	}

	x = 5;
	y = Screensize->y - Bitmaps[BITMAP_SERVERS_BG]->bitmap->h - 5;

	/* Background */
	sprite_blt(Bitmaps[BITMAP_INTRO], 0, 0, NULL, NULL);

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

	box.w = Bitmaps[BITMAP_NEWS_BG]->bitmap->w;
	string_blt_shadow(ScreenSurface, FONT_SERIF12, "Game News", x, y + 10, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER, &box);

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
		list_handle_enter(list_exists(LIST_SERVERS));
	}
}

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
 * The intro screen.
 *
 * @author Alex Tokar */

#include <global.h>

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
/** Button buffer. */
static button_struct button_play, button_refresh, button_settings, button_update, button_help, button_credits, button_quit;

/** The news list. */
static list_struct *list_news = NULL;
/** The servers list. */
static list_struct *list_servers = NULL;

/**
 * Handle enter key being pressed in the servers list.
 * @param list The servers list. */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
	/* Servers list? */
	if (list == list_servers)
	{
		/* Get selected server. */
		selected_server = server_get_id(list->row_selected - 1);

		/* Valid server, start connecting. */
		if (selected_server)
		{
			login_start();
			return;
		}
	}
	else if (list == list_news)
	{
		if (list->text && list->text[list->row_selected - 1])
		{
			game_news_open(list->text[list->row_selected - 1][0]);
		}
	}
}

/** @copydoc list_struct::esc_handle_func */
static void list_handle_esc(list_struct *list)
{
	(void) list;

	system_end();
	exit(0);
}

/**
 * Show the main GUI after starting the client -- servers list, chat box,
 * connecting to server, etc. */
void intro_show(void)
{
	SDL_Surface *texture;
	int x, y;
	size_t server_count;
	server_struct *node;
	char buf[MAX_BUF];
	SDL_Rect box;

	sound_start_bg_music("intro.ogg", setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);

	texture = TEXTURE_CLIENT("intro");

	/* Background */
	surface_show(ScreenSurface, 0, 0, NULL, texture);
	textwin_show(ScreenSurface, texture->w, 1, ScreenSurface->w - texture->w - 2, ScreenSurface->h - 3);

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
		surface_show(ScreenSurface, texture->w - 90, 310, NULL, TEXTURE_CLIENT("eyes"));
	}

	texture = TEXTURE_CLIENT("servers_bg");
	x = 15;
	y = ScreenSurface->h - texture->h - 5;
	surface_show(ScreenSurface, x, y, NULL, texture);

	server_count = server_get_count();

	/* Create the buttons. */
	if (!list_servers)
	{
		button_create(&button_play);
		button_create(&button_refresh);
		button_create(&button_settings);
		button_create(&button_update);
		button_create(&button_help);
		button_create(&button_credits);
		button_create(&button_quit);
	}

	/* List doesn't exist or the count changed? Create new list. */
	if (!list_servers || last_server_count != server_count)
	{
		size_t i;

		/* Remove it if it exists already. */
		if (list_servers)
		{
			list_remove(list_servers);
		}

		/* Create the servers list. */
		list_servers = list_create(11, 3, 8);
		list_servers->handle_enter_func = list_handle_enter;
		list_servers->handle_esc_func = list_handle_esc;
		list_scrollbar_enable(list_servers);
		list_set_column(list_servers, 0, 295, 7, "Server", -1);
		list_set_column(list_servers, 1, 50, 9, "Port", 1);
		list_set_column(list_servers, 2, 46, 7, "Players", 1);

		/* Add the servers to the list. */
		for (i = 0; i < server_count; i++)
		{
			node = server_get_id(i);

			list_add(list_servers, i, 0, node->name);
			snprintf(buf, sizeof(buf), "%d", node->port);
			list_add(list_servers, i, 1, buf);

			if (node->player >= 0)
			{
				snprintf(buf, sizeof(buf), "%d", node->player);
			}
			else
			{
				strcpy(buf, "-");
			}

			list_add(list_servers, i, 2, buf);

			if (clioption_settings.connect[0] && strcasecmp(clioption_settings.connect[0], node->name) == 0)
			{
				list_servers->row_selected = i + 1;
				event_push_key_once(SDLK_RETURN, 0);
			}
		}

		/* Store the new count. */
		last_server_count = server_count;
	}

	/* Actually draw the list. */
	list_show(list_servers, x + 12, y + 8);
	node = server_get_id(list_servers->row_selected - 1);

	/* Do we have any selected server? If so, show its version and
	 * description. */
	if (node)
	{
		snprintf(buf, sizeof(buf), "Version: %s", node->version);
		text_show_shadow(ScreenSurface, FONT_ARIAL10, buf, x + 13, y + 185, COLOR_HGOLD, COLOR_BLACK, 0, NULL);

		box.w = 410;
		box.h = 48;
		text_show(ScreenSurface, FONT_ARIAL10, node->desc, x + 13, y + 197, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	/* Show whether we are connecting to the metaserver or not. */
	if (ms_connecting(-1))
	{
		text_show_shadow(ScreenSurface, FONT_ARIAL10, "Connecting to metaserver, please wait...", x + 105, y + 8, COLOR_HGOLD, COLOR_BLACK, 0, NULL);
	}
	else
	{
		text_show_shadow(ScreenSurface, FONT_ARIAL10, "Select a server.", x + 226, y + 8, COLOR_GREEN, COLOR_BLACK, 0, NULL);
	}

	texture = TEXTURE_CLIENT("servers_bg_over");
	surface_show(ScreenSurface, x, y, NULL, texture);

	x += texture->w + 20;
	texture = TEXTURE_CLIENT("news_bg");
	surface_show(ScreenSurface, x, y, NULL, texture);

	box.w = texture->w;
	box.h = 0;
	text_show_shadow(ScreenSurface, FONT_SERIF12, "Game News", x, y + 10, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

	/* No list yet, make one and start downloading the data. */
	if (!list_news)
	{
		/* Start downloading. */
		news_data = curl_download_start(clioption_settings.game_news_url);

		list_news = list_create(18, 1, 8);
		list_news->focus = 0;
		list_news->handle_enter_func = list_handle_enter;
		list_news->handle_esc_func = list_handle_esc;
		list_set_column(list_news, 0, 150, 7, NULL, -1);
		list_set_font(list_news, FONT_ARIAL10);
	}

	/* Download in progress? */
	if (news_data)
	{
		/* Get the status. */
		int ret = curl_download_finished(news_data);

		/* Finished downloading, parse the data. */
		if (ret == 1)
		{
			char *mem = strdup(news_data->memory ? news_data->memory : "???"), *cp;
			size_t i = 0;

			cp = strtok(mem, "\n");

			while (cp)
			{
				list_add(list_news, i, 0, cp);
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
	list_show(list_news, x + 13, y + 10);

	button_play.x = button_refresh.x = button_settings.x = button_update.x = button_help.x = button_credits.x = button_quit.x = 489;
	y += 2;

	button_play.y = y + 10;
	button_show(&button_play, "Play");

	button_refresh.y = y + 35;
	button_show(&button_refresh, "Refresh");

	button_settings.y = y + 60;
	button_show(&button_settings, "Settings");

	button_update.y = y + 85;
	button_show(&button_update, "Update");

	button_help.y = y + 110;
	button_show(&button_help, "Help");

	button_credits.y = y + 135;
	button_show(&button_credits, "Credits");

	button_quit.y = y + 224;
	button_show(&button_quit, "Quit");
}

/**
 * Handle event in the main screen.
 * @param event The event to handle.
 * @return 1 if the event was handled, 0 otherwise. */
int intro_event(SDL_Event *event)
{
	if (!list_servers)
	{
		return 0;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		if (LIST_MOUSE_OVER(list_news, event->motion.x, event->motion.y))
		{
			list_news->focus = 1;
			list_servers->focus = 0;
		}
		else if (LIST_MOUSE_OVER(list_servers, event->motion.x, event->motion.y))
		{
			list_servers->focus = 1;
			list_news->focus = 0;
		}
	}

	if (button_event(&button_play, event))
	{
		list_handle_enter(list_servers, event);
		return 1;
	}
	else if (button_event(&button_refresh, event))
	{
		if (!ms_connecting(-1))
		{
			cpl.state = ST_META;
		}

		return 1;
	}
	else if (button_event(&button_settings, event))
	{
		settings_open();
		return 1;
	}
	else if (button_event(&button_update, event))
	{
		updater_open();
		return 1;
	}
	else if (button_event(&button_help, event))
	{
		help_show("main screen");
		return 1;
	}
	else if (button_event(&button_credits, event))
	{
		credits_show();
		return 1;
	}
	else if (button_event(&button_quit, event))
	{
		system_end();
		exit(0);
		return 1;
	}
	else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_TAB && list_news)
	{
		int news_focus = 0;

		if (list_servers->focus)
		{
			news_focus = 1;
		}

		list_news->focus = news_focus;
		list_servers->focus = !news_focus;
	}
	else if (list_handle_keyboard(list_news && list_news->focus ? list_news : list_servers, event))
	{
		return 1;
	}
	else if (list_handle_mouse(list_news, event))
	{
		return 1;
	}
	else if (list_handle_mouse(list_servers, event))
	{
		return 1;
	}

	return 0;
}

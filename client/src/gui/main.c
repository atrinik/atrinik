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
 * Servers list, logging to a server, creating new character, etc.
 *
 * @author Alex Tokar */

#include <global.h>

/** How often to blink the eyes in ticks. */
#define EYES_BLINK_TIME (10 * 1000)
/** How long the eyes remain 'closed' (not drawn). */
#define EYES_BLINK_DELAY (200)

/** Maximum width of the news text. */
#define NEWS_MAX_WIDTH 455
/** Maximum height of the news text. */
#define NEWS_MAX_HEIGHT 250
/** Font of the news text. */
#define NEWS_FONT FONT_SANS12

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

/** Character creation step. */
static int char_step = 0;
/** Selected race. */
static uint32 char_race_selected;
/** Selected @ref GENDER_xxx "gender". */
static int char_gender_selected;
/** Number of stat points left to assign. */
static int char_points_left;
/** Maximum number of character creation steps. */
const int char_step_max = 2;

/** Progress dots in login. */
static progress_dots progress;
/** Button buffer. */
static button_struct button_play, button_refresh, button_settings, button_update, button_help, button_quit;
/** News scrollbar. */
static scrollbar_struct scrollbar_news;
/** Buffers for scrolling text in the news popup. */
static uint32 news_scroll_offset, news_num_lines;

/** The news list. */
static list_struct *list_news = NULL;
/** The servers list. */
static list_struct *list_servers = NULL;
/** Character creation list. */
static list_struct *list_creation = NULL;

/** @copydoc popup_struct::draw_func */
static int news_popup_draw_func(popup_struct *popup)
{
	/* Got the news yet? */
	if (popup->buf)
	{
		SDL_Rect box;

		box.w = 420;
		box.h = 22;
		/* Show the news title. */
		string_show(popup->surface, FONT_SERIF12, list_news->text[list_news->row_selected - 1][0], 40, 8, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

		box.w = NEWS_MAX_WIDTH;
		box.h = NEWS_MAX_HEIGHT;

		/* Calculate number of last displayed lines. */
		if (!news_num_lines)
		{
			string_show(NULL, NEWS_FONT, popup->buf, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
			news_num_lines = box.h;
			scrollbar_create(&scrollbar_news, 15, 240, &news_scroll_offset, &news_num_lines, box.y);
			scrollbar_news.px = popup->x;
			scrollbar_news.py = popup->y;
			box.h = NEWS_MAX_HEIGHT;
		}

		/* Skip rows we scrolled past. */
		box.y = news_scroll_offset;
		/* Show the news. */
		text_offset_set(popup->x, popup->y);
		string_show(popup->surface, NEWS_FONT, popup->buf, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);
		text_offset_reset();

		scrollbar_show(&scrollbar_news, popup->surface, popup->surface->w - 28, 45);
		return 1;
	}
	/* Haven't started downloading yet. */
	else if (!popup->custom_data)
	{
		char url[MAX_BUF], *id;
		CURL *curl;

		/* Initialize cURL, escape the selected row's text and construct
		 * the url to use for downloading. */
		curl = curl_easy_init();
		id = curl_easy_escape(curl, list_news->text[list_news->row_selected - 1][0], 0);
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
			popup->buf = strdup(((curl_data *) popup->custom_data)->memory ? ((curl_data *) popup->custom_data)->memory : "???");
		}

		/* Free the cURL data if we finished. */
		if (ret != 0)
		{
			curl_data_free(popup->custom_data);
			popup->custom_data = NULL;
		}
	}

	/* Haven't downloaded the text yet, inform the user. */
	string_show(popup->surface, FONT_SERIF12, "Downloading news, please wait...", 10, 40, COLOR_WHITE, TEXT_ALIGN_CENTER, NULL);
	return 1;
}

/** @copydoc popup_struct::event_func */
static int news_popup_event_func(popup_struct *popup, SDL_Event *event)
{
	if (popup->buf && scrollbar_event(&scrollbar_news, event))
	{
		return 1;
	}

	if (event->type == SDL_KEYDOWN)
	{
		/* Escape was pressed? */
		if (event->key.keysym.sym == SDLK_ESCAPE)
		{
			/* Free the cURL data, if any. */
			if (popup->custom_data)
			{
				curl_data_free(popup->custom_data);
				popup->custom_data = NULL;
			}
		}
		/* Scroll the text. */
		else if (event->key.keysym.sym == SDLK_DOWN && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, 1);
			return 1;
		}
		else if (event->key.keysym.sym == SDLK_UP && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, -1);
			return 1;
		}
		else if (event->key.keysym.sym == SDLK_PAGEUP && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, -scrollbar_news.max_lines);
			return 1;
		}
		else if (event->key.keysym.sym == SDLK_PAGEDOWN && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, scrollbar_news.max_lines);
			return 1;
		}
	}
	/* Mouse wheel? */
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_WHEELDOWN && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, 1);
			return 1;
		}
		else if (event->button.button == SDL_BUTTON_WHEELUP && popup->buf)
		{
			scrollbar_scroll_adjust(&scrollbar_news, -1);
			return 1;
		}
	}

	return -1;
}

/**
 * Handle enter, double-clicking etc in character creation popup.
 * @param list Associated list; holds the selected entry etc. Will be
 * removed. */
static void char_creation_enter(list_struct *list)
{
	char buf[MAX_BUF];

	/* Picked race. */
	if (char_step == 0)
	{
		char_race_selected = list->row_selected - 1;
	}
	/* Picked gender. */
	else if (char_step == 1)
	{
		/* This is more complicated than the above, because we have to
		 * change the gender's first letter back to lowercase, and find
		 * its ID. */
		strncpy(buf, list->text[list->row_selected - 1][0], sizeof(buf) - 1);
		buf[0] = tolower(buf[0]);
		buf[sizeof(buf) - 1] = '\0';
		char_gender_selected = gender_to_id(buf);
	}
	/* Selected stats, create the character. */
	else if (char_step == 2)
	{
		packet_struct *packet;

		if (char_points_left)
		{
			return;
		}

		packet = packet_new(SERVER_CMD_NEW_CHAR, 64, 64);
		packet_append_string_terminated(packet, s_settings->characters[char_race_selected].gender_archetypes[char_gender_selected]);
		socket_send_packet(packet);
		return;
	}

	char_step++;

	if (list)
	{
		list_remove(list);
		list_creation = NULL;
	}
}

/** @copydoc popup_struct::draw_post_func */
static int popup_draw_post_func(popup_struct *popup)
{
	size_t i;
	int face = 0, x, y;
	SDL_Rect box;

	x = popup->x;
	y = popup->y;

	y += 65;

	if (char_step == 2)
	{
		y += 40;
	}

	if (!list_creation)
	{
		/* Create a new list. */
		list_creation = list_create(7, 1, 0);
		list_creation->handle_enter_func = char_creation_enter;

		/* Show list of races. */
		if (char_step == 0)
		{
			list_set_column(list_creation, 0, 250, 7, NULL, -1);

			for (i = 0; i < s_settings->num_characters; i++)
			{
				list_add(list_creation, i, 0, s_settings->characters[i].name);
			}
		}
		/* List of genders. */
		else if (char_step == 1)
		{
			char buf[30];
			size_t row = 0;

			list_set_column(list_creation, 0, 250, 7, NULL, -1);

			for (i = 0; i < GENDER_MAX; i++)
			{
				/* Does the selected race have this gender? */
				if (s_settings->characters[char_race_selected].gender_archetypes[i])
				{
					/* Uppercase the first letter of the gender's name
					 * and add it to the list. */
					strncpy(buf, gender_noun[i], sizeof(buf) - 1);
					buf[0] = toupper(buf[0]);
					buf[sizeof(buf) - 1] = '\0';
					list_add(list_creation, row, 0, buf);
					row++;
				}
			}
		}
		/* The stats. */
		else if (char_step == 2)
		{
			list_set_column(list_creation, 0, 30, 7, NULL, -1);
			list_creation->y += 2;
			list_creation->row_height_adjust = 6;
			list_creation->row_color_func = NULL;
			list_creation->row_highlight_func = NULL;
			list_creation->row_selected_func = NULL;
			list_creation->draw_frame_func = NULL;
			list_add(list_creation, 0, 0, "STR:");
			list_add(list_creation, 1, 0, "DEX:");
			list_add(list_creation, 2, 0, "CON:");
			list_add(list_creation, 3, 0, "INT:");
			list_add(list_creation, 4, 0, "WIS:");
			list_add(list_creation, 5, 0, "POW:");
			list_add(list_creation, 6, 0, "CHA:");
		}
	}

	list_show(list_creation, x + 20, y);

	/* Race picking, pick first possible gender. */
	if (char_step == 0)
	{
		box.w = 460;
		box.h = 96;
		string_show(ScreenSurface, FONT_SERIF12, s_settings->characters[list_creation->row_selected - 1].desc, x + 20, y + 125, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);

		for (i = 0; i < GENDER_MAX; i++)
		{
			/* Does the selected race have this gender? */
			if (s_settings->characters[list_creation->row_selected - 1].gender_archetypes[i])
			{
				face = get_bmap_id(s_settings->characters[list_creation->row_selected - 1].gender_faces[i]);
				break;
			}
		}
	}
	else if (char_step == 1)
	{
		char buf[MAX_BUF];

		strncpy(buf, list_creation->text[list_creation->row_selected - 1][0], sizeof(buf) - 1);
		buf[0] = tolower(buf[0]);
		buf[sizeof(buf) - 1] = '\0';
		face = get_bmap_id(s_settings->characters[char_race_selected].gender_faces[gender_to_id(buf)]);
	}

	if (char_step == 2)
	{
	}
	else
	{
		face_show(face, x + 300, y + 35);
	}

	/* Show the stat values and the range buttons. */
	if (char_step == 2)
	{
	}

	y += 100;

	if (char_step == 2)
	{
		y += 65;
	}

	return 1;
}

/**
 * Draw the server connection/character creation popup.
 * @param popup Popup. */
static int popup_draw_func(popup_struct *popup)
{
	uint8 downloading;
	SDL_Rect box;

	/* Waiting to log in. */
	if (cpl.state == ST_WAITFORPLAY)
	{
		box.w = popup->surface->w;
		box.h = popup->surface->h;
		string_show_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return 1;
	}
	/* Playing now, so destroy this popup. */
	else if (cpl.state == ST_PLAY)
	{
		return 0;
	}
	/* Connection terminated while we were trying to login. */
	else if (cpl.state <= ST_WAITLOOP)
	{
		return 0;
	}

	/* Downloading the files, or updates haven't finished yet? */
	downloading = cpl.state < ST_LOGIN || !file_updates_finished();

	progress.done = !downloading;
	progress_dots_show(&progress, popup->surface, 75, 42);

	/* Show that we are connecting to the server. */
	box.w = 420;
	box.h = 22;
	string_show_shadow(popup->surface, FONT_SERIF14, "Character Login", 40, 8, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	if (downloading)
	{
		return 1;
	}

	box.w = popup->surface->w / 2;

	return 1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback_func(popup_struct *popup)
{
	(void) popup;

	list_remove(list_creation);
	list_creation = NULL;

	if (cpl.state != ST_PLAY)
	{
		cpl.state = ST_START;
	}

	return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{

	return -1;
}

/**
 * Handle enter key being pressed in the servers list.
 * @param list The servers list. */
static void list_handle_enter(list_struct *list)
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
			popup_struct *popup = popup_create("popup");

			popup->draw_func = popup_draw_func;
			popup->draw_post_func = popup_draw_post_func;
			popup->event_func = popup_event_func;
			popup->destroy_callback_func = popup_destroy_callback_func;
			progress_dots_create(&progress);
		}
	}
	else if (list == list_news)
	{
		if (list->text && list->text[list->row_selected - 1])
		{
			popup_struct *popup = popup_create("popup");

			popup->draw_func = news_popup_draw_func;
			popup->event_func = news_popup_event_func;
			news_scroll_offset = news_num_lines = 0;
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
void main_screen_render(void)
{
	SDL_Surface *texture;
	int x, y;
	size_t server_count;
	server_struct *node;
	char buf[MAX_BUF];
	SDL_Rect box;

	texture = TEXTURE_CLIENT("intro");

	/* Background */
	surface_show(ScreenSurface, 0, 0, NULL, texture);
	textwin_show(texture->w, 1, ScreenSurface->w - texture->w - 2, ScreenSurface->h - 3);

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
		string_show_shadow(ScreenSurface, FONT_ARIAL10, buf, x + 13, y + 185, COLOR_HGOLD, COLOR_BLACK, 0, NULL);

		box.w = 410;
		box.h = 48;
		string_show(ScreenSurface, FONT_ARIAL10, node->desc, x + 13, y + 197, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	/* Show whether we are connecting to the metaserver or not. */
	if (ms_connecting(-1))
	{
		string_show_shadow(ScreenSurface, FONT_ARIAL10, "Connecting to metaserver, please wait...", x + 105, y + 8, COLOR_HGOLD, COLOR_BLACK, 0, NULL);
	}
	else
	{
		string_show_shadow(ScreenSurface, FONT_ARIAL10, "Select a server.", x + 226, y + 8, COLOR_GREEN, COLOR_BLACK, 0, NULL);
	}

	texture = TEXTURE_CLIENT("servers_bg_over");
	surface_show(ScreenSurface, x, y, NULL, texture);

	x += texture->w + 20;
	texture = TEXTURE_CLIENT("news_bg");
	surface_show(ScreenSurface, x, y, NULL, texture);

	box.w = texture->w;
	box.h = 0;
	string_show_shadow(ScreenSurface, FONT_SERIF12, "Game News", x, y + 10, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

	/* No list yet, make one and start downloading the data. */
	if (!list_news)
	{
		/* Start downloading. */
		news_data = curl_download_start("http://www.atrinik.org/client_news.php");

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

	button_play.x = button_refresh.x = button_settings.x = button_update.x = button_help.x = button_quit.x = 489;
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

	button_quit.y = y + 224;
	button_show(&button_quit, "Quit");
}

/**
 * Handle event in the main screen.
 * @param event The event to handle.
 * @return 1 if the event was handled, 0 otherwise. */
int main_screen_event(SDL_Event *event)
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
		list_handle_enter(list_servers);
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

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
/** Assigned stats points. */
static int char_points_assigned[7];
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
		string_blt(popup->surface, FONT_SERIF12, list_news->text[list_news->row_selected - 1][0], 40, 8, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

		box.w = NEWS_MAX_WIDTH;
		box.h = NEWS_MAX_HEIGHT;

		/* Calculate number of last displayed lines. */
		if (!news_num_lines)
		{
			string_blt(NULL, NEWS_FONT, popup->buf, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
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
		string_blt(popup->surface, NEWS_FONT, popup->buf, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);
		text_offset_reset();

		scrollbar_render(&scrollbar_news, popup->surface, Bitmaps[popup->bitmap_id]->bitmap->w - 28, 45);
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
	string_blt(popup->surface, FONT_SERIF12, "Downloading news, please wait...", 10, 40, COLOR_WHITE, TEXT_ALIGN_CENTER, NULL);
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
 * (Re-)initialize character creation. Used when first showing the popup,
 * or when using the 'Previous' buttons to reset the current data.
 * @param list If not NULL, will be removed. */
static void char_creation_reset(list_struct *list)
{
	/* Reset race. */
	if (char_step == 0)
	{
		char_race_selected = 0;
	}

	/* Reset gender. */
	if (char_step == 0 || char_step == 1)
	{
		char_gender_selected = 0;
	}

	/* Reset assigned points. */
	if (char_step == 0 || char_step == 2)
	{
		memset(&char_points_assigned, 0, sizeof(char_points_assigned));
	}

	/* Can we actually go back? */
	if (char_step)
	{
		char_step--;
	}

	if (list)
	{
		list_remove(list);
		list_creation = NULL;
	}
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
		/* Initialize maximum stat points that can be assigned. */
		char_points_left = s_settings->characters[char_race_selected].points_max;
	}
	/* Selected stats, create the character. */
	else if (char_step == 2)
	{
		if (char_points_left)
		{
			return;
		}

		snprintf(buf, sizeof(buf), "nc %s %d %d %d %d %d %d %d", s_settings->characters[char_race_selected].gender_archetypes[char_gender_selected], s_settings->characters[char_race_selected].stats_base[0] + char_points_assigned[0], s_settings->characters[char_race_selected].stats_base[1] + char_points_assigned[1], s_settings->characters[char_race_selected].stats_base[2] + char_points_assigned[2], s_settings->characters[char_race_selected].stats_base[3] + char_points_assigned[3], s_settings->characters[char_race_selected].stats_base[4] + char_points_assigned[4], s_settings->characters[char_race_selected].stats_base[5] + char_points_assigned[5], s_settings->characters[char_race_selected].stats_base[6] + char_points_assigned[6]);
		cs_write_string(buf, strlen(buf));
		return;
	}

	char_step++;

	if (list)
	{
		list_remove(list);
		list_creation = NULL;
	}
}

/**
 * Adjust specified stat, taking into account the character's min/max
 * stats.
 * @param stat_id Stat ID.
 * @param adjust If higher than 0 the stat will be increased, if lower
 * than 0 it will be decreased. */
static void char_stat_change(int stat_id, int adjust)
{
	int points = s_settings->characters[char_race_selected].stats_base[stat_id] + char_points_assigned[stat_id];

	/* Add to stat, if possible. */
	if (adjust > 0 && char_points_left && points < s_settings->characters[char_race_selected].stats_max[stat_id])
	{
		char_points_assigned[stat_id]++;
		char_points_left--;
	}
	/* Subtract from stat, if possible. */
	else if (adjust < 0 && points > s_settings->characters[char_race_selected].stats_min[stat_id])
	{
		char_points_assigned[stat_id]--;
		char_points_left++;
	}
}

/** @copydoc list_struct::key_event_func */
static int char_creation_key(list_struct *list, SDLKey key)
{
	switch (key)
	{
		/* Change selected stats using left/right arrow keys. */
		case SDLK_RIGHT:
		case SDLK_LEFT:
			char_stat_change(list->row_selected - 1, key == SDLK_LEFT ? -1 : 1);
			return 1;

		default:
			return -1;
	}
}

/** @copydoc popup_struct::draw_func_post */
static int popup_draw_func_post(popup_struct *popup)
{
	size_t i;
	int face = 0, x, y;
	SDL_Rect box;

	x = popup->x;
	y = popup->y;

	/* Not creating character, only show the message text window. */
	if (GameStatus != GAME_STATUS_NEW_CHAR)
	{
		textwin_show(x + Bitmaps[popup->bitmap_id]->bitmap->w / 2, y + 40, 220, 132);
		return 1;
	}

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
			list_creation->key_event_func = char_creation_key;
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
		string_blt(ScreenSurface, FONT_SERIF12, s_settings->characters[list_creation->row_selected - 1].desc, x + 20, y + 125, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);

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
		box.w = 370;
		box.h = 150;
		string_blt(ScreenSurface, FONT_ARIAL10, s_settings->text[SERVER_TEXT_STATS], x + 116, y + 10, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}
	else
	{
		blit_face(face, x + 300, y + 35);
	}

	/* Show the stat values and the range buttons. */
	if (char_step == 2)
	{
		int adjust = 0;

		for (i = 0; i < NUM_STATS; i++)
		{
			/* Calculate the current stat value and show it. */
			string_blt_shadow_format(ScreenSurface, FONT_ARIAL12, x + 60, y + 10 + i * 18 + 4, i == list_creation->row_selected - 1 ? COLOR_GREEN : COLOR_HGOLD, COLOR_BLACK, 0, NULL, "%.2d", s_settings->characters[char_race_selected].stats_base[i] + char_points_assigned[i]);

			/* One of the range buttons clicked? */
			if (range_buttons_show(x + 80, y + 10 + i * 18, &adjust, 1))
			{
				char_stat_change(i, adjust);
			}
		}

		string_blt_shadow(ScreenSurface, FONT_SANS12, "Left:", x + 20, y + 144, COLOR_WHITE, COLOR_BLACK, 0, NULL);
		string_blt_shadow_format(ScreenSurface, FONT_ARIAL12, x + 60, y + 145, COLOR_HGOLD, COLOR_BLACK, 0, NULL, "%d", char_points_left);
	}

	y += 100;

	if (char_step == 2)
	{
		y += 65;
	}

	/* Show previous button if we're not in the first step. */
	if (char_step > 0)
	{
		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, x + 19, y, "Previous", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			char_creation_reset(list_creation);
		}
	}

	/* Show the next button, or the play button if we're in the last step. */
	if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, x + (char_step == char_step_max ? 90 : 220), y, char_step == char_step_max ? "Play" : "Next", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
	{
		char_creation_enter(list_creation);
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
	char buf[MAX_BUF];
	int x, y;

	/* Waiting to log in. */
	if (GameStatus == GAME_STATUS_WAITFORPLAY)
	{
		box.w = Bitmaps[popup->bitmap_id]->bitmap->w;
		box.h = Bitmaps[popup->bitmap_id]->bitmap->h;
		string_blt_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return 1;
	}
	else if (GameStatus == GAME_STATUS_NEW_CHAR)
	{
		box.w = 420;
		box.h = 22;
		string_blt_shadow_format(popup->surface, FONT_SERIF14, 40, 8, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box, "Welcome, %s!", cpl.name);
		box.w = Bitmaps[popup->bitmap_id]->bitmap->w - 40;
		box.h = char_step == 2 ? 70 : 30;
		string_blt_shadow(popup->surface, FONT_ARIAL12, s_settings->text[SERVER_TEXT_STEP0 + char_step], 20, 45, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP, &box);
		return 1;
	}
	/* Playing now, so destroy this popup. */
	else if (GameStatus == GAME_STATUS_PLAY)
	{
		return 0;
	}
	/* Connection terminated while we were trying to login. */
	else if (GameStatus <= GAME_STATUS_WAITLOOP)
	{
		return 0;
	}

	/* Downloading the files, or updates haven't finished yet? */
	downloading = GameStatus < GAME_STATUS_LOGIN || !file_updates_finished();

	progress.done = !downloading;
	progress_dots_show(&progress, popup->surface, 75, 42);

	/* Show that we are connecting to the server. */
	box.w = 420;
	box.h = 22;
	string_blt_shadow(popup->surface, FONT_SERIF14, "Character Login", 40, 8, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	if (downloading)
	{
		return 1;
	}

	box.w = Bitmaps[popup->bitmap_id]->bitmap->w / 2;
	x = Bitmaps[popup->bitmap_id]->bitmap->w / 4 - text_input_center_offset();
	y = 85;

	/* Player name. */
	if (GameStatus == GAME_STATUS_NAME)
	{
		string_blt(popup->surface, FONT_ARIAL10, "Enter your name", 0, 65, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		text_input_string[0] = toupper(text_input_string[0]);
		text_input_show(popup->surface, x, y, FONT_ARIAL10, text_input_string, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
	}
	else if (cpl.name[0])
	{
		cpl.name[0] = toupper(cpl.name[0]);
		text_input_draw_background(popup->surface, x, y, BITMAP_LOGIN_INP);
		text_input_draw_text(popup->surface, x, y, FONT_ARIAL10, cpl.name, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
	}

	y += 35;

	/* Player password. */
	if (GameStatus == GAME_STATUS_PSWD || cpl.password[0])
	{
		char *cp;

		strncpy(buf, GameStatus == GAME_STATUS_PSWD ? text_input_string : cpl.password, sizeof(buf) - 1);

		for (cp = buf; *cp; cp++)
		{
			*cp = '*';
		}

		if (GameStatus == GAME_STATUS_PSWD)
		{
			string_blt(popup->surface, FONT_ARIAL10, "Enter your password", 0, 105, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
			text_input_show(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
		}
		else
		{
			text_input_draw_background(popup->surface, x, y, BITMAP_LOGIN_INP);
			text_input_draw_text(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
		}
	}

	y += 35;

	/* Verify password for character creation. */
	if (GameStatus == GAME_STATUS_VERIFYPSWD)
	{
		char *cp;

		strncpy(buf, text_input_string, sizeof(buf) - 1);

		for (cp = buf; *cp; cp++)
		{
			*cp = '*';
		}

		string_blt(popup->surface, FONT_ARIAL10, "New Character: Verify Password", 0, 140, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		text_input_show(popup->surface, x, y, FONT_ARIAL10, buf, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
		char_step = 0;
		char_creation_reset(NULL);
	}

	y += 20;

	box.w = Bitmaps[popup->bitmap_id]->bitmap->w - 45;
	box.h = 120;
	string_blt_shadow(popup->surface, FONT_ARIAL12, s_settings->text[SERVER_TEXT_LOGIN], x, y, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP, &box);

	return 1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback_func(popup_struct *popup)
{
	(void) popup;

	list_remove(list_creation);
	list_creation = NULL;

	if (GameStatus != GAME_STATUS_PLAY)
	{
		GameStatus = GAME_STATUS_START;
	}

	return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
	(void) popup;

	/* Handle events in character creation. */
	if (GameStatus == GAME_STATUS_NEW_CHAR)
	{
		/* Handle list events. */
		if (list_creation)
		{
			if (list_handle_keyboard(list_creation, event))
			{
				return 1;
			}
			else if (list_handle_mouse(list_creation, event))
			{
				return 1;
			}
		}
	}

	if (event->type == SDL_KEYDOWN)
	{
		/* Try to handle text input. */
		if (text_input_handle(&event->key))
		{
			return 1;
		}

		/* Ignore. */
		return 0;
	}

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
			popup_struct *popup = popup_create(BITMAP_POPUP);

			popup->draw_func = popup_draw_func;
			popup->draw_func_post = popup_draw_func_post;
			popup->event_func = popup_event_func;
			popup->destroy_callback_func = popup_destroy_callback_func;
			GameStatus = GAME_STATUS_STARTCONNECT;
			progress_dots_create(&progress);
		}
	}
	else if (list == list_news)
	{
		if (list->text && list->text[list->row_selected - 1])
		{
			popup_struct *popup = popup_create(BITMAP_POPUP);

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
	int x, y;
	size_t server_count;
	server_struct *node;
	char buf[MAX_BUF];
	SDL_Rect box;

	x = 15;
	y = ScreenSurface->h - Bitmaps[BITMAP_SERVERS_BG]->bitmap->h - 5;

	/* Background */
	sprite_blt(Bitmaps[BITMAP_INTRO], 0, 0, NULL, NULL);
	textwin_show(Bitmaps[BITMAP_INTRO]->bitmap->w, 1, ScreenSurface->w - Bitmaps[BITMAP_INTRO]->bitmap->w - 2, ScreenSurface->h - 3);
	sprite_blt(Bitmaps[BITMAP_SERVERS_BG], x, y, NULL, NULL);

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
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, buf, x + 13, y + 185, COLOR_HGOLD, COLOR_BLACK, 0, NULL);

		box.w = 410;
		box.h = 48;
		string_blt(ScreenSurface, FONT_ARIAL10, node->desc, x + 13, y + 197, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	/* Show whether we are connecting to the metaserver or not. */
	if (ms_connecting(-1))
	{
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, "Connecting to metaserver, please wait...", x + 105, y + 8, COLOR_HGOLD, COLOR_BLACK, 0, NULL);
	}
	else
	{
		string_blt_shadow(ScreenSurface, FONT_ARIAL10, "Select a server.", x + 226, y + 8, COLOR_GREEN, COLOR_BLACK, 0, NULL);
	}

	sprite_blt(Bitmaps[BITMAP_SERVERS_BG_OVER], x, y, NULL, NULL);

	x += Bitmaps[BITMAP_SERVERS_BG_OVER]->bitmap->w + 20;
	sprite_blt(Bitmaps[BITMAP_NEWS_BG], x, y, NULL, NULL);

	box.w = Bitmaps[BITMAP_NEWS_BG]->bitmap->w;
	box.h = 0;
	string_blt_shadow(ScreenSurface, FONT_SERIF12, "Game News", x, y + 10, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

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

	button_play.x = button_refresh.x = button_settings.x = button_update.x = button_help.x = button_quit.x = 489;
	y += 2;

	button_play.y = y + 10;
	button_render(&button_play, "Play");

	button_refresh.y = y + 35;
	button_render(&button_refresh, "Refresh");

	button_settings.y = y + 60;
	button_render(&button_settings, "Settings");

	button_update.y = y + 85;
	button_render(&button_update, "Update");

	button_help.y = y + 110;
	button_render(&button_help, "Help");

	button_quit.y = y + 224;
	button_render(&button_quit, "Quit");
}

/**
 * Handle event in the main screen.
 * @param event The event to handle.
 * @return 1 if the event was handled, 0 otherwise. */
int main_screen_event(SDL_Event *event)
{
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
			GameStatus = GAME_STATUS_META;
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

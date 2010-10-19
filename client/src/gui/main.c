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
		box.h = 0;
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
	}
}

/**
 * Adjust specified stat, taking into account the character's min/max
 * stats.
 * @param stat Stat ID.
 * @param adjust If higher than 0 the stat will be increased, if lower
 * than 0 it will be decreased. */
static void char_stat_change(int stat, int adjust)
{
	int points = s_settings->characters[char_race_selected].stats_base[stat] + char_points_assigned[stat];

	/* Add to stat, if possible. */
	if (adjust > 0 && char_points_left && points < s_settings->characters[char_race_selected].stats_max[stat])
	{
		char_points_assigned[stat]++;
		char_points_left--;
	}
	/* Subtract from stat, if possible. */
	else if (adjust < 0 && points > s_settings->characters[char_race_selected].stats_min[stat])
	{
		char_points_assigned[stat]--;
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
static void popup_draw_func_post(popup_struct *popup, int x, int y)
{
	list_struct *list = NULL;
	size_t i;
	int face = 0;
	SDL_Rect box;

	/* Not creating character, nothing to do. */
	if (GameStatus != GAME_STATUS_NEW_CHAR)
	{
		return;
	}

	(void) popup;

	list = list_exists(LIST_CREATION);

	y += 50;

	if (char_step == 2)
	{
		y += 40;
	}

	if (!list)
	{
		/* Create a new list. */
		list = list_create(LIST_CREATION, x + 20, y, 7, 1, 0);
		list_set_focus(list);
		list->handle_enter_func = char_creation_enter;

		/* Show list of races. */
		if (char_step == 0)
		{
			list_set_column(list, 0, 250, 7, NULL, -1);

			for (i = 0; i < s_settings->num_characters; i++)
			{
				list_add(list, i, 0, s_settings->characters[i].name);
			}
		}
		/* List of genders. */
		else if (char_step == 1)
		{
			char buf[30];
			size_t row = 0;

			list_set_column(list, 0, 250, 7, NULL, -1);

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
					list_add(list, row, 0, buf);
					row++;
				}
			}
		}
		/* The stats. */
		else if (char_step == 2)
		{
			list_set_column(list, 0, 30, 7, NULL, -1);
			list->y += 2;
			list->row_height_adjust = 6;
			list->row_color_func = NULL;
			list->row_highlight_func = NULL;
			list->row_selected_func = NULL;
			list->draw_frame_func = NULL;
			list->key_event_func = char_creation_key;
			list_add(list, 0, 0, "STR:");
			list_add(list, 1, 0, "DEX:");
			list_add(list, 2, 0, "CON:");
			list_add(list, 3, 0, "INT:");
			list_add(list, 4, 0, "WIS:");
			list_add(list, 5, 0, "POW:");
			list_add(list, 6, 0, "CHA:");
		}
	}

	list_show(list);

	/* Race picking, pick first possible gender. */
	if (char_step == 0)
	{
		box.w = 460;
		box.h = 96;
		string_blt(ScreenSurface, FONT_SERIF12, s_settings->characters[list->row_selected - 1].desc, x + 20, y + 125, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP, &box);

		for (i = 0; i < GENDER_MAX; i++)
		{
			/* Does the selected race have this gender? */
			if (s_settings->characters[list->row_selected - 1].gender_archetypes[i])
			{
				face = s_settings->characters[list->row_selected - 1].gender_faces[i];
				break;
			}
		}
	}
	else if (char_step == 1)
	{
		char buf[MAX_BUF];

		strncpy(buf, list->text[list->row_selected - 1][0], sizeof(buf) - 1);
		buf[0] = tolower(buf[0]);
		buf[sizeof(buf) - 1] = '\0';
		face = s_settings->characters[char_race_selected].gender_faces[gender_to_id(buf)];
	}

	if (char_step == 2)
	{
		box.w = 370;
		box.h = 150;
		string_blt(ScreenSurface, FONT_ARIAL10, s_settings->text[SERVER_TEXT_STATS], x + 116, y + 10, COLOR_SIMPLE(COLOR_WHITE), TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}
	else
	{
		blit_face(face, x + 300, y + 35);
	}

	/* Show the stat values and the range buttons. */
	if (char_step == 2)
	{
		int adjust = 0;
		size_t i;

		for (i = 0; i < NUM_STATS; i++)
		{
			/* Calculate the current stat value and show it. */
			string_blt_shadow_format(ScreenSurface, FONT_ARIAL12, x + 60, y + 10 + i * 18 + 4, i == list->row_selected - 1 ? COLOR_SIMPLE(COLOR_GREEN) : COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), 0, NULL, "%.2d", s_settings->characters[char_race_selected].stats_base[i] + char_points_assigned[i]);

			/* One of the range buttons clicked? */
			if (range_buttons_show(x + 80, y + 10 + i * 18, &adjust, 1))
			{
				char_stat_change(i, adjust);
			}
		}

		string_blt_shadow(ScreenSurface, FONT_SANS12, "Left:", x + 20, y + 150, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), 0, NULL);
		string_blt_shadow_format(ScreenSurface, FONT_ARIAL12, x + 60, y + 150, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), 0, NULL, "%d", char_points_left);
	}

	y += 100;

	if (char_step == 2)
	{
		y += 70;
	}

	/* Show previous button if we're not in the first step. */
	if (char_step > 0)
	{
		if (button_show(BITMAP_DIALOG_BUTTON_UP, -1, BITMAP_DIALOG_BUTTON_DOWN, x + 19, y, "Previous", FONT_ARIAL10, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK)))
		{
			char_creation_reset(list);
		}
	}

	/* Show the next button, or the play button if we're in the last step. */
	if (button_show(BITMAP_DIALOG_BUTTON_UP, -1, BITMAP_DIALOG_BUTTON_DOWN, x + (char_step == char_step_max ? 90 : 220), y, char_step == char_step_max ? "Play" : "Next", FONT_ARIAL10, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK)))
	{
		char_creation_enter(list);
	}
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
	else if (GameStatus == GAME_STATUS_NEW_CHAR)
	{
		box.w = Bitmaps[popup->bitmap_id]->bitmap->w;
		box.h = 0;
		string_blt_shadow_format(popup->surface, FONT_SERIF14, 0, 10, COLOR_SIMPLE(COLOR_HGOLD), COLOR_SIMPLE(COLOR_BLACK), TEXT_ALIGN_CENTER, &box, "Welcome, %s!", cpl.name);
		box.w = Bitmaps[popup->bitmap_id]->bitmap->w - 40;
		box.h = char_step == 2 ? 70 : 30;
		string_blt_shadow(popup->surface, FONT_ARIAL12, s_settings->text[SERVER_TEXT_STEP0 + char_step], 20, 30, COLOR_SIMPLE(COLOR_WHITE), COLOR_SIMPLE(COLOR_BLACK), TEXT_MARKUP | TEXT_WORD_WRAP, &box);
		return;
	}
	/* Playing now, so destroy this popup and remove any lists. */
	else if (GameStatus == GAME_STATUS_PLAY)
	{
		popup_destroy_visible();
		list_remove_all();
		return;
	}
	/* Connection terminated while we were trying to login. */
	else if (GameStatus <= GAME_STATUS_WAITLOOP)
	{
		popup_destroy_visible();
		return;
	}

	bltfx.surface = popup->surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;

	/* Update progress bar of requested files */
	sprite_blt(Bitmaps[BITMAP_PROGRESS_BACK], Bitmaps[popup->bitmap_id]->bitmap->w / 2 - Bitmaps[BITMAP_PROGRESS_BACK]->bitmap->w / 2, 30, NULL, &bltfx);

	progress = MIN(100, 8);
	box.x = 0;
	box.y = 0;
	box.h = Bitmaps[BITMAP_PROGRESS]->bitmap->h;
	box.w = (int) ((float) Bitmaps[BITMAP_PROGRESS]->bitmap->w / 100 * progress);
	sprite_blt(Bitmaps[BITMAP_PROGRESS], Bitmaps[popup->bitmap_id]->bitmap->w / 2 - Bitmaps[BITMAP_PROGRESS]->bitmap->w / 2, 30, &box, &bltfx);

	/* Show that we are connecting to the server. */
	box.w = Bitmaps[popup->bitmap_id]->bitmap->w;
	box.h = 0;
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
		char_step = 0;
		char_creation_reset(NULL);
	}
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback_func(popup_struct *popup)
{
	list_struct *list = list_exists(LIST_CREATION);

	(void) popup;

	if (list)
	{
		list_remove(list);
		list_set_focus(list_exists(LIST_SERVERS));
	}

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
		list_struct *list = list_exists(LIST_CREATION);

		/* Handle list events. */
		if (list)
		{
			if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
			{
				if (lists_handle_keyboard(&event->key))
				{
					return 1;
				}
			}
			else
			{
				if (LIST_MOUSE_OVER(list, event->motion.x, event->motion.y))
				{
					list_handle_mouse(list, event->motion.x, event->motion.y, event);
					return 1;
				}
			}
		}
	}

	if (event->type == SDL_KEYDOWN)
	{
		/* Try to handle text input. */
		if (key_string_event(&event->key))
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
	if (list->id == LIST_SERVERS)
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
	box.h = 0;
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

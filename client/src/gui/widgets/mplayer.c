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
 * Implements mplayer type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * File where the blacklist data is stored. */
#define FILE_MPLAYER_BLACKLIST "mplayer.blacklist"
/**
 * How many milliseconds the blacklist button must be held in order to
 * mass-change blacklist status. */
#define BLACKLIST_ALL_DELAY 1500

enum
{
	BUTTON_PLAY,
	BUTTON_SHUFFLE,
	BUTTON_BLACKLIST,
	BUTTON_CLOSE,
	BUTTON_HELP,

	BUTTON_NUM
};

/**
 * Is shuffle enabled? */
static uint8 shuffle = 0;
/**
 * Blacklisted music files. */
static uint8 *shuffle_blacklist = NULL;
/**
 * Button buffer. */
static button_struct buttons[BUTTON_NUM];
/**
 * Scrollbar buffer. */
static scrollbar_struct scrollbar_progress;
/**
 * Scrollbar info buffer. */
static scrollbar_info_struct scrollbar_progress_info;
/**
 * The music player list. */
static list_struct *list_mplayer = NULL;

/**
 * Handle music list double-click and "Play" button.
 * @param list The music list. */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
	sound_start_bg_music(list->text[list->row_selected - 1][0], setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);
	sound_map_background(1);
	shuffle = 0;
}

/** @copydoc list_struct::text_color_hook */
static void list_text_color_hook(list_struct *list, uint32 row, uint32 col, const char **color, const char **color_shadow)
{
	if (shuffle_blacklist[row])
	{
		*color = COLOR_RED;
	}
}

/**
 * Perform a shuffle of the selected row in the music list and start
 * playing the shuffled music.
 * @param list The music list. */
static void mplayer_do_shuffle(list_struct *list)
{
	size_t i;
	uint8 found_num = 0;
	uint32 *row_ids, row_num, selected;

	/* Calculate whether there are enough non-blacklisted songs to
	 * shuffle through. */
	for (i = 0; i < list->rows - 1 && found_num < 2; i++)
	{
		if (!shuffle_blacklist[i])
		{
			found_num++;
		}
	}

	if (found_num < 2)
	{
		return;
	}

	/* Build a list containing non-blacklisted row IDs. */
	row_num = 0;
	row_ids = malloc(sizeof(*row_ids) * (list->rows - 1));

	for (i = 0; i < list->rows - 1; i++)
	{
		if (!shuffle_blacklist[i])
		{
			row_ids[row_num++] = i;
		}
	}

	/* Select a row ID at random. */
	selected = row_ids[rndm(1, row_num) - 1];
	free(row_ids);

	list->row_selected = selected + 1;
	list->row_offset = MIN(list->rows - list->max_rows, selected);
	cur_widget[MPLAYER_ID]->redraw = 1;

	sound_start_bg_music(list->text[list->row_selected - 1][0], setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), 0);
}

/**
 * Check whether we need to start another song. */
static void mplayer_check_shuffle(void)
{
	if (!sound_playing_music())
	{
		mplayer_do_shuffle(list_mplayer);
	}
}

/**
 * Check whether the currently selected row in the music list is
 * blacklisted.
 * @param list The music list.
 * @return 1 if the selected row is blacklisted, 0 otherwise. */
static int mplayer_blacklisted(list_struct *list)
{
	if (list && shuffle_blacklist && shuffle_blacklist[list->row_selected - 1])
	{
		return 1;
	}

	return 0;
}

/**
 * Toggle blacklist status on the selected row.
 * @param list The music list. */
static void mplayer_blacklist_toggle(list_struct *list)
{
	if (list && shuffle_blacklist)
	{
		/* Clear blacklist status. */
		if (shuffle_blacklist[list->row_selected - 1])
		{
			shuffle_blacklist[list->row_selected - 1] = 0;
		}
		/* Enable blacklist status. */
		else
		{
			shuffle_blacklist[list->row_selected - 1] = 1;

			/* Shuffle mode and we're playing the music we just
			 * blacklisted, so stop playing it. */
			if (shuffle && !strcmp(sound_get_bg_music_basename(), list->text[list->row_selected - 1][0]))
			{
				sound_start_bg_music("no_music", 0, 0);
			}
		}

		cur_widget[MPLAYER_ID]->redraw = 1;
	}
}

/**
 * Change blacklist status of all the music files at once.
 * @param list The music list.
 * @param state 1 to blacklist all, 0 to clear blacklist status. */
static void mplayer_blacklist_mass_toggle(list_struct *list, uint8 state)
{
	if (list && shuffle_blacklist)
	{
		size_t row;

		for (row = 0; row < list->rows - 1; row++)
		{
			shuffle_blacklist[row] = state;
		}

		/* Shuffle mode and we're disabling all, turn off music. */
		if (shuffle && state == 1)
		{
			sound_start_bg_music("no_music", 0, 0);
		}

		cur_widget[MPLAYER_ID]->redraw = 1;
	}
}

/**
 * Save the blacklist data to file.
 * @param list The music list. */
static void mplayer_blacklist_save(list_struct *list)
{
	FILE *fp;
	size_t row;

	fp = fopen_wrapper(FILE_MPLAYER_BLACKLIST, "w");

	for (row = 0; row < list->rows; row++)
	{
		if (shuffle_blacklist[row])
		{
			fprintf(fp, "%s\n", list->text[row][0]);
		}
	}

	fclose(fp);
}

/**
 * Initialize music player list by reading the directory 'path'.
 * @param list The music player list.
 * @param path The directory to read.
 * @param duplicates Whether to check for and ignore duplicates in the
 * directory (entries already in the list). */
static void mplayer_list_init(list_struct *list, const char *path, uint8 duplicates)
{
	DIR *dir;
	struct dirent *currentfile;
	char buf[HUGE_BUF];

	/* Read the media directory and add the file names to the list. */
	dir = opendir(path);

	if (!dir)
	{
		return;
	}

	while ((currentfile = readdir(dir)))
	{
		/* Ignore hidden files and files without extension. */
		if (currentfile->d_name[0] == '.' || !strchr(currentfile->d_name, '.'))
		{
			continue;
		}

		/* Check for duplicates. */
		if (duplicates)
		{
			uint8 found = 0;
			uint32 row;

			for (row = 0; row < list->rows; row++)
			{
				if (!strcmp(list->text[row][0], currentfile->d_name))
				{
					found = 1;
					break;
				}
			}

			if (found)
			{
				continue;
			}
		}

		snprintf(buf, sizeof(buf), "%s/%s", path, currentfile->d_name);

		/* Ignore files that cannot be accessed for reading; insufficient
		 * permissions, or broken symlinks, for example. */
		if (access(buf, R_OK) == 0)
		{
			list_add(list, list->rows, 0, currentfile->d_name);
		}
	}

	closedir(dir);
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box;
	char buf[HUGE_BUF];
	size_t i;

	/* The list doesn't exist yet, create it. */
	if (!list_mplayer)
	{
		char version[MAX_BUF];

		/* Create the list and set up settings. */
		list_mplayer = list_create(12, 1, 8);
		list_mplayer->handle_enter_func = list_handle_enter;
		list_mplayer->text_color_hook = list_text_color_hook;
		list_mplayer->surface = widget->surface;
		list_scrollbar_enable(list_mplayer);
		list_set_column(list_mplayer, 0, 130, 7, NULL, -1);
		list_set_font(list_mplayer, FONT_ARIAL10);

		/* Add default media directory songs. */
		get_data_dir_file(buf, sizeof(buf), DIRECTORY_MEDIA);
		mplayer_list_init(list_mplayer, buf, 0);

		/* Now add custom ones, but ignore duplicates. */
		snprintf(buf, sizeof(buf), "%s/.atrinik/%s/"DIRECTORY_MEDIA, get_config_dir(), package_get_version_partial(version, sizeof(version)));
		mplayer_list_init(list_mplayer, buf, 1);

		/* If we added any, sort the list alphabetically and add an entry
		 * to disable background music. */
		if (list_mplayer->rows)
		{
			FILE *fp;

			/* Allocate the blacklist. + 1 is for the last entry added
			 * further down. It is not actually used by the blacklist as
			 * it's not possible to toggle it on/off using the button, but
			 * it simplifies other logic checks. */
			shuffle_blacklist = calloc(1, sizeof(*shuffle_blacklist) * (list_mplayer->rows + 1));

			/* Sort the list. */
			list_sort(list_mplayer, LIST_SORT_ALPHA);

			/* Read the blacklist file contents. */
			fp = fopen_wrapper(FILE_MPLAYER_BLACKLIST, "r");

			if (fp)
			{
				size_t row;

				while (fgets(buf, sizeof(buf) - 1, fp))
				{
					for (row = 0; row < list_mplayer->rows; row++)
					{
						if (!strncmp(buf, list_mplayer->text[row][0], strlen(buf) - 1))
						{
							shuffle_blacklist[row] = 1;
							break;
						}
					}
				}

				fclose(fp);
			}

			list_add(list_mplayer, list_mplayer->rows, 0, "Disable music");
		}

		for (i = 0; i < BUTTON_NUM; i++)
		{
			button_create(&buttons[i]);

			if (i == BUTTON_BLACKLIST || i == BUTTON_HELP || i == BUTTON_CLOSE)
			{
				buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
				buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
				buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
			}
		}

		scrollbar_create(&scrollbar_progress, 130, 11, &scrollbar_progress_info.scroll_offset, &scrollbar_progress_info.num_lines, 1);
		scrollbar_progress.redraw = &scrollbar_progress_info.redraw;
	}

	if (widget->redraw)
	{
		const char *bg_music;

		box.h = 0;
		box.w = widget->w;
		text_show(widget->surface, FONT_SERIF12, "Music Player", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		list_set_parent(list_mplayer, widget->x, widget->y);
		list_show(list_mplayer, 10, 2);
		box.w /= 2;
		text_show(widget->surface, FONT_SANS10, "Currently playing:", widget->w / 2, 22, COLOR_WHITE, TEXT_ALIGN_CENTER, &box);

		bg_music = sound_get_bg_music_basename();
		box.h = 0;
		box.w = widget->w / 2;

		/* Store the background music file name in temporary buffer and
		 * make sure it won't overflow by truncating it if necessary. */
		if (bg_music)
		{
			strncpy(buf, bg_music, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			text_truncate_overflow(FONT_SANS11, buf, 150);
		}

		/* Show the music that is being played. */
		text_show(widget->surface, FONT_SANS11, bg_music ? buf : "No music", widget->w / 2 - 5, 34, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		scrollbar_progress.px = widget->x;
		scrollbar_progress.py = widget->y;
		scrollbar_show(&scrollbar_progress, widget->surface, 170, 50);

		box.h = 120;
		box.w -= 6;
		text_show(widget->surface, FONT_ARIAL10, "You can use the music player to play your favorite tunes from the game, or play them all one-by-one in random order (shuffle).\n\nNote that if you use the music player, in-game areas won't change your music until you click <b>Stop</b>.", widget->w / 2, 62, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);

		for (i = 0; i < BUTTON_NUM; i++)
		{
			buttons[i].surface = widget->surface;
			button_set_parent(&buttons[i], widget->x, widget->y);
		}

		buttons[BUTTON_PLAY].x = 10;
		buttons[BUTTON_PLAY].y = widget->h - TEXTURE_CLIENT("button")->h - 4;
		button_show(&buttons[BUTTON_PLAY], sound_map_background(-1) ? "Stop" : "Play");

		buttons[BUTTON_SHUFFLE].x = 10 + TEXTURE_CLIENT("button")->w + 5;
		buttons[BUTTON_SHUFFLE].y = widget->h - TEXTURE_CLIENT("button")->h - 4;
		buttons[BUTTON_SHUFFLE].pressed_forced = shuffle;
		button_show(&buttons[BUTTON_SHUFFLE], "Shuffle");

		buttons[BUTTON_BLACKLIST].x = 10 + TEXTURE_CLIENT("button")->w * 2 + 5 * 2;
		buttons[BUTTON_BLACKLIST].y = widget->h - TEXTURE_CLIENT("button_round")->h - 5;
		buttons[BUTTON_BLACKLIST].disabled = list_mplayer->row_selected == list_mplayer->rows;
		button_show(&buttons[BUTTON_BLACKLIST], mplayer_blacklisted(list_mplayer) ? "+" : "-");

		/* Show close button. */
		buttons[BUTTON_CLOSE].x = widget->w - TEXTURE_CLIENT("button_round")->w - 4;
		buttons[BUTTON_CLOSE].y = 4;
		button_show(&buttons[BUTTON_CLOSE], "X");

		/* Show help button. */
		buttons[BUTTON_HELP].x = widget->w - TEXTURE_CLIENT("button_round")->w * 2 - 4;
		buttons[BUTTON_HELP].y = 4;
		button_show(&buttons[BUTTON_HELP], "?");
	}
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
	uint32 duration, num_lines;

	/* If shuffle is enabled, check whether we need to start playing
	 * another song. */
	if (shuffle)
	{
		mplayer_check_shuffle();
	}

	duration = sound_music_get_duration();
	num_lines = duration + MAX(1, duration / 10);

	if (num_lines != scrollbar_progress_info.num_lines)
	{
		scrollbar_progress_info.num_lines = num_lines;
		scrollbar_progress.max_lines = MAX(1, duration / 10);
		widget->redraw = 1;
	}

	if (list_mplayer)
	{
		if (scrollbar_progress_info.redraw && sound_music_can_seek())
		{
			sound_music_seek(scrollbar_progress_info.scroll_offset + 1);
			scrollbar_progress_info.redraw = 0;
			widget->redraw = 1;
		}
		else
		{
			uint32 offset;

			offset = scrollbar_progress_info.scroll_offset;
			scrollbar_progress.redraw = NULL;
			scrollbar_scroll_to(&scrollbar_progress, sound_music_get_offset());
			scrollbar_progress.redraw = &scrollbar_progress_info.redraw;

			if (offset != scrollbar_progress_info.scroll_offset)
			{
				widget->redraw = 1;
			}
		}
	}

	/* Do mass blacklist status change if the button has been held for
	 * some time. */
	if (buttons[BUTTON_BLACKLIST].pressed && SDL_GetTicks() - buttons[BUTTON_BLACKLIST].pressed_ticks > BLACKLIST_ALL_DELAY)
	{
		mplayer_blacklist_mass_toggle(list_mplayer, mplayer_blacklisted(list_mplayer));
		mplayer_blacklist_save(list_mplayer);
		buttons[BUTTON_BLACKLIST].pressed_ticks = SDL_GetTicks();
	}

	if (!widget->redraw)
	{
		widget->redraw = list_need_redraw(list_mplayer);
	}

	if (!widget->redraw)
	{
		size_t i;

		for (i = 0; i < BUTTON_NUM; i++)
		{
			if (button_need_redraw(&buttons[i]))
			{
				widget->redraw = 1;
				break;
			}
		}
	}
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	size_t i;

	if (list_mplayer)
	{
		if (list_handle_mouse(list_mplayer, event))
		{
			widget->redraw = 1;
			return 1;
		}
		else if (scrollbar_event(&scrollbar_progress, event))
		{
			widget->redraw = 1;
			return 1;
		}
	}

	for (i = 0; i < BUTTON_NUM; i++)
	{
		if (button_event(&buttons[i], event))
		{
			switch (i)
			{
				case BUTTON_PLAY:
					if (sound_map_background(-1))
					{
						sound_start_bg_music("no_music", 0, 0);
						sound_map_background(0);
						shuffle = 0;
					}
					else
					{
						list_handle_enter(list_mplayer, event);
					}

					break;

				case BUTTON_SHUFFLE:
					shuffle = !shuffle;

					if (shuffle)
					{
						mplayer_do_shuffle(list_mplayer);
						sound_map_background(1);
					}
					else
					{
						sound_start_bg_music("no_music", 0, 0);
						sound_map_background(0);
					}

					break;

				case BUTTON_BLACKLIST:
					/* Toggle the blacklist state of the selected row. */
					mplayer_blacklist_toggle(list_mplayer);
					mplayer_blacklist_save(list_mplayer);
					break;

				case BUTTON_CLOSE:
					widget->show = 0;
					break;

				case BUTTON_HELP:
					help_show("music player");
					break;
			}

			widget->redraw = 1;
			return 1;
		}

		if (buttons[i].redraw)
		{
			widget->redraw = 1;
		}
	}

	return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
	free(shuffle_blacklist);
	shuffle_blacklist = NULL;
}

/**
 * Initialize one mplayer widget. */
void widget_mplayer_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
	widget->background_func = widget_background;
	widget->event_func = widget_event;
	widget->deinit_func = widget_deinit;
}

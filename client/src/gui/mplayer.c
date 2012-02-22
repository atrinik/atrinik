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
 * Handles music player widget code.
 *
 * @author Alex Tokar */

#include <global.h>

/** File where the blacklist data is stored. */
#define FILE_MPLAYER_BLACKLIST "mplayer.blacklist"
/**
 * How many milliseconds the blacklist button must be held in order to
 * mass-change blacklist status. */
#define BLACKLIST_ALL_DELAY 1500

/** Is shuffle enabled? */
static uint8 shuffle = 0;
/** Blacklisted music files. */
static uint8 *shuffle_blacklist = NULL;
/** Button buffer. */
static button_struct button_play, button_shuffle, button_blacklist, button_close, button_help;
/** The music player list. */
static list_struct *list_mplayer = NULL;

/**
 * Handle music list double-click and "Play" button.
 * @param list The music list. */
static void list_handle_enter(list_struct *list)
{
	sound_start_bg_music(list->text[list->row_selected - 1][0], setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), -1);
	sound_map_background(1);
	shuffle = 0;
}

/**
 * Handle list::text_color_hook in the music player list. */
static const char *list_text_color_hook(list_struct *list, const char *default_color, uint32 row, uint32 col)
{
	(void) list;
	(void) col;

	if (shuffle_blacklist[row])
	{
		return COLOR_RED;
	}

	return default_color;
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
	sound_start_bg_music(list->text[list->row_selected - 1][0], setting_get_int(OPT_CAT_SOUND, OPT_VOLUME_MUSIC), 0);
	cur_widget[MPLAYER_ID]->redraw = 1;
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

		list_add(list, list->rows, 0, currentfile->d_name);
	}

	closedir(dir);
}

/**
 * Render the music player widget.
 * @param widget The widget. */
void widget_show_mplayer(widgetdata *widget)
{
	SDL_Rect box, box2;
	const char *bg_music;
	char buf[HUGE_BUF];

	if (!widget->widgetSF)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("content");
		widget->widgetSF = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	/* The list doesn't exist yet, create it. */
	if (!list_mplayer)
	{
		char version[MAX_BUF];

		/* Create the list and set up settings. */
		list_mplayer = list_create(12, 1, 8);
		list_mplayer->handle_enter_func = list_handle_enter;
		list_mplayer->text_color_hook = list_text_color_hook;
		list_mplayer->surface = widget->widgetSF;
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

		button_create(&button_play);
		button_create(&button_shuffle);
		button_create(&button_blacklist);
		button_create(&button_help);
		button_create(&button_close);
		button_blacklist.texture = button_help.texture = button_close.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
		button_blacklist.texture_pressed = button_help.texture_pressed = button_close.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
		button_blacklist.texture_over = button_help.texture_over = button_close.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
	}

	if (widget->redraw)
	{
		surface_show(widget->widgetSF, 0, 0, NULL, TEXTURE_CLIENT("content"));

		box.h = 0;
		box.w = widget->wd;
		string_blt(widget->widgetSF, FONT_SERIF12, "Music Player", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);
		list_set_parent(list_mplayer, widget->x1, widget->y1);
		list_show(list_mplayer, 10, 2);
		box.w /= 2;
		string_blt(widget->widgetSF, FONT_SANS10, "Currently playing:", widget->wd / 2, 22, COLOR_WHITE, TEXT_ALIGN_CENTER, &box);
		box.h = 120;
		box.w -= 6;
		string_blt(widget->widgetSF, FONT_ARIAL10, "You can use the music player to play your favorite tunes from the game, or play them all one-by-one in random order (shuffle).\n\nNote that if you use the music player, in-game areas won't change your music until you click <b>Stop</b>.", widget->wd / 2, 60, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP, &box);

		widget->redraw = list_need_redraw(list_mplayer);
	}

	box2.x = widget->x1;
	box2.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box2);

	bg_music = sound_get_bg_music_basename();
	box.h = 0;
	box.w = widget->wd / 2;

	/* Store the background music file name in temporary buffer and
	 * make sure it won't overflow by truncating it if necessary. */
	if (bg_music)
	{
		strncpy(buf, bg_music, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		string_truncate_overflow(FONT_SANS11, buf, 150);
	}

	/* Show the music that is being played. */
	string_blt(ScreenSurface, FONT_SANS11, bg_music ? buf : "No music", widget->x1 + widget->wd / 2 - 5, widget->y1 + 34, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

	button_play.x = widget->x1 + 10;
	button_play.y = widget->y1 + widget->ht - TEXTURE_CLIENT("button")->h - 4;
	button_render(&button_play, sound_map_background(-1) ? "Stop" : "Play");

	button_shuffle.x = widget->x1 + 10 + TEXTURE_CLIENT("button")->w + 5;
	button_shuffle.y = widget->y1 + widget->ht - TEXTURE_CLIENT("button")->h - 4;
	button_shuffle.pressed_forced = shuffle;
	button_render(&button_shuffle, "Shuffle");

	button_blacklist.x = widget->x1 + 10 + TEXTURE_CLIENT("button")->w * 2 + 5 * 2;
	button_blacklist.y = widget->y1 + widget->ht - TEXTURE_CLIENT("button_round")->h - 5;
	button_blacklist.disabled = list_mplayer->row_selected == list_mplayer->rows;

	/* Do mass blacklist status change if the button has been held for
	 * some time. */
	if (button_blacklist.pressed && SDL_GetTicks() - button_blacklist.pressed_ticks > BLACKLIST_ALL_DELAY)
	{
		mplayer_blacklist_mass_toggle(list_mplayer, mplayer_blacklisted(list_mplayer));
		mplayer_blacklist_save(list_mplayer);
		button_blacklist.pressed_ticks = SDL_GetTicks();
	}

	button_render(&button_blacklist, mplayer_blacklisted(list_mplayer) ? "+" : "-");

	/* Show close button. */
	button_close.x = widget->x1 + widget->wd - TEXTURE_CLIENT("button_round")->w - 4;
	button_close.y = widget->y1 + 4;
	button_render(&button_close, "X");

	/* Show help button. */
	button_help.x = widget->x1 + widget->wd - TEXTURE_CLIENT("button_round")->w * 2 - 4;
	button_help.y = widget->y1 + 4;
	button_render(&button_help, "?");
}

/**
 * Process background tasks of the music player widget.
 * @param widget The music player widget. */
void widget_mplayer_background(widgetdata *widget)
{
	(void) widget;

	/* If shuffle is enabled, check whether we need to start playing
	 * another song. */
	if (shuffle)
	{
		mplayer_check_shuffle();
	}
}

/**
 * Deinitialize the music player widget.
 * @param widget The music player widget. */
void widget_mplayer_deinit(widgetdata *widget)
{
	(void) widget;

	free(shuffle_blacklist);
	shuffle_blacklist = NULL;
}

/**
 * Handle mouse events for the music player widget.
 * @param widget The widget.
 * @param event The event. */
void widget_mplayer_mevent(widgetdata *widget, SDL_Event *event)
{
	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_mplayer && list_handle_mouse(list_mplayer, event))
	{
		widget->redraw = 1;
	}

	if (button_event(&button_play, event))
	{
		if (sound_map_background(-1))
		{
			sound_start_bg_music("no_music", 0, 0);
			sound_map_background(0);
			shuffle = 0;
		}
		else
		{
			list_handle_enter(list_mplayer);
		}
	}
	else if (button_event(&button_shuffle, event))
	{
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
	}
	else if (button_event(&button_blacklist, event))
	{
		/* Toggle the blacklist state of the selected row. */
		mplayer_blacklist_toggle(list_mplayer);
		mplayer_blacklist_save(list_mplayer);
	}
	else if (button_event(&button_close, event))
	{
		widget->show = 0;
	}
	else if (button_event(&button_help, event))
	{
		help_show("music player");
	}
}

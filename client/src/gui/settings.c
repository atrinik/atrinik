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
 * Settings GUI. */

#include <global.h>

static const char *const opt_types[OPT_TYPE_NUM] =
{
	"bool", "input_num", "input_text", "range", "select", "int"
};

static setting_category **setting_categories;
static size_t setting_categories_num;
static uint8 setting_update_mapsize = 0;

#define KEYWORD_IS_TRUE(_keyword) (!strcmp((_keyword), "yes") || !strcmp((_keyword), "on") || !strcmp((_keyword), "true"))
#define KEYWORD_IS_FALSE(_keyword) (!strcmp((_keyword), "no") || !strcmp((_keyword), "off") || !strcmp((_keyword), "false"))

void *setting_get(setting_struct *setting)
{
	switch (setting->type)
	{
		case OPT_TYPE_BOOL:
		case OPT_TYPE_INPUT_NUM:
		case OPT_TYPE_RANGE:
		case OPT_TYPE_INT:
		case OPT_TYPE_SELECT:
			return &setting->val.i;

		case OPT_TYPE_INPUT_TEXT:
			return setting->val.str;
	}

	return NULL;
}

const char *setting_get_str(int cat, int setting)
{
	return (const char *) setting_get(setting_categories[cat]->settings[setting]);
}

sint64 setting_get_int(int cat, int setting)
{
	return *(sint64 *) setting_get(setting_categories[cat]->settings[setting]);
}

static int setting_apply(int cat, int setting)
{
	switch (cat)
	{
		case OPT_CAT_GENERAL:
			switch (setting)
			{
				case OPT_PLAYERDOLL:
					cur_widget[PDOLL_ID]->show = setting_get_int(cat, setting);
					return 1;
			}

			break;

		case OPT_CAT_DEVEL:
			switch (setting)
			{
				case OPT_SHOW_FPS:
					cur_widget[FPS_ID]->show = setting_get_int(cat, setting);
					return 1;
			}
	}

	return 0;
}

void setting_apply_change(int cat, int setting)
{
	if (setting_apply(cat, setting))
	{
		return;
	}

	switch (cat)
	{
		case OPT_CAT_GENERAL:
			switch (setting)
			{
				case OPT_EXP_DISPLAY:
					WIDGET_REDRAW_ALL(SKILL_EXP_ID);
					break;
			}

			break;

		case OPT_CAT_CLIENT:
			switch (setting)
			{
				case OPT_RESOLUTION:
				{
					int w, h;

					if (sscanf(SETTING_SELECT(setting_categories[cat]->settings[setting])->options[setting_get_int(cat, setting)], "%dx%d", &w, &h) == 2 && (ScreenSurface->w != w || ScreenSurface->h != h))
					{
						resize_window(w, h);
						video_set_size();
					}

					break;
				}

				case OPT_FULLSCREEN:
					if ((setting_get_int(cat, setting) && !(ScreenSurface->flags & SDL_FULLSCREEN)) || (!setting_get_int(cat, setting) && ScreenSurface->flags & SDL_FULLSCREEN))
					{
						video_fullscreen_toggle(&ScreenSurface, NULL);
					}

					break;
			}

			break;

		case OPT_CAT_MAP:
			switch (setting)
			{
				case OPT_MAP_WIDTH:
				case OPT_MAP_HEIGHT:
					if (setting_update_mapsize)
					{
						char buf[MAX_BUF];

						snprintf(buf, sizeof(buf), "setup mapsize %"FMT64"x%"FMT64, setting_get_int(cat, OPT_MAP_WIDTH), setting_get_int(cat, OPT_MAP_HEIGHT));
						cs_write_string(buf, strlen(buf));
						setting_update_mapsize = 0;
					}

					break;
			}

			break;

		case OPT_CAT_SOUND:
			switch (setting)
			{
				case OPT_VOLUME_MUSIC:
					sound_update_volume();
					break;
			}

			break;
	}
}

void setting_set_int(int cat, int setting, sint64 val)
{
	void *dst = setting_get(setting_categories[cat]->settings[setting]);

	(*(sint64 *) dst) = val;

	if (cat == OPT_CAT_MAP && (setting == OPT_MAP_WIDTH || setting == OPT_MAP_HEIGHT))
	{
		setting_update_mapsize = 1;
	}
}

int setting_is_text(setting_struct *setting)
{
	switch (setting->type)
	{
		case OPT_TYPE_INPUT_TEXT:
			return 1;
	}

	return 0;
}

void setting_load_value(setting_struct *setting, const char *str)
{
	switch (setting->type)
	{
		case OPT_TYPE_BOOL:
			if (KEYWORD_IS_TRUE(str))
			{
				setting->val.i = 1;
			}
			else if (KEYWORD_IS_FALSE(str))
			{
				setting->val.i = 0;
			}
			else
			{
				setting->val.i = atoi(str);
			}

			break;

		case OPT_TYPE_INPUT_NUM:
		case OPT_TYPE_RANGE:
		case OPT_TYPE_INT:
		case OPT_TYPE_SELECT:
			setting->val.i = atoi(str);
			break;

		case OPT_TYPE_INPUT_TEXT:
			if (setting->val.str)
			{
				free(setting->val.str);
			}

			setting->val.str = strdup(str);
			break;
	}
}

void settings_apply()
{
	size_t i, j;

	for (i = 0; i < setting_categories_num; i++)
	{
		for (j = 0; j < setting_categories[i]->settings_num; j++)
		{
			setting_apply(i, j);
		}
	}
}

void settings_init()
{
	FILE *fp;
	char buf[HUGE_BUF], *cp;
	setting_category *category;
	setting_struct *setting;

	fp = fopen_wrapper("data/settings.txt", "r");

	if (!fp)
	{
		LOG(llevError, "settings_init(): Missing data/settings.txt file, cannot continue.\n");
	}

	category = NULL;
	setting = NULL;
	setting_categories = NULL;
	setting_categories_num = 0;

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		cp = strchr(buf, '\n');

		if (cp)
		{
			*cp = '\0';
		}

		cp = buf;

		while (*cp != '\0')
		{
			if (isspace(*cp))
			{
				cp++;
			}
			else
			{
				break;
			}
		}

		if (*cp == '#' || *cp == '\0')
		{
			continue;
		}

		if (!strcmp(cp, "end"))
		{
			if (setting)
			{
				category->settings = realloc(category->settings, sizeof(category->settings) * (category->settings_num + 1));
				category->settings[category->settings_num] = setting;
				category->settings_num++;
				setting = NULL;
			}
			else if (category)
			{
				setting_categories = realloc(setting_categories, sizeof(*setting_categories) * (setting_categories_num + 1));
				setting_categories[setting_categories_num] = category;
				setting_categories_num++;
				category = NULL;
			}
		}
		else if (setting)
		{
			if (!strncmp(cp, "type ", 5))
			{
				size_t type_id;

				for (type_id = 0; type_id < OPT_TYPE_NUM; type_id++)
				{
					if (!strcmp(cp + 5, opt_types[type_id]))
					{
						setting->type = type_id;
						break;
					}
				}

				if (type_id == OPT_TYPE_NUM)
				{
					LOG(llevError, "Invalid type: %s\n", cp + 5);
				}
				else if (type_id == OPT_TYPE_SELECT)
				{
					setting->custom_attrset = calloc(1, sizeof(setting_select));
				}
				else if (type_id == OPT_TYPE_RANGE)
				{
					setting->custom_attrset = calloc(1, sizeof(setting_range));
				}
			}
			else if (!strncmp(cp, "default ", 8))
			{
				setting_load_value(setting, cp + 8);
			}
			else if (!strncmp(cp, "desc ", 5))
			{
				setting->desc = strdup(cp + 5);
				convert_newline(setting->desc);
			}
			else if (!strncmp(cp, "internal ", 9))
			{
				setting->internal = KEYWORD_IS_TRUE(cp + 9) ? 1 : 0;
			}
			else if (setting->type == OPT_TYPE_SELECT && !strncmp(cp, "option ", 7))
			{
				setting_select *s_select = SETTING_SELECT(setting);

				s_select->options = realloc(s_select->options, sizeof(*s_select->options) * (s_select->options_len + 1));
				s_select->options[s_select->options_len] = strdup(cp + 7);
				s_select->options_len++;
			}
			else if (setting->type == OPT_TYPE_RANGE && !strncmp(cp, "range ", 6))
			{
				setting_range *range = SETTING_RANGE(setting);
				sint64 min, max;

				if (sscanf(cp + 6, "%"FMT64" - %"FMT64, &min, &max) == 2)
				{
					range->min = min;
					range->max = max;
				}
				else
				{
					LOG(llevBug, "settings_init(): Invalid line: %s\n", cp);
				}
			}
			else if (setting->type == OPT_TYPE_RANGE && !strncmp(cp, "advance ", 8))
			{
				SETTING_RANGE(setting)->advance = atoi(cp + 8);
			}
			else
			{
				LOG(llevBug, "settings_init(): Invalid line: %s\n", cp);
			}
		}
		else if (category)
		{
			if (!strncmp(cp, "setting ", 8))
			{
				setting = calloc(1, sizeof(*setting));
				setting->name = strdup(cp + 8);
			}
			else
			{
				LOG(llevBug, "settings_init(): Invalid line: %s\n", cp);
			}
		}
		else if (!strncmp(cp, "category ", 9))
		{
			category = calloc(1, sizeof(*category));
			category->name = strdup(cp + 9);
		}
	}

	fclose(fp);

	settings_load();
}

#define FILE_SETTINGS_DAT "settings/settings.dat"

void settings_save()
{
	FILE *fp;
	size_t cat, set;
	setting_struct *setting;

	fp = fopen_wrapper(FILE_SETTINGS_DAT, "w");

	if (!fp)
	{
		LOG(llevBug, "Could not open settings file ("FILE_SETTINGS_DAT").\n");
		return;
	}

	for (cat = 0; cat < setting_categories_num; cat++)
	{
		fprintf(fp, "category %s\n", setting_categories[cat]->name);

		for (set = 0; set < setting_categories[cat]->settings_num; set++)
		{
			setting = setting_categories[cat]->settings[set];

			fprintf(fp, "%s\n", setting->name);

			if (setting_is_text(setting))
			{
				fprintf(fp, "%s\n", setting->val.str);
			}
			else
			{
				fprintf(fp, "%"FMT64"\n", setting->val.i);
			}
		}
	}

	fclose(fp);
}

sint64 category_from_name(const char *name)
{
	size_t cat;

	for (cat = 0; cat < setting_categories_num; cat++)
	{
		if (!strcmp(setting_categories[cat]->name, name))
		{
			return cat;
		}
	}

	return -1;
}

sint64 setting_from_name(const char *name)
{
	size_t cat, setting;

	for (cat = 0; cat < setting_categories_num; cat++)
	{
		for (setting = 0; setting < setting_categories[cat]->settings_num; setting++)
		{
			if (!strcmp(setting_categories[cat]->settings[setting]->name, name))
			{
				return setting;
			}
		}
	}

	return -1;
}

void settings_load()
{
	FILE *fp;
	char buf[HUGE_BUF], *cp;
	size_t cat, setting;
	uint8 is_setting_name = 1;

	fp = fopen_wrapper(FILE_SETTINGS_DAT, "r");

	if (!fp)
	{
		return;
	}

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		cp = strchr(buf, '\n');

		if (cp)
		{
			*cp = '\0';
		}

		cp = buf;

		while (*cp != '\0')
		{
			if (isspace(*cp))
			{
				cp++;
			}
			else
			{
				break;
			}
		}

		if (*cp == '#' || *cp == '\0')
		{
			continue;
		}

		if (!strncmp(cp, "category ", 9))
		{
			cat = category_from_name(cp + 9);
		}
		else
		{
			if (is_setting_name)
			{
				setting = setting_from_name(cp);
			}
			else
			{
				setting_load_value(setting_categories[cat]->settings[setting], cp);
			}

			is_setting_name = !is_setting_name;
		}
	}

	fclose(fp);
}

void settings_deinit()
{
	size_t cat, setting;

	settings_save();

	for (cat = 0; cat < setting_categories_num; cat++)
	{
		for (setting = 0; setting < setting_categories[cat]->settings_num; setting++)
		{
			if (setting_is_text(setting_categories[cat]->settings[setting]))
			{
				free(setting_categories[cat]->settings[setting]->val.str);
			}

			free(setting_categories[cat]->settings[setting]->name);

			if (setting_categories[cat]->settings[setting]->desc)
			{
				free(setting_categories[cat]->settings[setting]->desc);
			}

			if (setting_categories[cat]->settings[setting]->type == OPT_TYPE_SELECT)
			{
				setting_select *s_select = SETTING_SELECT(setting_categories[cat]->settings[setting]);
				size_t option;

				for (option = 0; option < s_select->options_len; option++)
				{
					free(s_select->options[option]);
				}

				if (s_select->options)
				{
					free(s_select->options);
				}
			}

			if (setting_categories[cat]->settings[setting]->custom_attrset)
			{
				free(setting_categories[cat]->settings[setting]->custom_attrset);
			}
		}

		if (setting_categories[cat]->settings)
		{
			free(setting_categories[cat]->settings);
		}

		free(setting_categories[cat]->name);
		free(setting_categories[cat]);
	}

	if (setting_categories)
	{
		free(setting_categories);
		setting_categories = NULL;
	}

	setting_categories_num = 0;
}

static void settings_apply_change()
{
	size_t cat, setting;

	for (cat = 0; cat < setting_categories_num; cat++)
	{
		for (setting = 0; setting < setting_categories[cat]->settings_num; setting++)
		{
			setting_apply_change(cat, setting);
		}
	}
}

/** The different buttons of the settings popup. */
enum
{
	BUTTON_SETTINGS,
	BUTTON_KEY_SETTINGS,
	BUTTON_LOGOUT,
	BUTTON_BACK,

	BUTTON_NUM
};

/** Names of the buttons. */
static const char *const button_names[BUTTON_NUM] =
{
	"Client Settings", "Key settings", "Logout", "Back to play"
};

/** Currently selected button. */
static size_t button_selected;
static size_t setting_category_selected;
static uint32 list_row_clicked;
static uint32 list_clicked = 0;

static void settings_list_reload(list_struct *list)
{
	size_t i;
	setting_struct *setting;

	list_clear(list);

	for (i = 0; i < setting_categories[setting_category_selected]->settings_num; i++)
	{
		setting = setting_categories[setting_category_selected]->settings[i];

		if (setting->internal)
		{
			continue;
		}

		list_add(list, list->rows, 0, "");
	}
}

static void list_color_func(list_struct *list, int row, SDL_Rect box)
{
	if (row & 1)
	{
		SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 67, 67, 67));
	}
	else
	{
		SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 83, 83, 83));
	}
}

static void list_handle_mouse_row(list_struct *list, uint32 row, SDL_Event *event)
{
	(void) list;

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		list_row_clicked = row;
		list_clicked = 1;
	}
	else if (event->type == SDL_MOUSEMOTION)
	{
		list->row_selected = row + 1;
	}
}

static void list_handle_esc(list_struct *list)
{
	(void) list;
	popup_destroy_visible();
}

static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
	setting_struct *setting;
	int x, y, mx, my, mstate;

	(void) col;

	setting = setting_categories[setting_category_selected]->settings[row];

	if (setting->internal)
	{
		return;
	}

	x = list->x + list->frame_offset;
	y = LIST_ROWS_START(list) + (row * LIST_ROW_HEIGHT(list)) + list->frame_offset;

	string_blt_shadow_format(list->surface, FONT_ARIAL11, x + 4, y + 5, list->row_selected == row + 1 ? COLOR_HGOLD : COLOR_WHITE, COLOR_BLACK, 0, NULL, "%s:", setting->name);

	mstate = SDL_GetMouseState(&mx, &my);

	if (setting->type == OPT_TYPE_BOOL)
	{
		SDL_Rect checkbox;

		checkbox.x = x + list->width - 17;
		checkbox.y = y + 1;
		checkbox.w = 14;
		checkbox.h = 14;
		SDL_FillRect(list->surface, &checkbox, SDL_MapRGB(list->surface->format, 0, 0, 0));

		if (setting_get_int(setting_category_selected, row))
		{
			lineRGBA(list->surface, checkbox.x, checkbox.y, checkbox.x + checkbox.w, checkbox.y + checkbox.h, 212, 213, 83, 255);
			lineRGBA(list->surface, checkbox.x + checkbox.w, checkbox.y, checkbox.x, checkbox.y + checkbox.h, 212, 213, 83, 255);
		}

		draw_frame(list->surface, checkbox.x, checkbox.y, checkbox.w, checkbox.h);

		if (mx >= checkbox.x && mx <= checkbox.x + checkbox.w && my >= checkbox.y && my <= checkbox.y + checkbox.h)
		{
			border_create_color(list->surface, &checkbox, "b09a9a");

			if (list_clicked && list_row_clicked == row && mstate == SDL_BUTTON_LEFT)
			{
				setting_set_int(setting_category_selected, row, !setting_get_int(setting_category_selected, row));
				list_clicked = 0;
			}
		}
		else
		{
			border_create_color(list->surface, &checkbox, "8c7a7a");
		}
	}
	else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
	{
		SDL_Rect dst;
		sint64 val, new_val, advance;

		val = new_val = setting_get_int(setting_category_selected, row);
		advance = 1;

		if (setting->type == OPT_TYPE_RANGE)
		{
			advance = SETTING_RANGE(setting)->advance;
		}

		x += list->width - 1;
		y += 1;

		x -= Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;

		if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, x, y, ">", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			new_val = val + advance;
		}

		dst.x = x - 150;
		dst.y = y;
		dst.w = 150;
		dst.h = LIST_ROW_HEIGHT(list) - 2;

		SDL_FillRect(list->surface, &dst, SDL_MapRGB(list->surface->format, 0, 0, 0));

		if (setting->type == OPT_TYPE_SELECT)
		{
			string_blt(list->surface, FONT_ARIAL10, SETTING_SELECT(setting)->options[val], dst.x, dst.y, COLOR_WHITE, TEXT_ALIGN_CENTER, &dst);
		}
		else if (setting->type == OPT_TYPE_RANGE)
		{
			string_blt_format(list->surface, FONT_ARIAL10, dst.x, dst.y, COLOR_WHITE, TEXT_ALIGN_CENTER, &dst, "%"FMT64, val);
		}

		dst.x -= Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w + 1;

		if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, dst.x, y, "<", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			new_val = val - advance;
		}

		if (setting->type == OPT_TYPE_SELECT)
		{
			setting_select *s_select = SETTING_SELECT(setting);

			if (new_val >= (sint64) s_select->options_len)
			{
				new_val = 0;
			}
			else if (new_val < 0)
			{
				new_val = s_select->options_len - 1;
			}
		}
		else if (setting->type == OPT_TYPE_RANGE)
		{
			setting_range *range = SETTING_RANGE(setting);

			new_val = MAX(range->min, MIN(range->max, new_val));
		}

		if (val != new_val)
		{
			setting_set_int(setting_category_selected, row, new_val);
		}
	}
}

static void settings_popup_draw_func_post(popup_struct *popup)
{
	list_struct *list;
	int x, y;

	list = list_exists(LIST_SETTINGS);

	if (list)
	{
		size_t new_cat = setting_category_selected;
		setting_struct *setting;
		SDL_Rect dst;

		x = popup->x + 30;
		y = popup->y + 50;
		setting = setting_categories[setting_category_selected]->settings[list->row_selected - 1];

		if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, x, y, "<", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			if (new_cat == 0)
			{
				new_cat = setting_categories_num - 1;
			}
			else
			{
				new_cat--;
			}
		}

		dst.w = list->width + 8 - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;
		dst.h = 0;

		if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, x + dst.w, y, ">", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			if (new_cat == setting_categories_num - 1)
			{
				new_cat = 0;
			}
			else
			{
				new_cat++;
			}
		}

		dst.w -= Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;
		string_blt(list->surface, FONT_SERIF14, setting_categories[setting_category_selected]->name, x + Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w, y - 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &dst);

		dst.h = 66;
		string_blt_shadow(list->surface, FONT_ARIAL11, setting->desc ? setting->desc : "", x - 2, popup->y + popup->surface->h - 75, COLOR_WHITE, COLOR_BLACK, TEXT_WORD_WRAP | TEXT_MARKUP, &dst);

		if (new_cat != setting_category_selected)
		{
			setting_category_selected = new_cat;
			settings_list_reload(list);
		}

		list_show(list, x, y);

		if (button_show(BITMAP_BUTTON, -1, BITMAP_BUTTON_DOWN, popup->x + popup->surface->w - 10 - Bitmaps[BITMAP_BUTTON]->bitmap->w, popup->y + popup->surface->h - 55, "Apply", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			settings_apply_change();
		}

		if (button_show(BITMAP_BUTTON, -1, BITMAP_BUTTON_DOWN, popup->x + popup->surface->w - 10 - Bitmaps[BITMAP_BUTTON]->bitmap->w, popup->y + popup->surface->h - 30, "Done", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
		{
			popup_destroy_visible();
		}
	}
}

/**
 * Draw the settings popup.
 * @param popup The popup. */
static void settings_popup_draw_func(popup_struct *popup)
{
	SDL_Rect box;
	size_t i;
	int x, y, mx, my;
	char buf[MAX_BUF];
	uint64 flags;

	box.x = 0;
	box.y = 10;
	box.w = popup->surface->w;
	box.h = 0;
	string_blt(popup->surface, FONT_SERIF20, "<u>Settings</u>", box.x, box.y, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box);
	box.y += 50;

	if (!list_exists(LIST_SETTINGS))
	{
		SDL_GetMouseState(&mx, &my);

		for (i = 0; i < BUTTON_NUM; i++)
		{
			if (GameStatus != GAME_STATUS_PLAY && (i == BUTTON_BACK || i == BUTTON_LOGOUT))
			{
				continue;
			}

			flags = TEXT_ALIGN_CENTER;
			x = ScreenSurface->w / 2 - popup->surface->w / 2 + box.x;
			y = ScreenSurface->h / 2 - popup->surface->h / 2 + box.y;

			if (mx >= x && mx < x + popup->surface->w && my >= y && my < y + FONT_HEIGHT(FONT_SERIF40))
			{
				snprintf(buf, sizeof(buf), "<u>%s</u>", button_names[i]);
				flags |= TEXT_MARKUP;
			}
			else
			{
				strncpy(buf, button_names[i], sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
			}

			if (button_selected == i)
			{
				string_blt_shadow_format(popup->surface, FONT_SERIF40, box.x, box.y, COLOR_HGOLD, COLOR_BLACK, flags | TEXT_MARKUP, &box, "<c=#9f0408>&gt;</c> %s <c=#9f0408>&lt;</c>", buf);
			}
			else
			{

				string_blt_shadow(popup->surface, FONT_SERIF40, buf, box.x, box.y, COLOR_WHITE, COLOR_BLACK, flags, &box);
			}

			box.y += FONT_HEIGHT(FONT_SERIF40);
		}
	}
}

static int settings_popup_destroy_callback(popup_struct *popup)
{
	(void) popup;
	settings_apply_change();
	list_remove(list_exists(LIST_SETTINGS));
	return 1;
}

/**
 * Handle pressing a button in the settings popup.
 * @param button The button ID. */
static void settings_button_handle(size_t button)
{
	if (button == BUTTON_KEY_SETTINGS || button == BUTTON_SETTINGS)
	{
		list_struct *list;

		setting_category_selected = 0;

		list = list_create(LIST_SETTINGS, 9, 1, 8);
		list->row_color_func = list_color_func;
		list->post_column_func = list_post_column;
		list->row_highlight_func = NULL;
		list->row_selected_func = NULL;
		list->handle_mouse_row_func = list_handle_mouse_row;
		list->handle_esc_func = list_handle_esc;
		list_scrollbar_enable(list);
		list_set_column(list, 0, 430, 7, NULL, -1);
		list_set_focus(list);
		list_set_font(list, FONT_SANS14);
		settings_list_reload(list);
		return;
	}
	else if (button == BUTTON_LOGOUT)
	{
		socket_close(&csocket);
		GameStatus = GAME_STATUS_INIT;
	}

	popup_destroy_visible();
}

/**
 * Handle events for the settings popup. */
static int settings_popup_event_func(popup_struct *popup, SDL_Event *event)
{
	list_struct *list = list_exists(LIST_SETTINGS);

	if (list)
	{
		if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
		{
			if (list_handle_keyboard(list, &event->key))
			{
				return 1;
			}
		}
		else
		{
			if (list_handle_mouse(list, event->motion.x, event->motion.y, event))
			{
				return 1;
			}
		}

		return -1;
	}

	if (event->type == SDL_KEYDOWN)
	{
		/* Move the selected button up and down. */
		if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_DOWN)
		{
			int selected = button_selected, num_buttons;

			selected += event->key.keysym.sym == SDLK_DOWN ? 1 : -1;
			num_buttons = (GameStatus == GAME_STATUS_PLAY ? BUTTON_NUM : BUTTON_LOGOUT) - 1;

			if (selected < 0)
			{
				selected = num_buttons;
			}
			else if (selected > num_buttons)
			{
				selected = 0;
			}

			button_selected = selected;
			return 1;
		}
		else if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER)
		{
			settings_button_handle(button_selected);
			return 1;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			int x, y;
			size_t i;

			x = ScreenSurface->w / 2 - popup->surface->w / 2;
			y = ScreenSurface->h / 2 - popup->surface->h / 2 + 60;

			for (i = 0; i < BUTTON_NUM; i++)
			{
				if (event->motion.x >= x && event->motion.x < x + popup->surface->w && event->motion.y >= y && event->motion.y < y + FONT_HEIGHT(FONT_SERIF40))
				{
					settings_button_handle(i);
					break;
				}

				y += FONT_HEIGHT(FONT_SERIF40);
			}
		}
	}

	return -1;
}

/**
 * Open the settings popup. */
void settings_open()
{
	popup_struct *popup;

	popup = popup_create(BITMAP_POPUP);
	popup->draw_func = settings_popup_draw_func;
	popup->event_func = settings_popup_event_func;
	popup->destroy_callback_func = settings_popup_destroy_callback;
	popup->draw_func_post = settings_popup_draw_func_post;

	button_selected = GameStatus == GAME_STATUS_PLAY ? BUTTON_BACK : BUTTON_SETTINGS;
}

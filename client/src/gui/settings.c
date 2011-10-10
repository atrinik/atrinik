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

/** Password button. */
static button_struct button_password;

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
	"Client Settings", "Key Settings", "Logout", "Back to Play"
};

/** Help filenames for setting actions. */
static const char *const setting_type_help[SETTING_TYPE_NUM] =
{
	"esc menu", "client settings", "keybinding settings", "password change"
};

/** Currently selected setting type. */
static uint8 setting_type = SETTING_TYPE_NONE;
/** Currently selected button. */
static size_t button_selected;
/** Selected setting category. */
static size_t setting_category_selected;
/** Which row was clicked. */
static uint32 list_row_clicked;
/** 1 if there is a clicked to handle. */
static uint32 list_clicked = 0;
/** Step in the keybinding GUI. */
static uint8 setting_keybind_step;
/** Key of the shortcut being added/edited. */
static SDLKey setting_keybind_key;
/** Modifier for the shortcut. */
static SDLMod setting_keybind_mod;
/** If not -1, we're editing this keybinding ID. */
static sint32 setting_keybind_id;
/** Whether to ignore keybind keypress. */
static uint8 setting_keybind_ignore = 0;
/**
 * Whether player has entered their current password in the password
 * changing GUI. */
static uint8 setting_password_confirmed;
/**
 * Pointer to the visible settings popup, if any. */
static popup_struct *settings_popup = NULL;
/**
 * The settings list. */
static list_struct *list_settings = NULL;

/**
 * Reload the settings list.
 * @param list The list. */
static void settings_list_reload(list_struct *list)
{
	size_t i;

	/* Clear all the rows. */
	list_clear_rows(list);

	/* Settings? */
	if (setting_type == SETTING_TYPE_SETTINGS)
	{
		setting_struct *setting;

		for (i = 0; i < setting_categories[setting_category_selected]->settings_num; i++)
		{
			setting = setting_categories[setting_category_selected]->settings[i];

			/* Internal, no need to go any further. */
			if (setting->internal)
			{
				break;
			}

			list_add(list, list->rows, 0, "");
		}
	}
	/* Keybindings? */
	else if (setting_type == SETTING_TYPE_KEYBINDINGS)
	{
		char buf[MAX_BUF];

		for (i = 0; i < keybindings_num; i++)
		{
			list_add(list, i, 0, keybindings[i]->command);
			list_add(list, i, 1, keybind_get_key_shortcut(keybindings[i]->key, keybindings[i]->mod, buf, sizeof(buf)));
			list_add(list, i, 2, keybindings[i]->repeat ? "on" : "off");
		}
	}

	list_offsets_ensure(list);
}

/**
 * Handle mouse event in a list.
 * @param list The list.
 * @param row Row the mouse is over.
 * @param event The mouse event. */
static void list_handle_mouse_row(list_struct *list, uint32 row, SDL_Event *event)
{
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

/**
 * Handle ESC in a list.
 *
 * In this case, just destroys the visible popup.
 * @param list The list. */
static void list_handle_esc(list_struct *list)
{
	(void) list;
	popup_destroy_all();
}

/**
 * Change setting value.
 * @param cat Category.
 * @param set Setting.
 * @param val Modifier. */
static void setting_change_value(int cat, int set, sint64 val)
{
	setting_struct *setting = setting_categories[cat]->settings[set];

	if (setting->type == OPT_TYPE_BOOL)
	{
		setting_set_int(cat, set, !setting_get_int(cat, set));
	}
	else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
	{
		sint64 old_val, new_val, advance;

		if (!val)
		{
			return;
		}

		new_val = old_val = setting_get_int(cat, set);
		advance = 1;

		if (setting->type == OPT_TYPE_RANGE)
		{
			advance = SETTING_RANGE(setting)->advance;
		}

		new_val = old_val + (advance * val);

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

		if (old_val != new_val)
		{
			setting_set_int(cat, set, new_val);
		}
	}
}

/**
 * Do custom drawing after the list has finished drawing a column.
 * @param list The list.
 * @param row The row.
 * @param col Column being drawn. */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
	int x, y;

	(void) col;

	x = list->x + list->frame_offset;
	y = LIST_ROWS_START(list) + (row * LIST_ROW_HEIGHT(list)) + list->frame_offset;

	/* Settings list. */
	if (setting_type == SETTING_TYPE_SETTINGS)
	{
		setting_struct *setting;
		int mx, my, mstate;

		setting = setting_categories[setting_category_selected]->settings[row];

		if (setting->internal)
		{
			return;
		}

		/* Show the actual setting name. */
		string_blt_shadow_format(list->surface, FONT_ARIAL11, x + 4, y + 5, list->row_selected == row + 1 ? COLOR_HGOLD : COLOR_WHITE, COLOR_BLACK, 0, NULL, "%s:", setting->name);

		mstate = SDL_GetMouseState(&mx, &my);

		/* Checkbox? */
		if (setting->type == OPT_TYPE_BOOL)
		{
			SDL_Rect checkbox;

			checkbox.x = x + list->width - 17;
			checkbox.y = y + 1;
			checkbox.w = 15;
			checkbox.h = 15;
			SDL_FillRect(list->surface, &checkbox, SDL_MapRGB(list->surface->format, 0, 0, 0));

			if (setting_get_int(setting_category_selected, row))
			{
				lineRGBA(list->surface, checkbox.x, checkbox.y, checkbox.x + checkbox.w, checkbox.y + checkbox.h, 212, 213, 83, 255);
				lineRGBA(list->surface, checkbox.x + checkbox.w, checkbox.y, checkbox.x, checkbox.y + checkbox.h, 212, 213, 83, 255);
			}

			draw_frame(list->surface, checkbox.x, checkbox.y, checkbox.w, checkbox.h);

			if (popup_get_head() == settings_popup && mx >= checkbox.x && mx < checkbox.x + checkbox.w && my >= checkbox.y && my < checkbox.y + checkbox.h)
			{
				border_create_color(list->surface, &checkbox, "b09a9a");

				if (list_clicked && list_row_clicked == row && mstate == SDL_BUTTON_LEFT)
				{
					setting_change_value(setting_category_selected, row, 0);
					list_clicked = 0;
				}
			}
			else
			{
				border_create_color(list->surface, &checkbox, "8c7a7a");
			}
		}
		/* Select or range. */
		else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
		{
			SDL_Rect dst;
			sint64 val;

			val = setting_get_int(setting_category_selected, row);

			x += list->width - 1;
			y += 1;

			x -= Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;

			if (button_show(BITMAP_BUTTON_ROUND, BITMAP_BUTTON_ROUND_HOVER, BITMAP_BUTTON_ROUND_DOWN, x, y, ">", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == settings_popup))
			{
				setting_change_value(setting_category_selected, row, 1);
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

			if (button_show(BITMAP_BUTTON_ROUND, BITMAP_BUTTON_ROUND_HOVER, BITMAP_BUTTON_ROUND_DOWN, dst.x, y, "<", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == settings_popup))
			{
				setting_change_value(setting_category_selected, row, -1);
			}
		}
	}
}

/**
 * Apply keybinding changes - adds or edits a keybinding.
 * @param list The keybinding list. */
static void setting_keybind_apply(list_struct *list)
{
	/* Nothing to apply. */
	if (text_input_string[0] == '\0' || setting_keybind_key == SDLK_UNKNOWN)
	{
		return;
	}

	/* Add new. */
	if (setting_keybind_id == -1)
	{
		keybind_add(setting_keybind_key, setting_keybind_mod, text_input_string);
		/* It'll be added to the end, so select it. */
		list->row_selected = list->rows + 1;
		list->row_offset = MIN(list->rows + 1 - list->max_rows, list->row_selected - 1);
	}
	/* Edit existing. */
	else
	{
		keybind_edit(setting_keybind_id, setting_keybind_key, setting_keybind_mod, text_input_string);
	}

	text_input_string_flag = 0;
	settings_list_reload(list);
}

/**
 * Do an action related to the keybind settings list.
 * @param key Key used.
 * @param list The keybinding list.
 * @return 1 if the action was handled, 0 otherwise. */
static int setting_keybind_action(SDLKey key, list_struct *list)
{
	/* Create a new keybinding. */
	if (key == SDLK_n)
	{
		text_input_open(255);
		setting_keybind_step = KEYBIND_STEP_COMMAND;
		setting_keybind_key = SDLK_UNKNOWN;
		setting_keybind_mod = KMOD_NONE;
		setting_keybind_id = -1;
		setting_keybind_ignore = 0;
		return 1;
	}
	/* Delete existing keybinding. */
	else if (key == SDLK_DELETE)
	{
		keybind_remove(list->row_selected - 1);
		settings_list_reload(list);
		return 1;
	}
	/* Toggle repeat on/off. */
	else if (key == SDLK_r)
	{
		keybind_repeat_toggle(list->row_selected - 1);
		settings_list_reload(list);
		return 1;
	}

	return 0;
}

/**
 * Change the currently selected setting category.
 * @param advance If -1, change to the previous category; if 1, change to
 * the next one. */
static void setting_category_change(int advance)
{
	size_t new_cat = setting_category_selected;

	if (advance == 1)
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
	else if (advance == -1)
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

	if (new_cat != setting_category_selected)
	{
		setting_category_selected = new_cat;
		settings_list_reload(list_settings);
	}
}

/**
 * Do drawing immediately after finishing drawing the settings popup.
 * @param popup The settings popup. */
static int settings_popup_draw_func_post(popup_struct *popup)
{
	int x, y, mx, my, mstate;

	mstate = SDL_GetMouseState(&mx, &my);

	if (setting_type == SETTING_TYPE_NONE)
	{
		if (GameStatus == GAME_STATUS_PLAY)
		{
			button_password.x = popup->x + 10;
			button_password.y = popup->y + 42;
			button_render(&button_password, "Password");
		}
	}
	else if (setting_type == SETTING_TYPE_SETTINGS)
	{
		setting_struct *setting;
		SDL_Rect dst;

		x = popup->x + 30;
		y = popup->y + 50;

		if (button_show(BITMAP_BUTTON_ROUND, BITMAP_BUTTON_ROUND_HOVER, BITMAP_BUTTON_ROUND_DOWN, x, y, "<", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			setting_category_change(-1);
		}

		dst.w = list_settings->width + 8 - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;
		dst.h = 0;

		if (button_show(BITMAP_BUTTON_ROUND, BITMAP_BUTTON_ROUND_HOVER, BITMAP_BUTTON_ROUND_DOWN, x + dst.w, y, ">", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			setting_category_change(1);
		}

		setting = setting_categories[setting_category_selected]->settings[list_settings->row_selected - 1];

		dst.w -= Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w;
		string_blt(list_settings->surface, FONT_SERIF14, setting_categories[setting_category_selected]->name, x + Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w, y - 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &dst);

		dst.h = 66;
		string_blt_shadow(list_settings->surface, FONT_ARIAL11, setting->desc ? setting->desc : "", x - 2, popup->y + popup->surface->h - 75, COLOR_WHITE, COLOR_BLACK, TEXT_WORD_WRAP | TEXT_MARKUP, &dst);

		list_show(list_settings, x, y);

		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, popup->x + popup->surface->w - 10 - Bitmaps[BITMAP_BUTTON]->bitmap->w, popup->y + popup->surface->h - 55, "Apply", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			settings_apply_change();
		}

		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, popup->x + popup->surface->w - 10 - Bitmaps[BITMAP_BUTTON]->bitmap->w, popup->y + popup->surface->h - 30, "Done", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			return 0;
		}
	}
	else if (setting_type == SETTING_TYPE_KEYBINDINGS)
	{
		SDL_Rect dst;
		char key_buf[MAX_BUF];

		x = popup->x + 30;
		y = popup->y + 50;
		list_show(list_settings, x, y);

		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, popup->x + 30, popup->y + popup->surface->h - 74, "Add", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			setting_keybind_action(SDLK_n, list_settings);
		}

		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, popup->x + 30, popup->y + popup->surface->h - 51, "Remove", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			setting_keybind_action(SDLK_DELETE, list_settings);
		}

		if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, popup->x + 30, popup->y + popup->surface->h - 28, "Repeat", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
		{
			setting_keybind_action(SDLK_r, list_settings);
		}

		if (text_input_string_flag)
		{
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Command: ", popup->x + 100, popup->y + popup->surface->h - 70, COLOR_WHITE, COLOR_BLACK, 0, NULL);
			string_blt_shadow(ScreenSurface, FONT_ARIAL11, "Key: ", popup->x + 100, popup->y + popup->surface->h - 47, COLOR_WHITE, COLOR_BLACK, 0, NULL);
			string_blt_shadow(ScreenSurface, FONT_ARIAL10, "Press ESC to cancel.", popup->x + 160, popup->y + popup->surface->h - 34, COLOR_WHITE, COLOR_BLACK, 0, NULL);

			dst.x = popup->x + 160;
			dst.y = popup->y + popup->surface->h - 74;
			dst.w = Bitmaps[BITMAP_LOGIN_INP]->bitmap->w;
			dst.h = Bitmaps[BITMAP_LOGIN_INP]->bitmap->h;

			if (mstate == SDL_BUTTON_LEFT && mx >= dst.x && mx < dst.x + dst.w && my >= dst.y && my < dst.y + dst.h)
			{
				setting_keybind_step = KEYBIND_STEP_COMMAND;
			}

			if (setting_keybind_step == KEYBIND_STEP_COMMAND)
			{
				text_input_show(ScreenSurface, dst.x, dst.y, FONT_ARIAL11, text_input_string, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
			}
			else
			{
				text_input_draw_background(ScreenSurface, dst.x, dst.y, BITMAP_LOGIN_INP);
				text_input_draw_text(ScreenSurface, dst.x, dst.y, FONT_ARIAL11, text_input_string, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);
			}

			dst.y += 20;

			if (mstate == SDL_BUTTON_LEFT && mx >= dst.x && mx < dst.x + dst.w && my >= dst.y && my < dst.y + dst.h)
			{
				setting_keybind_step = KEYBIND_STEP_KEY;
			}

			if (setting_keybind_step == KEYBIND_STEP_COMMAND || setting_keybind_step == KEYBIND_STEP_DONE)
			{
				keybind_get_key_shortcut(setting_keybind_key, setting_keybind_mod, key_buf, sizeof(key_buf));
			}
			else if (setting_keybind_step == KEYBIND_STEP_KEY)
			{
				strcpy(key_buf, "Press keyboard shortcut");
			}

			text_input_draw_background(ScreenSurface, dst.x, dst.y, BITMAP_LOGIN_INP);
			text_input_draw_text(ScreenSurface, dst.x, dst.y, FONT_ARIAL11, key_buf, COLOR_WHITE, TEXT_ALIGN_CENTER, BITMAP_LOGIN_INP, NULL);

			if (button_show(BITMAP_BUTTON, BITMAP_BUTTON_HOVER, BITMAP_BUTTON_DOWN, dst.x + dst.w - Bitmaps[BITMAP_BUTTON]->bitmap->w, dst.y + 20, "Apply", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0, popup_get_head() == popup))
			{
				setting_keybind_apply(list_settings);
			}
		}
	}

	return 1;
}

/**
 * Draw the settings popup.
 * @param popup The popup. */
static int settings_popup_draw_func(popup_struct *popup)
{
	SDL_Rect box;

	box.x = 40;
	box.y = 6;
	box.w = 420;
	box.h = 26;

	string_blt(popup->surface, FONT_SERIF20, "Settings", box.x, box.y, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	box.x = 0;
	box.y = 10;
	box.w = popup->surface->w;
	box.h = 0;

	if (setting_type == SETTING_TYPE_NONE)
	{
		size_t i;

		box.y += 50;

		for (i = 0; i < BUTTON_NUM; i++)
		{
			if (GameStatus != GAME_STATUS_PLAY && (i == BUTTON_BACK || i == BUTTON_LOGOUT))
			{
				continue;
			}

			if (button_selected == i)
			{
				string_blt_shadow_format(popup->surface, FONT_SERIF40, box.x, box.y, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box, "<c=#9f0408>&gt;</c> %s <c=#9f0408>&lt;</c>", button_names[i]);
			}
			else
			{

				string_blt_shadow(popup->surface, FONT_SERIF40, button_names[i], box.x, box.y, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			}

			box.y += FONT_HEIGHT(FONT_SERIF40);
		}
	}
	else if (setting_type == SETTING_TYPE_PASSWORD)
	{
		char buf[MAX_BUF];
		int i;

		for (i = 0; i < text_input_count; i++)
		{
			buf[i] = '*';
		}

		buf[i] = '\0';

		box.x = popup->surface->w / 2 - Bitmaps[BITMAP_LOGIN_INP]->bitmap->w / 2;
		box.y = popup->surface->h / 2 - Bitmaps[BITMAP_LOGIN_INP]->bitmap->h / 2 - 50;

		text_input_show(popup->surface, box.x, box.y, FONT_ARIAL10, buf, COLOR_WHITE, 0, BITMAP_LOGIN_INP, NULL);

		if (!setting_password_confirmed)
		{
			string_blt_shadow(popup->surface, FONT_ARIAL11, "Enter your current password:", 0, box.y - 14, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "Allows you to change your character's", 0, box.y + 24, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "password. Press Esc to cancel.", 0, box.y + 36, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
		}
		else
		{
			string_blt_shadow(popup->surface, FONT_ARIAL11, "Enter the new password:", 0, box.y - 14, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "Make sure to use a strong password.", 0, box.y + 24, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "A good password usually consists of a ", 0, box.y + 36, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "mix of uppercase and lowercase letters,", 0, box.y + 48, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "numbers, symbols, and does not include", 0, box.y + 60, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
			string_blt_shadow(popup->surface, FONT_ARIAL11, "words found in common dictionaries.", 0, box.y + 72, COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
		}
	}

	return 1;
}

/**
 * Exit the settings popup.
 * @param popup The popup.
 * @return 1. */
static int settings_popup_destroy_callback(popup_struct *popup)
{
	(void) popup;
	settings_apply_change();
	list_remove(list_settings);
	list_settings = NULL;

	if (text_input_string_flag)
	{
		SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
		text_input_string_flag = 0;
	}

	keybind_save();
	settings_popup = NULL;

	return 1;
}

/**
 * Handle using enter in the settings list.
 * @param list The list. */
static void list_handle_enter(list_struct *list)
{
	if (list->text && list->row_selected)
	{
		if (setting_type == SETTING_TYPE_KEYBINDINGS)
		{
			setting_keybind_action(SDLK_n, list);

			setting_keybind_id = list->row_selected - 1;
			text_input_set_string(keybindings[setting_keybind_id]->command);
			setting_keybind_key = keybindings[setting_keybind_id]->key;
			setting_keybind_mod = keybindings[setting_keybind_id]->mod;
			setting_keybind_ignore = 1;
		}
		else if (setting_type == SETTING_TYPE_SETTINGS)
		{
			setting_change_value(setting_category_selected, list->row_selected - 1, 0);
			list_clicked = 0;
		}
	}
}

/**
 * Handle list key events.
 * @param list List.
 * @param key Key that was pressed.
 * @return 1 if the key was handled, -1 otherwise. */
static int list_key_event(list_struct *list, SDLKey key)
{
	switch (key)
	{
		case SDLK_RIGHT:
			if (SDL_GetModState() & KMOD_SHIFT)
			{
				setting_category_change(1);
			}
			else
			{
				setting_change_value(setting_category_selected, list->row_selected - 1, 1);
			}

			break;

		case SDLK_LEFT:
			if (SDL_GetModState() & KMOD_SHIFT)
			{
				setting_category_change(-1);
			}
			else
			{
				setting_change_value(setting_category_selected, list->row_selected - 1, -1);
			}

			break;

		default:
			return -1;
	}

	return 1;
}

/**
 * Handle pressing a button in the settings popup.
 * @param button The button ID. */
static void settings_button_handle(size_t button)
{
	if (button == BUTTON_KEY_SETTINGS || button == BUTTON_SETTINGS)
	{
		uint32 cols, max_rows;

		if (button == BUTTON_SETTINGS)
		{
			setting_type = SETTING_TYPE_SETTINGS;
			cols = 1;
			max_rows = 9;
		}
		else if (button == BUTTON_KEY_SETTINGS)
		{
			setting_type = SETTING_TYPE_KEYBINDINGS;
			cols = 3;
			max_rows = 12;
		}

		setting_category_selected = 0;

		list_settings = list_create(max_rows, cols, 8);
		list_settings->handle_esc_func = list_handle_esc;
		list_settings->handle_enter_func = list_handle_enter;

		if (button == BUTTON_SETTINGS)
		{
			list_set_column(list_settings, 0, 430, 7, NULL, -1);
			list_set_font(list_settings, FONT_SANS14);
			list_settings->handle_mouse_row_func = list_handle_mouse_row;
			list_settings->key_event_func = list_key_event;
			list_settings->row_highlight_func = NULL;
			list_settings->row_selected_func = NULL;
			list_settings->post_column_func = list_post_column;
		}
		else if (button == BUTTON_KEY_SETTINGS)
		{
			list_set_font(list_settings, FONT_ARIAL11);
			list_set_column(list_settings, 0, 273, 7, "Command", -1);
			list_set_column(list_settings, 1, 93, 7, "Key", 1);
			list_set_column(list_settings, 2, 50, 7, "Repeat", 1);
			list_settings->header_height = 7;
		}

		list_scrollbar_enable(list_settings);
		settings_list_reload(list_settings);
		return;
	}
	else if (button == BUTTON_LOGOUT)
	{
		socket_close(&csocket);
		GameStatus = GAME_STATUS_INIT;
	}

	popup_destroy_all();
}

/** @copydoc popup_button::event_func */
static int setting_popup_button_event_func(popup_button *button)
{
	(void) button;
	help_show(setting_type_help[setting_type]);
	return 1;
}

/**
 * Handle events for the settings popup. */
static int settings_popup_event_func(popup_struct *popup, SDL_Event *event)
{
	if (setting_type == SETTING_TYPE_NONE)
	{
		if (GameStatus == GAME_STATUS_PLAY && button_event(&button_password, event))
		{
			setting_type = SETTING_TYPE_PASSWORD;
			setting_password_confirmed = 0;
			text_input_open(64);
			return 1;
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
		else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEMOTION)
		{
			int x, y, width;
			size_t i;

			y = popup->y + 60;

			for (i = 0; i < BUTTON_NUM; i++)
			{
				if (GameStatus != GAME_STATUS_PLAY && (i == BUTTON_BACK || i == BUTTON_LOGOUT))
				{
					continue;
				}

				if (event->motion.y >= y && event->motion.y < y + FONT_HEIGHT(FONT_SERIF40))
				{
					width = string_get_width(FONT_SERIF40, button_names[i], 0);
					x = popup->x + popup->surface->w / 2 - width / 2;

					if (event->motion.x >= x && event->motion.x < x + width)
					{
						if (event->type == SDL_MOUSEMOTION)
						{
							button_selected = i;
						}
						else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
						{
							settings_button_handle(i);
							return 1;
						}

						break;
					}
				}

				y += FONT_HEIGHT(FONT_SERIF40);
			}
		}
	}
	else if (setting_type == SETTING_TYPE_PASSWORD)
	{
		if (event->type == SDL_KEYDOWN)
		{
			if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER || event->key.keysym.sym == SDLK_TAB)
			{
				if (!setting_password_confirmed)
				{
					if (!strcmp(text_input_string, cpl.password))
					{
						setting_password_confirmed = 1;
						text_input_open(64);
						return 1;
					}
					else
					{
						draw_info(COLOR_RED, "The current password did not match.");
					}
				}
				else
				{
					if (text_input_string[0] == '\0')
					{
						draw_info(COLOR_WHITE, "Canceled password change.");
					}
					else
					{
						SockList sl;
						unsigned char sockbuf[MAX_BUF];

						sl.buf = sockbuf;
						sl.len = 0;
						SockList_AddString(&sl, "pc ");
						SockList_AddStringTerminated(&sl, cpl.password);
						SockList_AddStringTerminated(&sl, text_input_string);
						send_socklist(sl);

						strncpy(cpl.password, text_input_string, sizeof(cpl.password) - 1);
						cpl.password[sizeof(cpl.password) - 1] = '\0';
					}
				}
			}
			else if (event->key.keysym.sym != SDLK_ESCAPE)
			{
				text_input_handle(&event->key);
				return 1;
			}

			popup_destroy_all();
			return 1;
		}
	}
	else if (list_settings)
	{
		if (setting_type == SETTING_TYPE_KEYBINDINGS && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP))
		{
			if (text_input_string_flag && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)
			{
				if (text_input_string_flag)
				{
					SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
					text_input_string_flag = 0;
					return 1;
				}
			}
			else if (text_input_string_flag && setting_keybind_step == KEYBIND_STEP_KEY)
			{
				if (event->type == SDL_KEYUP)
				{
					setting_keybind_key = event->key.keysym.sym;
					setting_keybind_mod = event->key.keysym.mod;
					setting_keybind_step = KEYBIND_STEP_DONE;
				}

				return 1;
			}
			else if (text_input_string_flag && (event->key.keysym.sym == SDLK_KP_ENTER || event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_TAB))
			{
				if (setting_keybind_ignore)
				{
					setting_keybind_ignore = 0;
				}
				else if (setting_keybind_step == KEYBIND_STEP_COMMAND && event->type == SDL_KEYUP)
				{
					setting_keybind_step = KEYBIND_STEP_KEY;
				}
				else if (setting_keybind_step == KEYBIND_STEP_DONE && event->type == SDL_KEYDOWN)
				{
					setting_keybind_apply(list_settings);
				}

				return 1;
			}
			else if (text_input_string_flag && event->type == SDL_KEYDOWN)
			{
				if (setting_keybind_step == KEYBIND_STEP_COMMAND && text_input_handle(&event->key))
				{
					return 1;
				}
			}
			else if (event->type == SDL_KEYDOWN && setting_keybind_action(event->key.keysym.sym, list_settings))
			{
				return 1;
			}
		}

		if (list_handle_keyboard(list_settings, event))
		{
			return 1;
		}
		else if (list_handle_mouse(list_settings, event))
		{
			return 1;
		}
	}

	return -1;
}

/**
 * Open the settings popup. */
void settings_open(void)
{
	/* Sanity check. */
	if (settings_popup)
	{
		return;
	}

	/* Create the popup. */
	settings_popup = popup_create(BITMAP_POPUP);
	settings_popup->draw_func = settings_popup_draw_func;
	settings_popup->event_func = settings_popup_event_func;
	settings_popup->destroy_callback_func = settings_popup_destroy_callback;
	settings_popup->draw_func_post = settings_popup_draw_func_post;

	settings_popup->button_left.event_func = setting_popup_button_event_func;
	popup_button_set_text(&settings_popup->button_left, "?");

	setting_type = SETTING_TYPE_NONE;

	button_create(&button_password);

	button_selected = GameStatus == GAME_STATUS_PLAY ? BUTTON_BACK : BUTTON_SETTINGS;
}

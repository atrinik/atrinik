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
 * Client settings GUI.
 *
 * @author Alex Tokar */

#include <global.h>

typedef union list_settings_graphic_union
{
	/**
	 * Button buffer. */
	button_struct button[2];

	/**
	 * Text-related buffers. */
	struct
	{
		/**
		 * Text input buffer. */
		text_input_struct text_input;

		/**
		 * Button buffer. */
		button_struct button;
	} text;
} list_settings_graphic_union;

/**
 * Button buffer. */
static button_struct button_category_left, button_category_right, button_apply, button_done;
/**
 * Selected setting category. */
static size_t setting_category_selected;
/**
 * The settings list. */
static list_struct *list_settings;
/**
 * Currently focused text input. */
static text_input_struct *text_input_focused;
/**
 * Currently selected text input for color picker. */
static text_input_struct *text_input_selected;

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
 * Figure out setting ID by text input structure pointer.
 * @param text_input Text input to find.
 * @return The setting ID if found, -1 otherwise. */
static int setting_find_by_text_input(text_input_struct *text_input)
{
	uint32 row;
	setting_struct *setting;

	for (row = 0; row < list_settings->rows; row++)
	{
		setting = setting_categories[setting_category_selected]->settings[row];

		if (setting->type == OPT_TYPE_INPUT_TEXT || setting->type == OPT_TYPE_COLOR)
		{
			if (&((list_settings_graphic_union *) list_settings->data)[row].text.text_input == text_input)
			{
				return row;
			}
		}
	}

	return -1;
}

/** @copydoc button_struct::repeat_func */
static void settings_list_button_repeat(button_struct *button)
{
	uint32 row;
	setting_struct *setting;

	for (row = 0; row < list_settings->rows; row++)
	{
		setting = setting_categories[setting_category_selected]->settings[row];

		if (setting->internal)
		{
			break;
		}

		if (setting->type == OPT_TYPE_BOOL)
		{
			button_struct *button_checkbox;

			button_checkbox = &((list_settings_graphic_union *) list_settings->data)[row].button[0];

			if (button == button_checkbox)
			{
				setting_change_value(setting_category_selected, row, 0);
				break;
			}
		}
		else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
		{
			button_struct *button_left, *button_right;

			button_left = &((list_settings_graphic_union *) list_settings->data)[row].button[0];
			button_right = &((list_settings_graphic_union *) list_settings->data)[row].button[1];

			if (button == button_left)
			{
				setting_change_value(setting_category_selected, row, -1);
			}
			else if (button == button_right)
			{
				setting_change_value(setting_category_selected, row, 1);
			}
		}
	}
}

/**
 * Reload the client settings list. */
static void settings_list_reload(void)
{
	size_t i;
	setting_struct *setting;

	text_input_focused = text_input_selected = NULL;
	list_settings->data = memory_reallocz(list_settings->data, sizeof(list_settings_graphic_union) * list_settings->rows, sizeof(list_settings_graphic_union) * setting_categories[setting_category_selected]->settings_num);

	/* Clear all the rows. */
	list_clear_rows(list_settings);

	for (i = 0; i < setting_categories[setting_category_selected]->settings_num; i++)
	{
		setting = setting_categories[setting_category_selected]->settings[i];

		/* Internal, no need to go any further. */
		if (setting->internal)
		{
			break;
		}

		list_add(list_settings, list_settings->rows, 0, NULL);

		if (setting->type == OPT_TYPE_BOOL)
		{
			button_struct *button_checkbox;

			button_checkbox = &((list_settings_graphic_union *) list_settings->data)[i].button[0];
			button_create(button_checkbox);
			button_checkbox->texture = texture_get(TEXTURE_TYPE_SOFTWARE, "rectangle:15,15");
			button_checkbox->texture_over = button_checkbox->texture_pressed = NULL;
			button_checkbox->flags = TEXT_MARKUP;
			button_checkbox->center = 0;
			button_checkbox->color_shadow = button_checkbox->color_over_shadow = NULL;
		}
		else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
		{
			button_struct *button_left, *button_right;

			button_left = &((list_settings_graphic_union *) list_settings->data)[i].button[0];
			button_right = &((list_settings_graphic_union *) list_settings->data)[i].button[1];

			button_create(button_left);
			button_create(button_right);
			button_left->texture = button_right->texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
			button_left->texture_over = button_right->texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
			button_left->texture_pressed = button_right->texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
			button_left->repeat_func = button_right->repeat_func = settings_list_button_repeat;
		}
		else if (setting->type == OPT_TYPE_INPUT_TEXT || setting->type == OPT_TYPE_COLOR)
		{
			text_input_struct *text_input;

			text_input = &((list_settings_graphic_union *) list_settings->data)[i].text.text_input;
			text_input_create(text_input);
			text_input_set(text_input, setting_get_str(setting_category_selected, i));
			text_input->font = FONT_ARIAL10;
			text_input->w = 100;
			text_input->h = FONT_HEIGHT(text_input->font);
			text_input->focus = 0;

			if (setting->type == OPT_TYPE_COLOR)
			{
				button_struct *button;

				button = &((list_settings_graphic_union *) list_settings->data)[i].text.button;
				button_create(button);
				button->color_shadow = button->color_over_shadow = NULL;
				button->texture = texture_get(TEXTURE_TYPE_SOFTWARE, "rectangle:40,14");
				button->texture_over = button->texture_pressed = NULL;
			}
		}
	}

	list_offsets_ensure(list_settings);
}

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
	setting_struct *setting;
	int x, y, mx, my;

	setting = setting_categories[setting_category_selected]->settings[row];

	if (setting->internal)
	{
		return;
	}

	x = list->x + list->frame_offset;
	y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list));

	/* Show the actual setting name. */
	text_show_shadow_format(list->surface, FONT_ARIAL11, x + 4, y + 3, list->row_selected == row + 1 ? COLOR_HGOLD : COLOR_WHITE, COLOR_BLACK, 0, NULL, "%s:", setting->name);

	SDL_GetMouseState(&mx, &my);

	/* Checkbox. */
	if (setting->type == OPT_TYPE_BOOL)
	{
		button_struct *button_checkbox;
		SDL_Rect box;

		button_checkbox = &((list_settings_graphic_union *) list_settings->data)[row].button[0];

		button_checkbox->surface = list->surface;
		button_checkbox->x = x + list->width - texture_surface(button_checkbox->texture)->w - 2;
		button_checkbox->y = y + 1;
		button_checkbox->color = "8c7a7a";
		button_checkbox->color_over = "b09a9a";
		button_set_parent(button_checkbox, list->px, list->py);
		button_show(button_checkbox, setting_get_int(setting_category_selected, row) ? "<x=1><y=1><c=#"COLOR_HGOLD"><line=0,0,12,12><line=12,0,0,12></c>" : NULL);

		box.x = button_checkbox->x;
		box.y = button_checkbox->y;
		box.w = texture_surface(button_checkbox->texture)->w;
		box.h = texture_surface(button_checkbox->texture)->h;

		if (button_checkbox->mouse_over)
		{
			border_create_color(list->surface, &box, 1, "b09a9a");
		}
		else
		{
			border_create_color(list->surface, &box, 1, "8c7a7a");
		}
	}
	/* Select or range. */
	else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
	{
		button_struct *button_left, *button_right;
		sint64 val;
		SDL_Rect dst;

		button_left = &((list_settings_graphic_union *) list_settings->data)[row].button[0];
		button_right = &((list_settings_graphic_union *) list_settings->data)[row].button[1];
		val = setting_get_int(setting_category_selected, row);

		dst.x = x + list->width - 1 - 150 - texture_surface(button_left->texture)->w;
		dst.y = y + 1;
		dst.w = 150;
		dst.h = LIST_ROW_HEIGHT(list) - 2;

		SDL_FillRect(list->surface, &dst, SDL_MapRGB(list->surface->format, 0, 0, 0));

		if (setting->type == OPT_TYPE_SELECT)
		{
			text_show(list->surface, FONT_ARIAL10, SETTING_SELECT(setting)->options[val], dst.x, dst.y, COLOR_WHITE, TEXT_ALIGN_CENTER, &dst);
		}
		else if (setting->type == OPT_TYPE_RANGE)
		{
			text_show_format(list->surface, FONT_ARIAL10, dst.x, dst.y, COLOR_WHITE, TEXT_ALIGN_CENTER, &dst, "%"FMT64, val);
		}

		button_left->surface = list->surface;
		button_left->x = dst.x - texture_surface(button_left->texture)->w - 1;
		button_left->y = dst.y;
		button_set_parent(button_left, list->px, list->py);
		button_show(button_left, "<");

		button_right->surface = list->surface;
		button_right->x = dst.x + dst.w;
		button_right->y = dst.y;
		button_set_parent(button_right, list->px, list->py);
		button_show(button_right, ">");
	}
	else if (setting->type == OPT_TYPE_INPUT_TEXT || setting->type == OPT_TYPE_COLOR)
	{
		text_input_struct *text_input;
		SDL_Rect dst;

		dst.x = x + list->width - 2;
		dst.y = y + 2;

		if (setting->type == OPT_TYPE_COLOR)
		{
			button_struct *button;
			SDL_Color color;
			SDL_Rect box;
			char color_notation[COLOR_BUF];

			button = &((list_settings_graphic_union *) list_settings->data)[row].text.button;
			dst.x -= texture_surface(button->texture)->w;
			button_set_parent(button, list->px, list->py);
			button->surface = list->surface;
			button->x = dst.x;
			button->y = dst.y;
			button_show(button, NULL);

			if (text_color_parse(setting_get_str(setting_category_selected, row), &color))
			{
				snprintf(color_notation, sizeof(color_notation), "%.2X%.2X%.2X", 255 - color.r, 255 - color.g, 255 - color.b);
			}
			else
			{
				color.r = color.g = color.b = 0;
				snprintf(color_notation, sizeof(color_notation), "%.2X%.2X%.2X", 255, 255, 255);
			}

			box.w = texture_surface(button->texture)->w;
			box.h = texture_surface(button->texture)->h;
			text_show_format(list->surface, FONT_ARIAL11, button->x, button->y, COLOR_BLACK, TEXT_MARKUP, NULL, "<bar=#%.2X%.2X%.2X %d %d>", color.r, color.g, color.b, box.w, box.h);
			text_show(list->surface, FONT_ARIAL11, "Pick", button->x, button->y, color_notation, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

			dst.x -= 5;
		}

		text_input = &((list_settings_graphic_union *) list_settings->data)[row].text.text_input;
		dst.x -= text_input->w;
		text_input->focus = text_input_focused == text_input;
		text_input_set_parent(text_input, list->px, list->py);
		text_input_show(text_input, list->surface, dst.x, dst.y + 1);
		text_input->focus = 0;
	}
}

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
	if (list->row_selected)
	{
		setting_change_value(setting_category_selected, list->row_selected - 1, 0);
	}
}

/** @copydoc list_struct::handle_mouse_row_func */
static void list_handle_mouse_row(list_struct *list, uint32 row, SDL_Event *event)
{
	if (event->type == SDL_MOUSEMOTION)
	{
		list->row_selected = row + 1;
	}
}

/** @copydoc color_picker::callback_func */
static void color_picker_callback(color_picker_struct *color_picker)
{
	char color_notation[MAX_BUF];
	double rgb[3];
	int i;

	colorspace_hsv2rgb(color_picker->hsv, rgb);
	snprintf(color_notation, sizeof(color_notation), "#%.2X%.2X%.2X", (int) (255 * rgb[0]), (int) (255 * rgb[1]), (int) (255 * rgb[2]));

	i = setting_find_by_text_input(text_input_selected);

	if (i != -1)
	{
		text_input_set(text_input_selected, color_notation);
		setting_set_str(setting_category_selected, i, color_notation);
	}
}

/** @copydoc button_struct::repeat_func */
static void button_repeat(button_struct *button)
{
	size_t new_cat;

	new_cat = setting_category_selected;

	if (button == &button_category_right)
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
	else if (button == &button_category_left)
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
		settings_list_reload();
	}
}

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
	SDL_Rect box;
	setting_struct *setting;

	box.w = popup->surface->w;
	box.h = 38;
	text_show(popup->surface, FONT_SERIF20, "Client Settings", 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	list_show(list_settings, 30, 50);
	list_set_parent(list_settings, popup->x, popup->y);

	setting = setting_categories[setting_category_selected]->settings[list_settings->row_selected - 1];

	box.w = list_settings->width;
	box.h = 0;
	text_show(popup->surface, FONT_SERIF14, setting_categories[setting_category_selected]->name, list_settings->x, list_settings->y - 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

	button_set_parent(&button_category_left, popup->x, popup->y);
	button_set_parent(&button_category_right, popup->x, popup->y);
	button_set_parent(&button_apply, popup->x, popup->y);
	button_set_parent(&button_done, popup->x, popup->y);

	button_category_left.x = list_settings->x;
	button_category_left.y = 50;
	button_show(&button_category_left, "<");

	button_category_right.x = list_settings->x + list_settings->width - texture_surface(button_category_right.texture)->w;
	button_category_right.y = 50;
	button_show(&button_category_right, ">");

	button_apply.x = list_settings->x + LIST_WIDTH_FULL(list_settings) - texture_surface(button_apply.texture)->w;
	button_apply.y = popup->surface->h - 72;
	button_show(&button_apply, "Apply");

	button_done.x = list_settings->x + LIST_WIDTH_FULL(list_settings) - texture_surface(button_done.texture)->w;
	button_done.y = popup->surface->h - 50;
	button_show(&button_done, "Done");

	if (setting->desc)
	{
		box.w = LIST_WIDTH_FULL(list_settings) - texture_surface(button_apply.texture)->w;
		box.h = 66;
		text_show_shadow(popup->surface, FONT_ARIAL11, setting->desc, list_settings->x - 2, popup->surface->h - 75, COLOR_WHITE, COLOR_BLACK, TEXT_WORD_WRAP | TEXT_MARKUP, &box);
	}

	return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
	uint32 row;
	setting_struct *setting;

	if (event->type == SDL_KEYDOWN)
	{
		if (text_input_focused)
		{
			text_input_focused->focus = 1;

			if (event->key.keysym.sym == SDLK_ESCAPE)
			{
				text_input_focused = NULL;
				return 1;
			}
			else if (IS_ENTER(event->key.keysym.sym))
			{
				int i;
				text_input_struct *text_input;

				i = setting_find_by_text_input(text_input_focused);

				if (i == -1)
				{
					return 1;
				}

				setting = setting_categories[setting_category_selected]->settings[i];
				text_input = &((list_settings_graphic_union *) list_settings->data)[i].text.text_input;
				text_input_focused = NULL;

				if (setting->type == OPT_TYPE_COLOR && !text_color_parse(text_input->str, NULL))
				{
					text_input_set(text_input, setting_get_str(setting_category_selected, i));
					return 1;
				}

				setting_set_str(setting_category_selected, i, text_input->str);
				return 1;
			}
			else if (text_input_event(text_input_focused, event))
			{
				return 1;
			}
		}

		if (event->key.keysym.sym == SDLK_ESCAPE)
		{
			popup_destroy(popup);
			return 1;
		}
		else if (event->key.keysym.sym == SDLK_RIGHT)
		{
			if (event->key.keysym.mod & KMOD_SHIFT)
			{
				button_repeat(&button_category_right);
			}
			else
			{
				setting_change_value(setting_category_selected, list_settings->row_selected - 1, 1);
			}

			return 1;
		}
		else if (event->key.keysym.sym == SDLK_LEFT)
		{
			if (event->key.keysym.mod & KMOD_SHIFT)
			{
				button_repeat(&button_category_left);
			}
			else
			{
				setting_change_value(setting_category_selected, list_settings->row_selected - 1, -1);
			}

			return 1;
		}
	}

	if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
	{
		text_input_focused = NULL;
	}

	for (row = 0; row < list_settings->rows; row++)
	{
		setting = setting_categories[setting_category_selected]->settings[row];

		if (setting->internal)
		{
			break;
		}

		if (setting->type == OPT_TYPE_BOOL)
		{
			button_struct *button_checkbox;

			button_checkbox = &((list_settings_graphic_union *) list_settings->data)[row].button[0];

			if (button_event(button_checkbox, event))
			{
				settings_list_button_repeat(button_checkbox);
				return 1;
			}
		}
		else if (setting->type == OPT_TYPE_SELECT || setting->type == OPT_TYPE_RANGE)
		{
			button_struct *button_left, *button_right;

			button_left = &((list_settings_graphic_union *) list_settings->data)[row].button[0];
			button_right = &((list_settings_graphic_union *) list_settings->data)[row].button[1];

			if (button_event(button_left, event))
			{
				settings_list_button_repeat(button_left);
				return 1;
			}
			else if (button_event(button_right, event))
			{
				settings_list_button_repeat(button_right);
				return 1;
			}
		}
		else if (setting->type == OPT_TYPE_INPUT_TEXT || setting->type == OPT_TYPE_COLOR)
		{
			text_input_struct *text_input;

			text_input = &((list_settings_graphic_union *) list_settings->data)[row].text.text_input;

			if (setting->type == OPT_TYPE_COLOR)
			{
				button_struct *button;

				button = &((list_settings_graphic_union *) list_settings->data)[row].text.button;

				if (button_event(button, event))
				{
					color_picker_struct *color_picker;

					text_input_selected = text_input;

					color_picker = color_chooser_open();
					color_picker->callback_func = color_picker_callback;
					color_picker_set_notation(color_picker, setting_get_str(setting_category_selected, row));

					return 1;
				}
			}

			if ((event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT && text_input_mouse_over(text_input, event->motion.x, event->motion.y)) || (event->type == SDL_KEYDOWN && IS_ENTER(event->key.keysym.sym)))
			{
				text_input_focused = text_input;
				return 1;
			}
		}
	}

	if (button_event(&button_category_left, event))
	{
		button_repeat(&button_category_left);
		return 1;
	}
	else if (button_event(&button_category_right, event))
	{
		button_repeat(&button_category_right);
		return 1;
	}
	else if (button_event(&button_apply, event))
	{
		settings_apply_change();
		return 1;
	}
	else if (button_event(&button_done, event))
	{
		popup_destroy(popup);
		return 1;
	}
	else if (list_handle_keyboard(list_settings, event))
	{
		return 1;
	}
	else if (list_handle_mouse(list_settings, event))
	{
		return 1;
	}

	return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
	settings_apply_change();
	list_remove(list_settings);
	list_settings = NULL;
	return 1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event(popup_button *button)
{
	help_show("client settings");
	return 1;
}

void settings_client_open(void)
{
	popup_struct *popup;

	setting_category_selected = 0;

	popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
	popup->draw_func = popup_draw;
	popup->event_func = popup_event;
	popup->destroy_callback_func = popup_destroy_callback;

	popup->button_left.event_func = popup_button_event;
	popup_button_set_text(&popup->button_left, "?");

	button_create(&button_category_left);
	button_create(&button_category_right);
	button_create(&button_apply);
	button_create(&button_done);

	button_category_left.texture = button_category_right.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
	button_category_left.texture_over = button_category_right.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
	button_category_left.texture_pressed = button_category_right.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
	button_category_left.repeat_func = button_category_right.repeat_func = button_repeat;

	button_category_left.surface = button_category_right.surface = button_apply.surface = button_done.surface = popup->surface;

	list_settings = list_create(9, 1, 8);
	list_settings->surface = popup->surface;
	list_settings->post_column_func = list_post_column;
	list_settings->row_highlight_func = NULL;
	list_settings->row_selected_func = NULL;
	list_settings->handle_enter_func = list_handle_enter;
	list_settings->handle_mouse_row_func = list_handle_mouse_row;
	list_set_column(list_settings, 0, 430, 7, NULL, -1);
	list_set_font(list_settings, FONT_SANS14);
	list_scrollbar_enable(list_settings);
	settings_list_reload();
}

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
 * Implements the characters chooser.
 *
 * @author Alex Tokar */

#include <global.h>

enum
{
	TEXT_INPUT_CHARNAME,
	TEXT_INPUT_PASSWORD,
	TEXT_INPUT_PASSWORD_NEW,
	TEXT_INPUT_PASSWORD_NEW2,

	TEXT_INPUT_NUM
};

/**
 * Progress dots buffer. */
static progress_dots progress;
/**
 * Button buffer. */
static button_struct button_tab_characters, button_tab_new, button_tab_password, button_character_gender, button_character_left, button_character_right;
/**
 * Text input buffers. */
static text_input_struct text_inputs[TEXT_INPUT_NUM];
/**
 * Characters list. */
static list_struct *list_characters;
/**
 * Currently selected text input. */
static size_t text_input_current;
/**
 * Which character race is selected in the character creation tab. */
static size_t character_race;
/**
 * Which gender is selected in the character creation tab. */
static size_t character_gender;

/** @copydoc text_input_struct::character_check_func */
static int text_input_character_check(text_input_struct *text_input, char c)
{
	if (text_input == &text_inputs[TEXT_INPUT_CHARNAME] && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_CHARNAME]))
	{
		return 0;
	}
	else if (!char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD]))
	{
		return 0;
	}

	return 1;
}

/** @copydoc list_struct::text_color_hook */
static void list_text_color(list_struct *list, uint32 row, uint32 col, const char **color, const char **color_shadow)
{
	if (col == 0)
	{
		*color_shadow = NULL;
	}
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len)
{
	if (strcmp(anchor_action, "charname") == 0)
	{
		packet_struct *packet;

		packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);
		packet_append_uint8(packet, CMD_ACCOUNT_LOGIN_CHAR);
		packet_append_string_terminated(packet, buf);
		socket_send_packet(packet);
		return 1;
	}

	return 0;
}

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list)
{
	if (list->row_selected)
	{
		text_info_struct info;

		text_anchor_parse(&info, list->text[list->row_selected - 1][1]);
		text_set_anchor_handle(text_anchor_handle);
		text_anchor_execute(&info);
		text_set_anchor_handle(NULL);
	}
}

/** @copydoc popup_struct::popup_draw_func */
static int popup_draw(popup_struct *popup)
{
	SDL_Rect box;
	size_t i;

	box.w = popup->surface->w;
	box.h = 38;
	string_show_shadow_format(popup->surface, FONT_SERIF14, 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box, "Welcome, %s", cpl.account);

	/* Waiting to log in. */
	if (cpl.state == ST_WAITFORPLAY)
	{
		box.w = popup->surface->w;
		box.h = popup->surface->h;
		string_show_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return 1;
	}
	else if (cpl.state == ST_PLAY || cpl.state < ST_STARTCONNECT)
	{
		return 0;
	}

	textwin_show(popup->surface, 265, 45, 220, 132);

	button_set_parent(&button_tab_characters, popup->x, popup->y);
	button_set_parent(&button_tab_new, popup->x, popup->y);
	button_set_parent(&button_tab_password, popup->x, popup->y);
	button_set_parent(&button_character_gender, popup->x, popup->y);
	button_set_parent(&button_character_left, popup->x, popup->y);
	button_set_parent(&button_character_right, popup->x, popup->y);

	button_tab_characters.x = 38;
	button_tab_characters.y = 38;
	button_show(&button_tab_characters, "Characters");
	button_tab_new.x = button_tab_characters.x + TEXTURE_SURFACE(button_tab_characters.texture)->w + 1;
	button_tab_new.y = button_tab_characters.y;
	button_show(&button_tab_new, "New");
	button_tab_password.x = button_tab_new.x + TEXTURE_SURFACE(button_tab_new.texture)->w + 1;
	button_tab_password.y = button_tab_new.y;
	button_show(&button_tab_password, "Password");

	for (i = 0; i < TEXT_INPUT_NUM; i++)
	{
		text_input_set_parent(&text_inputs[i], popup->x, popup->y);
	}

	if (button_tab_characters.pressed_forced)
	{
		list_set_parent(list_characters, popup->x, popup->y);
		list_show(list_characters, 36, 50);
	}
	else if (button_tab_new.pressed_forced)
	{
		int max_width, width;

		max_width = 0;

		for (i = 0; i < s_settings->num_characters; i++)
		{
			width = string_get_width(FONT_SERIF12, s_settings->characters[i].name, 0);

			if (width > max_width)
			{
				max_width = width;
			}
		}

		string_show_format(popup->surface, FONT_ARIAL10, 38, 80, COLOR_WHITE, TEXT_MARKUP, NULL, "<bar=#202020 50 60><icon=%s 50 60><border=#909090 50 60>", s_settings->characters[character_race].gender_faces[character_gender]);

		button_character_left.x = 100;
		button_character_left.y = 80;
		button_show(&button_character_left, "<");

		box.w = max_width;
		box.h = 0;
		string_show_shadow(popup->surface, FONT_SERIF12, s_settings->characters[character_race].name, button_character_left.x + TEXTURE_SURFACE(button_character_left.texture)->w + 5, button_character_left.y, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);

		button_character_right.x = button_character_left.x + TEXTURE_SURFACE(button_character_left.texture)->w + 5 + max_width + 5;
		button_character_right.y = 80;
		button_show(&button_character_right, ">");

		button_character_gender.x = button_character_left.x;
		button_character_gender.y = button_character_left.y + 30;
		button_show(&button_character_gender, character_gender == GENDER_MALE ? "Female" : "Male");

		box.w = text_inputs[TEXT_INPUT_CHARNAME].w;
		string_show(popup->surface, FONT_ARIAL12, "Character name [<tooltip=Enter your character's name.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 162, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
		text_input_show(&text_inputs[TEXT_INPUT_CHARNAME], popup->surface, 50, 180);
	}
	else if (button_tab_password.pressed_forced)
	{
		for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++)
		{
			text_input_show(&text_inputs[i], popup->surface, 50, 50 + i * 30);
		}
	}

	return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
	size_t i;

	if (button_event(&button_tab_characters, event))
	{
		button_tab_new.pressed_forced = button_tab_password.pressed_forced = 0;
		button_tab_characters.pressed_forced = 1;
		return 1;
	}
	else if (button_event(&button_tab_new, event))
	{
		button_tab_characters.pressed_forced = button_tab_password.pressed_forced = 0;
		button_tab_new.pressed_forced = 1;

		text_input_set(&text_inputs[TEXT_INPUT_CHARNAME], NULL);
		text_inputs[TEXT_INPUT_CHARNAME].focus = 1;

		character_race = 0;
		character_gender = GENDER_MALE;

		return 1;
	}
	else if (button_event(&button_tab_password, event))
	{
		button_tab_characters.pressed_forced = button_tab_new.pressed_forced = 0;
		button_tab_password.pressed_forced = 1;

		for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++)
		{
			text_input_set(&text_inputs[i], NULL);
		}

		text_input_current = TEXT_INPUT_PASSWORD;
		text_inputs[text_input_current].focus = 1;
		return 1;
	}

	if (button_tab_characters.pressed_forced)
	{
		if (list_handle_keyboard(list_characters, event) || list_handle_mouse(list_characters, event))
		{
			return 1;
		}
	}
	else if (button_tab_new.pressed_forced)
	{
		if (text_input_event(&text_inputs[TEXT_INPUT_CHARNAME], event))
		{
			return 1;
		}
		else if (button_event(&button_character_gender, event))
		{
			character_gender = character_gender == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;
			return 1;
		}
		else if (button_event(&button_character_left, event))
		{
			if (character_race == 0)
			{
				character_race = s_settings->num_characters - 1;
			}
			else
			{
				character_race--;
			}
		}
		else if (button_event(&button_character_right, event))
		{
			if (character_race == s_settings->num_characters - 1)
			{
				character_race = 0;
			}
			else
			{
				character_race++;
			}
		}
	}
	else if (button_tab_password.pressed_forced)
	{
		if (event->type == SDL_KEYDOWN)
		{
			if (IS_NEXT(event->key.keysym.sym))
			{
				if (IS_ENTER(event->key.keysym.sym) && text_input_current == TEXT_INPUT_PASSWORD_NEW2)
				{
					packet_struct *packet;
					uint32 lower, upper;

					if (strcmp(text_inputs[TEXT_INPUT_PASSWORD_NEW].str, text_inputs[TEXT_INPUT_PASSWORD_NEW2].str) != 0)
					{
						draw_info(COLOR_RED, "The new passwords do not match.");
						return 1;
					}

					packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);
					packet_append_uint8(packet, CMD_ACCOUNT_PSWD);

					for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++)
					{
						if (*text_inputs[i].str == '\0')
						{
							draw_info(COLOR_RED, "You must enter a valid value for all text inputs.");
							packet_free(packet);
							return 1;
						}
						else if (sscanf(s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD_MAX], "%u-%u", &lower, &upper) == 2 && (text_inputs[i].num < lower || text_inputs[i].num > upper))
						{
							draw_info_format(COLOR_RED, "Password must be between %d and %d characters long.", lower, upper);
							packet_free(packet);
							return 1;
						}

						packet_append_string_terminated(packet, text_inputs[i].str);
					}

					socket_send_packet(packet);

					for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++)
					{
						text_input_set(&text_inputs[i], NULL);
						text_inputs[i].focus = 0;
					}

					text_input_current = TEXT_INPUT_PASSWORD;
					text_inputs[text_input_current].focus = 1;

					return 1;
				}

				text_inputs[text_input_current].focus = 0;
				text_input_current++;

				if (text_input_current > TEXT_INPUT_PASSWORD_NEW2)
				{
					text_input_current = TEXT_INPUT_PASSWORD;
				}

				text_inputs[text_input_current].focus = 1;

				return 1;
			}
		}
		else if (event->type == SDL_MOUSEBUTTONDOWN)
		{
			if (event->button.button == SDL_BUTTON_LEFT)
			{
				for (i = TEXT_INPUT_PASSWORD; i < TEXT_INPUT_NUM; i++)
				{
					if (text_input_mouse_over(&text_inputs[i], event->motion.x, event->motion.y))
					{
						text_inputs[text_input_current].focus = 0;
						text_input_current = i;
						text_inputs[text_input_current].focus = 1;
						return 1;
					}
				}
			}
		}

		if (text_input_event(&text_inputs[text_input_current], event))
		{
			return 1;
		}
	}

	return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
	list_remove(list_characters);

	if (cpl.state != ST_PLAY)
	{
		cpl.state = ST_START;
	}

	return 1;
}

/**
 * Open the characters chooser popup. */
void characters_open(void)
{
	popup_struct *popup;
	size_t i;

	progress_dots_create(&progress);

	popup = popup_create("popup");
	popup->draw_func = popup_draw;
	popup->event_func = popup_event;
	popup->destroy_callback_func = popup_destroy_callback;

	button_create(&button_tab_characters);
	button_create(&button_tab_new);
	button_create(&button_tab_password);
	button_create(&button_character_gender);
	button_create(&button_character_left);
	button_create(&button_character_right);
	button_tab_characters.pressed_forced = 1;
	button_tab_characters.surface = button_tab_new.surface = button_tab_password.surface = button_character_gender.surface = button_character_left.surface = button_character_right.surface = popup->surface;
	button_tab_characters.texture = button_tab_new.texture = button_tab_password.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_tab");
	button_tab_characters.texture_over = button_tab_new.texture_over = button_tab_password.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_over");
	button_tab_characters.texture_pressed = button_tab_new.texture_pressed = button_tab_password.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_down");
	button_character_left.texture = button_character_right.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
	button_character_left.texture_over = button_character_right.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
	button_character_left.texture_pressed = button_character_right.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");

	for (i = 0; i < TEXT_INPUT_NUM; i++)
	{
		text_input_create(&text_inputs[i]);
		text_inputs[i].character_check_func = text_input_character_check;
		text_inputs[i].w = 150;
		text_inputs[i].focus = 0;

		if (i != TEXT_INPUT_CHARNAME)
		{
			text_inputs[i].show_edit_func = text_input_show_edit_password;
		}
	}

	list_characters = list_create(3, 2, 8);
	list_characters->text_color_hook = list_text_color;
	list_characters->handle_enter_func = list_handle_enter;
	list_characters->surface = popup->surface;
	list_characters->row_height_adjust = 45;
	list_characters->text_flags = TEXT_MARKUP;
	list_set_column(list_characters, 0, 55, 0, NULL, -1);
	list_set_column(list_characters, 1, 150, 0, NULL, -1);
	list_scrollbar_enable(list_characters);

	cpl.state = ST_CHARACTERS;
}

static int archname_to_character(const char *archname, size_t *race, size_t *gender)
{
	for (*race = 0; *race < s_settings->num_characters; (*race)++)
	{
		for (*gender = 0; *gender < GENDER_MAX; (*gender)++)
		{
			if (s_settings->characters[*race].gender_archetypes[*gender] && strcmp(s_settings->characters[*race].gender_archetypes[*gender], archname) == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}

void socket_command_characters(uint8 *data, size_t len, size_t pos)
{
	char archname[MAX_BUF], name[MAX_BUF];
	uint8 level;
	size_t race, gender;

	if (cpl.state != ST_CHARACTERS)
	{
		characters_open();
		cpl.state = ST_CHARACTERS;
	}

	list_clear(list_characters);

	packet_to_string(data, len, &pos, cpl.account, sizeof(cpl.account));
	packet_to_string(data, len, &pos, cpl.host, sizeof(cpl.host));
	packet_to_string(data, len, &pos, cpl.last_host, sizeof(cpl.last_host));
	packet_to_string(data, len, &pos, cpl.last_time, sizeof(cpl.last_time));

	while (pos < len)
	{
		packet_to_string(data, len, &pos, archname, sizeof(archname));
		packet_to_string(data, len, &pos, name, sizeof(name));
		level = packet_to_uint8(data, len, &pos);

		if (archname_to_character(archname, &race, &gender))
		{
			char buf[MAX_BUF];

			snprintf(buf, sizeof(buf), "<icon=%s 40 60>", s_settings->characters[race].gender_faces[gender]);
			list_add(list_characters, list_characters->rows, 0, buf);
			snprintf(buf, sizeof(buf), "<y=3><a=charname><c=#"COLOR_HGOLD"><font=serif 12>%s</font></c></a>\n<y=2>Level: %d", name, level);
			list_add(list_characters, list_characters->rows - 1, 1, buf);
		}
	}
}

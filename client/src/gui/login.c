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
 * Implements the login popup.
 *
 * @author Alex Tokar */

#include <global.h>

enum
{
	LOGIN_TEXT_INPUT_NAME,
	LOGIN_TEXT_INPUT_PASSWORD,
	LOGIN_TEXT_INPUT_PASSWORD2,

	LOGIN_TEXT_INPUT_NUM
};

#define LOGIN_TEXT_INPUT_MAX ((button_tab_login.pressed_forced ? LOGIN_TEXT_INPUT_PASSWORD : LOGIN_TEXT_INPUT_PASSWORD2) + 1)

/**
 * Progress dots when connecting. */
static progress_dots progress;
/**
 * Button buffer. */
static button_struct button_tab_login, button_tab_register;
/**
 * Text input buffers. */
static text_input_struct text_inputs[LOGIN_TEXT_INPUT_NUM];
/**
 * Currently selected text input. */
static size_t text_input_current;

static int text_input_character_check(text_input_struct *text_input, char c)
{
	if (text_input == &text_inputs[LOGIN_TEXT_INPUT_NAME] && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_ACCOUNT]))
	{
		return 0;
	}
	else if ((text_input == &text_inputs[LOGIN_TEXT_INPUT_PASSWORD] || text_input == &text_inputs[LOGIN_TEXT_INPUT_PASSWORD2]) && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD]))
	{
		return 0;
	}

	return 1;
}

/** @copydoc popup_struct::popup_draw_func */
static int popup_draw(popup_struct *popup)
{
	SDL_Rect box;
	size_t i;

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
	else if (cpl.state < ST_STARTCONNECT)
	{
		return 0;
	}

	box.w = popup->surface->w;
	box.h = 38;
	string_show_shadow(popup->surface, FONT_SERIF14, "Login", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	if (cpl.state < ST_LOGIN || !file_updates_finished())
	{
		progress_dots_show(&progress, popup->surface, 70, 90);
		return 1;
	}

	box.w = text_inputs[LOGIN_TEXT_INPUT_NAME].w;
	text_offset_set(popup->x, popup->y);
	string_show(popup->surface, FONT_ARIAL12, "Account name [<tooltip=Enter your account's name.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 92, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
	string_show(popup->surface, FONT_ARIAL12, "Password [<tooltip=Enter your password.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 132, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);

	if (button_tab_register.pressed_forced)
	{
		string_show(popup->surface, FONT_ARIAL12, "Verify password [<tooltip=Enter your password again.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 172, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
	}

	text_offset_reset();

	for (i = 0; i < LOGIN_TEXT_INPUT_NUM; i++)
	{
		text_input_set_parent(&text_inputs[i], popup->x, popup->y);
	}

	text_input_show(&text_inputs[LOGIN_TEXT_INPUT_NAME], popup->surface, 50, 110);
	text_input_show(&text_inputs[LOGIN_TEXT_INPUT_PASSWORD], popup->surface, 50, 150);

	if (button_tab_register.pressed_forced)
	{
		text_input_show(&text_inputs[LOGIN_TEXT_INPUT_PASSWORD2], popup->surface, 50, 190);
	}

	return 1;
}

/** @copydoc popup_struct::popup_draw_post_func */
static int popup_draw_post(popup_struct *popup)
{
	textwin_show(popup->x + 265, popup->y + 45, 220, 132);

	button_tab_login.x = popup->x + 38;
	button_tab_login.y = popup->y + 38;
	button_show(&button_tab_login, "Login");
	button_tab_register.x = button_tab_login.x + TEXTURE_SURFACE(button_tab_login.texture)->w + 1;
	button_tab_register.y = button_tab_login.y;
	button_show(&button_tab_register, "Register");
	return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
	size_t i;

	if (button_event(&button_tab_login, event))
	{
		button_tab_login.pressed_forced = 1;
		button_tab_register.pressed_forced = 0;
		return 1;
	}
	else if (button_event(&button_tab_register, event))
	{
		button_tab_register.pressed_forced = 1;
		button_tab_login.pressed_forced = 0;
		return 1;
	}

	if (cpl.state < ST_LOGIN || !file_updates_finished())
	{
		return -1;
	}

	if (event->type == SDL_KEYDOWN)
	{
		if (IS_NEXT(event->key.keysym.sym))
		{
			text_inputs[text_input_current].focus = 0;
			text_input_current++;

			if (text_input_current == LOGIN_TEXT_INPUT_MAX)
			{
				if (IS_ENTER(event->key.keysym.sym))
				{
					packet_struct *packet;

					packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);

					if (button_tab_login.pressed_forced)
					{
						packet_append_uint8(packet, CMD_ACCOUNT_LOGIN);
					}
					else
					{
						packet_append_uint8(packet, CMD_ACCOUNT_REGISTER);
					}

					for (i = 0; i < LOGIN_TEXT_INPUT_MAX; i++)
					{
						if (*text_inputs[i].str == '\0')
						{
							draw_info(COLOR_RED, "You must enter a valid value for all text inputs.");
							packet_free(packet);
							return 1;
						}

						packet_append_string_terminated(packet, text_inputs[i].str);
					}

					socket_send_packet(packet);
					return 1;
				}

				text_input_current = LOGIN_TEXT_INPUT_NAME;
			}

			text_inputs[text_input_current].focus = 1;

			return 1;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_LEFT)
		{
			for (i = 0; i < LOGIN_TEXT_INPUT_MAX; i++)
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

	return -1;
}

/**
 * Start the login procedure. */
void login_start(void)
{
	popup_struct *popup;
	size_t i;

	progress_dots_create(&progress);

	button_create(&button_tab_login);
	button_create(&button_tab_register);
	button_tab_login.pressed_forced = 1;
	button_tab_login.texture = button_tab_register.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_tab");
	button_tab_login.texture_over = button_tab_register.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_over");
	button_tab_login.texture_pressed = button_tab_register.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_down");

	for (i = 0; i < LOGIN_TEXT_INPUT_NUM; i++)
	{
		text_input_create(&text_inputs[i]);
		text_inputs[i].character_check_func = text_input_character_check;
		text_inputs[i].w = 150;
		text_inputs[i].focus = 0;
	}

	text_inputs[LOGIN_TEXT_INPUT_NAME].focus = 1;
	text_inputs[LOGIN_TEXT_INPUT_PASSWORD].show_edit_func = text_inputs[LOGIN_TEXT_INPUT_PASSWORD2].show_edit_func = text_input_show_edit_password;
	text_input_current = LOGIN_TEXT_INPUT_NAME;

	popup = popup_create("popup");
	popup->draw_func = popup_draw;
	popup->draw_post_func = popup_draw_post;
	popup->event_func = popup_event;

	cpl.state = ST_STARTCONNECT;
}

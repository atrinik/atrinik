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

	box.w = popup->surface->w;
	box.h = 38;
	string_blt_shadow(popup->surface, FONT_SERIF14, "Login", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

	box.w = text_inputs[LOGIN_TEXT_INPUT_NAME].w;
	text_offset_set(popup->x, popup->y);
	string_blt(popup->surface, FONT_ARIAL12, "Account name [<tooltip=Enter your account's name.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 92, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
	string_blt(popup->surface, FONT_ARIAL12, "Password [<tooltip=Enter your password.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 132, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);

	if (button_tab_register.pressed_forced)
	{
		string_blt(popup->surface, FONT_ARIAL12, "Verify password [<tooltip=Enter your password again.><h=#"COLOR_HGOLD">?</h></tooltip>]", 50, 172, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
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
	button_render(&button_tab_login, "Login");
	button_tab_register.x = button_tab_login.x + TEXTURE_SURFACE(button_tab_login.texture)->w + 1;
	button_tab_register.y = button_tab_login.y;
	button_render(&button_tab_register, "Register");
	return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
	size_t i;

	if (event->type == SDL_KEYDOWN)
	{
		if (event->key.keysym.sym == SDLK_TAB || IS_ENTER(event->key.keysym.sym))
		{
			text_inputs[text_input_current].focus = 0;
			text_input_current++;

			if ((button_tab_login.pressed_forced && text_input_current == LOGIN_TEXT_INPUT_PASSWORD2) || text_input_current == LOGIN_TEXT_INPUT_NUM)
			{
				if (IS_ENTER(event->key.keysym.sym))
				{

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
			for (i = 0; i < (button_tab_login.pressed_forced ? LOGIN_TEXT_INPUT_PASSWORD2 : LOGIN_TEXT_INPUT_NUM); i++)
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

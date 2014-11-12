/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Progress dots buffer. */
static progress_dots progress;
/**
 * Button buffer. */
static button_struct button_tab_login, button_tab_register, button_done;
/**
 * Text input buffers. */
static text_input_struct text_inputs[LOGIN_TEXT_INPUT_NUM];
/**
 * Currently selected text input. */
static size_t text_input_current;

/** @copydoc text_input_struct::character_check_func */
static int text_input_character_check(text_input_struct *text_input, char c)
{
    if (text_input == &text_inputs[LOGIN_TEXT_INPUT_NAME] && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_ACCOUNT])) {
        return 0;
    }
    else if ((text_input == &text_inputs[LOGIN_TEXT_INPUT_PASSWORD] || text_input == &text_inputs[LOGIN_TEXT_INPUT_PASSWORD2]) && !char_contains(c, s_settings->text[SERVER_TEXT_ALLOWED_CHARS_PASSWORD])) {
        return 0;
    }

    return 1;
}

/** @copydoc popup_struct::popup_draw_func */
static int popup_draw(popup_struct *popup)
{
    SDL_Rect box;
    size_t i;

    /* Connection terminated while we were trying to login. */
    if (cpl.state < ST_STARTCONNECT || cpl.state == ST_CHARACTERS) {
        return 0;
    }

    box.w = popup->surface->w;
    box.h = 38;
    text_show_shadow(popup->surface, FONT_SERIF14, "Login", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    textwin_show(popup->surface, 265, 45, 220, 132);

    button_set_parent(&button_tab_login, popup->x, popup->y);
    button_set_parent(&button_tab_register, popup->x, popup->y);
    button_set_parent(&button_done, popup->x, popup->y);

    button_tab_login.x = 38;
    button_tab_login.y = 38;
    button_show(&button_tab_login, "Login");

    button_tab_register.x = button_tab_login.x + texture_surface(button_tab_login.texture)->w + 1;
    button_tab_register.y = button_tab_login.y;
    button_show(&button_tab_register, "Register");

    if (cpl.state < ST_LOGIN || !file_updates_finished()) {
        progress_dots_show(&progress, popup->surface, 70, 90);
        return 1;
    }

    if ((string_isempty(clioption_settings.connect[0]) || strcasecmp(selected_server->name, clioption_settings.connect[0]) == 0) &&
        cpl.state < ST_WAITLOGIN) {
        if (clioption_settings.connect[1]) {
            text_input_set(&text_inputs[LOGIN_TEXT_INPUT_NAME], clioption_settings.connect[1]);

            if (!clioption_settings.reconnect) {
                efree(clioption_settings.connect[1]);
                clioption_settings.connect[1] = NULL;
            }

            event_push_key_once(SDLK_RETURN, 0);
        }

        if (clioption_settings.connect[2]) {
            text_input_set(&text_inputs[LOGIN_TEXT_INPUT_PASSWORD], clioption_settings.connect[2]);

            if (!clioption_settings.reconnect) {
                efree(clioption_settings.connect[2]);
                clioption_settings.connect[2] = NULL;
            }

            event_push_key_once(SDLK_RETURN, 0);
        }
    }

    box.w = text_inputs[LOGIN_TEXT_INPUT_NAME].coords.w;
    text_show(popup->surface, FONT_ARIAL12, "Account name &lsqb;[tooltip=Enter your account's name.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 92, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
    text_show(popup->surface, FONT_ARIAL12, "Password &lsqb;[tooltip=Enter your password.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 132, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);

    for (i = 0; i < LOGIN_TEXT_INPUT_NUM; i++) {
        text_input_set_parent(&text_inputs[i], popup->x, popup->y);
    }

    text_input_show(&text_inputs[LOGIN_TEXT_INPUT_NAME], popup->surface, 50, 110);
    text_input_show(&text_inputs[LOGIN_TEXT_INPUT_PASSWORD], popup->surface, 50, 150);

    if (button_tab_register.pressed_forced) {
        text_show(popup->surface, FONT_ARIAL12, "Verify password &lsqb;[tooltip=Enter your password again.][h=#"COLOR_HGOLD "]?[/h][/tooltip]&rsqb;", 50, 172, COLOR_WHITE, TEXT_MARKUP | TEXT_ALIGN_CENTER, &box);
        text_input_show(&text_inputs[LOGIN_TEXT_INPUT_PASSWORD2], popup->surface, 50, 190);
    }

    button_done.x = text_inputs[LOGIN_TEXT_INPUT_NAME].coords.x + text_inputs[LOGIN_TEXT_INPUT_NAME].coords.w - texture_surface(button_done.texture)->w;
    button_done.y = button_tab_register.pressed_forced ? 210 : 170;
    button_show(&button_done, "Done");

    return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    size_t i;

    if (button_event(&button_tab_login, event)) {
        button_tab_login.pressed_forced = 1;
        button_tab_register.pressed_forced = 0;
        return 1;
    }
    else if (button_event(&button_tab_register, event)) {
        button_tab_register.pressed_forced = 1;
        button_tab_login.pressed_forced = 0;
        return 1;
    }
    else if (button_event(&button_done, event)) {
        event_push_key_once(SDLK_RETURN, 0);
        return 1;
    }

    if (cpl.state != ST_LOGIN || !file_updates_finished()) {
        return -1;
    }

    if (event->type == SDL_KEYDOWN) {
        if (IS_NEXT(event->key.keysym.sym)) {
            if (text_input_current == LOGIN_TEXT_INPUT_MAX - 1 && IS_ENTER(event->key.keysym.sym)) {
                packet_struct *packet;
                uint32 lower, upper;

                if (button_tab_register.pressed_forced && strcmp(text_inputs[LOGIN_TEXT_INPUT_PASSWORD].str, text_inputs[LOGIN_TEXT_INPUT_PASSWORD2].str) != 0) {
                    draw_info(COLOR_RED, "The passwords do not match.");
                    return 1;
                }

                packet = packet_new(SERVER_CMD_ACCOUNT, 64, 64);

                if (button_tab_login.pressed_forced) {
                    packet_append_uint8(packet, CMD_ACCOUNT_LOGIN);
                }
                else {
                    packet_append_uint8(packet, CMD_ACCOUNT_REGISTER);
                }

                for (i = 0; i < LOGIN_TEXT_INPUT_MAX; i++) {
                    if (*text_inputs[i].str == '\0') {
                        draw_info(COLOR_RED, "You must enter a valid value for all text inputs.");
                        packet_free(packet);
                        return 1;
                    }
                    else if (sscanf(s_settings->text[i == LOGIN_TEXT_INPUT_NAME ? SERVER_TEXT_ALLOWED_CHARS_ACCOUNT_MAX : SERVER_TEXT_ALLOWED_CHARS_PASSWORD_MAX], "%u-%u", &lower, &upper) == 2 && (text_inputs[i].num < lower || text_inputs[i].num > upper)) {
                        draw_info_format(COLOR_RED, "%s must be between %d and %d characters long.", i == LOGIN_TEXT_INPUT_NAME ? "Account name" : "Password", lower, upper);
                        packet_free(packet);
                        return 1;
                    }

                    packet_append_string_terminated(packet, text_inputs[i].str);
                }

                strncpy(cpl.password, text_inputs[LOGIN_TEXT_INPUT_PASSWORD].str, sizeof(cpl.password) - 1);
                cpl.password[sizeof(cpl.password) - 1] = '\0';

                for (i = 0; i < LOGIN_TEXT_INPUT_MAX; i++) {
                    text_input_reset(&text_inputs[i]);
                }

                text_inputs[text_input_current].focus = 0;
                text_input_current = LOGIN_TEXT_INPUT_NAME;
                text_inputs[text_input_current].focus = 1;

                socket_send_packet(packet);
                cpl.state = ST_WAITLOGIN;

                return 1;
            }

            text_inputs[text_input_current].focus = 0;
            text_input_current++;

            if (text_input_current == LOGIN_TEXT_INPUT_MAX) {
                text_input_current = LOGIN_TEXT_INPUT_NAME;
            }

            text_inputs[text_input_current].focus = 1;

            return 1;
        }
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            for (i = 0; i < LOGIN_TEXT_INPUT_MAX; i++) {
                if (text_input_mouse_over(&text_inputs[i], event->motion.x, event->motion.y)) {
                    text_inputs[text_input_current].focus = 0;
                    text_input_current = i;
                    text_inputs[text_input_current].focus = 1;
                    return 1;
                }
            }
        }
    }

    if (text_input_event(&text_inputs[text_input_current], event)) {
        return 1;
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    if (cpl.state != ST_CHARACTERS) {
        cpl.state = ST_START;
    }

    return 1;
}

/**
 * Start the login procedure. */
void login_start(void)
{
    popup_struct *popup;
    size_t i;

    progress_dots_create(&progress);

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;

    button_create(&button_tab_login);
    button_create(&button_tab_register);
    button_create(&button_done);
    button_tab_login.surface = button_tab_register.surface = button_done.surface = popup->surface;
    button_tab_login.pressed_forced = 1;
    button_tab_login.texture = button_tab_register.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_tab");
    button_tab_login.texture_over = button_tab_register.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_over");
    button_tab_login.texture_pressed = button_tab_register.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_tab_down");

    for (i = 0; i < LOGIN_TEXT_INPUT_NUM; i++) {
        text_input_create(&text_inputs[i]);
        text_inputs[i].character_check_func = text_input_character_check;
        text_inputs[i].coords.w = 150;
        text_inputs[i].focus = 0;
    }

    text_inputs[LOGIN_TEXT_INPUT_NAME].focus = 1;
    text_inputs[LOGIN_TEXT_INPUT_PASSWORD].show_edit_func = text_inputs[LOGIN_TEXT_INPUT_PASSWORD2].show_edit_func = text_input_show_edit_password;
    text_input_current = LOGIN_TEXT_INPUT_NAME;

    cpl.state = ST_STARTCONNECT;
}

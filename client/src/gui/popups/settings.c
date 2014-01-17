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
 * Settings GUI.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * The different buttons of the settings popup. */
enum
{
    BUTTON_SETTINGS,
    BUTTON_KEY_SETTINGS,
    BUTTON_LOGOUT,
    BUTTON_DISCONNECT,

    BUTTON_NUM
};

/**
 * Names of the buttons. */
static const char *const button_names[BUTTON_NUM] =
{
    "Client Settings", "Key Settings", "Logout", "Disconnect"
};

/**
 * Currently selected button. */
static size_t button_selected;

/**
 * Handle pressing a button in the settings popup.
 * @param popup The popup.
 * @param button The button ID. */
static void settings_button_handle(popup_struct *popup, size_t button)
{
    if (button == BUTTON_SETTINGS) {
        settings_client_open();
    }
    else if (button == BUTTON_KEY_SETTINGS) {
        settings_keybinding_open();
    }
    else if (button == BUTTON_LOGOUT) {
        clioption_settings.connect[1] = strdup(cpl.account);
        clioption_settings.connect[2] = strdup(cpl.password);
        socket_close(&csocket);
        login_start();
    }
    else if (button == BUTTON_DISCONNECT) {
        socket_close(&csocket);
        cpl.state = ST_INIT;
    }

    popup_destroy(popup);
}

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
    SDL_Rect box;
    size_t i;

    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface, FONT_SERIF20, "Settings", 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    box.h = 0;

    for (i = 0; i < BUTTON_NUM; i++) {
        if (cpl.state != ST_PLAY && (i == BUTTON_DISCONNECT || i == BUTTON_LOGOUT)) {
            continue;
        }

        if (button_selected == i) {
            text_show_shadow_format(popup->surface, FONT_SERIF40, 0, 60 + i * FONT_HEIGHT(FONT_SERIF40), COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_MARKUP, &box, "<c=#9f0408>&gt;</c> %s <c=#9f0408>&lt;</c>", button_names[i]);
        }
        else {
            text_show_shadow(popup->surface, FONT_SERIF40, button_names[i], 0, 60 + i * FONT_HEIGHT(FONT_SERIF40), COLOR_WHITE, COLOR_BLACK, TEXT_ALIGN_CENTER, &box);
        }
    }

    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN) {
        /* Move the selected button up and down. */
        if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_DOWN) {
            int selected, num_buttons;

            selected = button_selected + (event->key.keysym.sym == SDLK_DOWN ? 1 : -1);
            num_buttons = (cpl.state == ST_PLAY ? BUTTON_NUM : BUTTON_LOGOUT) - 1;

            if (selected < 0) {
                selected = num_buttons;
            }
            else if (selected > num_buttons) {
                selected = 0;
            }

            button_selected = selected;
            return 1;
        }
        else if (IS_ENTER(event->key.keysym.sym)) {
            settings_button_handle(popup, button_selected);
            return 1;
        }
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEMOTION) {
        size_t i;
        int x, y, width;

        for (i = 0; i < BUTTON_NUM; i++) {
            if (cpl.state != ST_PLAY && (i == BUTTON_DISCONNECT || i == BUTTON_LOGOUT)) {
                continue;
            }

            y = popup->y + 60 + i * FONT_HEIGHT(FONT_SERIF40);

            if (event->motion.y >= y && event->motion.y < y + FONT_HEIGHT(FONT_SERIF40)) {
                width = text_get_width(FONT_SERIF40, button_names[i], 0);
                x = popup->x + popup->surface->w / 2 - width / 2;

                if (event->motion.x >= x && event->motion.x < x + width) {
                    if (event->type == SDL_MOUSEMOTION) {
                        button_selected = i;
                    }
                    else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
                        settings_button_handle(popup, i);
                        return 1;
                    }

                    break;
                }
            }
        }
    }

    return -1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event(popup_button *button)
{
    help_show("esc menu");
    return 1;
}

/**
 * Open the settings popup. */
void settings_open(void)
{
    popup_struct *popup;

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;

    popup->button_left.event_func = popup_button_event;
    popup_button_set_text(&popup->button_left, "?");

    button_selected = cpl.state == ST_PLAY ? BUTTON_DISCONNECT : BUTTON_SETTINGS;
}

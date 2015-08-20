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
 * Handles the popup that appears when one clicks "Server" button in the
 * main intro screen.
 */

#include <global.h>
#include <toolkit_string.h>

/** Text input buffers. */
static text_input_struct text_input_server_host, text_input_server_port;
/** Add button. */
static button_struct button_add;

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
    SDL_Rect box;

    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface, FONT_SERIF16, "Add a server", 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    box.w = popup->surface->w - 20;
    box.h = 60;

    text_show(popup->surface, FONT_ARIAL11, "Here you can add a server to the list of available servers if you know the server's hostname and port.\n\nBe careful about connecting to unlisted servers, since they could be dangerous.", 10, 45, COLOR_WHITE, TEXT_WORD_WRAP, &box);

    text_show(popup->surface, FONT_ARIAL11, "[b]Hostname:[/b]", 10, 130, COLOR_WHITE, TEXT_MARKUP, NULL);
    text_show(popup->surface, FONT_ARIAL11, "[b]Port:[/b]", 10, 150, COLOR_WHITE, TEXT_MARKUP, NULL);

    text_input_set_parent(&text_input_server_host, popup->x, popup->y);
    text_input_set_parent(&text_input_server_port, popup->x, popup->y);

    text_input_show(&text_input_server_host, popup->surface, 80, 130);
    text_input_show(&text_input_server_port, popup->surface, 80, 150);

    button_set_parent(&button_add, popup->x, popup->y);
    button_add.x = 230;
    button_add.y = 170;
    button_add.surface = popup->surface;
    button_show(&button_add, "Add");

    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    if (button_event(&button_add, event) || (event->type == SDL_KEYDOWN && IS_ENTER(event->key.keysym.sym))) {
        char buf[HUGE_BUF];

        if (*text_input_server_host.str == '\0' || *text_input_server_port.str == '\0' || atoi(text_input_server_port.str) <= 0) {
            return -1;
        }

        snprintf(buf, sizeof(buf), "%s %s", text_input_server_host.str, text_input_server_port.str);

        clioption_settings.servers = erealloc(clioption_settings.servers, sizeof(*clioption_settings.servers) * (clioption_settings.servers_num + 1));
        clioption_settings.servers[clioption_settings.servers_num] = estrdup(buf);
        clioption_settings.servers_num++;

        if (!ms_connecting(-1)) {
            cpl.state = ST_META;
        }

        popup_destroy(popup);
        return 1;
    }

    if (text_input_event(&text_input_server_host, event) || text_input_event(&text_input_server_port, event)) {
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (text_input_mouse_over(&text_input_server_host, event->button.x, event->button.y)) {
            text_input_server_port.focus = 0;
            text_input_server_host.focus = 1;
        } else if (text_input_mouse_over(&text_input_server_port, event->button.x, event->button.y)) {
            text_input_server_host.focus = 0;
            text_input_server_port.focus = 1;
        }
    } else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_TAB) {
            text_input_server_host.focus = !text_input_server_host.focus;
            text_input_server_port.focus = !text_input_server_port.focus;
            return 1;
        }
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    text_input_destroy(&text_input_server_host);
    text_input_destroy(&text_input_server_port);

    button_destroy(&button_add);

    return 1;
}

/**
 * Open the server add popup.
 */
void server_add_open(void)
{
    popup_struct *popup;

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;

    text_input_create(&text_input_server_host);
    text_input_create(&text_input_server_port);

    text_input_server_port.focus = 0;
    text_input_set(&text_input_server_port, "13327");

    button_create(&button_add);
}

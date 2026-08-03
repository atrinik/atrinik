/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 2 of the License, or    *
 *   (at your option) any later version.                                  *
 ************************************************************************/

/** @file Session-only server join-password prompt. */

#include <global.h>
#include <openssl/crypto.h>
#include <toolkit/string.h>

static button_struct button_connect;
static popup_struct *join_password_popup;
static server_struct *join_password_server;
static text_input_struct password_input;

static int
popup_draw (popup_struct *popup)
{
    SDL_Rect box = {0, 0, popup->surface->w, 38};
    text_show(popup->surface,
              FONT_SERIF16,
              "Server password",
              0,
              0,
              COLOR_HGOLD,
              TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER,
              &box);

    box.x = 18;
    box.y = 48;
    box.w = popup->surface->w - 36;
    box.h = 45;
    text_show(popup->surface,
              FONT_ARIAL11,
              "This server requires a join password. It will be kept only "
              "for this client session.",
              box.x,
              box.y,
              COLOR_WHITE,
              TEXT_WORD_WRAP,
              &box);

    text_show(popup->surface,
              FONT_ARIAL11,
              "[b]Password:[/b]",
              30,
              118,
              COLOR_WHITE,
              TEXT_MARKUP,
              NULL);
    text_input_set_parent(&password_input, popup->x, popup->y);
    text_input_show(&password_input, popup->surface, 110, 118);

    button_set_parent(&button_connect, popup->x, popup->y);
    button_connect.x = 190;
    button_connect.y = 170;
    button_connect.surface = popup->surface;
    button_show(&button_connect, "Connect");
    return 1;
}

static int
popup_event (popup_struct *popup, SDL_Event *event)
{
    if (button_event(&button_connect, event) ||
        (event->type == SDL_KEYDOWN && IS_ENTER(event->key.keysym.sym))) {
        if (join_password_server == NULL || password_input.str[0] == '\0') {
            return -1;
        }

        if (join_password_server->join_password != NULL) {
            OPENSSL_cleanse(join_password_server->join_password,
                            strlen(join_password_server->join_password));
            efree(join_password_server->join_password);
        }
        join_password_server->join_password = estrdup(password_input.str);
        popup_destroy(popup);
        login_start();
        return 1;
    }

    if (text_input_event(&password_input, event)) {
        return 1;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        text_input_mouse_over(&password_input,
                              event->button.x,
                              event->button.y)) {
        password_input.focus = 1;
        return 1;
    }
    return -1;
}

static int
popup_destroy_callback (popup_struct *popup)
{
    (void) popup;
    OPENSSL_cleanse(password_input.str, sizeof(password_input.str));
    text_input_destroy(&password_input);
    button_destroy(&button_connect);
    join_password_popup = NULL;
    join_password_server = NULL;
    return 1;
}

void
join_password_open (server_struct *server)
{
    HARD_ASSERT(server != NULL);

    join_password_server = server;
    join_password_popup =
        popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    join_password_popup->draw_func = popup_draw;
    join_password_popup->event_func = popup_event;
    join_password_popup->destroy_callback_func = popup_destroy_callback;

    text_input_create(&password_input);
    password_input.coords.w = 230;
    password_input.max = MAX_BUF - 1;
    password_input.show_edit_func = text_input_show_edit_password;
    button_create(&button_connect);
}

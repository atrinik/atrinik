/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/** @file Per-server direct connection preference popup. */

#include <global.h>

static list_struct *preference_list;
static button_struct button_use;
static popup_struct *preference_popup;
static server_struct *preference_server;

static const char *
preference_description (socket_connection_preference_t preference)
{
    switch (preference) {
    case SOCKET_CONNECTION_PREFERENCE_LAN:
        return "Try a private local-network address first. Best when the "
               "client and server are on the same LAN.";

    case SOCKET_CONNECTION_PREFERENCE_IPV6:
        return "Try a direct IPv6 address first. This avoids IPv4 NAT, but "
               "both networks must provide working IPv6.";

    case SOCKET_CONNECTION_PREFERENCE_MAPPED:
        return "Try the public UDP route created by PCP, NAT-PMP, or UPnP "
               "first. This is usually best across IPv4 routers.";

    case SOCKET_CONNECTION_PREFERENCE_SRFLX:
        return "Try the public address discovered through STUN first. It "
               "works only when the NAT preserves a usable UDP mapping.";

    case SOCKET_CONNECTION_PREFERENCE_DIRECTORY:
        return "Try the address advertised by the metaserver first. The "
               "metaserver still never relays game traffic.";

    case SOCKET_CONNECTION_PREFERENCE_AUTO:
    default:
        return "Let Atrinik choose the most promising direct route. This is "
               "the recommended choice for most players.";
    }
}

static void
preference_apply (void)
{
    if (preference_list == NULL || preference_server == NULL ||
        preference_list->row_selected == 0) {
        return;
    }

    socket_connection_preference_t preference =
        (socket_connection_preference_t) (preference_list->row_selected - 1);
    connection_preference_set(preference_server, preference);
    LOG(INFO,
        "Preferred connection for %s is now %s",
        preference_server->name,
        socket_connection_preference_name(preference));
    popup_destroy(preference_popup);
}

static int
popup_draw (popup_struct *popup)
{
    SDL_Rect box = {0, 0, popup->surface->w, 38};
    text_show(popup->surface,
              FONT_SERIF16,
              "Preferred connection",
              0,
              0,
              COLOR_HGOLD,
              TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER,
              &box);

    char buf[HUGE_BUF];
    snprintf(VS(buf),
             "Choose which direct route %s should try first. If it is "
             "unavailable, the client continues with Automatic fallback.",
             preference_server->name);
    box.x = 18;
    box.y = 42;
    box.w = popup->surface->w - 36;
    box.h = 38;
    text_show(popup->surface,
              FONT_ARIAL10,
              buf,
              box.x,
              box.y,
              COLOR_WHITE,
              TEXT_WORD_WRAP,
              &box);

    list_set_parent(preference_list, popup->x, popup->y);
    list_show(preference_list, 24, 88);

    socket_connection_preference_t selected =
        preference_list->row_selected > 0
            ? (socket_connection_preference_t)
                (preference_list->row_selected - 1)
            : SOCKET_CONNECTION_PREFERENCE_AUTO;
    box.x = 205;
    box.y = 88;
    box.w = popup->surface->w - box.x - 18;
    box.h = 88;
    text_show(popup->surface,
              FONT_ARIAL10,
              preference_description(selected),
              box.x,
              box.y,
              COLOR_HGOLD,
              TEXT_WORD_WRAP,
              &box);

    button_set_parent(&button_use, popup->x, popup->y);
    button_use.x = 190;
    button_use.y = 190;
    button_use.surface = popup->surface;
    button_show(&button_use, "Use selection");
    return 1;
}

static int
popup_event (popup_struct *popup, SDL_Event *event)
{
    (void) popup;
    if (button_event(&button_use, event) ||
        (event->type == SDL_KEYDOWN && IS_ENTER(event->key.keysym.sym))) {
        preference_apply();
        return 1;
    }
    if (list_handle_keyboard(preference_list, event) ||
        list_handle_mouse(preference_list, event)) {
        return 1;
    }
    return -1;
}

static int
popup_destroy_callback (popup_struct *popup)
{
    (void) popup;
    list_remove(preference_list);
    preference_list = NULL;
    button_destroy(&button_use);
    preference_popup = NULL;
    preference_server = NULL;
    return 1;
}

void
connection_preference_open (server_struct *server)
{
    HARD_ASSERT(server != NULL);

    preference_server = server;
    preference_popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    preference_popup->draw_func = popup_draw;
    preference_popup->event_func = popup_event;
    preference_popup->destroy_callback_func = popup_destroy_callback;

    preference_list = list_create(SOCKET_CONNECTION_PREFERENCE_NUM, 1, 8);
    preference_list->surface = preference_popup->surface;
    list_set_column(preference_list, 0, 160, 7, NULL, -1);
    list_set_font(preference_list, FONT_ARIAL11);
    for (int i = 0; i < SOCKET_CONNECTION_PREFERENCE_NUM; i++) {
        list_add(preference_list,
                 (uint32_t) i,
                 0,
                 socket_connection_preference_name(
                     (socket_connection_preference_t) i));
    }
    preference_list->row_selected = connection_preference_get(server) + 1;
    button_create(&button_use);
    button_use.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_large");
    button_use.texture_over =
        texture_get(TEXTURE_TYPE_CLIENT, "button_large_over");
    button_use.texture_pressed =
        texture_get(TEXTURE_TYPE_CLIENT, "button_large_down");
}

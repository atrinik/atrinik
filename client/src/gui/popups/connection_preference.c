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
#include <connection_preferences.h>
#include <toolkit/string.h>

static list_struct *preference_list;
static button_struct button_use;
static popup_struct *preference_popup;
static server_struct *preference_server;

#define PREFERENCE_CONTENT_X 26
#define PREFERENCE_CONTENT_Y 100
#define PREFERENCE_LIST_WIDTH 180
#define PREFERENCE_CONTENT_GAP 18

static const char *preference_description(socket_connection_preference_t preference) {
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

static void preference_apply(void) {
    if (preference_list == NULL || preference_server == NULL ||
        preference_list->row_selected == 0) {
        return;
    }

    socket_connection_preference_t preference =
        (socket_connection_preference_t)(preference_list->row_selected - 1);
    connection_preference_set(preference_server, preference);
    LOG(INFO,
        "Preferred connection for %s is now %s",
        preference_server->name,
        socket_connection_preference_name(preference));
    popup_destroy(preference_popup);
}

static int popup_draw(popup_struct *popup) {
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
    text_show(popup->surface, FONT_ARIAL10, buf, box.x, box.y, COLOR_WHITE, TEXT_WORD_WRAP, &box);

    list_set_parent(preference_list, popup->x, popup->y);
    list_show(preference_list, PREFERENCE_CONTENT_X, PREFERENCE_CONTENT_Y);

    socket_connection_preference_t selected =
        preference_list->row_selected > 0
            ? (socket_connection_preference_t)(preference_list->row_selected - 1)
            : SOCKET_CONNECTION_PREFERENCE_AUTO;
    SDL_Rect help = {PREFERENCE_CONTENT_X + PREFERENCE_LIST_WIDTH + PREFERENCE_CONTENT_GAP,
                     PREFERENCE_CONTENT_Y,
                     popup->surface->w - PREFERENCE_CONTENT_X * 2 - PREFERENCE_LIST_WIDTH -
                         PREFERENCE_CONTENT_GAP,
                     LIST_ROWS_HEIGHT(preference_list)};
    SDL_FillSurfaceRect(popup->surface,
                        &help,
                        pixel_format_map_rgb(popup->surface->format, 0x45, 0x45, 0x45));
    draw_frame(popup->surface, help.x, help.y, help.w, help.h);

    box.x = help.x + 12;
    box.y = help.y + 10;
    box.w = help.w - 24;
    box.h = 18;
    text_show(popup->surface,
              FONT_ARIAL11,
              socket_connection_preference_name(selected),
              box.x,
              box.y,
              COLOR_HGOLD,
              0,
              &box);

    box.y += 24;
    box.h = help.h - 42;
    text_show(popup->surface,
              FONT_ARIAL10,
              preference_description(selected),
              box.x,
              box.y,
              COLOR_WHITE,
              TEXT_WORD_WRAP,
              &box);

    button_set_parent(&button_use, popup->x, popup->y);
    button_use.x = popup->surface->w / 2 - texture_surface(button_use.texture)->w / 2;
    button_use.y = PREFERENCE_CONTENT_Y + LIST_ROWS_HEIGHT(preference_list) + 22;
    button_use.surface = popup->surface;
    button_show(&button_use, "Use selection");
    return 1;
}

static int popup_event(popup_struct *popup, SDL_Event *event) {
    (void)popup;
    if (button_event(&button_use, event) ||
        (event->type == SDL_EVENT_KEY_DOWN && IS_ENTER(event->key.key))) {
        preference_apply();
        return 1;
    }
    if (list_handle_keyboard(preference_list, event) || list_handle_mouse(preference_list, event)) {
        return 1;
    }
    return -1;
}

static int popup_destroy_callback(popup_struct *popup) {
    (void)popup;
    list_remove(preference_list);
    preference_list = NULL;
    button_destroy(&button_use);
    preference_popup = NULL;
    free(preference_server->name);
    free(preference_server->hostname);
    free(preference_server->server_id);
    free(preference_server);
    preference_server = NULL;
    return 1;
}

void connection_preference_open(server_struct *server) {
    HARD_ASSERT(server != NULL);

    preference_server = xcalloc(1, sizeof(*preference_server));
    preference_server->name = xstrdup(server->name);
    if (server->hostname != NULL) {
        preference_server->hostname = xstrdup(server->hostname);
    }
    if (server->server_id != NULL) {
        preference_server->server_id = xstrdup(server->server_id);
    }
    preference_server->port = server->port;
    preference_popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    preference_popup->draw_func = popup_draw;
    preference_popup->event_func = popup_event;
    preference_popup->destroy_callback_func = popup_destroy_callback;

    preference_list = list_create(SOCKET_CONNECTION_PREFERENCE_NUM, 1, 8);
    preference_list->surface = preference_popup->surface;
    preference_list->header_height = 0;
    preference_list->spacing = 0;
    preference_list->frame_offset = 0;
    preference_list->row_height_adjust = 4;
    list_set_column(preference_list, 0, PREFERENCE_LIST_WIDTH, 0, NULL, 1);
    list_set_font(preference_list, FONT_ARIAL11);
    for (int i = 0; i < SOCKET_CONNECTION_PREFERENCE_NUM; i++) {
        list_add(preference_list,
                 (uint32_t)i,
                 0,
                 socket_connection_preference_name((socket_connection_preference_t)i));
    }
    preference_list->row_selected = connection_preference_get(server) + 1;
    button_create(&button_use);
    button_use.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_large");
    button_use.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_large_over");
    button_use.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_large_down");
}

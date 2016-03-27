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
 * Implements the interface used by NPCs and the like.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <packet.h>
#include <toolkit_string.h>

/**
 * The current interface data.
 */
static interface_struct *interface_data = NULL;
/**
 * The interface popup.
 */
static popup_struct *interface_popup = NULL;
/**
 * Button buffers.
 */
static button_struct button_hello, button_close;
/**
 * Character shortcuts for links.
 */
static const char character_shortcuts[] = "123456789qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM{}<>/?~!@#$%^&*()";
/**
 * Text input history.
 */
static text_input_history_struct *text_input_history = NULL;
/**
 * Text input in the interface.
 */
static text_input_struct text_input;

/**
 * Destroy the specified interface data.
 * @param data
 * Interface data to destroy.
 */
static void interface_destroy(interface_struct *data)
{
    if (!data) {
        return;
    }

    if (data->message) {
        efree(data->message);
    }

    if (data->title) {
        efree(data->title);
    }

    if (data->icon) {
        efree(data->icon);
    }

    if (data->text_input_prepend) {
        efree(data->text_input_prepend);
    }

    if (data->text_autocomplete) {
        efree(data->text_autocomplete);
    }

    if (data->anim != NULL) {
        object_remove(data->anim);
    }

    object_remove(data->objects);
    cpl.interface = NULL;

    utarray_free(data->links);
    font_free(data->font);
    efree(data);
}

/** @copydoc text_anchor_handle_func */
static int text_anchor_handle(const char *anchor_action, const char *buf, size_t len, void *custom_data)
{
    if (anchor_action[0] == '\0' && buf[0] != '/') {
        if (!interface_data->progressed || SDL_GetTicks() >= interface_data->progressed_ticks) {
            StringBuffer *sb = stringbuffer_new();
            char *cp;

            stringbuffer_append_printf(sb, "/talk 1 %s", buf);
            cp = stringbuffer_finish(sb);
            send_command_check(cp);
            efree(cp);

            interface_data->progressed = 1;
            interface_data->progressed_ticks = SDL_GetTicks() + INTERFACE_PROGRESSED_TICKS;
        }

        return 1;
    } else if (!strcmp(anchor_action, "close")) {
        interface_data->destroy = 1;
        return 1;
    }

    return 0;
}

static void interface_execute_link(size_t link_id)
{
    char **p;
    text_info_struct info;

    p = (char **) utarray_eltptr(interface_data->links, link_id);

    if (!p) {
        return;
    }

    text_anchor_parse(&info, *p);
    text_set_anchor_handle(text_anchor_handle);
    text_anchor_execute(&info, NULL);
    text_set_anchor_handle(NULL);
}

/** @copydoc popup_struct::draw_func */
static int popup_draw_func(popup_struct *popup)
{
    if (interface_data->anim != NULL &&
            SDL_GetTicks() - interface_data->last_anim > 125) {
        interface_data->last_anim = SDL_GetTicks();

        if (object_animate(interface_data->anim)) {
            popup->redraw = 1;
        }
    }

    if (popup->redraw) {
        SDL_Rect box;

        surface_show(popup->surface, 0, 0, NULL, texture_surface(popup->texture));

        if (interface_data->icon != NULL) {
            text_show_format(popup->surface, FONT_ARIAL10,
                    INTERFACE_ICON_STARTX, INTERFACE_ICON_STARTY, COLOR_WHITE,
                    TEXT_MARKUP, NULL, "[icon=%s %d %d]", interface_data->icon,
                    INTERFACE_ICON_WIDTH, INTERFACE_ICON_HEIGHT);
        } else if (interface_data->anim != NULL) {
            request_face(interface_data->anim->face);
            object_show_centered(popup->surface, interface_data->anim,
                    INTERFACE_ICON_STARTX, INTERFACE_ICON_STARTY,
                    INTERFACE_ICON_WIDTH, INTERFACE_ICON_HEIGHT, false);
        }

        box.w = INTERFACE_TITLE_WIDTH;
        box.h = FONT_HEIGHT(FONT_SERIF14);
        text_show(popup->surface, FONT_SERIF14, interface_data->title, INTERFACE_TITLE_STARTX, INTERFACE_TITLE_STARTY + INTERFACE_TITLE_HEIGHT / 2 - box.h / 2, COLOR_HGOLD, TEXT_MARKUP | TEXT_WORD_WRAP, &box);

        box.w = INTERFACE_TEXT_WIDTH;
        box.h = INTERFACE_TEXT_HEIGHT;
        box.x = 0;
        box.y = interface_data->scroll_offset;
        text_set_anchor_handle(text_anchor_handle);
        text_set_selection(&popup->selection_start, &popup->selection_end, &popup->selection_started);
        text_show(popup->surface, interface_data->font, interface_data->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);
        text_set_selection(NULL, NULL, NULL);
        text_set_anchor_handle(NULL);

        popup->redraw = 0;
    }

    return !interface_data->destroy;
}

/** @copydoc popup_struct::draw_post_func */
static int popup_draw_post_func(popup_struct *popup)
{
    scrollbar_show(&interface_data->scrollbar, ScreenSurface, popup->x + 432, popup->y + 71);

    button_hello.x = popup->x + INTERFACE_BUTTON_HELLO_STARTX;
    button_hello.y = popup->y + INTERFACE_BUTTON_HELLO_STARTY;
    button_show(&button_hello, "Hello");

    button_close.x = popup->x + INTERFACE_BUTTON_CLOSE_STARTX;
    button_close.y = popup->y + INTERFACE_BUTTON_CLOSE_STARTY;
    button_show(&button_close, "Close");

    if (interface_data->text_input) {
        text_input_show(&text_input, ScreenSurface, popup->x + popup->surface->w / 2 - text_input.coords.w / 2, popup->y + popup->surface->h - text_input.coords.h - 15);
    }

    surface_show(ScreenSurface, popup->x, popup->y, NULL, TEXTURE_CLIENT("interface_border"));
    return 1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    interface_destroy(interface_data);
    interface_data = NULL;
    interface_popup = NULL;

    button_destroy(&button_hello);
    button_destroy(&button_close);

    packet_struct *packet = packet_new(SERVER_CMD_TALK, 32, 0);
    packet_append_uint8(packet, CMD_TALK_CLOSE);
    socket_send_packet(packet);

    return 1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event_func(popup_button *button)
{
    help_show("npc interface");
    return 1;
}

/**
 * Handles clicking the 'hello' button.
 */
static void button_hello_event(void)
{
    if (!interface_data->progressed || SDL_GetTicks() >= interface_data->progressed_ticks) {
        keybind_process_command("?HELLO");
        interface_data->progressed = 1;
        interface_data->progressed_ticks = SDL_GetTicks() + INTERFACE_PROGRESSED_TICKS;
    }
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
    if (scrollbar_event(&interface_data->scrollbar, event)) {
        return 1;
    } else if (button_event(&button_hello, event)) {
        button_hello_event();
        return 1;
    } else if (button_event(&button_close, event)) {
        popup_destroy(popup);
        return 1;
    } else if (event->type == SDL_KEYDOWN) {
        if (interface_data->text_input) {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                interface_data->text_input = 0;
                return 1;
            } else if (IS_ENTER(event->key.keysym.sym) || (event->key.keysym.sym == SDLK_TAB && interface_data->text_autocomplete && !string_iswhite(text_input.str) && text_input.pos == text_input.num)) {
                char *input_string;

                input_string = estrdup(text_input.str);

                if (!interface_data->input_cleanup_disable) {
                    string_whitespace_squeeze(input_string);
                    string_whitespace_trim(input_string);
                }

                if (*input_string != '\0' || interface_data->input_allow_empty) {
                    StringBuffer *sb;
                    char *cp;

                    sb = stringbuffer_new();

                    if (!interface_data->text_input_prepend || interface_data->text_input_prepend[0] != '/') {
                        stringbuffer_append_string(sb, "/talk 1 ");
                    }

                    if (interface_data->text_input_prepend) {
                        stringbuffer_append_string(sb, interface_data->text_input_prepend);
                    }

                    if (event->key.keysym.sym == SDLK_TAB) {
                        stringbuffer_append_string(sb, interface_data->text_autocomplete);
                    }

                    stringbuffer_append_string(sb, input_string);

                    cp = stringbuffer_finish(sb);
                    send_command_check(cp);
                    efree(cp);
                }

                efree(input_string);

                if (event->key.keysym.sym != SDLK_TAB) {
                    interface_data->text_input = 0;
                }
            } else if (event->key.keysym.sym == SDLK_TAB && interface_data->allow_tab) {
                text_input_add_char(&text_input, '\t');
            }

            if (text_input_event(&text_input, event)) {
                return 1;
            }
        }

        switch (event->key.keysym.sym) {
        case SDLK_DOWN:
            scrollbar_scroll_adjust(&interface_data->scrollbar, 1);
            return 1;

        case SDLK_UP:
            scrollbar_scroll_adjust(&interface_data->scrollbar, -1);
            return 1;

        case SDLK_PAGEDOWN:
            scrollbar_scroll_adjust(&interface_data->scrollbar, interface_data->scrollbar.max_lines);
            return 1;

        case SDLK_PAGEUP:
            scrollbar_scroll_adjust(&interface_data->scrollbar, -interface_data->scrollbar.max_lines);
            return 1;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            interface_data->text_input = 1;
            text_input_reset(&text_input);
            return 1;

        default:

            if (!keys[event->key.keysym.sym].repeated) {
                char c;
                size_t i, len, links_len;

                if (event->key.keysym.sym >= SDLK_KP0 && event->key.keysym.sym <= SDLK_KP9) {
                    c = '0' + event->key.keysym.sym - SDLK_KP0;
                } else {
                    c = event->key.keysym.unicode & 0xff;
                }

                len = strlen(character_shortcuts);
                links_len = utarray_len(interface_data->links);

                for (i = 0; i < len && i < links_len; i++) {
                    if (c == character_shortcuts[i]) {
                        interface_execute_link(i);
                        return 1;
                    }
                }
            }

            break;
        }

        if (keybind_command_matches_event("?HELLO", &event->key) && !keys[event->key.keysym.sym].repeated) {
            button_hello_event();
            return 1;
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->motion.x >= popup->x && event->motion.x < popup->x + popup->surface->w && event->motion.y >= popup->y && event->motion.y < popup->y + popup->surface->h) {
        if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            scrollbar_scroll_adjust(&interface_data->scrollbar, 1);
            return 1;
        } else if (event->button.button == SDL_BUTTON_WHEELUP) {
            scrollbar_scroll_adjust(&interface_data->scrollbar, -1);
            return 1;
        }
    }

    return -1;
}

/** @copydoc popup_struct::clipboard_copy_func */
static const char *popup_clipboard_copy_func(popup_struct *popup)
{
    return interface_data->message;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_interface(uint8_t *data, size_t len, size_t pos)
{
    uint8_t scroll_bottom = 0, type;
    StringBuffer *sb_message;
    SDL_Rect box;
    interface_struct *old_interface_data;

    if (len == 0) {
        if (interface_data) {
            interface_data->destroy = 1;
        }

        return;
    }

    if (!interface_popup) {
        interface_popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "interface"));
        interface_popup->draw_func = popup_draw_func;
        interface_popup->draw_post_func = popup_draw_post_func;
        interface_popup->destroy_callback_func = popup_destroy_callback;
        interface_popup->event_func = popup_event_func;
        interface_popup->clipboard_copy_func = popup_clipboard_copy_func;
        interface_popup->disable_texture_drawing = 1;

        interface_popup->button_left.event_func = popup_button_event_func;
        interface_popup->button_left.x = 380;
        interface_popup->button_left.y = 4;
        popup_button_set_text(&interface_popup->button_left, "?");

        interface_popup->button_right.x = 411;
        interface_popup->button_right.y = 4;
    } else {
        button_destroy(&button_hello);
        button_destroy(&button_close);
    }

    if (!text_input_history) {
        text_input_history = text_input_history_create();
        text_input_create(&text_input);
        text_input_set_history(&text_input, text_input_history);
    }

    old_interface_data = interface_data;
    sb_message = NULL;

    /* Create new interface. */
    interface_data = ecalloc(1, sizeof(*interface_data));
    interface_popup->redraw = 1;
    interface_popup->selection_start = interface_popup->selection_end = -1;
    interface_data->font = font_get("arial", 11);
    interface_data->objects = object_create(NULL, 0, 0);
    utarray_new(interface_data->links, &ut_str_icd);

    /* Parse the data. */
    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        switch (type) {
        case CMD_INTERFACE_TEXT:

            if (!sb_message) {
                sb_message = stringbuffer_new();
            }

            packet_to_stringbuffer(data, len, &pos, sb_message);
            break;

        case CMD_INTERFACE_LINK:
        {
            char interface_link[HUGE_BUF], *cp;

            packet_to_string(data, len, &pos, interface_link, sizeof(interface_link));
            cp = interface_link;
            utarray_push_back(interface_data->links, &cp);
            break;
        }

        case CMD_INTERFACE_ICON:
        {
            char icon[MAX_BUF];

            packet_to_string(data, len, &pos, icon, sizeof(icon));
            interface_data->icon = estrdup(icon);
            break;
        }

        case CMD_INTERFACE_TITLE:
        {
            char title[HUGE_BUF];

            packet_to_string(data, len, &pos, title, sizeof(title));
            interface_data->title = estrdup(title);
            break;
        }

        case CMD_INTERFACE_INPUT:
        {
            char text_input_content[HUGE_BUF];

            interface_data->text_input = 1;
            packet_to_string(data, len, &pos, text_input_content, sizeof(text_input_content));
            text_input_reset(&text_input);
            text_input_set(&text_input, text_input_content);
            break;
        }

        case CMD_INTERFACE_INPUT_PREPEND:
        {
            if (interface_data->text_input_prepend != NULL) {
                efree(interface_data->text_input_prepend);
            }

            char text_input_prepend[HUGE_BUF];

            packet_to_string(data, len, &pos, text_input_prepend, sizeof(text_input_prepend));
            interface_data->text_input_prepend = estrdup(text_input_prepend);
            break;
        }

        case CMD_INTERFACE_ALLOW_TAB:
            interface_data->allow_tab = 1;
            break;

        case CMD_INTERFACE_INPUT_CLEANUP_DISABLE:
            interface_data->input_cleanup_disable = 1;
            break;

        case CMD_INTERFACE_INPUT_ALLOW_EMPTY:
            interface_data->input_allow_empty = 1;
            break;

        case CMD_INTERFACE_SCROLL_BOTTOM:
            scroll_bottom = 1;
            break;

        case CMD_INTERFACE_AUTOCOMPLETE:
        {
            if (interface_data->text_autocomplete != NULL) {
                efree(interface_data->text_autocomplete);
            }

            char text_autocomplete[HUGE_BUF];

            packet_to_string(data, len, &pos, text_autocomplete, sizeof(text_autocomplete));
            interface_data->text_autocomplete = estrdup(text_autocomplete);
            break;
        }

        case CMD_INTERFACE_RESTORE:

            if (old_interface_data) {
                interface_destroy(interface_data);
                interface_data = old_interface_data;
            }

            break;

        case CMD_INTERFACE_APPEND_TEXT:

            if (interface_data->message) {
                StringBuffer *sb;

                sb = stringbuffer_new();
                stringbuffer_append_string(sb, interface_data->message);
                packet_to_stringbuffer(data, len, &pos, sb);

                efree(interface_data->message);
                interface_data->message = stringbuffer_finish(sb);
            }

            break;

        case CMD_INTERFACE_ANIM:
        {
            interface_data->anim = object_create(NULL, 0, 0);
            interface_data->anim->animation_id = packet_to_uint16(data, len,
                    &pos);
            interface_data->anim->anim_speed = packet_to_uint8(data, len, &pos);
            interface_data->anim->direction = packet_to_uint8(data, len, &pos);
            interface_data->anim->last_anim = interface_data->anim->anim_speed;
            break;
        }

        case CMD_INTERFACE_OBJECT:
        {
            uint16_t flags = packet_to_uint16(data, len, &pos);
            tag_t tag = packet_to_uint32(data, len, &pos);
            object *old_obj = object_find(tag);
            object *obj = object_create(interface_data->objects, tag, 0);
            command_item_update(data, len, &pos, flags, obj);

            if (old_obj != NULL && old_obj->env != cpl.interface) {
                object_remove(obj);
            }

            break;
        }

        default:
            break;
        }
    }

    if (sb_message) {
        size_t links_len, char_shortcuts_len, i;

        links_len = utarray_len(interface_data->links);

        if (links_len) {
            stringbuffer_append_string(sb_message, "\n");
        }

        char_shortcuts_len = strlen(character_shortcuts);

        for (i = 0; i < links_len; i++) {
            stringbuffer_append_string(sb_message, "\n");

            if (i < char_shortcuts_len) {
                stringbuffer_append_printf(sb_message, "[c=#AF7817]&lsqb;%c&rsqb;[/c] ", character_shortcuts[i]);
            }

            stringbuffer_append_string(sb_message, *((char **) utarray_eltptr(interface_data->links, i)));
        }

        interface_data->message = stringbuffer_finish(sb_message);
    }

    if (!interface_data->message) {
        interface_data->message = estrdup("");
    }

    box.w = INTERFACE_TEXT_WIDTH;
    box.h = INTERFACE_TEXT_HEIGHT;
    text_show(NULL, interface_data->font, interface_data->message, INTERFACE_TEXT_STARTX, INTERFACE_TEXT_STARTY, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
    interface_data->num_lines = box.h;

    scrollbar_create(&interface_data->scrollbar, 11, 434, &interface_data->scroll_offset, &interface_data->num_lines, box.y);
    interface_data->scrollbar.redraw = &interface_popup->redraw;

    if (scroll_bottom) {
        interface_data->scroll_offset = interface_data->num_lines - box.y;
    }

    button_create(&button_hello);
    button_create(&button_close);

    button_hello.texture = button_close.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_large");
    button_hello.texture_over = button_close.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_large_over");
    button_hello.texture_pressed = button_close.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_large_down");
    button_set_font(&button_hello, FONT_ARIAL13);
    button_set_font(&button_close, FONT_ARIAL13);

    /* Destroy previous interface data. */
    if (interface_data != old_interface_data) {
        interface_destroy(old_interface_data);
    }

    cpl.interface = interface_data->objects;
}

/**
 * Redraw the interface.
 */
void interface_redraw(void)
{
    if (interface_popup) {
        interface_popup->redraw = 1;
    }
}

/**
 * Deinitialize the interface system.
 */
void interface_deinit(void)
{
    if (text_input_history != NULL) {
        text_input_history_free(text_input_history);
        text_input_destroy(&text_input);
    }
}

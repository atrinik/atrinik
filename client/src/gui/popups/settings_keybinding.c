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
 * Keybinding settings GUI.
 *
 * @author Alex Tokar */

#include <global.h>

enum {
    KEYBINDING_STATE_LIST,
    KEYBINDING_STATE_ADD,
    KEYBINDING_STATE_EDIT
} ;

/**
 * Button buffer. */
static button_struct button_new, button_remove, button_apply;
/**
 * Text input buffer. */
static text_input_struct text_input_command, text_input_key;
/**
 * What is being done in the keybinding GUI. */
static int keybinding_state;
/**
 * Keybinding ID that is being edited, if applicable. */
static size_t keybinding_id;
/**
 * The keybindings list. */
static list_struct *list_keybindings;

static void keybinding_list_reload(void)
{
    size_t i;
    char buf[MAX_BUF];

    /* Clear all the rows. */
    list_clear_rows(list_keybindings);

    for (i = 0; i < keybindings_num; i++) {
        list_add(list_keybindings, i, 0, keybindings[i]->command);
        list_add(list_keybindings, i, 1, keybind_get_key_shortcut(keybindings[i]->key, keybindings[i]->mod, buf, sizeof(buf)));
        list_add(list_keybindings, i, 2, keybindings[i]->repeat ? "on" : "off");
    }

    list_offsets_ensure(list_keybindings);
}

static void keybinding_apply(void)
{
    int key, mod;

    /* Nothing to apply. */
    if (*text_input_command.str == '\0' || sscanf(text_input_key.str, "%d %d", &key, &mod) != 2) {
        return;
    }

    if (keybinding_state == KEYBINDING_STATE_ADD) {
        keybind_add(key, mod, text_input_command.str);
        /* It'll be added to the end, so select it. */
        list_keybindings->row_selected = list_keybindings->rows + 1;
        list_keybindings->row_offset = MIN(list_keybindings->rows + 1 - list_keybindings->max_rows, list_keybindings->row_selected - 1);
    } else if (keybinding_state == KEYBINDING_STATE_EDIT) {
        keybind_edit(keybinding_id, key, mod, text_input_command.str);
    }

    keybinding_state = KEYBINDING_STATE_LIST;
    keybinding_list_reload();
}

/**
 * Try to handle keybinding action.
 * @param key The key to try to handle.
 * @return 1 if the key was handled, 0 otherwise. */
static int keybinding_action(SDLKey key)
{
    if (key == SDLK_n) {
        /* Create a new keybinding. */
        keybinding_state = KEYBINDING_STATE_ADD;
        text_input_command.focus = 1;
        text_input_set(&text_input_command, NULL);
        text_input_key.focus = 0;
        text_input_set(&text_input_key, "0 0");
        return 1;
    } else if (key == SDLK_DELETE) {
        /* Delete existing keybinding. */
        keybind_remove(list_keybindings->row_selected - 1);
        keybinding_list_reload();
        return 1;
    } else if (key == SDLK_r) {
        /* Toggle repeat on/off. */
        keybind_repeat_toggle(list_keybindings->row_selected - 1);
        keybinding_list_reload();
        return 1;
    }

    return 0;
}

/** @copydoc list_struct::handle_enter_func */
static void list_handle_enter(list_struct *list, SDL_Event *event)
{
    if (list->row_selected) {
        char buf[MAX_BUF];

        keybinding_action(SDLK_n);

        keybinding_state = KEYBINDING_STATE_EDIT;
        keybinding_id = list->row_selected - 1;

        text_input_command.focus = !EVENT_IS_KEY(event);
        text_input_set(&text_input_command, keybindings[keybinding_id]->command);
        snprintf(buf, sizeof(buf), "%d %d", keybindings[keybinding_id]->key, keybindings[keybinding_id]->mod);
        text_input_set(&text_input_key, buf);
    }
}

/** @copydoc text_input_struct::show_edit_func */
static void text_input_show_edit(text_input_struct *text_input)
{
    *text_input->str = '\0';
}

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
    SDL_Rect box;

    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface, FONT_SERIF20, "Keybinding Settings", 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    list_show(list_keybindings, 30, 50);
    list_set_parent(list_keybindings, popup->x, popup->y);

    button_set_parent(&button_new, popup->x, popup->y);
    button_set_parent(&button_remove, popup->x, popup->y);
    button_set_parent(&button_apply, popup->x, popup->y);

    button_new.x = 30;
    button_new.y = popup->surface->h - 72;
    button_show(&button_new, "Add");

    button_remove.x = 30;
    button_remove.y = popup->surface->h - 49;
    button_show(&button_remove, "Remove");

    if (keybinding_state == KEYBINDING_STATE_ADD || keybinding_state == KEYBINDING_STATE_EDIT) {
        int key, mod;

        text_show_shadow_format(popup->surface, FONT_ARIAL11, 100, popup->surface->h - 72, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP, NULL, "[hcenter=%d]Command: [/hcenter]", text_input_command.coords.h);
        text_show_shadow_format(popup->surface, FONT_ARIAL11, 100, popup->surface->h - 49, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP, NULL, "[hcenter=%d]Key: [/hcenter]", text_input_command.coords.h);
        text_show_shadow_format(popup->surface, FONT_ARIAL10, 160, text_input_key.coords.y + text_input_key.coords.h + 5, COLOR_WHITE, COLOR_BLACK, TEXT_MARKUP, NULL, "[hcenter=%d]Press ESC to cancel.[/hcenter]", button_apply.texture->surface->h);

        text_input_set_parent(&text_input_command, popup->x, popup->y);
        text_input_set_parent(&text_input_key, popup->x, popup->y);

        text_input_show(&text_input_command, popup->surface, 160, popup->surface->h - 72);
        text_input_show(&text_input_key, popup->surface, 160, popup->surface->h - 49);

        box.w = text_input_key.coords.w;
        box.h = text_input_key.coords.h;

        if (sscanf(text_input_key.str, "%d %d", &key, &mod) == 2 && key != SDLK_UNKNOWN) {
            char buf[MAX_BUF];

            keybind_get_key_shortcut(key, mod, buf, sizeof(buf));
            text_show(popup->surface, text_input_key.font, buf, text_input_key.coords.x, text_input_key.coords.y + TEXT_INPUT_PADDING, COLOR_WHITE, TEXT_ALIGN_CENTER, &box);
        } else if (text_input_key.focus) {
            text_show(popup->surface, text_input_key.font, "Press keyboard shortcut", text_input_key.coords.x, text_input_key.coords.y + TEXT_INPUT_PADDING, COLOR_WHITE, TEXT_ALIGN_CENTER, &box);
        }

        button_apply.x = text_input_key.coords.x + text_input_key.coords.w - texture_surface(button_apply.texture)->w;
        button_apply.y = text_input_key.coords.y + text_input_key.coords.h + 5;
        button_show(&button_apply, "Apply");
    }

    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    if (keybinding_state == KEYBINDING_STATE_ADD || keybinding_state == KEYBINDING_STATE_EDIT) {
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                keybinding_state = KEYBINDING_STATE_LIST;
                return 1;
            }

            if (text_input_command.focus) {
                text_input_event(&text_input_command, event);
            }

            return 1;
        } else if (event->type == SDL_KEYUP) {
            if (text_input_command.focus) {
                if (IS_NEXT(event->key.keysym.sym)) {
                    text_input_command.focus = 0;
                    text_input_key.focus = 1;

                    if (keybinding_state == KEYBINDING_STATE_EDIT) {
                        text_input_set(&text_input_key, "0 0");
                    }

                    return 1;
                }
            } else if (text_input_key.focus) {
                if (strcmp(text_input_key.str, "0 0") == 0) {
                    char buf[MAX_BUF];

                    snprintf(buf, sizeof(buf), "%d %d", event->key.keysym.sym, event->key.keysym.mod);
                    text_input_set(&text_input_key, buf);
                    return 1;
                } else if (IS_ENTER(event->key.keysym.sym)) {
                    keybinding_apply();
                    return 1;
                }
            } else if (IS_ENTER(event->key.keysym.sym)) {
                text_input_command.focus = 1;
                return 1;
            }
        }
    }

    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            popup_destroy(popup);
            return 1;
        } else if (keybinding_action(event->key.keysym.sym)) {
            return 1;
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            uint32_t row, col;

            if (text_input_mouse_over(&text_input_command, event->motion.x, event->motion.y)) {
                text_input_command.focus = 1;
                text_input_key.focus = 0;

                /* If we are editing and we switched from the key text
                 * input, restore the original value. */
                if (keybinding_state == KEYBINDING_STATE_EDIT && strcmp(text_input_key.str, "0 0") == 0) {
                    char buf[MAX_BUF];

                    snprintf(buf, sizeof(buf), "%d %d", keybindings[keybinding_id]->key, keybindings[keybinding_id]->mod);
                    text_input_set(&text_input_key, buf);
                }

                return 1;
            } else if (text_input_mouse_over(&text_input_key, event->motion.x, event->motion.y)) {
                text_input_set(&text_input_key, "0 0");
                text_input_key.focus = 1;
                text_input_command.focus = 0;
                return 1;
            } else if (list_mouse_get_pos(list_keybindings, event->motion.x, event->motion.y, &row, &col)) {
                if (col == 2) {
                    keybind_repeat_toggle(row);
                    keybinding_list_reload();
                }
            }
        }
    }

    if (button_event(&button_new, event)) {
        keybinding_action(SDLK_n);
        return 1;
    } else if (button_event(&button_remove, event)) {
        keybinding_action(SDLK_DELETE);
        return 1;
    } else if (button_event(&button_apply, event)) {
        keybinding_apply();
        return 1;
    } else if (list_handle_keyboard(list_keybindings, event)) {
        return 1;
    } else if (list_handle_mouse(list_keybindings, event)) {
        return 1;
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    list_remove(list_keybindings);
    list_keybindings = NULL;
    keybind_save();
    return 1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event(popup_button *button)
{
    help_show("keybinding settings");
    return 1;
}

void settings_keybinding_open(void)
{
    popup_struct *popup;

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;

    popup->button_left.event_func = popup_button_event;
    popup_button_set_text(&popup->button_left, "?");

    button_create(&button_new);
    button_create(&button_remove);
    button_create(&button_apply);

    button_new.surface = button_remove.surface = button_apply.surface = popup->surface;

    text_input_create(&text_input_command);
    text_input_create(&text_input_key);

    text_input_command.focus = text_input_key.focus = 0;
    text_input_key.show_edit_func = text_input_show_edit;

    list_keybindings = list_create(12, 3, 8);
    list_keybindings->handle_enter_func = list_handle_enter;
    list_keybindings->surface = popup->surface;
    list_keybindings->header_height = 7;
    list_set_font(list_keybindings, FONT_ARIAL11);
    list_set_column(list_keybindings, 0, 273, 7, "Command", -1);
    list_set_column(list_keybindings, 1, 93, 7, "Key", 1);
    list_set_column(list_keybindings, 2, 50, 7, "Repeat", 1);
    list_scrollbar_enable(list_keybindings);
    keybinding_list_reload();

    keybinding_state = KEYBINDING_STATE_LIST;
}

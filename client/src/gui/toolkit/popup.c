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
 * Popup API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Doubly-linked list of the visible popups. */
static popup_struct *popup_head = NULL;

/**
 * Create a new popup.
 * @param texture The texture to use.
 * @return The created popup. */
popup_struct *popup_create(texture_struct *texture)
{
    popup_struct *popup;
    int mx, my;

    popup = ecalloc(1, sizeof(popup_struct));
    popup->texture = texture;
    /* Create the surface used by the popup. */
    popup->surface = SDL_ConvertSurface(texture_surface(popup->texture), texture_surface(popup->texture)->format, texture_surface(popup->texture)->flags);
    DL_PREPEND(popup_head, popup);

    SDL_GetMouseState(&mx, &my);
    /* Make sure the mouse is no longer moving any widget. */
    widget_event_move_stop(mx, my);

    button_create(&popup->button_left.button);
    button_create(&popup->button_right.button);

    popup->button_left.button.texture = popup->button_right.button.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round_large");
    popup->button_left.button.texture_pressed = popup->button_right.button.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_large_down");
    popup->button_left.button.texture_over = popup->button_right.button.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_large_over");

    popup->button_left.x = 6;
    popup->button_left.y = 6;

    popup->button_right.x = popup->surface->w - texture_surface(popup->button_right.button.texture)->w - 6;
    popup->button_right.y = 6;
    popup->button_right.text = estrdup("X");

    popup->selection_start = popup->selection_end = -1;
    popup->redraw = 1;
    popup->modal = 1;

    return popup;
}

/**
 * Free the data used by a popup button.
 * @param button The button. */
static void popup_button_free(popup_button *button)
{
    if (button->text) {
        efree(button->text);
    }
}

/**
 * Destroy the visible popup, freeing it. */
void popup_destroy(popup_struct *popup)
{
    if (popup->destroy_callback_func && !popup->destroy_callback_func(popup)) {
        return;
    }

    DL_DELETE(popup_head, popup);
    SDL_FreeSurface(popup->surface);

    if (popup->buf) {
        efree(popup->buf);
    }

    if (popup->custom_data) {
        efree(popup->custom_data);
    }

    popup_button_free(&popup->button_right);
    popup_button_free(&popup->button_left);

    efree(popup);

    keybind_state_ensure();
}

/**
 * Destroy all visible popups. */
void popup_destroy_all(void)
{
    popup_struct *popup, *tmp;

    DL_FOREACH_SAFE(popup_head, popup, tmp)
    {
        popup_destroy(popup);
    }
}

/**
 * Render a single popup button.
 * @param popup Popup.
 * @param button The button to render. */
static void popup_button_show(popup_struct *popup, popup_button *button)
{
    if (button->button.texture) {
        button->button.x = popup->x + button->x;
        button->button.y = popup->y + button->y;
        button_show(&button->button, button->text ? button->text : "");
    }
}

/**
 * Render the specified popup.
 * @param popup The popup to render. */
void popup_render(popup_struct *popup)
{
    SDL_Rect box;

    if (!popup->disable_texture_drawing) {
        surface_show(popup->surface, 0, 0, NULL, texture_surface(popup->texture));
    }

    /* Calculate the popup's X/Y positions. */
    popup->x = ScreenSurface->w / 2 - popup->surface->w / 2;
    popup->y = ScreenSurface->h / 2 - popup->surface->h / 2;

    /* Handle drawing inside the popup. */
    if (popup->draw_func) {
        text_offset_set(popup->x, popup->y);

        if (!popup->draw_func(popup)) {
            text_offset_reset();
            popup_destroy(popup);
            return;
        }

        text_offset_reset();
    }

    /* Show the popup in the middle of the screen. */
    box.x = popup->x;
    box.y = popup->y;
    SDL_BlitSurface(popup->surface, NULL, ScreenSurface, &box);

    popup_button_show(popup, &popup->button_left);
    popup_button_show(popup, &popup->button_right);

    if (popup->draw_post_func) {
        if (!popup->draw_post_func(popup)) {
            popup_destroy(popup);
            return;
        }
    }
}

/**
 * Render the visible popups. */
void popup_render_all(void)
{
    popup_struct *popup, *tmp;

    DL_FOREACH_REVERSE_SAFE(popup_head, popup, tmp)
    {
        popup_render(popup);
    }
}

/**
 * Handle popup button event.
 * @param button The button.
 * @param event The event.
 * @retval 1 Handled the event.
 * @retval -1 Handled the event and the button was handled by callback
 * function.
 * @retval 0 Did not handle the event. */
static int popup_button_handle_event(popup_button *button, SDL_Event *event)
{
    if (button->text && button_event(&button->button, event)) {
        if (button->event_func && button->event_func(button)) {
            return -1;
        }

        return 1;
    }

    return 0;
}

/**
 * Handle mouse and keyboard events when a popup is active.
 * @param event Event.
 * @return 1 to disable any other mouse/keyboard actions, 0 otherwise. */
int popup_handle_event(SDL_Event *event)
{
    int ret;

    if (popup_head && !popup_head->modal && event->type == SDL_MOUSEBUTTONDOWN && !(event->motion.x >= popup_head->x && event->motion.x < popup_head->x + texture_surface(popup_head->texture)->w && event->motion.y >= popup_head->y && event->motion.y < popup_head->y + texture_surface(popup_head->texture)->h)) {
        if (popup_head->destroy_on_switch) {
            popup_destroy(popup_head);
            return 1;
        }
    }

    /* No popup is visible. */
    if (!popup_head) {
        return 0;
    }

    if (popup_head->clipboard_copy_func) {
        if (event->type == SDL_KEYDOWN && keybind_command_matches_event("?COPY", &event->key)) {
            const char *contents;
            int64_t start, end;
            char *str;

            contents = popup_head->clipboard_copy_func(popup_head);

            if (!contents) {
                return 1;
            }

            start = popup_head->selection_start;
            end = popup_head->selection_end;

            if (end < start) {
                start = popup_head->selection_end;
                end = popup_head->selection_start;
            }

            if (end - start <= 0) {
                return 1;
            }

            /* Get the string to copy, depending on the start and end positions.
             * */
            str = emalloc(sizeof(char) * (end - start + 1 + 1));
            memcpy(str, contents + start, end - start + 1);
            str[end - start + 1] = '\0';
            x11_clipboard_set(SDL_display, SDL_window, str);
            efree(str);

            return 1;
        } else if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEMOTION) && event->motion.x >= popup_head->x && event->motion.x < popup_head->x + texture_surface(popup_head->texture)->w && event->motion.y >= popup_head->y && event->motion.y < popup_head->y + texture_surface(popup_head->texture)->h) {
            if (event->type == SDL_MOUSEMOTION) {
                popup_head->redraw = 1;

                if (event->button.button == SDL_BUTTON_LEFT) {
                    popup_head->selection_started = 1;
                }
            } else if (event->button.button == SDL_BUTTON_LEFT) {
                if (event->type == SDL_MOUSEBUTTONUP) {
                    popup_head->selection_started = 0;
                } else if (event->type == SDL_MOUSEBUTTONDOWN) {
                    popup_head->selection_started = 0;
                    popup_head->selection_start = -1;
                    popup_head->selection_end = -1;
                    popup_head->redraw = 1;
                }
            }
        }
    }

    /* Handle custom events? */
    if (popup_head->event_func) {
        ret = popup_head->event_func(popup_head, event);

        if (ret != -1) {
            return ret;
        }
    }

    /* Key is down. */
    if (event->type == SDL_KEYDOWN) {
        /* Escape, destroy the popup. */
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            popup_destroy(popup_head);
        }
    } else if ((ret = popup_button_handle_event(&popup_head->button_left, event))) {
        return 1;
    } else if ((ret = popup_button_handle_event(&popup_head->button_right, event))) {
        if (ret == 1) {
            popup_destroy(popup_head);
        }

        return 1;
    }

    return 1;
}

/**
 * Get the currently visible popup.
 * @return The visible popup, or NULL if there isn't any. */
popup_struct *popup_get_head(void)
{
    return popup_head;
}

/**
 * Set the text of a generic popup button.
 * @param button The button.
 * @param text Text to set. */
void popup_button_set_text(popup_button *button, const char *text)
{
    if (button->text) {
        efree(button->text);
    }

    button->text = text ? estrdup(text) : NULL;
}

/**
 * Check whether any popup needs redrawing.
 * @return 1 if any popup needs redrawing, 0 otherwise.
 * @todo Actual redrawing logic in popups.
 */
int popup_need_redraw(void)
{
    return popup_get_head() != NULL;
}

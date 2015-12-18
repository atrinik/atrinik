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
 * Button API.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Initialize the button API. */
void button_init()
{
}

/**
 * Determine button's texture, based on its texture settings and whether
 * it is currently pressed, or mouse is over it.
 * @param button Button.
 * @return Texture to use. */
static texture_struct *button_determine_texture(button_struct *button)
{
    if (button->pressed && button->texture_pressed) {
        return button->texture_pressed;
    } else if (button->mouse_over && button->texture_over) {
        return button->texture_over;
    } else {
        return button->texture;
    }
}

/**
 * Initialize a button's default values.
 * @param button Button. */
void button_create(button_struct *button)
{
    memset(button, 0, sizeof(*button));

    /* Initialize default values. */
    button->surface = ScreenSurface;
    button->x = button->y = 0;
    button->texture = texture_get(TEXTURE_TYPE_CLIENT, "button");
    button->texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_down");
    button->texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_over");
    button->flags = 0;
    button->center = 1;
    button->color = COLOR_WHITE;
    button->color_shadow = COLOR_BLACK;
    button->color_over = COLOR_HGOLD;
    button->color_over_shadow = COLOR_BLACK;
    button->mouse_over = button->pressed = button->pressed_forced = button->disabled = 0;
    button->pressed_ticks = button->hover_ticks = button->pressed_repeat_ticks = 0;
    button->repeat_func = NULL;

    button_set_font(button, FONT_ARIAL10);
}

/**
 * Destroy data associated with the specified button. The button structure
 * itself is not freed.
 * @param button Button to destroy.
 */
void button_destroy(button_struct *button)
{
    if (button->font) {
        font_free(button->font);
    }
}

void button_set_parent(button_struct *button, int px, int py)
{
    button->px = px;
    button->py = py;
}

/**
 * Set font of the specified button.
 * @param button Button.
 * @param font Font to set.
 */
void button_set_font(button_struct *button, font_struct *font)
{
    if (button->font) {
        font_free(button->font);
    }

    if (font) {
        FONT_INCREF(font);
    }

    button->font = font;
}

int button_need_redraw(button_struct *button)
{
    int ret;

    ret = 0;

    if (button->mouse_over || button->pressed) {
        int state, mx, my, mover;

        state = SDL_GetMouseState(&mx, &my);
        mover = BUTTON_MOUSE_OVER(button, mx, my, texture_surface(button->texture));

        if (button->mouse_over && !mover) {
            button->mouse_over = 0;
            ret = 1;
        }

        if (button->pressed && !button->pressed_forced &&
                (!mover || state != SDL_BUTTON_LEFT)) {
            button->pressed = 0;
            ret = 1;
        }
    }

    return ret;
}

/**
 * Render a button.
 * @param button Button to render.
 * @param text Optional text to render. */
void button_show(button_struct *button, const char *text)
{
    SDL_Surface *texture;

    (void) button_need_redraw(button);

    if (button->pressed_forced) {
        button->pressed = 1;
    }

    texture = texture_surface(button_determine_texture(button));
    surface_show(button->surface, button->x, button->y, NULL, texture);

    if (text) {
        const char *color, *color_shadow;
        int x, y;

        if (button->mouse_over) {
            color = button->color_over;
            color_shadow = button->color_over_shadow;
        } else {
            color = button->color;
            color_shadow = button->color_shadow;
        }

        x = button->x;
        y = button->y;

        if (button->center) {
            x += texture->w / 2 - text_get_width(button->font, text, button->flags) / 2;
            y += texture->h / 2 - FONT_HEIGHT(button->font) / 2;
        }

        if (!color_shadow) {
            text_show(button->surface, button->font, text, x, y, color, button->flags, NULL);
        } else {
            text_show_shadow(button->surface, button->font, text, x, y - 2, color, color_shadow, button->flags, NULL);
        }
    }

    if (button->repeat_func && button->pressed && SDL_GetTicks() - button->pressed_ticks > button->pressed_repeat_ticks) {
        button->repeat_func(button);
        button->pressed_ticks = SDL_GetTicks();
        button->pressed_repeat_ticks = 150;
    }

    button->redraw = 0;
}

/**
 * Handle SDL event for a button.
 * @param button Button to handle.
 * @param event The event.
 * @return 1 if the event makes the button pressed, 0 otherwise. */
int button_event(button_struct *button, SDL_Event *event)
{
    SDL_Surface *texture;
    int old_mouse_over;

    if (event->type != SDL_MOUSEBUTTONUP && event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEMOTION) {
        return 0;
    }

    /* Mouse button is released, the button is no longer being pressed. */
    if (event->type == SDL_MOUSEBUTTONUP) {
        button->pressed = 0;
        button->redraw = 1;
        return 0;
    }

    old_mouse_over = button->mouse_over;
    /* Always reset this. */
    button->mouse_over = 0;

    /* The button is disabled, we don't care about the mouse. */
    if (button->disabled) {
        return 0;
    }

    texture = texture_surface(button_determine_texture(button));

    if (BUTTON_MOUSE_OVER(button, event->motion.x, event->motion.y, texture)) {
        if (event->type == SDL_MOUSEMOTION) {
            cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_pointer");
        }

        /* Left mouse click, the button has been pressed. */
        if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
            button->pressed = 1;
            button->pressed_ticks = SDL_GetTicks();
            button->pressed_repeat_ticks = 750;
            button->redraw = 1;
            return 1;
        } else {
            button->mouse_over = 1;

            /* Do not reset hover ticks if the previous state was already
             * in highlight mode. */
            if (!old_mouse_over) {
                button->hover_ticks = SDL_GetTicks();
            }
        }
    }

    if (old_mouse_over != button->mouse_over) {
        button->redraw = 1;
    }

    return 0;
}

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
 * Scrollbar API.
 *
 * @author Alex Tokar */

#include <global.h>

/** Scrollbar background color. */
static SDL_Color scrollbar_color_bg;
/** Scrollbar foreground color. */
static SDL_Color scrollbar_color_fg;
/** Highlight color. */
static SDL_Color scrollbar_color_highlight;

/**
 * Initialize the scrollbar API. */
void scrollbar_init()
{
    text_color_parse("000000", &scrollbar_color_bg);
    text_color_parse("b5a584", &scrollbar_color_fg);
    text_color_parse("ffffff", &scrollbar_color_highlight);
}

/**
 * Initialize a single scrollbar element.
 * @param elem Element to initialize.
 * @param x X position.
 * @param y Y position.
 * @param w Width of the element.
 * @param h Height of the element. */
static void scrollbar_element_init(scrollbar_element *elem, int x, int y, int w, int h)
{
    elem->x = x;
    elem->y = y;
    elem->w = w;
    elem->h = h;
}

/** @copydoc scrollbar_element::render_func */
static void scrollbar_element_render_background(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem, uint8 horizontal)
{
    (void) elem;
    (void) horizontal;
    SDL_FillRect(surface, box, SDL_MapRGB(surface->format, scrollbar_color_bg.r, scrollbar_color_bg.g, scrollbar_color_bg.b));
    border_create_sdl_color(surface, box, 1, &scrollbar_color_fg);
}

/** @copydoc scrollbar_element::render_func */
static void scrollbar_element_render_arrow_up(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem, uint8 horizontal)
{
    SDL_Color *color = &scrollbar_color_fg;

    /* If highlighted, use the highlight color. */
    if (elem->highlight) {
        color = &scrollbar_color_highlight;
    }

    border_create_sdl_color(surface, box, 1, color);

    /* Create the arrow. */
    if (horizontal) {
        lineRGBA(surface, box->x + 2, box->y + box->h / 2, box->x + box->w - 3, box->y + box->h / 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + 2, box->y + box->h / 2, box->x + box->w / 2, box->y + 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + 2, box->y + box->h / 2, box->x + box->w / 2, box->y + box->h - 2 - 1, color->r, color->g, color->b, 255);
    } else {
        lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + box->w / 2, box->y + box->h - 3, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + 2, box->y + box->h / 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + box->w - 2 - 1, box->y + box->h / 2, color->r, color->g, color->b, 255);
    }
}

/** @copydoc scrollbar_element::render_func */
static void scrollbar_element_render_arrow_down(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem, uint8 horizontal)
{
    SDL_Color *color = &scrollbar_color_fg;

    /* If highlighted, use the highlight color. */
    if (elem->highlight) {
        color = &scrollbar_color_highlight;
    }

    border_create_sdl_color(surface, box, 1, color);

    /* Create the arrow. */
    if (horizontal) {
        lineRGBA(surface, box->x + box->w - 3, box->y + box->h / 2, box->x + 2, box->y + box->h / 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w - 3, box->y + box->h / 2, box->x + box->w / 2, box->y + 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w - 3, box->y + box->h / 2, box->x + box->w / 2, box->y + box->h - 3, color->r, color->g, color->b, 255);
    } else {
        lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + box->w / 2, box->y + 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + 2, box->y + box->h / 2, color->r, color->g, color->b, 255);
        lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + box->w - 2 - 1, box->y + box->h / 2, color->r, color->g, color->b, 255);
    }
}

/** @copydoc scrollbar_element::render_func */
static void scrollbar_element_render_slider(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem, uint8 horizontal)
{
    (void) horizontal;

    SDL_FillRect(surface, box, SDL_MapRGB(surface->format, scrollbar_color_fg.r, scrollbar_color_fg.g, scrollbar_color_fg.b));

    /* If highlighted, create highlighted border around the edges of the
     * slider. */
    if (elem->highlight) {
        border_create_sdl_color(surface, box, 1, &scrollbar_color_highlight);
    }
}

/**
 * Check if scrollbar element should be highlighted.
 * @param scrollbar The scrollbar.
 * @param elem The element.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @return 1 if the element is highlighted, 0 otherwise. */
static int scrollbar_element_highlight_check(scrollbar_struct *scrollbar, scrollbar_element *elem, int mx, int my)
{
    SDL_Rect box;

    box.x = scrollbar->x + elem->x;
    box.y = scrollbar->y + elem->y;
    box.w = elem->w;
    box.h = elem->h;

    if (mx >= box.x && mx < box.x + box.w && my >= box.y && my < box.y + box.h) {
        elem->highlight = 1;
    } else {
        elem->highlight = 0;
    }

    return elem->highlight;
}

/**
 * Render a single scrollbar element.
 * @param scrollbar The scrollbar.
 * @param elem The element.
 * @param surface The surface to draw on. */
static void scrollbar_element_render(scrollbar_struct *scrollbar, scrollbar_element *elem, SDL_Surface *surface)
{
    SDL_Rect box;

    box.x = scrollbar->x + elem->x;
    box.y = scrollbar->y + elem->y;
    box.w = elem->w;
    box.h = elem->h;

    /* Make sure the element should still be highlighted. */
    if (elem->highlight) {
        int mx, my;

        SDL_GetMouseState(&mx, &my);

        mx -= scrollbar->px;
        my -= scrollbar->py;

        scrollbar_element_highlight_check(scrollbar, elem, mx, my);
    }

    elem->render_func(surface, &box, elem, scrollbar->background.w > scrollbar->background.h);
}

/**
 * Handle clicking an element in scrollbar.
 * @param scrollbar The scrollbar.
 * @param test If 1, only test whether anything can be clicked.
 * @return 1 if the click was handled, 0 otherwise. */
static int scrollbar_click_scroll(scrollbar_struct *scrollbar, int test)
{
    /* Dragging the slider, do not allow clicking anything. */
    if (scrollbar->dragging) {
        return 0;
    }

    if (scrollbar->arrow_up.highlight) {
        /* Mouse over the up arrow. */
        if (!test) {
            scrollbar_scroll_adjust(scrollbar, -scrollbar->arrow_adjust);
        }

        return 1;
    } else if (scrollbar->arrow_down.highlight) {
        /* Mouse over the down arrow. */
        if (!test) {
            scrollbar_scroll_adjust(scrollbar, scrollbar->arrow_adjust);
        }

        return 1;
    } else if (scrollbar->background.highlight && scrollbar->scroll_direction != SCROLL_DIRECTION_NONE) {
        /* Mouse over the background and there's a known scroll direction. */

        if (scrollbar->scroll_direction == SCROLL_DIRECTION_UP) {
            if (!test) {
                scrollbar_scroll_adjust(scrollbar, -scrollbar->max_lines);
            }

            return 1;
        } else if (scrollbar->scroll_direction == SCROLL_DIRECTION_DOWN) {
            if (!test) {
                scrollbar_scroll_adjust(scrollbar, scrollbar->max_lines);
            }

            return 1;
        }
    }

    return 0;
}

/**
 * Check whether scrollbar needs redrawing.
 * @param scrollbar Scrollbar to check.
 * @return 1 if the scrollbar needs redrawing, 0 otherwise. */
int scrollbar_need_redraw(scrollbar_struct *scrollbar)
{
    if (scrollbar_click_scroll(scrollbar, 1) && SDL_GetMouseState(NULL, NULL) == SDL_BUTTON_LEFT) {
        return 1;
    }

    return 0;
}

/**
 * Calculate scrollbar's slider X position.
 * @param scrollbar The scrollbar.
 * @return Slider's X position. */
static int scrollbar_slider_startx(scrollbar_struct *scrollbar)
{
    if (scrollbar->background.w > scrollbar->background.h) {
        return scrollbar->background.h + 1;
    } else {
        return 2;
    }
}

/**
 * Calculate scrollbar's slider Y position.
 * @param scrollbar The scrollbar.
 * @return Slider's Y position. */
static int scrollbar_slider_starty(scrollbar_struct *scrollbar)
{
    if (scrollbar->background.w > scrollbar->background.h) {
        return 2;
    } else {
        return scrollbar->background.w + 1;
    }
}

/**
 * Calculate scrollbar's slider width.
 * @param scrollbar The scrollbar.
 * @return Slider's width. */
static int scrollbar_slider_width(scrollbar_struct *scrollbar)
{
    if (scrollbar->background.w > scrollbar->background.h) {
        return scrollbar->background.w - (scrollbar->background.h + 1) * 2;
    } else {
        return scrollbar->background.w - 2 * 2;
    }
}

/**
 * Calculate scrollbar's slider height.
 * @param scrollbar The scrollbar.
 * @return Slider's height. */
static int scrollbar_slider_height(scrollbar_struct *scrollbar)
{
    if (scrollbar->background.w > scrollbar->background.h) {
        return scrollbar->background.h - 2 * 2;
    } else {
        return scrollbar->background.h - (scrollbar->background.w + 1) * 2;
    }
}

/**
 * Initialize a single scrollbar structure.
 * @param scrollbar Structure to initialize.
 * @param w Width of the scrollbar. Should be an odd number, otherwise
 * the arrow calculations will not work correctly.
 * @param h Height of the scrollbar. */
void scrollbar_create(scrollbar_struct *scrollbar, int w, int h, uint32 *scroll_offset, uint32 *num_lines, uint32 max_lines)
{
    memset(scrollbar, 0, sizeof(*scrollbar));

    scrollbar->scroll_offset = scroll_offset;
    scrollbar->num_lines = num_lines;
    scrollbar->max_lines = max_lines;
    scrollbar->arrow_adjust = 1;

    /* Initialize the elements. */
    scrollbar_element_init(&scrollbar->background, 0, 0, w, h);
    scrollbar_element_init(&scrollbar->slider, scrollbar_slider_startx(scrollbar), scrollbar_slider_starty(scrollbar), scrollbar_slider_width(scrollbar), scrollbar_slider_height(scrollbar));

    if (w > h) {
        scrollbar_element_init(&scrollbar->arrow_up, 0, 0, h, h);
        scrollbar_element_init(&scrollbar->arrow_down, w - h, 0, h, h);
    } else {
        scrollbar_element_init(&scrollbar->arrow_up, 0, 0, w, w);
        scrollbar_element_init(&scrollbar->arrow_down, 0, h - w, w, w);
    }

    scrollbar->background.render_func = scrollbar_element_render_background;
    scrollbar->arrow_up.render_func = scrollbar_element_render_arrow_up;
    scrollbar->arrow_down.render_func = scrollbar_element_render_arrow_down;
    scrollbar->slider.render_func = scrollbar_element_render_slider;
}

/**
 * Initialize scrollbar information structure.
 * @param info The structure to initialize. */
void scrollbar_info_create(scrollbar_info_struct *info)
{
    memset(info, 0, sizeof(*info));
}

/**
 * Scroll the specified scrollbar to the specified offset.
 *
 * If the scroll offset has changed at all, value of the
 * scrollbar_struct::redraw pointer will be set to 1.
 * @param scrollbar The scrollbar.
 * @param scroll Offset to scroll to. */
void scrollbar_scroll_to(scrollbar_struct *scrollbar, int scroll)
{
    /* Make sure the scroll offset is in a valid range. */
    if (scroll < SCROLL_TOP(scrollbar)) {
        scroll = SCROLL_TOP(scrollbar);
    } else if ((uint32) scroll > SCROLL_BOTTOM(scrollbar)) {
        scroll = SCROLL_BOTTOM(scrollbar);
    }

    /* If the scroll offset changed, update it and set the redraw flag,
     * if possible. */
    if ((uint32) scroll != *scrollbar->scroll_offset) {
        *scrollbar->scroll_offset = scroll;

        if (scrollbar->redraw) {
            *scrollbar->redraw = 1;
        }
    }
}

/**
 * Scroll the scrollbar by the specified amount.
 *
 * If the scroll offset has changed at all, value of the
 * scrollbar_struct::redraw pointer will be set to 1.
 * @param scrollbar The scrollbar to scroll.
 * @param adjust How much to scroll by. */
void scrollbar_scroll_adjust(scrollbar_struct *scrollbar, int adjust)
{
    scrollbar_scroll_to(scrollbar, *scrollbar->scroll_offset + adjust);
}

/**
 * Render a scrollbar.
 * @param scrollbar The scrollbar to render.
 * @param surface Surface to render on.
 * @param x X position on the surface.
 * @param y Y position on the surface. */
void scrollbar_show(scrollbar_struct *scrollbar, SDL_Surface *surface, int x, int y)
{
    int horizontal;

    scrollbar->x = x;
    scrollbar->y = y;

    horizontal = scrollbar->background.w > scrollbar->background.h;

    /* If the scroll direction is set but the left mouse button is no
     * longer being held, clear the scroll direction. */
    if (scrollbar->scroll_direction != SCROLL_DIRECTION_NONE && SDL_GetMouseState(NULL, NULL) != SDL_BUTTON_LEFT) {
        scrollbar->scroll_direction = SCROLL_DIRECTION_NONE;
    }

    /* Handle click repeating. */
    if (SDL_GetMouseState(NULL, NULL) == SDL_BUTTON_LEFT && SDL_GetTicks() - scrollbar->click_ticks > scrollbar->click_repeat_ticks) {
        if (scrollbar_click_scroll(scrollbar, 0)) {
            scrollbar->click_ticks = SDL_GetTicks();
            scrollbar->click_repeat_ticks = 35;
        }
    }

    /* Calculate slider height and y offset if necessary. */
    if (*scrollbar->num_lines > scrollbar->max_lines) {
        int scroll;

        scroll = scrollbar->max_lines + *scrollbar->scroll_offset;

        if (horizontal) {
            scrollbar->slider.w = scrollbar_slider_width(scrollbar) * scrollbar->max_lines / *scrollbar->num_lines;
            scrollbar->slider.x = ((scroll - scrollbar->max_lines) * scrollbar_slider_width(scrollbar)) / *scrollbar->num_lines;

            if (scrollbar->slider.w < 1) {
                scrollbar->slider.w = 1;
            }

            if (scroll - scrollbar->max_lines > 0 && scrollbar->slider.x + scrollbar->slider.w < scrollbar_slider_width(scrollbar)) {
                scrollbar->slider.x++;
            }
        } else {
            scrollbar->slider.h = scrollbar_slider_height(scrollbar) * scrollbar->max_lines / *scrollbar->num_lines;
            scrollbar->slider.y = ((scroll - scrollbar->max_lines) * scrollbar_slider_height(scrollbar)) / *scrollbar->num_lines;

            if (scrollbar->slider.h < 1) {
                scrollbar->slider.h = 1;
            }

            if (scroll - scrollbar->max_lines > 0 && scrollbar->slider.y + scrollbar->slider.h < scrollbar_slider_height(scrollbar)) {
                scrollbar->slider.y++;
            }
        }
    } else {
        /* Not necessary to calculate, so full slider height. */

        if (horizontal) {
            scrollbar->slider.w = scrollbar_slider_width(scrollbar);
            scrollbar->slider.x = 0;
        } else {
            scrollbar->slider.h = scrollbar_slider_height(scrollbar);
            scrollbar->slider.y = 0;
        }
    }

    if (horizontal) {
        scrollbar->slider.x += scrollbar_slider_startx(scrollbar);
    } else {
        scrollbar->slider.y += scrollbar_slider_starty(scrollbar);
    }

    /* Render the elements. */
    scrollbar_element_render(scrollbar, &scrollbar->background, surface);
    scrollbar_element_render(scrollbar, &scrollbar->arrow_up, surface);
    scrollbar_element_render(scrollbar, &scrollbar->arrow_down, surface);
    scrollbar_element_render(scrollbar, &scrollbar->slider, surface);
}

/**
 * Try to handle a scrollbar event.
 * @param scrollbar Scrollbar to handle the event for.
 * @param event The event.
 * @return 1 if the event was handled, 0 otherwise. */
int scrollbar_event(scrollbar_struct *scrollbar, SDL_Event *event)
{
    if (event->type == SDL_MOUSEMOTION) {
        /* If dragging but the left mouse button is no longer being held,
         * quit dragging the slider. */
        if (scrollbar->dragging && !(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LEFT)) {
            scrollbar->dragging = 0;
        }

        /* Try dragging the scrollbar. */
        if (scrollbar->dragging) {
            int slider_pos;
            uint32 scroll_offset;

            if (scrollbar->background.w > scrollbar->background.h) {
                slider_pos = event->motion.x - scrollbar->px - scrollbar->old_slider_pos;

                if (slider_pos > scrollbar_slider_width(scrollbar) - scrollbar->slider.w) {
                    slider_pos = scrollbar_slider_width(scrollbar) - scrollbar->slider.w;
                } else if (slider_pos < 0) {
                    slider_pos = 0;
                }

                scroll_offset = MIN(*scrollbar->num_lines - scrollbar->max_lines, MAX(0, slider_pos) * *scrollbar->num_lines / scrollbar_slider_width(scrollbar));
            } else {
                slider_pos = event->motion.y - scrollbar->py - scrollbar->old_slider_pos;

                if (slider_pos > scrollbar_slider_height(scrollbar) - scrollbar->slider.h) {
                    slider_pos = scrollbar_slider_height(scrollbar) - scrollbar->slider.h;
                } else if (slider_pos < 0) {
                    slider_pos = 0;
                }

                scroll_offset = MIN(*scrollbar->num_lines - scrollbar->max_lines, MAX(0, slider_pos) * *scrollbar->num_lines / scrollbar_slider_height(scrollbar));
            }

            /* Redraw if the scroll offset changed. */
            if (scroll_offset != *scrollbar->scroll_offset) {
                *scrollbar->scroll_offset = scroll_offset;

                if (scrollbar->redraw) {
                    *scrollbar->redraw = 1;
                }
            }

            return 1;
        } else if (scrollbar_element_highlight_check(scrollbar, &scrollbar->arrow_up, event->motion.x - scrollbar->px, event->motion.y - scrollbar->py)) {
            return 1;
        } else if (scrollbar_element_highlight_check(scrollbar, &scrollbar->arrow_down, event->motion.x - scrollbar->px, event->motion.y - scrollbar->py)) {
            return 1;
        } else if (scrollbar_element_highlight_check(scrollbar, &scrollbar->slider, event->motion.x - scrollbar->px, event->motion.y - scrollbar->py)) {
            return 1;
        } else if (scrollbar_element_highlight_check(scrollbar, &scrollbar->background, event->motion.x - scrollbar->px, event->motion.y - scrollbar->py)) {
            return 1;
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        /* Start dragging the slider. */
        if (scrollbar->slider.highlight) {
            if (scrollbar->background.w > scrollbar->background.h) {
                scrollbar->old_slider_pos = event->motion.x - scrollbar->px - scrollbar->slider.x + scrollbar_slider_startx(scrollbar);
            } else {
                scrollbar->old_slider_pos = event->motion.y - scrollbar->py - scrollbar->slider.y + scrollbar_slider_starty(scrollbar);
            }

            scrollbar->dragging = 1;
            return 1;
        } else if (scrollbar->background.highlight) {
            /* Set scroll direction if clicked on the background. */

            if (scrollbar->background.w > scrollbar->background.h) {
                if (event->motion.x - scrollbar->px < scrollbar->x + scrollbar->slider.x) {
                    scrollbar->scroll_direction = SCROLL_DIRECTION_UP;
                } else if (event->motion.x - scrollbar->px > scrollbar->x + scrollbar->slider.x + scrollbar->slider.w) {
                    scrollbar->scroll_direction = SCROLL_DIRECTION_DOWN;
                }
            } else {
                if (event->motion.y - scrollbar->py < scrollbar->y + scrollbar->slider.y) {
                    scrollbar->scroll_direction = SCROLL_DIRECTION_UP;
                } else if (event->motion.y - scrollbar->py > scrollbar->y + scrollbar->slider.y + scrollbar->slider.h) {
                    scrollbar->scroll_direction = SCROLL_DIRECTION_DOWN;
                }
            }
        }

        if (scrollbar_click_scroll(scrollbar, 0)) {
            scrollbar->click_ticks = SDL_GetTicks();
            scrollbar->click_repeat_ticks = 400;
            return 1;
        }
    }

    return 0;
}

/**
 * Get the scrollbar's width.
 * @param scrollbar The scrollbar to get width of.
 * @return The width. */
int scrollbar_get_width(scrollbar_struct *scrollbar)
{
    return scrollbar->background.w;
}

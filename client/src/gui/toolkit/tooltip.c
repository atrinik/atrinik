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
 * Tooltip API.
 *
 * @author Alex Tokar */

#include <global.h>

/** Tooltip's text. */
static char tooltip_text[HUGE_BUF * 4];
/** Font of the tooltip text. */
static font_struct *tooltip_font;
/** Tooltip's X position. */
static int tooltip_x = -1;
/** Tooltip's Y position. */
static int tooltip_y = -1;
static int tooltip_h = -1;
static int tooltip_w = -1;
static uint32_t tooltip_created = 0;
static uint32_t tooltip_delay = 0;
static uint8_t tooltip_opacity = 0;

/**
 * Creates a new tooltip. This must be called every frame in order for
 * the tooltip to remain shown.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param font Font to use, one of @ref FONT_xxx.
 * @param text The text to show in the tooltip. */
void tooltip_create(int mx, int my, font_struct *font, const char *text)
{
    if (tooltip_font != NULL) {
        font_free(tooltip_font);
    }

    tooltip_delay = 0;
    FONT_INCREF(font);
    tooltip_font = font;
    tooltip_x = mx;
    tooltip_y = my;
    tooltip_w = -1;
    tooltip_h = -1;
    strncpy(tooltip_text, text, sizeof(tooltip_text) - 1);
    tooltip_text[sizeof(tooltip_text) - 1] = '\0';
}

/**
 * Adds a delay before the tooltip is shown.
 * @param delay Delay in milliseconds. */
void tooltip_enable_delay(uint32_t delay)
{
    tooltip_delay = delay;

    if (tooltip_created + delay < SDL_GetTicks()) {
        tooltip_created = SDL_GetTicks();
    }
}

/**
 * Calculate multi-line tooltip height and width.
 * @param max_width Maximum width of the tooltip. */
void tooltip_multiline(int max_width)
{
    SDL_Rect box;

    box.x = 0;
    box.y = 0;
    box.w = max_width;
    box.h = 0;
    text_show(NULL, tooltip_font, tooltip_text, 3, 0, COLOR_WHITE, TEXT_MARKUP | TEXT_WORD_WRAP | TEXT_HEIGHT, &box);
    tooltip_w = max_width;
    tooltip_h = box.h;

    box.h = 0;
    text_show(NULL, tooltip_font, tooltip_text, 3, 0, COLOR_WHITE, TEXT_MARKUP | TEXT_WORD_WRAP | TEXT_MAX_WIDTH, &box);
    tooltip_w = box.w;
}

/**
 * Actually show the tooltip. */
void tooltip_show(void)
{
    SDL_Rect box, text_box;

    /* No tooltip to show. */
    if (tooltip_x == -1 || tooltip_y == -1) {
        return;
    }

    if (tooltip_delay) {
        if (SDL_GetTicks() - tooltip_created < tooltip_delay) {
            tooltip_opacity = 0;
            return;
        }

        tooltip_created = SDL_GetTicks() + tooltip_delay;
    } else {
        tooltip_opacity = 255;
    }

    text_box.w = tooltip_w;
    text_box.h = tooltip_h;

    if (tooltip_w == -1 || tooltip_h == -1) {
        text_get_width_height(tooltip_font,
                              tooltip_text,
                              TEXT_MARKUP,
                              &text_box,
                              tooltip_w == -1 ? &text_box.w : NULL,
                              tooltip_h == -1 ? &text_box.h : NULL);
    }

    /* Generate the tooltip's background. */
    box.x = tooltip_x + 9;
    box.y = tooltip_y + 17;
    box.w = text_box.w + 4;
    box.h = text_box.h;

    /* Push the tooltip to the left if it would go beyond maximum screen
     * size. */
    if (box.x + box.w >= ScreenSurface->w) {
        box.x -= (box.x + box.w + 1) - ScreenSurface->w;
    }

    if (box.y + box.h >= ScreenSurface->h) {
        box.y -= (box.y + box.h + 1) - ScreenSurface->h;
    }

    boxRGBA(ScreenSurface, box.x, box.y, box.x + box.w, box.y + box.h, 255, 255, 255, tooltip_opacity);
    text_show_format(ScreenSurface, tooltip_font, box.x + 3, box.y, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP, &text_box, "[alpha=%d]%s[/alpha]", tooltip_opacity, tooltip_text);

    if (tooltip_delay) {
        tooltip_opacity = MIN(255, tooltip_opacity + 25);
    }
}

/**
 * Dismiss the currently shown tooltip. */
void tooltip_dismiss(void)
{
    tooltip_x = -1;
    tooltip_y = -1;
    tooltip_w = -1;
    tooltip_h = -1;

    if (tooltip_font != NULL) {
        font_free(tooltip_font);
        tooltip_font = NULL;
    }
}

/**
 * Check whether the tooltip needs redrawing.
 * @return 1 if the tooltip needs redrawing, 0 otherwise.
 * @todo This needs some actual redrawing check logic. Need various checks like
 * whether the x/y is the same, text/font is the same (in other words, reset
 * old x/y when creating if text/font has changed)
 */
int tooltip_need_redraw(void)
{
    if (tooltip_x == -1 || tooltip_y == -1) {
        return 0;
    }

    return 1;
}

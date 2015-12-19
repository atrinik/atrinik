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
 * Implements the game news popup.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>

/**
 * Maximum width of the text.
 */
#define NEWS_MAX_WIDTH 455
/**
 * Maximum height of the text.
 */
#define NEWS_MAX_HEIGHT 250
/**
 * Font of the text.
 */
#define NEWS_FONT FONT_SANS12

/**
 * Structure that holds the game news popup data.
 */
typedef struct game_news {
    /**
     * Title of the game news entry to read about.

 */
    char *title;

    /**
     * cURL data for downloading the game news message.

 */
    curl_data *data;

    /**
     * cURL data state.

 */
    curl_state_t state;

    /**
     * The downloaded message; NULL while still downloading.

 */
    char *msg;

    /**
     * Scrollbar buffer.

 */
    scrollbar_struct scrollbar;

    /**
     * Scroll offset.

 */
    uint32_t scroll_offset;

    /**
     * Number of lines.

 */
    uint32_t num_lines;
} game_news_t;

/** @copydoc popup_struct::draw_func */
static int
popup_draw (popup_struct *popup)
{
    game_news_t *game_news = popup->custom_data;

    curl_state_t state = curl_download_get_state(game_news->data);
    if (game_news->state != state) {
        game_news->state = state;
        popup->redraw = 1;
    }

    if (!popup->redraw) {
        return 1;
    }

    popup->redraw = 0;
    surface_show(popup->surface, 0, 0, NULL, texture_surface(popup->texture));

    SDL_Rect box;
    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface, FONT_SERIF16, game_news->title, 0, 0,
              COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    box.w = popup->surface->w;
    box.h = popup->surface->h - box.h;

    if (state == CURL_STATE_ERROR) {
        text_show(popup->surface, FONT_SERIF12, "Connection timed out.", 0, 0,
                  COLOR_WHITE, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
        return 1;
    } else if (state == CURL_STATE_DOWNLOAD) {
        text_show(popup->surface, FONT_SERIF12,
                  "Downloading news, please wait...", 0, 0, COLOR_WHITE,
                  TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
        return 1;
    } else if (state == CURL_STATE_OK) {
        if (game_news->msg == NULL) {
            if (game_news->data->memory != NULL) {
                game_news->msg = estrdup(game_news->data->memory);
            } else {
                game_news->msg = estrdup("???");
            }

            box.w = NEWS_MAX_WIDTH;
            box.h = NEWS_MAX_HEIGHT;
            text_show(NULL, NEWS_FONT, game_news->msg, 10, 40, COLOR_WHITE,
                      TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
            game_news->num_lines = box.h;
            scrollbar_create(&game_news->scrollbar, 15, 240,
                             &game_news->scroll_offset, &game_news->num_lines,
                             box.y);
            box.h = NEWS_MAX_HEIGHT;
        }
    }

    box.w = NEWS_MAX_WIDTH;
    box.h = NEWS_MAX_HEIGHT;
    box.y = game_news->scroll_offset;
    text_show(popup->surface, NEWS_FONT, game_news->msg, 10, 40, COLOR_WHITE,
              TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);

    game_news->scrollbar.px = popup->x;
    game_news->scrollbar.py = popup->y;
    scrollbar_show(&game_news->scrollbar, popup->surface,
                   popup->surface->w - 28, 45);
    return 1;
}

/** @copydoc popup_struct::event_func */
static int
popup_event (popup_struct *popup, SDL_Event *event)
{
    game_news_t *game_news = popup->custom_data;

    if (game_news->msg == NULL) {
        return -1;
    }

    if (scrollbar_event(&game_news->scrollbar, event)) {
        popup->redraw = 1;
        return 1;
    }

    if (event->type == SDL_KEYDOWN) {
        /* Scroll the text. */
        if (event->key.keysym.sym == SDLK_DOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar, 1);
            popup->redraw = 1;
            return 1;
        } else if (event->key.keysym.sym == SDLK_UP) {
            scrollbar_scroll_adjust(&game_news->scrollbar, -1);
            popup->redraw = 1;
            return 1;
        } else if (event->key.keysym.sym == SDLK_PAGEUP) {
            scrollbar_scroll_adjust(&game_news->scrollbar,
                                    -game_news->scrollbar.max_lines);
            popup->redraw = 1;
            return 1;
        } else if (event->key.keysym.sym == SDLK_PAGEDOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar,
                                    game_news->scrollbar.max_lines);
            popup->redraw = 1;
            return 1;
        }
    }/* Mouse wheel? */
    else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar, 1);
            popup->redraw = 1;
            return 1;
        } else if (event->button.button == SDL_BUTTON_WHEELUP) {
            scrollbar_scroll_adjust(&game_news->scrollbar, -1);
            popup->redraw = 1;
            return 1;
        }
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int
popup_destroy_callback (popup_struct *popup)
{
    game_news_t *game_news = popup->custom_data;
    efree(game_news->title);
    curl_data_free(game_news->data);

    if (game_news->msg != NULL) {
        efree(game_news->msg);
    }

    return 1;
}

/**
 * Open the game news popup.
 *
 * @param title
 * Title of the news entry that we want to read.
 */
void
game_news_open (const char *title)
{
    popup_struct *popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT,
                                                   "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;
    popup->disable_texture_drawing = 1;

    game_news_t *game_news = ecalloc(1, sizeof(*game_news));
    popup->custom_data = game_news;
    game_news->title = estrdup(title);

    /* Initialize cURL, escape the game news title and construct
     * the url to use for downloading. */
    CURL *curl = curl_easy_init();
    char *news_id = curl_easy_escape(curl, title, 0);
    char url[HUGE_BUF];
    snprintf(VS(url), "%s?news=%s", clioption_settings.game_news_url, news_id);
    curl_free(news_id);
    curl_easy_cleanup(curl);

    /* Start downloading. */
    game_news->data = curl_download_start(url, NULL);
    game_news->state = CURL_STATE_NONE;
}

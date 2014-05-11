/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * @author Alex Tokar */

#include <global.h>

/**
 * Maximum width of the text. */
#define NEWS_MAX_WIDTH 455
/**
 * Maximum height of the text. */
#define NEWS_MAX_HEIGHT 250
/**
 * Font of the text. */
#define NEWS_FONT FONT_SANS12

/**
 * Structure that holds the game news popup data. */
typedef struct game_news_struct
{
    /**
     * Title of the game news entry to read about. */
    char *title;

    /**
     * cURL data for downloading the game news message. */
    curl_data *data;

    /**
     * The downloaded message; NULL while still downloading. */
    char *msg;

    /**
     * Scrollbar buffer. */
    scrollbar_struct scrollbar;

    /**
     * Scroll offset. */
    uint32 scroll_offset;

    /**
     * Number of lines. */
    uint32 num_lines;
} game_news_struct;

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
    game_news_struct *game_news;
    SDL_Rect box;
    int ret;

    game_news = popup->custom_data;

    box.w = popup->surface->w;
    box.h = 38;
    text_show(popup->surface, FONT_SERIF16, game_news->title, 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    ret = curl_download_finished(game_news->data);

    box.w = popup->surface->w;
    box.h = popup->surface->h - box.h;

    if (ret == -1) {
        text_show(popup->surface, FONT_SERIF12, "Connection timed out.", 0, 0, COLOR_WHITE, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
        return 1;
    }
    else if (ret == 0) {
        text_show(popup->surface, FONT_SERIF12, "Downloading news, please wait...", 0, 0, COLOR_WHITE, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
        return 1;
    }
    else if (ret == 1) {
        if (!game_news->msg) {
            game_news->msg = estrdup(game_news->data->memory ? game_news->data->memory : "???");

            text_show(NULL, NEWS_FONT, game_news->msg, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_CALC, &box);
            game_news->num_lines = box.h;
            scrollbar_create(&game_news->scrollbar, 15, 240, &game_news->scroll_offset, &game_news->num_lines, box.y);
            box.h = NEWS_MAX_HEIGHT;
        }
    }

    box.w = NEWS_MAX_WIDTH;
    box.h = NEWS_MAX_HEIGHT;
    box.y = game_news->scroll_offset;
    text_show(popup->surface, NEWS_FONT, game_news->msg, 10, 40, COLOR_WHITE, TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_LINES_SKIP, &box);

    game_news->scrollbar.px = popup->x;
    game_news->scrollbar.py = popup->y;
    scrollbar_show(&game_news->scrollbar, popup->surface, popup->surface->w - 28, 45);
    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    game_news_struct *game_news;

    game_news = popup->custom_data;

    if (!game_news->msg) {
        return -1;
    }

    if (scrollbar_event(&game_news->scrollbar, event)) {
        return 1;
    }

    if (event->type == SDL_KEYDOWN) {
        /* Scroll the text. */
        if (event->key.keysym.sym == SDLK_DOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar, 1);
            return 1;
        }
        else if (event->key.keysym.sym == SDLK_UP) {
            scrollbar_scroll_adjust(&game_news->scrollbar, -1);
            return 1;
        }
        else if (event->key.keysym.sym == SDLK_PAGEUP) {
            scrollbar_scroll_adjust(&game_news->scrollbar, -game_news->scrollbar.max_lines);
            return 1;
        }
        else if (event->key.keysym.sym == SDLK_PAGEDOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar, game_news->scrollbar.max_lines);
            return 1;
        }
    }
    /* Mouse wheel? */
    else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            scrollbar_scroll_adjust(&game_news->scrollbar, 1);
            return 1;
        }
        else if (event->button.button == SDL_BUTTON_WHEELUP) {
            scrollbar_scroll_adjust(&game_news->scrollbar, -1);
            return 1;
        }
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    game_news_struct *game_news;

    game_news = popup->custom_data;

    efree(game_news->title);
    curl_data_free(game_news->data);

    if (game_news->msg) {
        efree(game_news->msg);
    }

    return 1;
}

/**
 * Open the game news popup.
 * @param title Title of the news entry that we want to read. */
void game_news_open(const char *title)
{
    popup_struct *popup;
    game_news_struct *game_news;
    char url[MAX_BUF], *id;
    CURL *curl;

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "popup"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->destroy_callback_func = popup_destroy_callback;

    popup->custom_data = game_news = ecalloc(1, sizeof(*game_news));
    game_news->title = estrdup(title);

    /* Initialize cURL, escape the game news title and construct
     * the url to use for downloading. */
    curl = curl_easy_init();
    id = curl_easy_escape(curl, title, 0);
    snprintf(url, sizeof(url), "%s?news=%s", clioption_settings.game_news_url, id);
    curl_free(id);
    curl_easy_cleanup(curl);

    /* Start downloading. */
    game_news->data = curl_download_start(url);
}

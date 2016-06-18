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
 * Painting GUI code.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <resources.h>
#include <curl.h>

/**
 * Structure used to store data related to the painting popup.
 */
typedef struct popup_painting {
    char *resource_name; ///< Resource name identifier.
    char *name; ///< Title of the painting.
    char *msg; ///< Message associated with the painting.
    sprite_struct *sprite; ///< Sprite of the painting.
    SDL_Surface *zoomed; ///< Zoomed copy.
    SDL_Rect coords; ///< Painting coords.
    int mx; ///< Mouse X cache.
    int my; ///< Mouse Y cache.
    double zoom_x; ///< X zoom.
    double zoom_y; ///< Y zoom.
    Uint32 last_redraw; ///< Last redraw time.
} popup_painting_t;

/**
 * Acquire the surface to use for rendering the painting.
 *
 * @param data
 * Painting data.
 * @return
 * Surface to use.
 */
static inline SDL_Surface *
popup_painting_data_surface (popup_painting_t *data)
{
    HARD_ASSERT(data != NULL);

    if (data->zoomed != NULL) {
        return data->zoomed;
    }

    if (data->sprite != NULL) {
        return data->sprite->bitmap;
    }

    return NULL;
}

/**
 * Frees the specified painting data structure.
 *
 * @param data
 * What to free.
 */
static void
popup_painting_data_free (popup_painting_t *data)
{
    HARD_ASSERT(data != NULL);

    if (data->sprite != NULL) {
        sprite_free_sprite(data->sprite);
    }

    if (data->zoomed != NULL) {
        SDL_FreeSurface(data->zoomed);
    }

    efree(data->resource_name);
    efree(data->name);
    efree(data->msg);
    efree(data);
}

/** @copydoc popup_struct::draw_func */
static int
popup_draw_func (popup_struct *popup)
{
    if (!popup->redraw) {
        return 1;
    }

    SDL_Rect box;
    popup->redraw = 0;

    surface_show(popup->surface, 0, 0, NULL, texture_surface(popup->texture));

    popup_painting_t *painting_data = popup->custom_data;

    box.w = 700;
    box.h = 26;
    text_show_format(popup->surface,
                     FONT("logisoso", 18),
                     30,
                     25,
                     COLOR_HGOLD,
                     TEXT_OUTLINE | TEXT_VALIGN_CENTER | TEXT_MARKUP,
                     &box,
                     "[b]%s[/b]",
                     painting_data->name);

    box.w = 730;
    box.h = 80;
    text_show(popup->surface,
              FONT_SANS14,
              painting_data->msg,
              35,
              590,
              COLOR_WHITE,
              TEXT_OUTLINE | TEXT_MARKUP | TEXT_WORD_WRAP,
              &box);

    resource_t *resource = resources_find(painting_data->resource_name);
    if (resource == NULL) {
        return 1;
    }

    box.w = painting_data->coords.w;
    box.h = painting_data->coords.h;

    if (!resources_is_ready(resource)) {
        if (resource->request == NULL) {
            text_show(popup->surface,
                      FONT_SERIF16,
                      "Painting download failed.",
                      25,
                      60,
                      COLOR_WHITE,
                      TEXT_OUTLINE | TEXT_VALIGN_CENTER | TEXT_ALIGN_CENTER,
                      &box);
            return 1;
        }

        char buf[MAX_BUF];
        curl_request_speedinfo(resource->request, VS(buf));
        text_show_format(popup->surface,
                         FONT_SERIF16,
                         25,
                         60,
                         COLOR_WHITE,
                         TEXT_OUTLINE | TEXT_VALIGN_CENTER | TEXT_ALIGN_CENTER,
                         &box,
                         "Downloading data, please wait...\n%s",
                         buf);

        return 1;
    }

    if (painting_data->sprite == NULL) {
        char path[HUGE_BUF];
        snprintf(VS(path), "resources/%s", resource->digest);
        painting_data->sprite = sprite_tryload_file(path, 0, NULL);
        if (painting_data->sprite == NULL) {
            return 1;
        }

        if (painting_data->sprite->bitmap->w != painting_data->coords.w ||
            painting_data->sprite->bitmap->h != painting_data->coords.h) {
            painting_data->zoom_x = (double) painting_data->coords.w /
                                    painting_data->sprite->bitmap->w;
            painting_data->zoom_y = (double) painting_data->coords.h /
                                    painting_data->sprite->bitmap->h;
        }
    }

    if (painting_data->zoomed == NULL &&
        !DBL_EQUAL(painting_data->zoom_x, 1.0) &&
        !DBL_EQUAL(painting_data->zoom_y, 1.0)) {
        bool smooth = setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH);
        painting_data->zoomed = rotozoomSurfaceXY(painting_data->sprite->bitmap,
                                                  0,
                                                  painting_data->zoom_x,
                                                  painting_data->zoom_y,
                                                  smooth);
        surface_pan(popup_painting_data_surface(painting_data),
                    &painting_data->coords);
    }

    surface_show(popup->surface,
                 25,
                 60,
                 &painting_data->coords,
                 popup_painting_data_surface(painting_data));

    return 1;
}

/** @copydoc popup_struct::draw_post_func */
static int
popup_draw_post_func (popup_struct *popup)
{
    popup_painting_t *painting_data = popup->custom_data;

    resource_t *resource = resources_find(painting_data->resource_name);
    if (resource == NULL) {
        return 1;
    }

    if (!resources_is_ready(resource)) {
        if (SDL_GetTicks() - painting_data->last_redraw > 200) {
            painting_data->last_redraw = SDL_GetTicks();
            popup->redraw = 1;
        }
    } else if (painting_data->sprite == NULL) {
        popup->redraw = 1;
    }

    return 1;
}

/** @copydoc popup_struct::event_func */
static int
popup_event_func (popup_struct *popup, SDL_Event *event)
{
    popup_painting_t *painting_data = popup->custom_data;

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            painting_data->mx = event->motion.x;
            painting_data->my = event->motion.y;
        } else if (event->button.button == SDL_BUTTON_WHEELUP ||
                   event->button.button == SDL_BUTTON_WHEELDOWN) {
            if (painting_data->zoomed != NULL) {
                SDL_FreeSurface(painting_data->zoomed);
                painting_data->zoomed = NULL;
            }

            double zoom = 0.1;
            if (event->button.button == SDL_BUTTON_WHEELDOWN) {
                zoom = -zoom;
            }

            painting_data->zoom_x += zoom;
            painting_data->zoom_x = MIN(5.0, MAX(0.1, painting_data->zoom_x));
            painting_data->zoom_y += zoom;
            painting_data->zoom_y = MIN(5.0, MAX(0.1, painting_data->zoom_y));
            popup->redraw = 1;
        }
    } else if (event->type == SDL_MOUSEBUTTONUP) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            painting_data->mx = painting_data->my = -1;
        }
    } else if (event->type == SDL_MOUSEMOTION &&
        painting_data->mx != -1 &&
        painting_data->my != -1) {
        painting_data->coords.x += painting_data->mx - event->motion.x;
        painting_data->coords.y += painting_data->my - event->motion.y;
        painting_data->mx = event->motion.x;
        painting_data->my = event->motion.y;
        surface_pan(popup_painting_data_surface(painting_data),
                    &painting_data->coords);
        popup->redraw = 1;
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int
popup_destroy_callback (popup_struct *popup)
{
    popup_painting_t *painting_data = popup->custom_data;
    popup_painting_data_free(painting_data);
    popup->custom_data = NULL;
    return 1;
}

/** @copydoc socket_command_struct::handle_func */
void
socket_command_painting (uint8_t *data, size_t len, size_t pos)
{
    popup_painting_t *painting_data = ecalloc(1, sizeof(*painting_data));

    painting_data->coords.w = 750;
    painting_data->coords.h = 500;
    painting_data->mx = painting_data->my = -1;
    painting_data->zoom_x = painting_data->zoom_y = 1.0;

    StringBuffer *sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    painting_data->resource_name = stringbuffer_finish(sb);

    sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    painting_data->name = stringbuffer_finish(sb);

    sb = stringbuffer_new();
    packet_to_stringbuffer(data, len, &pos, sb);
    painting_data->msg = stringbuffer_finish(sb);

    if (string_isempty(painting_data->resource_name) ||
        string_isempty(painting_data->name)) {
        LOG(PACKET, "Empty resource name or painting name");
        popup_painting_data_free(painting_data);
        return;
    }

    popup_struct *popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT,
                                                   "painting"));
    popup->custom_data = painting_data;
    popup->draw_func = popup_draw_func;
    popup->draw_post_func = popup_draw_post_func;
    popup->event_func = popup_event_func;
    popup->destroy_callback_func = popup_destroy_callback;
    popup->disable_texture_drawing = 1;
    popup->button_left.button.texture = NULL;
    popup->button_right.y = 25;
    popup->button_right.x = popup->texture->surface->w - 25 -
                            popup->button_right.button.texture->surface->w;
}

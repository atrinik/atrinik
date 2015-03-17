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
 * Implements minimap type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>

enum {
    MINIMAP_TEXTURE_BG,
    MINIMAP_TEXTURE_MASK,
    MINIMAP_TEXTURE_BORDER,

    MINIMAP_TEXTURE_NUM
};

typedef struct minimap_widget {
    SDL_Surface *surface;
    SDL_Surface *textures[MINIMAP_TEXTURE_NUM];
} minimap_widget_t;

static const char *const minimap_texture_names[MINIMAP_TEXTURE_NUM] = {
    "minimap_bg", "minimap_mask", "minimap_border"
};

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    minimap_widget_t *minimap;
    SDL_Rect box;
    size_t i;

    minimap = (minimap_widget_t *) widget->subwidget;

    if (widget->surface == NULL || widget->surface->w != widget->w ||
            widget->surface->h != widget->h) {
        SDL_Surface *texture;

        if (widget->surface != NULL) {
            SDL_FreeSurface(widget->surface);
        }

        for (i = 0; i < MINIMAP_TEXTURE_NUM; i++) {
            SDL_FreeSurface(minimap->textures[i]);
        }

        widget->surface = SDL_CreateRGBSurface(get_video_flags(), widget->w,
                widget->h, video_get_bpp(), 0, 0, 0, 0);
        minimap_redraw_flag = 1;

        for (i = 0; i < MINIMAP_TEXTURE_NUM; i++) {
            texture = TEXTURE_CLIENT(minimap_texture_names[i]);
            minimap->textures[i] = zoomSurface(texture,
                    (double) widget->w / texture->w,
                    (double) widget->h / texture->h,
                    setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
        }
    }

    if (minimap_redraw_flag) {
        SDL_Surface *zoomed;

        SDL_FillRect(widget->surface, NULL, 0);
        SDL_FillRect(minimap->surface, NULL, 0);
        map_draw_map(minimap->surface);
        minimap_redraw_flag = 0;

        zoomed = zoomSurface(minimap->surface,
                (double) widget->w / minimap->surface->w,
                (double) widget->h / minimap->surface->h,
                setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
        SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_MASK], NULL,
                zoomed, NULL);
        SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_BG], NULL,
                widget->surface, NULL);
        SDL_SetColorKey(zoomed, SDL_SRCCOLORKEY,
                SDL_MapRGBA(zoomed->format, 0, 0, 0, 0));
        SDL_BlitSurface(zoomed, NULL, widget->surface, NULL);
        SDL_SetColorKey(widget->surface, SDL_SRCCOLORKEY,
                getpixel(zoomed, 0, 0));
        SDL_FreeSurface(zoomed);

        SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_BORDER], NULL,
                widget->surface, NULL);
    }

    box.x = widget->x;
    box.y = widget->y;
    SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    minimap_widget_t *minimap;
    size_t i;

    minimap = (minimap_widget_t *) widget->subwidget;
    SDL_FreeSurface(minimap->surface);

    for (i = 0; i < MINIMAP_TEXTURE_NUM; i++) {
        SDL_FreeSurface(minimap->textures[i]);
    }
}

void widget_minimap_init(widgetdata *widget)
{
    minimap_widget_t *minimap;

    minimap = calloc(1, sizeof(*minimap));
    minimap->surface = SDL_CreateRGBSurface(get_video_flags(),
            850 * (MAP_FOW_SIZE / 2), 600 * (MAP_FOW_SIZE / 2),
            video_get_bpp(), 0, 0, 0, 0);

    widget->draw_func = widget_draw;
    widget->deinit_func = widget_deinit;
    widget->subwidget = minimap;
}

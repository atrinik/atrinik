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
#include <region_map.h>

/**
 * Textures used by the minimap.
 */
enum {
    MINIMAP_TEXTURE_BG,
    MINIMAP_TEXTURE_MASK,
    MINIMAP_TEXTURE_BORDER,
    MINIMAP_TEXTURE_BORDER_ROTATED,
    MINIMAP_TEXTURE_NUM
};

/**
 * Possible minimap display types.
 */
typedef enum {
    MINIMAP_TYPE_PREFER_REGION_MAP,
    MINIMAP_TYPE_REGION_MAP,
    MINIMAP_TYPE_DYNAMIC,

    MINIMAP_TYPE_NUM
} minimap_type_t;

/**
 * Number of pixels from the border to the circle in the minimap texture.
 */
#define MINIMAP_CIRCLE_PADDING(widget) (10. * ((double) (widget)->w / \
    TEXTURE_CLIENT(minimap_texture_names[MINIMAP_TEXTURE_BG])->w))

/**
 * Minimap widget sub-structure.
 */
typedef struct minimap_widget {
    /**
     * Surface used when rendering dynamic maps.
     */
    SDL_Surface *surface;

    /**
     * Cached minimap textures.
     */
    SDL_Surface *textures[MINIMAP_TEXTURE_NUM];

    /**
     * Display type.
     */
    minimap_type_t type;
} minimap_widget_t;

/**
 * Texture names to load.
 */
static const char *const minimap_texture_names[MINIMAP_TEXTURE_NUM] = {
    "minimap_bg", "minimap_mask", "minimap_border", "minimap_border_rotated"
};

/**
 * String representations of the display types.
 */
static const char *const minimap_display_modes[MINIMAP_TYPE_NUM] = {
    "Prefer region maps", "Only region maps", "Only dynamic maps"
};

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    minimap_widget_t *minimap;
    SDL_Rect box;
    size_t i;

    minimap = widget->subwidget;

    /* No surface or the widget dimensions changed, (re-)create the surface
     * and the zoomed minimap textures. */
    if (widget->surface == NULL || widget->surface->w != widget->w ||
            widget->surface->h != widget->h) {
        SDL_Surface *texture;

        if (widget->surface != NULL) {
            SDL_FreeSurface(widget->surface);
        }

        widget->surface = SDL_CreateRGBSurface(get_video_flags(), widget->w,
                widget->h, video_get_bpp(), 0, 0, 0, 0);
        minimap_redraw_flag = 1;

        for (i = 0; i < MINIMAP_TEXTURE_NUM; i++) {
            if (minimap->textures[i] != NULL) {
                SDL_FreeSurface(minimap->textures[i]);
            }

            texture = TEXTURE_CLIENT(minimap_texture_names[i]);
            minimap->textures[i] = zoomSurface(texture,
                    (double) widget->w / texture->w + 0.001,
                    (double) widget->h / texture->h + 0.001,
                    setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
        }
    }

    if (minimap_redraw_flag) {
        minimap_redraw_flag = 0;
        SDL_FillRect(widget->surface, NULL, 0);
        SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_BG], NULL,
                widget->surface, NULL);

        /* Determine which version of the minimap to show based on the user's
         * preferences. */
        if (minimap->type == MINIMAP_TYPE_REGION_MAP || (minimap->type ==
                MINIMAP_TYPE_PREFER_REGION_MAP && MapData.region_has_map)) {
            /* Free dynamic map surface. */
            if (minimap->surface != NULL) {
                SDL_FreeSurface(minimap->surface);
                minimap->surface = NULL;
            }

            if (region_map_ready(MapData.region_map)) {
                int cx, cy, sx, sy;
                double rad;
                SDL_Rect rect;
                SDL_Surface *surface;

                cx = (widget->w - MINIMAP_CIRCLE_PADDING(widget) * 2) / 2;
                cy = (widget->h - MINIMAP_CIRCLE_PADDING(widget) * 2) / 2;
                rad = 45.0 * (M_PI / 180.0);
                sx = cx * cos(rad);
                sy = cy * sin(rad);

                rect.x = cx - sx + MINIMAP_CIRCLE_PADDING(widget);
                rect.y = cy - sy + MINIMAP_CIRCLE_PADDING(widget);
                rect.w = widget->surface->w - MINIMAP_CIRCLE_PADDING(widget) *
                        2 - (cx - sx) * 2;
                rect.h = widget->surface->h - MINIMAP_CIRCLE_PADDING(widget) *
                        2 - (cy - sy) * 2;

                MapData.region_map->pos.x += rect.x;
                MapData.region_map->pos.y += rect.y;
                MapData.region_map->pos.w = rect.w;
                MapData.region_map->pos.h = rect.h;
                region_map_resize(MapData.region_map, 0);
                region_map_pan(MapData.region_map);
                MapData.region_map->pos.x -= rect.x;
                MapData.region_map->pos.y -= rect.y;
                MapData.region_map->pos.w = widget->surface->w;
                MapData.region_map->pos.h = widget->surface->h;

                surface = region_map_surface(MapData.region_map);

                SDL_BlitSurface(surface, &MapData.region_map->pos,
                        widget->surface, NULL);
                region_map_render_fow(MapData.region_map,
                        widget->surface, 0, 0);
                region_map_render_marker(MapData.region_map,
                        widget->surface, 0, 0);
            } else {
                SDL_Rect tmp;

                tmp.w = widget->w;
                tmp.h = widget->h;
                text_show(widget->surface, FONT_SANS10, "Downloading...", 0, 0,
                        COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER |
                        TEXT_OUTLINE, &tmp);
            }

            SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_MASK], NULL,
                    widget->surface, NULL);
            SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_BORDER_ROTATED],
                    NULL, widget->surface, NULL);
        } else {
            SDL_Surface *zoomed;
            SDL_Rect zoomedbox;

            if (minimap->surface == NULL) {
                minimap->surface = SDL_CreateRGBSurface(get_video_flags(),
                        850 * (MAP_FOW_SIZE / 2), 600 * (MAP_FOW_SIZE / 2),
                        video_get_bpp(), 0, 0, 0, 0);
            }

            double zoomx = (double) widget->w / minimap->surface->w *
                    (minimap->surface->w / (MAP_FOW_SIZE + 1.0));
            double zoomy = (double) widget->h / minimap->surface->h *
                    (minimap->surface->h / (MAP_FOW_SIZE + 1.0));

            SDL_FillRect(minimap->surface, NULL, 0);
            map_draw_map(minimap->surface);

            zoomx = (MapData.region_map->zoom) / 100.0 * (zoomx / 100.0);
            zoomy = (MapData.region_map->zoom) / 100.0 * (zoomy / 100.0);
            zoomed = zoomSurface(minimap->surface, zoomx, zoomy,
                    setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH));
            zoomedbox.x = zoomed->w / 2 - widget->surface->w / 2;
            zoomedbox.y = zoomed->h / 2 - widget->surface->h / 2;
            zoomedbox.w = widget->surface->w;
            zoomedbox.h = widget->surface->h;
            SDL_BlitSurface(zoomed, &zoomedbox, widget->surface, NULL);
            SDL_FreeSurface(zoomed);

            SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_MASK], NULL,
                    widget->surface, NULL);
            SDL_BlitSurface(minimap->textures[MINIMAP_TEXTURE_BORDER], NULL,
                    widget->surface, NULL);
        }

        SDL_SetColorKey(widget->surface, SDL_SRCCOLORKEY,
                getpixel(widget->surface, 0, 0));
    }

    box.x = widget->x;
    box.y = widget->y;
    SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELUP) {
            /* Zoom in. */
            if (MapData.region_map->zoom < 100) {
                MapData.region_map->zoom += 5;
                minimap_redraw_flag = 1;
                return 1;
            }
        } else if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            /* Zoom out. */
            if (MapData.region_map->zoom > 10) {
                MapData.region_map->zoom -= 5;
                minimap_redraw_flag = 1;
                return 1;
            }
        }
    }

    return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    minimap_widget_t *minimap;
    size_t i;

    minimap = widget->subwidget;
    SDL_FreeSurface(minimap->surface);

    for (i = 0; i < MINIMAP_TEXTURE_NUM; i++) {
        SDL_FreeSurface(minimap->textures[i]);
    }
}

/** @copydoc widgetdata::load_func */
static int widget_load(widgetdata *widget, const char *keyword, const char *parameter)
{
    minimap_widget_t *minimap;

    minimap = widget->subwidget;

    if (strcmp(keyword, "type") == 0) {
        minimap->type = atoi(parameter);
        return 1;
    }

    return 0;
}

/** @copydoc widgetdata::save_func */
static void widget_save(widgetdata *widget, FILE *fp, const char *padding)
{
    minimap_widget_t *minimap;

    minimap = widget->subwidget;

    fprintf(fp, "%stype = %d\n", padding, minimap->type);
}

static void menu_minimap_display_change(widgetdata *widget,
        widgetdata *menuitem, SDL_Event *event)
{
    minimap_widget_t *minimap;
    widgetdata *tmp2;
    _widget_label *label;
    size_t i;

    minimap = widget->subwidget;

    for (tmp2 = menuitem->inv; tmp2; tmp2 = tmp2->next) {
        if (tmp2->type == LABEL_ID) {
            label = LABEL(tmp2);

            for (i = 0; i < MINIMAP_TYPE_NUM; i++) {
                if (strcmp(minimap_display_modes[i], label->text) == 0) {
                    minimap->type = i;
                    break;
                }
            }

            minimap_redraw_flag = 1;

            break;
        }
    }
}

static void menu_minimap_display(widgetdata *widget, widgetdata *menuitem,
        SDL_Event *event)
{
    minimap_widget_t *minimap;
    widgetdata *submenu;
    size_t i;

    minimap = widget->subwidget;
    submenu = MENU(menuitem->env)->submenu;

    for (i = 0; i < MINIMAP_TYPE_NUM; i++) {
        add_menuitem(submenu, minimap_display_modes[i],
                &menu_minimap_display_change, MENU_RADIO, minimap->type == i);
    }
}

/** @copydoc widgetdata::menu_handle_func */
static int widget_menu_handle(widgetdata *widget, SDL_Event *event)
{
    widgetdata *menu;

    menu = create_menu(event->motion.x, event->motion.y, widget);

    widget_menu_standard_items(widget, menu);
    add_menuitem(menu, "Display  >", &menu_minimap_display, MENU_SUBMENU, 0);

    menu_finalize(menu);

    return 1;
}

void widget_minimap_init(widgetdata *widget)
{
    minimap_widget_t *minimap;

    minimap = ecalloc(1, sizeof(*minimap));
    minimap->type = MINIMAP_TYPE_PREFER_REGION_MAP;
    MapData.region_map->zoom = 50;

    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
    widget->deinit_func = widget_deinit;
    widget->load_func = widget_load;
    widget->save_func = widget_save;
    widget->menu_handle_func = widget_menu_handle;
    widget->subwidget = minimap;
}

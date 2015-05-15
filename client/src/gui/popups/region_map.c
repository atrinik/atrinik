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
 * The region map dialog.
 *
 * @author Alex Tokar */

#include <global.h>
#include <region_map.h>

/** Size of the book GUI borders. */
#define RM_BORDER_SIZE 25

/** Number of pixels to scroll using the keyboard arrows. */
#define RM_SCROLL 10
/** Number of pixels to scroll using the keyboard arrows when shift is held. */
#define RM_SCROLL_SHIFT 50

/**
 * @defgroup RM_MAP_xxx Region map content coords
 * Region map content coordinates.
 *@{*/
/** The map X position. */
#define RM_MAP_STARTX 25
/** The map Y position. */
#define RM_MAP_STARTY 60
/** Maximum width of the map. */
#define RM_MAP_WIDTH 630
/** Maximum height of the map. */
#define RM_MAP_HEIGHT 345
/*@}*/

/**
 * @defgroup RM_BUTTON_LEFT_xxx Region map left button coords
 * Region map left button coordinates.
 *@{*/
/** X position of the left button. */
#define RM_BUTTON_LEFT_STARTX 25
/** Y position of the left button. */
#define RM_BUTTON_LEFT_STARTY 25
/*@}*/

/**
 * @defgroup RM_BUTTON_RIGHT_xxx Region map right button coords
 * Region map right button coordinates.
 *@{*/
/** X position of the right button. */
#define RM_BUTTON_RIGHT_STARTX 649
/** Y position of the right button. */
#define RM_BUTTON_RIGHT_STARTY 25
/*@}*/

/**
 * @defgroup RM_TITLE_xxx Region map title coords
 * Region map title coordinates.
 *@{*/
/** X position of the title text. */
#define RM_TITLE_STARTX 60
/** Y position of the title text. */
#define RM_TITLE_STARTY 27
/** Maximum width of the title text. */
#define RM_TITLE_WIDTH 580
/** Maximum height of the title text. */
#define RM_TITLE_HEIGHT 22
/*@}*/

/**
 * @defgroup RM_SCROLLBAR_xxx Region map scrollbar coords
 * Region map scrollbar coordinates.
 *@{*/
/** X position of the vertical scrollbar. */
#define RM_SCROLLBAR_STARTX 662
/** Y position of the vertical scrollbar. */
#define RM_SCROLLBAR_STARTY 60
/** Width of the vertical scrollbar. */
#define RM_SCROLLBAR_WIDTH 13
/** Height of the vertical scrollbar. */
#define RM_SCROLLBAR_HEIGHT 345
/*@}*/

/**
 * @defgroup RM_SCROLLBARH_xxx Region map horizontal scrollbar coords
 * Region map horizontal scrollbar coordinates.
 *@{*/
/** X position of the horizontal scrollbar. */
#define RM_SCROLLBARH_STARTX 25
/** Y position of the horizontal scrollbar. */
#define RM_SCROLLBARH_STARTY 412
/** Width of the horizontal scrollbar. */
#define RM_SCROLLBARH_WIDTH 630
/** Height of the horizontal scrollbar. */
#define RM_SCROLLBARH_HEIGHT 13
/*@}*/

/**
 * Check whether the mouse is inside the region map.
 */
#define RM_IN_MAP(_popup, _mx, _my) ((_mx) >= (_popup)->x + RM_MAP_STARTX && \
    (_mx) < (_popup)->x + RM_MAP_STARTX + RM_MAP_WIDTH && \
    (_my) >= (_popup)->y + RM_MAP_STARTY && \
    (_my) < (_popup)->y + RM_MAP_STARTY + RM_MAP_HEIGHT)

/** Region map scrollbar. */
static scrollbar_struct scrollbar;
/** Storage for the scroll offset and the like of the scrollbar. */
static scrollbar_info_struct scrollbar_info;
/** Region map horizontal scrollbar. */
static scrollbar_struct scrollbar_horizontal;
/**
 * Storage for the scroll offset and the like of the horizontal
 * scrollbar. */
static scrollbar_info_struct scrollbar_horizontal_info;
/**
 * Whether the region map is being dragged with the mouse. */
static bool region_map_dragging = false;
/**
 * Local copy of the region map.
 */
static region_map_t *region_map = NULL;

/** @copydoc popup_struct::draw_post_func */
static int popup_draw_post_func(popup_struct *popup)
{
    SDL_Rect box, dest;
    SDL_Surface *surface;

    box.w = popup->surface->w;
    box.h = popup->surface->h;

    /* Show direction markers. */
    text_show(ScreenSurface, FONT_SERIF14, "N", popup->x,
            popup->y + RM_BORDER_SIZE / 2 - FONT_HEIGHT(FONT_SERIF14) / 2,
            COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_OUTLINE, &box);
    text_show(ScreenSurface, FONT_SERIF14, "E",
            popup->x + popup->surface->w - RM_BORDER_SIZE / 2 -
            text_get_width(FONT_SERIF14, "E", 0) / 2, popup->y,
            COLOR_HGOLD, TEXT_OUTLINE | TEXT_VALIGN_CENTER, &box);
    text_show(ScreenSurface, FONT_SERIF14, "S", popup->x,
            popup->y + popup->surface->h - RM_BORDER_SIZE / 2 -
            FONT_HEIGHT(FONT_SERIF14) / 2,
            COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_OUTLINE, &box);
    text_show(ScreenSurface, FONT_SERIF14, "W", popup->x + RM_BORDER_SIZE / 2 -
            text_get_width(FONT_SERIF14, "W", 0) / 2, popup->y,
            COLOR_HGOLD, TEXT_OUTLINE | TEXT_VALIGN_CENTER, &box);

    box.w = RM_TITLE_WIDTH;
    box.h = RM_TITLE_HEIGHT;
    text_show(ScreenSurface, FONT_SERIF14, MapData.region_longname, popup->x + RM_TITLE_STARTX, popup->y + RM_TITLE_STARTY, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);

    box.x = popup->x + RM_MAP_STARTX;
    box.y = popup->y + RM_MAP_STARTY;
    box.w = RM_MAP_WIDTH;
    box.h = RM_MAP_HEIGHT;

    if (!region_map_ready(MapData.region_map)) {
        int ret_png, ret_def;

        /* Check the status of the downloads. */
        ret_png = curl_download_finished(MapData.region_map->data_png);
        ret_def = curl_download_finished(MapData.region_map->data_def);

        /* We failed. */
        if (ret_png == -1 || ret_def == -1) {
            curl_data *tmp;

            tmp = ret_png == -1 ? MapData.region_map->data_png :
                MapData.region_map->data_def;

            if (tmp->http_code != -1) {
                text_show_format(ScreenSurface, FONT_SERIF14,
                        box.x, box.y,  COLOR_WHITE,
                        TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER | TEXT_OUTLINE,
                        &box, "Error: %d", tmp->http_code);
            } else {
                text_show(ScreenSurface, FONT_SERIF14,
                        "Connection timed out.", box.x, box.y, COLOR_WHITE,
                        TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER | TEXT_OUTLINE,
                        &box);
            }
        } else if (ret_png == 0 || ret_def == 0) {
            char buf[MAX_BUF];
            curl_data *tmp;

            tmp = ret_png == 0 ? MapData.region_map->data_png :
                MapData.region_map->data_def;

            text_show_format(ScreenSurface, FONT_SERIF14,
                    box.x, box.y, COLOR_WHITE,
                    TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER | TEXT_OUTLINE, &box,
                    "Downloading the map, please wait...\n%s",
                    curl_download_speedinfo(tmp, VS(buf)));
        }

        return 1;
    }

    if (region_map == NULL) {
        region_map = region_map_clone(MapData.region_map);
        region_map->pos.w = RM_MAP_WIDTH;
        region_map->pos.h = RM_MAP_HEIGHT;
        region_map_pan(region_map);
    }

    surface = region_map_surface(region_map);

    if (scrollbar_info.redraw) {
        scrollbar_info.redraw = 0;
        region_map->pos.y = scrollbar_info.scroll_offset;
        surface_pan(surface, &region_map->pos);
    } else {
        scrollbar_info.num_lines = surface->h;
        scrollbar_info.scroll_offset = region_map->pos.y;
        scrollbar.max_lines = region_map->pos.h;
    }

    if (scrollbar_horizontal_info.redraw) {
        scrollbar_horizontal_info.redraw = 0;
        region_map->pos.x = scrollbar_horizontal_info.scroll_offset;
        surface_pan(surface, &region_map->pos);
    } else {
        scrollbar_horizontal_info.num_lines = surface->w;
        scrollbar_horizontal_info.scroll_offset = region_map->pos.x;
        scrollbar_horizontal.max_lines = region_map->pos.w;
    }

    scrollbar_show(&scrollbar, ScreenSurface,
            popup->x + RM_SCROLLBAR_STARTX, popup->y + RM_SCROLLBAR_STARTY);
    scrollbar_show(&scrollbar_horizontal, ScreenSurface,
            popup->x + RM_SCROLLBARH_STARTX, popup->y + RM_SCROLLBARH_STARTY);

    dest.x = box.x;
    dest.y = box.y;

    /* Actually draw the map. */
    SDL_BlitSurface(surface, &region_map->pos, ScreenSurface, &dest);
    region_map_render_fow(region_map, ScreenSurface, dest.x, dest.y);
    region_map_render_marker(region_map, ScreenSurface, dest.x, dest.y);

    return 1;
}

/** @copydoc popup_button::event_func */
static int popup_button_event_func(popup_button *button)
{
    help_show("region map");
    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event_func(popup_struct *popup, SDL_Event *event)
{
    if (region_map == NULL) {
        return -1;
    }

    /* Start dragging the map. */
    if (event->type == SDL_MOUSEBUTTONDOWN &&
            event->button.button == SDL_BUTTON_LEFT &&
            RM_IN_MAP(popup, event->motion.x, event->motion.y)) {
        region_map_dragging = 1;
    }

    /* Dragging the map? */
    if (region_map_dragging) {
        /* Stop dragging the map if the left mouse button has been released. */
        if (event->type == SDL_MOUSEBUTTONUP &&
                event->button.button == SDL_BUTTON_LEFT) {
            region_map_dragging = 0;
            return 1;
        }

        /* Fake slider events. */
        scrollbar.slider.highlight = 1;
        scrollbar_horizontal.slider.highlight = 1;
        /* Reverse the x/y, so "dragging the map" makes more sense. */
        event->motion.x = -event->motion.x;
        event->motion.y = -event->motion.y;
        scrollbar_event(&scrollbar, event);
        scrollbar_event(&scrollbar_horizontal, event);
        return 1;
    }

    /* Handle scrollbars. */
    if (scrollbar_event(&scrollbar, event)) {
        return 1;
    } else if (scrollbar_event(&scrollbar_horizontal, event)) {
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_WHEELUP) {
            /* Zoom in. */
            if (region_map->zoom < RM_ZOOM_MAX) {
                region_map_resize(region_map, RM_ZOOM_PROGRESS);
                return 1;
            }
        } else if (event->button.button == SDL_BUTTON_WHEELDOWN) {
            /* Zoom out. */
            if (region_map->zoom > RM_ZOOM_MIN) {
                region_map_resize(region_map, -RM_ZOOM_PROGRESS);
                return 1;
            }
        } else if (event->button.button == SDL_BUTTON_MIDDLE &&
                setting_get_int(OPT_CAT_DEVEL, OPT_OPERATOR) &&
                RM_IN_MAP(popup, event->motion.x, event->motion.y)) {
            int xpos, ypos, map_x, map_y, map_w, map_h;
            double zoomfactor;
            size_t i;
            char buf[HUGE_BUF];

            /* Quickport. */
            xpos = region_map->pos.x + event->motion.x - popup->x -
                    RM_MAP_STARTX;
            ypos = region_map->pos.y + event->motion.y - popup->y -
                    RM_MAP_STARTY;
            zoomfactor = region_map->zoom / 100.0;
            map_w = region_map->def->map_size_x * region_map->def->pixel_size *
                    zoomfactor;
            map_h = region_map->def->map_size_y * region_map->def->pixel_size *
                    zoomfactor;

            for (i = 0; i < region_map->def->num_maps; i++) {
                map_x = region_map->def->maps[i].xpos * zoomfactor;
                map_y = region_map->def->maps[i].ypos * zoomfactor;

                if (xpos < map_x || xpos > map_x + map_w ||
                        ypos < map_y || ypos > map_y + map_h) {
                    continue;
                }

                xpos = (xpos - region_map->def->maps[i].xpos * zoomfactor) /
                        (region_map->def->pixel_size * zoomfactor);
                ypos = (ypos - region_map->def->maps[i].ypos * zoomfactor) /
                        (region_map->def->pixel_size * zoomfactor);
                snprintf(buf, sizeof(buf), "/tpto %s %d %d",
                        region_map->def->maps[i].path, xpos, ypos);
                send_command(buf);

                popup_destroy(popup);
                return 1;
            }
        }
    } else if (event->type == SDL_MOUSEMOTION) {
        if (RM_IN_MAP(popup, event->motion.x, event->motion.y)) {
            int xpos, ypos, tooltip_x, tooltip_y, tooltip_w, tooltip_h;
            size_t i;
            double zoomfactor;

            xpos = region_map->pos.x + event->motion.x - popup->x -
                    RM_MAP_STARTX;
            ypos = region_map->pos.y + event->motion.y - popup->y -
                    RM_MAP_STARTY;
            zoomfactor = region_map->zoom / 100.0;

            for (i = 0; i < region_map->def->num_tooltips; i++) {
                tooltip_x = region_map->def->tooltips[i].x * zoomfactor;
                tooltip_y = region_map->def->tooltips[i].y * zoomfactor;
                tooltip_w = region_map->def->tooltips[i].w * zoomfactor;
                tooltip_h = region_map->def->tooltips[i].h * zoomfactor;

                if (xpos < tooltip_x || xpos > tooltip_x + tooltip_w ||
                        ypos < tooltip_y || ypos > tooltip_y + tooltip_h) {
                    continue;
                }

                if (region_map_fow_is_visible(MapData.region_map, xpos /
                        (double) region_map->def->pixel_size / zoomfactor,
                        ypos / (double) region_map->def->pixel_size /
                        zoomfactor)) {
                    tooltip_create(event->motion.x, event->motion.y,
                            FONT_ARIAL11, region_map->def->tooltips[i].text);
                    tooltip_multiline(200);
                    tooltip_enable_delay(100);
                }

                break;
            }
        }
    } else if (event->type == SDL_KEYDOWN) {
        int pos = RM_SCROLL;

        if (event->key.keysym.mod & KMOD_SHIFT) {
            pos = RM_SCROLL_SHIFT;
        }

        if (event->key.keysym.sym == SDLK_UP) {
            region_map->pos.y -= pos;
            surface_pan(region_map_surface(region_map), &region_map->pos);
            return 1;
        } else if (event->key.keysym.sym == SDLK_DOWN) {
            region_map->pos.y += pos;
            surface_pan(region_map_surface(region_map), &region_map->pos);
            return 1;
        } else if (event->key.keysym.sym == SDLK_LEFT) {
            region_map->pos.x -= pos;
            surface_pan(region_map_surface(region_map), &region_map->pos);
            return 1;
        } else if (event->key.keysym.sym == SDLK_RIGHT) {
            region_map->pos.x += pos;
            surface_pan(region_map_surface(region_map), &region_map->pos);
            return 1;
        } else if (event->key.keysym.sym == SDLK_PAGEUP) {
            if (region_map->zoom < RM_ZOOM_MAX) {
                region_map_resize(region_map, RM_ZOOM_PROGRESS);
                return 1;
            }
        } else if (event->key.keysym.sym == SDLK_PAGEDOWN) {
            if (region_map->zoom > RM_ZOOM_MIN) {
                region_map_resize(region_map, -RM_ZOOM_PROGRESS);
                return 1;
            }
        }
    }

    return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
    if (region_map != NULL) {
        region_map_free(region_map);
        region_map = NULL;
    }

    return 1;
}

/**
 * Opens the region map popup.
 */
void region_map_open(void)
{
    popup_struct *popup;

    if (MapData.region_name[0] == '\0') {
        draw_info(COLOR_WHITE, "You don't appear to be in a region...");
        return;
    }

    if (region_map != NULL) {
        region_map_free(region_map);
        region_map = NULL;
    }

    popup = popup_create(texture_get(TEXTURE_TYPE_CLIENT, "region_map"));
    popup->draw_post_func = popup_draw_post_func;
    popup->event_func = popup_event_func;
    popup->destroy_callback_func = popup_destroy_callback;

    popup->button_left.x = RM_BUTTON_LEFT_STARTX;
    popup->button_left.y = RM_BUTTON_LEFT_STARTY;
    popup->button_left.event_func = popup_button_event_func;
    popup_button_set_text(&popup->button_left, "?");

    popup->button_right.x = RM_BUTTON_RIGHT_STARTX;
    popup->button_right.y = RM_BUTTON_RIGHT_STARTY;

    scrollbar_info_create(&scrollbar_info);
    scrollbar_create(&scrollbar, RM_SCROLLBAR_WIDTH, RM_SCROLLBAR_HEIGHT, &scrollbar_info.scroll_offset, &scrollbar_info.num_lines, 0);
    scrollbar.redraw = &scrollbar_info.redraw;
    scrollbar.arrow_adjust = RM_SCROLL;

    scrollbar_info_create(&scrollbar_horizontal_info);
    scrollbar_create(&scrollbar_horizontal, RM_SCROLLBARH_WIDTH, RM_SCROLLBARH_HEIGHT, &scrollbar_horizontal_info.scroll_offset, &scrollbar_horizontal_info.num_lines, 0);
    scrollbar_horizontal.redraw = &scrollbar_horizontal_info.redraw;
    scrollbar_horizontal.arrow_adjust = RM_SCROLL;
}

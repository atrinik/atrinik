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
 * Header file for the region map code.
 *
 * @author Alex Tokar
 */

#ifndef REGION_MAP_H
#define REGION_MAP_H

/** Default zoom level. */
#define RM_ZOOM_DEFAULT 100
/** Minimum zoom level. */
#define RM_ZOOM_MIN 50
/** Maximum zoom level. */
#define RM_ZOOM_MAX 200
/** How much to progress the zoom level with a single mouse wheel event. */
#define RM_ZOOM_PROGRESS 10

/**
 * Single map.
 */
typedef struct region_map_def_map {
    /** The map path. */
    char *path;

    char **regions; ///< Regions this map belongs to.
    size_t regions_num; ///< Number of entries in 'regions'.

    /** X position. */
    int xpos;

    /** Y position. */
    int ypos;
} region_map_def_map_t;

/**
 * Single map label.
 */
typedef struct region_map_def_label {
    /** X position. */
    int x;

    /** Y position. */
    int y;

    /** Unique name of the label. */
    char *name;

    /** Text of the label (markup allowed). */
    char *text;
} region_map_def_label_t;

/**
 * Map tooltips.
 */
typedef struct region_map_def_tooltip {
    /** X position. */
    int x;

    /** Y position. */
    int y;

    /** Width. */
    int w;

    /** Height. */
    int h;

    /** Unique name of this tooltip. */
    char *name;

    /** Tooltip text. */
    char *text;

    /** Show an outline? */
    uint8_t outline;

    /** Outline's color. */
    SDL_Color outline_color;

    /** Size of the outline. */
    uint8_t outline_size;
} region_map_def_tooltip_t;

/*
 *  Map region definitions.
 */
typedef struct region_map_def {
    /** The maps. */
    region_map_def_map_t *maps;

    /** Number of maps. */
    size_t num_maps;

    /** The tooltips. */
    region_map_def_tooltip_t *tooltips;

    /** Number of tooltips. */
    size_t num_tooltips;

    /** The map labels. */
    region_map_def_label_t *labels;

    /** Number of labels. */
    size_t num_labels;

    /** Pixel size of one map tile. */
    int pixel_size;

    /** X Size of the map. */
    int map_size_x;

    /** Y Size of the map. */
    int map_size_y;

    /** Reference count. */
    int refcount;
} region_map_def_t;

/**
 * Tile entry array. Used as a buffer to store visited locations in the region
 * while the region map is still being downloaded.
 */
typedef struct region_map_fow_tile {
    char *path; ///< Map path.
    int x; ///< X coordinate.
    int y; ///< Y coordinate.
} region_map_fow_tile_t;

/**
 * Fog of war.
 */
typedef struct region_map_fow {
    /** Reference count. */
    int refcount;

    char *path;

    SDL_Surface *surface;

    uint32_t *bitmap;

    UT_array *tiles;
} region_map_fow_t;

/**
 * Region map structure.
 */
typedef struct region_map {
    /**
     * Region map image.
     *
     * @internal
     */
    SDL_Surface *surface;

    /**
     * Zoomed version of the region map image.
     *
     * @internal
     */
    SDL_Surface *zoomed;

    /**
     * Zoomed version of the region map's fog of war state.
     *
     * Not in the region_map_fow_t structure because that structure is
     * refcounted, and different GUI elements may have different zoom levels.
     *
     * @internal
     */
    SDL_Surface *fow_zoomed;

    /**
     * Parsed definitions.
     *
     * @warning This structure is refcounted.
     */
    region_map_def_t *def;

    /*
     * Fog of war.
     *
     * @internal
     * @warning This structure is refcounted.
     */
    region_map_fow_t *fow;

    /**
     * Current zoom levels.
     */
    int zoom;

    /**
     * Coordinates for the region map image.
     */
    SDL_Rect pos;

    /**
     * cURL pointer for downloading the region map image.
     */
    curl_data *data_png;

    /**
     * cURL pointer for downloading the region definitions.
     */
    curl_data *data_def;
} region_map_t;

#define RM_MAP_FOW_BITMAP_SIZE(region_map) \
    (sizeof(*(region_map)->fow->bitmap) * \
    (((region_map)->surface->w / (region_map)->def->pixel_size + 31) / 32) * \
    ((region_map)->surface->h / (region_map)->def->pixel_size))

/* Prototypes */

region_map_def_map_t *region_map_find_map(region_map_t *region_map,
        const char *map_path);
void region_map_resize(region_map_t *region_map, int adjust);
bool region_map_ready(region_map_t *region_map);
void region_map_pan(region_map_t *region_map);
void region_map_render_marker(region_map_t *region_map, SDL_Surface *surface,
        int x, int y);
void region_map_render_fow(region_map_t *region, SDL_Surface *surface,
        int x, int y);
SDL_Surface *region_map_surface(region_map_t *region_map);
void region_map_reset(region_map_t *region_map);
region_map_t *region_map_create(void);
region_map_t *region_map_clone(region_map_t *region_map);
void region_map_free(region_map_t *region_map);
void region_map_update(region_map_t *region_map, const char *region_name);
void region_map_fow_update(region_map_t *region_map);
bool region_map_fow_set_visited(region_map_t *region_map,
        region_map_def_map_t *map, const char *map_path, int x, int y);
SDL_Surface *region_map_fow_surface(region_map_t *region_map);
bool region_map_fow_is_visited(region_map_t *region_map, int x, int y);
bool region_map_fow_is_visible(region_map_t *region_map, int x, int y);

#endif

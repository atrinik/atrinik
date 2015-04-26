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
 * Map header file. */

#ifndef MAP_H
#define MAP_H

/** Map start X offset */
#define MAP_START_XOFF 386

/** Map start Y offset */
#define MAP_START_YOFF 143

/** Map tile position Y offset */
#define MAP_TILE_POS_YOFF 23

/** Map tile position Y offset 2 */
#define MAP_TILE_POS_YOFF2 12

/** Map tile position X offset */
#define MAP_TILE_POS_XOFF 48

/** Map tile position X offset 2 */
#define MAP_TILE_POS_XOFF2 24

/** Map tile X offset */
#define MAP_TILE_XOFF 12

/** Map tile Y offset */
#define MAP_TILE_YOFF 24

/**
 * @defgroup LAYER_xxx Layer types
 * The layer types used for different objects.
 *@{*/
/** System objects. */
#define LAYER_SYS 0
/** Floor. */
#define LAYER_FLOOR 1
/** Floor masks. */
#define LAYER_FMASK 2
/** Items: weapons, armour, books, etc. */
#define LAYER_ITEM 3
/** Another layer for items, often decoration. */
#define LAYER_ITEM2 4
/** Walls. */
#define LAYER_WALL 5
/** Living objects like players and monsters. */
#define LAYER_LIVING 6
/** Spell effects. */
#define LAYER_EFFECT 7
/*@}*/

/**
 * The number of object layers. */
#define NUM_LAYERS 7
/**
 * Number of sub-layers. */
#define NUM_SUB_LAYERS 7
/**
 * Effective number of all the visible layers. */
#define NUM_REAL_LAYERS (NUM_LAYERS * NUM_SUB_LAYERS)

#define GET_MAP_LAYER(_layer, _sub_layer) \
    (NUM_LAYERS * (_sub_layer) + (_layer) - 1)

/** Multi part object tile structure */
typedef struct _multi_part_tile {
    /** X-offset */
    int xoff;

    /** Y-offset */
    int yoff;
} _multi_part_tile;

/** Table of predefined multi arch objects.
 * mpart_id and mpart_nr in the arches are committed from server
 * to analyze the exact tile position inside a mpart object.
 *
 * The way of determinate the starting and shift points is explained
 * in the dev/multi_arch folder of the arches, where the multi arch templates &
 * masks are. */
typedef struct _multi_part_obj {
    /** Natural xlen of the whole multi arch */
    int xlen;

    /** Same for ylen */
    int ylen;

    /** Tile */
    _multi_part_tile part[16];
} _multi_part_obj;

/** Map data structure */
typedef struct _mapdata {
    /** Map name. */
    char name[HUGE_BUF];

    /** New map name. */
    char name_new[HUGE_BUF];

    /** Region's name. */
    char region_name[MAX_BUF];

    /** Whether the region itself actually has map. */
    bool region_has_map;

    /** Region's long name. */
    char region_longname[MAX_BUF];

    /** Map path. */
    char map_path[HUGE_BUF];

    uint32_t name_fadeout_start;

    /** X length. */
    int xlen;

    /** Y length. */
    int ylen;

    /** Position X. */
    int posx;

    /** Position Y. */
    int posy;

    /**
     * If set, height difference will be taken into account when rendering
     * tiles (even if they are not FoW tiles).
     */
    int height_diff : 1;

    /**
     * If 1, the player is currently in a building.
     */
    int in_building : 1;

    /**
     * Player's current sub-layer.
     */
    uint8_t player_sub_layer;

    /**
     * Region map.
     */
    struct region_map *region_map;
} _mapdata;

/**
 * Map cell structure.
 */
typedef struct MapCell {
    /** Name of player on this cell. */
    char pname[NUM_REAL_LAYERS][64];

    /** Player name color on this cell. */
    char pcolor[NUM_REAL_LAYERS][COLOR_BUF];

    /** Position. */
    uint8_t quick_pos[NUM_REAL_LAYERS];

    /** If this is where our enemy is. */
    uint8_t probe[NUM_REAL_LAYERS];

    /** Cell darkness. */
    uint8_t darkness[NUM_SUB_LAYERS];

    /** Object flags. */
    uint8_t flags[NUM_REAL_LAYERS];

    /** Double drawing. */
    uint8_t draw_double[NUM_REAL_LAYERS];

    /** Alpha value. */
    uint8_t alpha[NUM_REAL_LAYERS];

    /** Faces. */
    int16_t faces[NUM_REAL_LAYERS];

    /** Height of this maptile. */
    int16_t height[NUM_REAL_LAYERS];

    /** Zoom X. */
    int16_t zoom_x[NUM_REAL_LAYERS];

    /** Zoom Y. */
    int16_t zoom_y[NUM_REAL_LAYERS];

    /** Align. */
    int16_t align[NUM_REAL_LAYERS];

    /** Rotate. */
    int16_t rotate[NUM_REAL_LAYERS];

    /** Whether to show the object in red. */
    uint8_t infravision[NUM_REAL_LAYERS];

    /** How we stretch this is really 8 char for N S E W. */
    int32_t stretch[NUM_SUB_LAYERS];

    /**
     * Target object.
     */
    uint32_t target_object_count[NUM_REAL_LAYERS];

    /**
     * Whether the target is a friend.
     */
    uint8_t target_is_friend[NUM_REAL_LAYERS];

    uint8_t anim_last[NUM_REAL_LAYERS];

    uint8_t anim_speed[NUM_REAL_LAYERS];

    uint8_t anim_facing[NUM_REAL_LAYERS];

    uint8_t anim_state[NUM_REAL_LAYERS];

    uint8_t anim_flags[NUM_SUB_LAYERS];

    /**
     * Whether Fog of War is enabled on this cell.
     */
    uint8_t fow;

    uint8_t priority[NUM_SUB_LAYERS];
} MapCell;

#define MAP_STARTX map_width * (MAP_FOW_SIZE / 2)
#define MAP_STARTY map_height * (MAP_FOW_SIZE / 2)
#define MAP_WIDTH map_width
#define MAP_HEIGHT map_height

#define MAP_CELL_GET(_x, _y) (&cells[(_y) * (map_width * MAP_FOW_SIZE) + (_x)])
#define MAP_CELL_GET_MIDDLE(_x, _y) \
    (&cells[((_y) + map_height * (MAP_FOW_SIZE / 2)) * \
    (map_width * MAP_FOW_SIZE) + (_x) + map_width * (MAP_FOW_SIZE / 2)])

typedef struct map_target_struct {
    uint32_t count;
    int x;
    int y;
} map_target_struct;

/** Font used for the map name. */
#define MAP_NAME_FONT FONT_SERIF14

/** Time in milliseconds for fade out/in effect of the map name. */
#define MAP_NAME_FADEOUT 500

/**
 * Maximum height difference between the rendered tile and the player's tile.
 *
 * Tiles that are lower/higher than this (relative to the player) will not
 * be rendered.
 *
 * Only applicable to tiles that are in the Fog of War, or if
 * MapData::height_diff is set.
 */
#define HEIGHT_MAX_RENDER 50

/**
 * @defgroup ANIM_xxx Animation types
 * Animation types.
 *@{*/
/** Damage animation. */
#define ANIM_DAMAGE     1
/** Kill animation. */
#define ANIM_KILL       2
/*@}*/

/**
 * Map animation structure.
 */
typedef struct map_anim {
    struct map_anim *next; ///< Next animation.
    struct map_anim *prev; ///< Previous animation.

    int type; ///< Type of the animation, one of @ref ANIM_xxx.
    int value; ///< This is the number to display.
    int mapx; ///< Map position X.
    int mapy; ///< Map position Y.

    int x; ///< X position.
    int y; ///< Y position.
    double xoff; ///< Movement in X per tick.
    double yoff; ///< Movement in Y per tick.

    uint32_t start_tick; ///< The time we started this anim.
    uint32_t last_tick; ///< This is the end-tick.
} map_anim_t;

#endif

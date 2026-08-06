/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Map header file.
 */

#ifndef MAP_H
#define MAP_H

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
 * Number of off-screen tile anchors requested on each edge of the logical
 * look window. Large isometric sprites can project into the viewport from
 * these tiles even though their owning tile is outside it.
 */
#define MAP_RENDER_OVERSCAN 2

/** Convert a user-selected logical look size to the map protocol size. */
#define MAP_LOOK_TO_WIRE_SIZE(_size) ((_size) + MAP_RENDER_OVERSCAN * 2)

/** Convert a map protocol size back to the user-selected logical look size. */
#define MAP_WIRE_TO_LOOK_SIZE(_size) ((_size) - MAP_RENDER_OVERSCAN * 2)

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
 * The number of object layers.
 */
#define NUM_LAYERS MAP2_PROTOCOL_OBJECT_LAYERS
/**
 * Number of sub-layers.
 */
#define NUM_SUB_LAYERS MAP2_PROTOCOL_SUB_LAYERS
/**
 * Effective number of all the visible layers.
 */
#define NUM_REAL_LAYERS MAP2_PROTOCOL_REAL_LAYERS

#define GET_MAP_LAYER(_layer, _sub_layer) (NUM_LAYERS * (_sub_layer) + (_layer) - 1)

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
    unsigned int height_diff : 1;

    /**
     * If 1, the player is currently in a building.
     */

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
    /** Name of the living object on each sub-layer. */
    char pname[NUM_SUB_LAYERS][64];

    /** Living-object name color on each sub-layer. */
    char pcolor[NUM_SUB_LAYERS][COLOR_BUF];

    /** Position. */
    uint8_t quick_pos[NUM_REAL_LAYERS];

    /** Target HP percentage for the living object on each sub-layer. */
    uint8_t probe[NUM_SUB_LAYERS];

    /** Normalized cell light levels: zero is unlit, 255 is fully lit. */
    uint8_t light_level[NUM_SUB_LAYERS];

    /** Whether each light level has been received from the server. */
    uint8_t light_known[NUM_SUB_LAYERS];

    /** Object flags. */
    uint8_t flags[NUM_REAL_LAYERS];

    /** Whether fogged geometry is an authoritative structural boundary. */
    uint8_t structural_fow;

    /** Whether terrain stretch must be recomputed for this cell. */
    uint8_t stretch_dirty;

    /** Topmost nonzero floor height, cached for negative terrain seams. */
    int16_t stretch_top_height;

    /** Topmost nonzero floor height above the base sub-layer. */
    int16_t stretch_upper_height;

    /** Maximum nonnegative floor elevation supporting linked upper levels. */
    int16_t level_support_height;

    /** Server-provided base-map elevation used to project linked upper levels. */
    int16_t structural_support_height;

    /** Maximum floor/effect elevation used for screen-space rejection. */
    int16_t render_max_height;

    /** Whether a wall-layer object is a roof/camera surface. */
    uint8_t roof[NUM_REAL_LAYERS];

    /** Door bits for each object layer, grouped by sub-layer. */
    uint8_t door[NUM_SUB_LAYERS];

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

    /** Targetable living-object ID on each sub-layer. */
    uint32_t target_object_count[NUM_SUB_LAYERS];

    /** Whether the targetable living object on each sub-layer is a friend. */
    uint8_t target_is_friend[NUM_SUB_LAYERS];

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

    uint8_t secondpass[NUM_SUB_LAYERS];

    char glow[NUM_REAL_LAYERS][COLOR_BUF];
    uint8_t glow_speed[NUM_REAL_LAYERS];
    uint8_t glow_state[NUM_REAL_LAYERS];
} MapCell;

#define MAP_STARTX map_width *(MAP_FOW_SIZE / 2)
#define MAP_STARTY map_height *(MAP_FOW_SIZE / 2)
#define MAP_WIDTH map_width
#define MAP_HEIGHT map_height

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
#define ANIM_DAMAGE 1
/** Kill animation. */
#define ANIM_KILL 2
/*@}*/

/**
 * Map animation structure.
 */
typedef struct map_anim {
    struct map_anim *next; ///< Next animation.
    struct map_anim *prev; ///< Previous animation.

    int type; ///< Type of the animation, one of @ref ANIM_xxx.
    int sub_layer; ///< Sub-layer the damage is happening on.
    int8_t depth; ///< Linked-map depth where the animation occurred.
    int value; ///< This is the number to display.
    int mapx; ///< Map position X.
    int mapy; ///< Map position Y.

    double xoff; ///< Movement in X per tick.
    double yoff; ///< Movement in Y per tick.

    uint32_t start_tick; ///< The time we started this anim.
    uint32_t last_tick; ///< This is the end-tick.
} map_anim_t;

/** Public API implemented in src/gui/widgets/map.c. */

extern _mapdata MapData;

extern _multi_part_obj MultiArchs[16];

extern struct map_anim *
map_anims_add(int type, int mapx, int mapy, int sub_layer, int depth, int value);

extern void maps_anims_remove(map_anim_t *anim);

extern void map_anims_mapscroll(int xoff, int yoff);

extern void map_anims_clear(void);

extern void map_anims_play(void);

extern int map_anims_need_redraw(void);

extern void load_mapdef_dat(void);

extern void clear_map(_Bool hard);

extern void map_update_size(int w, int h);

extern void display_mapscroll(int dx, int dy, int old_w, int old_h);

extern void update_map_name(const char *name);

extern void update_map_weather(const char *weather);

extern void update_map_height_diff(uint8_t height_diff);

extern void update_map_region_name(const char *region_name);

extern void update_map_region_longname(const char *region_longname);

extern void update_map_path(const char *map_path);

extern int map_get_player_direction(void);

extern void map_get_real_coords(int *x, int *y);

extern void init_map_data(int xl, int yl, int px, int py);

extern void adjust_tile_stretch(void);

extern void map_set_data(int x,
                         int y,
                         int layer,
                         int16_t face,
                         uint8_t quick_pos,
                         uint8_t obj_flags,
                         const char *name,
                         const char *name_color,
                         int16_t height,
                         uint8_t probe,
                         int16_t zoom_x,
                         int16_t zoom_y,
                         int16_t align,
                         uint8_t draw_double,
                         uint8_t alpha,
                         int16_t rotate,
                         uint8_t infravision,
                         uint32_t target_object_count,
                         uint8_t target_is_friend,
                         uint8_t anim_speed,
                         uint8_t anim_facing,
                         uint8_t anim_flags,
                         uint8_t anim_state,
                         uint8_t priority,
                         uint8_t secondpass,
                         uint8_t roof,
                         uint8_t door,
                         const char *glow,
                         uint8_t glow_speed);

extern bool map_select_level(int depth, bool create);

extern void map_set_level_mask(uint16_t mask);

extern void map_level_scroll(int dz);

extern void map_clear_cell(int x, int y, bool hard);

extern void map_set_structural_support_height(int x, int y, int16_t height);

extern void map_set_fow(int x, int y, bool fow);

extern bool map_get_fow(int x, int y);

extern void map_set_light_level(int x, int y, int sub_layer, uint8_t light_level);

extern void map_animate(void);

extern void map_draw_map(SDL_Surface *surface);

extern void map_draw_one(int x, int y, SDL_Surface *surface);

extern void map_target_handle(uint8_t is_friend);

extern bool mouse_to_tile_coords(int mx, int my, int *tx, int *ty);

extern bool map_mouse_fire(void);

extern void widget_map_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/minimap.c. */

extern bool minimap_redraw_due(void);

extern void widget_minimap_init(widgetdata *widget);

#endif

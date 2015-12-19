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
 * The mapstruct is allocated each time a new map is opened.
 * It contains pointers (very indirectly) to all objects on the map.
 */

#ifndef MAP_H
#define MAP_H

/** Number of darkness level. Add +1 for "total dark" */
#define MAX_DARKNESS 7

/** For exit objects: this is a player unique map */
#define MAP_PLAYER_MAP 1

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
#define NUM_LAYERS 7
/**
 * Number of sub-layers.
 */
#define NUM_SUB_LAYERS 7
/**
 * Effective number of all the visible layers.
 */
#define NUM_REAL_LAYERS (NUM_LAYERS * NUM_SUB_LAYERS)

/**
 * @defgroup map_struct_macros Map structure macros
 * These macros are used to access flags of a map, the map reset time,
 * the map owner, etc.
 *@{*/

/** This is when the map will reset */
#define MAP_WHEN_RESET(m)      ((m)->reset_time)
/** The map reset timeout */
#define MAP_RESET_TIMEOUT(m)   ((m)->reset_timeout)
/** The map difficulty */
#define MAP_DIFFICULTY(m)      ((m)->difficulty)
/** The map timeout */
#define MAP_TIMEOUT(m)         ((m)->timeout)
/** The map swap time */
#define MAP_SWAP_TIME(m)       ((m)->swap_time)
/** Is the map outdoors map? */
#define MAP_OUTDOORS(m)        ((m)->map_flags & MAP_FLAG_OUTDOOR)
/** Is the map unique map? */
#define MAP_UNIQUE(m)          ((m)->map_flags & MAP_FLAG_UNIQUE)
/** Does the map have fixed reset time? */
#define MAP_FIXED_RESETTIME(m) ((m)->map_flags & MAP_FLAG_FIXED_RTIME)
/** Is the map no-save map? */
#define MAP_NOSAVE(m)          ((m)->map_flags & MAP_FLAG_NO_SAVE)
/** Does the map disallow magic? */
#define MAP_NOMAGIC(m)         ((m)->map_flags & MAP_FLAG_NOMAGIC)
/*
 * Height difference will be taken into account when rendering the map.
 */
#define MAP_HEIGHT_DIFF(m)     ((m)->map_flags & MAP_FLAG_HEIGHT_DIFF)
/** Does the map disallow harmful spells? */
#define MAP_NOHARM(m)          ((m)->map_flags & MAP_FLAG_NOHARM)
/** Does the map disallow summoning spells? */
#define MAP_NOSUMMON(m)        ((m)->map_flags & MAP_FLAG_NOSUMMON)
/** Is the map a fixed login map? */
#define MAP_FIXEDLOGIN(m)      ((m)->map_flags & MAP_FLAG_FIXED_LOGIN)
/** Are players unable to save on this map? */
#define MAP_PLAYER_NO_SAVE(m) ((m)->map_flags & MAP_FLAG_PLAYER_NO_SAVE)
/** Is the map PVP area? */
#define MAP_PVP(m)             ((m)->map_flags & MAP_FLAG_PVP)
/** Darkness of a map */
#define MAP_DARKNESS(m)        (m)->darkness
/** Width of a map */
#define MAP_WIDTH(m)           (m)->width
/** Height of a map */
#define MAP_HEIGHT(m)          (m)->height
/**
 * Convenience function - total number of spaces on map is used in many
 * places.
 */
#define MAP_SIZE(m)            ((m)->width * (m)->height)
/** Enter X position of a map */
#define MAP_ENTER_X(m)         (m)->enter_x
/** Enter Y position of a map */
#define MAP_ENTER_Y(m)         (m)->enter_y
/*@}*/

/**
 * @defgroup map_load_flags Map loading flags
 * Flags passed to map loading functions such as ready_map_name() and
 * load_original_map().
 *@{*/

/**
 * Always load map from the map directory, and don't do unique items or
 * the like.
 */
#define MAP_FLUSH         1
/**
 * This map is player-specific. Don't do any more name translation on
 * it.
 */
#define MAP_PLAYER_UNIQUE 2
/** Unused. */
#define MAP_BLOCK         4
/** Active objects shouldn't be put on active list. */
#define MAP_STYLE         8
/** Unused. */
#define MAP_ARTIFACT      16
/** Indicates that the name string is a shared string */
#define MAP_NAME_SHARED   32
/** Original map. Generate treasures */
#define MAP_ORIGINAL      64
/** No dynamic maps. */
#define MAP_NO_DYNAMIC    128
/*@}*/

/**
 * @defgroup map_memory_flags Map memory flags
 * Map memory flags, used from @ref mapstruct::in_memory.
 *@{*/

/** Map is fully loaded. */
#define MAP_IN_MEMORY   1
/** Map spaces have been saved to disk. */
#define MAP_SWAPPED     2
/** This map is being loaded. */
#define MAP_LOADING     3
/** Map being saved. Will stop object_remove() from some processing. */
#define MAP_SAVING      4
/*@}*/

/* new macros for map layer system */
#define GET_MAP_SPACE_PTR(M_, X_, Y_) \
    (&((M_)->spaces[(X_) + (M_)->width * (Y_)]))

#define GET_MAP_SPACE_FIRST(M_) \
    ((M_)->first)
#define GET_MAP_SPACE_LAST(M_) \
    ((M_)->last)
#define GET_MAP_SPACE_LAYER(M_, L_, SL_) \
    ((M_)->layer[NUM_LAYERS * (SL_) + (L_) -1])

#define SET_MAP_SPACE_FIRST(M_, O_) \
    ((M_)->first = (O_))
#define SET_MAP_SPACE_LAST(M_, O_) \
    ((M_)->last = (O_))
#define SET_MAP_SPACE_LAYER(M_, L_, SL_, O_) \
    ((M_)->layer[NUM_LAYERS * (SL_) + (L_) -1] = (O_))

#define GET_MAP_UPDATE_COUNTER(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].update_tile)

#define INC_MAP_UPDATE_COUNTER(M, X, Y) \
    ((M)->spaces[((X) + (M)->width * (Y))].update_tile++)

#define GET_MAP_MOVE_FLAGS(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].move_flags)
#define SET_MAP_MOVE_FLAGS(M, X, Y, C) \
    ((M)->spaces[(X) + (M)->width * (Y)].move_flags = C)
#define GET_MAP_FLAGS(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].flags)
#define SET_MAP_FLAGS(M, X, Y, C) \
    ((M)->spaces[(X) + (M)->width * (Y)].flags = C)
#define GET_MAP_LIGHT(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].light_value)
#define SET_MAP_LIGHT(M, X, Y, L) \
    ((M)->spaces[(X) + (M)->width * (Y)].light_value = L)

#define GET_MAP_OB(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].first)
#define GET_MAP_OB_LAST(M, X, Y) \
    ((M)->spaces[(X) + (M)->width * (Y)].last)
#define GET_MAP_OB_LAYER(_M_, _X_, _Y_, _Z_, _SL_) \
    ((_M_)->spaces[(_X_) + (_M_)->width * (_Y_)].layer[NUM_LAYERS * (_SL_) + (_Z_) -1])

#define SET_MAP_DAMAGE(M, X, Y, SUB_LAYER, tmp) \
    ((M)->spaces[(X) + (M)->width * (Y)].last_damage[(SUB_LAYER)] = (int16_t) (tmp))
#define GET_MAP_DAMAGE(M, X, Y, SUB_LAYER) \
    ((M)->spaces[(X) + (M)->width * (Y)].last_damage[(SUB_LAYER)])

#define SET_MAP_RTAG(M, X, Y, SUB_LAYER, tmp) \
    ((M)->spaces[(X) + (M)->width * (Y)].round_tag[(SUB_LAYER)] = (uint32_t) (tmp))
#define GET_MAP_RTAG(M, X, Y, SUB_LAYER) \
    ((M)->spaces[(X) + (M)->width * (Y)].round_tag[(SUB_LAYER)])

#define GET_BOTTOM_MAP_OB(O) ((O)->map ? GET_MAP_OB((O)->map, (O)->x, (O)->y) : NULL)

/**
 * You should really know what you are doing before using this - you
 * should almost always be using get_map_from_coord() instead, which
 * takes into account map tiling.
 */
#define OUT_OF_MAP(M, X, Y) \
    ((X) < 0 || (Y) < 0 || (X) >= (M)->width || (Y) >= (M)->height)

/**
 * @defgroup map_look_flags Map look flags
 * These are used in the MapLook flags element.  They are not used in
 * in the object flags structure.
 *@{*/

/** Blocks the view. */
#define P_BLOCKSVIEW          0x01
/** Spells (some) can't pass this object */
#define P_NO_MAGIC            0x02
/** Nothing can pass (wall() is true) */
#define P_NO_PASS             0x04
/** There is one or more player on this tile */
#define P_IS_PLAYER           0x08
/** There is a monster on this square. */
#define P_IS_MONSTER            0x10
/** There is an exit on this tile. */
#define P_IS_EXIT             0x20
/**
 * Only players are allowed to enter this space. This excludes mobs
 * and golems but also spell effects and thrown / fired items.
 * It works like a no_pass for players only (pass_thru doesn't work for
 * it).
 */
#define P_PLAYER_ONLY         0x40
/**
 * A closed door is blocking this space - if we want to approach, we must
 * first check if it's possible to open it.
 */
#define P_DOOR_CLOSED         0x80
/**
 * We have something like an inventory checker in this tile node.
 */
#define P_CHECK_INV           0x100
/**
 * PvP is not possible on this tile.
 */
#define P_NO_PVP              0x200
/**
 * Same as NO_PASS - but objects with PASS_THRU set can cross it.
 *
 * @note If a node has NO_PASS and P_PASS_THRU set, there are 2 objects
 * in the node, one with pass_thru and one with real no_pass - then
 * no_pass will overrule pass_thru
 */
#define P_PASS_THRU           0x400
/** For moving objects and what happens when they enter */
#define P_WALK_ON             0x1000
/** For moving objects and what happens when they leave */
#define P_WALK_OFF            0x2000
/** For flying objects and what happens when they leave */
#define P_FLY_OFF             0x4000
/** For flying objects and what happens when they enter */
#define P_FLY_ON              0x8000
/** There is a @ref MAGIC_MIRROR object on this tile. */
#define P_MAGIC_MIRROR 0x10000
/** The tile has object with 'outdoor 1' flag. */
#define P_OUTDOOR 0x20000
/**
 * Of course not set for map tiles but from blocked_xx() function where
 * the get_map_from_coord() fails to grab a valid map or tile.
 */
#define P_OUT_OF_MAP          0x4000000
/** Skip the layer update, do flags only */
#define P_FLAGS_ONLY          0x8000000
/** If set, update the flags by looping the map objects */
#define P_FLAGS_UPDATE        0x10000000
/** Resort the layer when updating */
#define P_NEED_UPDATE         0x20000000
/**
 * Purely temporary - if set, update_position
 * does not complain if the flags are different.
 */
#define P_NO_ERROR            0x40000000
/**
 * Do <b>NOT</b> use this with SET_MAP_FLAGS(). This is just to mark for
 * return values of blocked().
 */
#define P_NO_TERRAIN          0x80000000
/*@}*/

/**
 * @defgroup MSP_EXTRA_xxx Map space extra flags
 * Map space extra flags
 *@{*/
/** No harmful spells. */
#define MSP_EXTRA_NO_HARM 1
/** No PvP. */
#define MSP_EXTRA_NO_PVP 2
/** No magic. */
#define MSP_EXTRA_NO_MAGIC 4
/** The tile is part of a building. */
#define MSP_EXTRA_IS_BUILDING 8
/** The tile is a balcony. */
#define MSP_EXTRA_IS_BALCONY 16
/** The tile shows objects under it in a building. */
#define MSP_EXTRA_IS_OVERLOOK 32
/*@}*/

/** Single tile on a map */
typedef struct MapSpace_s {
    /** Start of the objects on this map tile */
    object *first;

    /** Array of visible layer objects. */
    object *layer[NUM_REAL_LAYERS];

    /** Last object in this list */
    object *last;

    /** Map info object bound to this tile. */
    object *map_info;

    /** Ambient sound effect object bound to this tile. */
    object *sound_ambient;

    /** Used to create chained light source list. */
    struct MapSpace_s *prev_light;

    /** Used to create chained light source list. */
    struct MapSpace_s *next_light;

    /** Tag for last_damage */
    uint32_t round_tag[NUM_SUB_LAYERS];

    /** ID of ::map_info. */
    tag_t map_info_count;

    /** ID of ::sound_ambient. */
    tag_t sound_ambient_count;

    /** Counter for update tile */
    uint32_t update_tile;

    /** Light source counter - the higher the brighter light source here */
    int32_t light_source;

    /**
     * How much light is on this tile. 0 = total dark
     * 255+ = full daylight.
     */
    int32_t light_value;

    /**
     * Flags about this space
     * @see map_look_flags
     */
    int flags;

    /** last_damage tmp backbuffer */
    int16_t last_damage[NUM_SUB_LAYERS];

    /** Terrain type flags (water, underwater,...) */
    uint16_t move_flags;

    /** Extra flags from @ref MSP_EXTRA_xxx. */
    uint8_t extra_flags;
} MapSpace;

/**
 * @defgroup map_flags Map flags
 * Map flags for global map settings, used in @ref mapstruct::map_flags.
 *@{*/
/** Map is outdoor map - daytime effects are on */
#define MAP_FLAG_OUTDOOR 1
/** Special unique map  */
#define MAP_FLAG_UNIQUE 2
/**
 * If true, reset time is not affected by players entering / exiting
 * map
 */
#define MAP_FLAG_FIXED_RTIME 4
/** No wizardry based spells */
#define MAP_FLAG_NOMAGIC 8
/**
 * Height difference will be taken into account when rendering the map.
 */
#define MAP_FLAG_HEIGHT_DIFF 16
/** No harmful spells like fireball, magic bullet, etc. */
#define MAP_FLAG_NOHARM 32
/**
 * Don't allow any summoning spells.
 */
#define MAP_FLAG_NOSUMMON 64
/**
 * When set, a player login on this map will be forced to default
 * @ref mapstruct::enter_x and @ref mapstruct::enter_y of this map.
 * This avoids getting stuck in a map and treasure camping.
 */
#define MAP_FLAG_FIXED_LOGIN 128
/** Players cannot save on this map. */
#define MAP_FLAG_PLAYER_NO_SAVE 256
/** Unused. */
#define MAP_FLAG_UNUSED2 1024
/** Unused. */
#define MAP_FLAG_UNUSED3 2048
/** PvP is possible on this map. */
#define MAP_FLAG_PVP 4096
/** Don't save maps - only used with unique maps. */
#define MAP_FLAG_NO_SAVE 8192
/*@}*/

/**
 * @defgroup map_default_values Default map values
 * Nonzero default map values.
 *@{*/

/** Default darkness */
#define MAP_DEFAULT_DARKNESS    0
/*@}*/

/**
 * This is a game region.
 *
 * Each map is in a given region of the game world and links to a region
 * definition.
 */
typedef struct region_struct {
    /** Pointer to next region, NULL for the last one */
    struct region_struct *next;

    /**
     * Pointer to the region that is a parent of the current
     * region, if a value isn't defined in the current region
     * we traverse this series of pointers until it is.
     */
    struct region_struct *parent;

    /** Shortened name of the region as maps refer to it */
    char *name;

    /**
     * So that parent and child regions can be defined in
     * any order, we keep hold of the parent_name during
     * initialization, and the children get assigned to their
     * parents later. (before runtime on the server though)
     * nothing outside the init code should ever use this value.
     */
    char *parent_name;

    /**
     * Official title of the region, this might be defined
     * to be the same as name
     */
    char *longname;

    /** The description of the region. */
    char *msg;

    /** Where a player that is arrested in this region should be imprisoned. */
    char *jailmap;

    /** One of the maps in the region. */
    char *map_first;

    /** Client map background to use: if not set, will use transparency. */
    char *map_bg;

    /** X coodinate in jailmap to which the player should be sent. */
    int16_t jailx;

    /** Y coodinate in jailmap to which the player should be sent. */
    int16_t jaily;

    bool child_maps:1; ///< If true, map of this region has all the children.
} region_struct;

/**
 * A single map event, holding a pointer to map event object on map.
 */
typedef struct map_event {
    /** Next map event in linked list. */
    struct map_event *next;

    /** Pointer to the actual map event. */
    object *event;

    /**
     * Plugin the map event object is using.
     */
    struct atrinik_plugin *plugin;
} map_event;

typedef struct map_exit {
    struct map_exit *next;

    struct map_exit *prev;

    object *obj;
} map_exit_t;

struct path_node;

/**
 * In general, code should always use the macros above (or functions in
 * map.c) to access many of the values in the map structure. Failure to
 * do this will almost certainly break various features.
 *
 * You may think it is safe to look at width and height values directly
 * (or even through the macros), but doing so will completely break map
 * tiling.
 */
typedef struct mapdef {
    /**
     * Next map in a doubly-linked list.
     * @private

 */
    struct mapdef *next;

    /**
     * Previous map in a doubly-linked list.
     * @private

 */
    struct mapdef *prev;

    /** Any maps tiled together to this one */
    struct mapdef *tile_map[TILED_NUM];

    /** Name of map as given by its creator */
    shstr *name;

    /** Background music of the map */
    shstr *bg_music;

    /** Weather effect active on this map. */
    shstr *weather;

    /** Name of temporary file */
    char *tmpname;

    /** Message map creator may have left */
    char *msg;

    /** Filename of the map (shared string now) */
    shstr *path;

    /** Path to adjoining maps (shared strings) */
    shstr *tile_path[TILED_NUM];

    /** Array of spaces on this map */
    MapSpace *spaces;

    /** List of tile spaces with light sources */
    MapSpace *first_light;

    /** Pointer to the region this map is in. */
    region_struct *region;

    /** Map-wide events for this map. */
    struct map_event *events;

    map_exit_t *exits;

    /** Linked list of linked lists of buttons */
    objectlink *buttons;

    /** Linked list of linked spawn points. */
    objectlink *linked_spawn_points;

    /** Chained list of players on this map */
    object *player_first;

    /** Bitmap used for marking visited tiles in pathfinding */
    uint32_t *bitmap;

    /** For which traversal is @ref mapstruct::bitmap valid. */
    uint32_t pathfinding_id;

    struct path_node **path_nodes;

    /** Map flags for various map settings */
    uint32_t map_flags;

    /** When this map should reset */
    uint32_t reset_time;

    /**
     * How many seconds must elapse before this map
     * should be reset
     */
    uint32_t reset_timeout;

    /** When this reaches 0, the map will be swapped out. */
    int32_t timeout;

    /** How long to wait before a map is swapped out. */
    uint32_t swap_time;

    /**
     * If not true, the map has been freed and must
     * be loaded before used. The map, omap and map_ob
     * arrays will be allocated when the map is loaded
     */
    uint32_t in_memory;

    /** Used by relative_tile_position() to mark visited maps */
    uint32_t traversed;

    /**
     * Indicates the base light value on this map.
     *
     * This value is only used when the map is not marked as outdoor.
     * @see MAP_DEFAULT_DARKNESS
     */
    int darkness;

    /**
     * The real light_value, built out from darkness and possible other
     * factors.
     */
    int light_value;

    /**
     * What level the player should be to play here. Affects treasures,
     * random shops and various other things.
     */
    int difficulty;

    /** Height of the map. */
    int height;

    /** Width of the map. */
    int width;

    /**
     * Used to indicate the X position of where to put the player when he
     * logs in to the map if the map has flag MAP_FLAG_FIXED_LOGIN set.
     *
     * Also used by exits as the default X location if the exit doesn't
     * have one set.
     * @see MAP_FLAG_FIXED_LOGIN
     */
    int enter_x;

    /**
     * Used to indicate the Y position of where to put the player when he
     * logs in to the map if the map has flag MAP_FLAG_FIXED_LOGIN set.
     *
     * Also used by exits as the default Y location if the exit doesn't
     * have one set.
     * @see MAP_FLAG_FIXED_LOGIN
     */
    int enter_y;

    int16_t coords[3]; ///< X, Y and Z coordinates.

    int8_t level_min; ///< Minimum level offset that is part of this map.

    int8_t level_max; ///< Maximum level offset that is part of this map.

    bool global_removed; ///< If true, the map was removed from the global list.

    tag_t count; ///< Unique identifier for the map.
} mapstruct;

/**
 * Checks if the specified map level offset is part of the map.
 * @param _m
 * Map.
 * @param _z
 * Level offset.
 */
#define MAP_TILE_IS_SAME_LEVEL(_m, _z) \
    ((_m)->coords[2] + (_z) >= (_m)->level_min && \
    (_m)->coords[2] + (_z) <= (_m)->level_max)

#define MAP_TILES_WALK_INTERNAL(_m, _fnc, ...) \
    if (__ret == 0) { \
        __ret = (_fnc)((_m), (_m), ##__VA_ARGS__); \
    } \
\
    for (__idx = 0; __ret == 0 && __idx < TILED_NUM_DIR; __idx++) { \
        if ((_m)->tile_map[__idx] == NULL || \
                (_m)->tile_map[__idx]->in_memory != MAP_IN_MEMORY) { \
            continue; \
        } \
\
        __ret = (_fnc)((_m)->tile_map[__idx], (_m), ##__VA_ARGS__); \
    }

/**
 * Walks all the tiled maps, including up/down maps that are on the same level
 * as the specified map, including the direction-based tiled maps of those
 * up/down maps.
 * @param _m
 * The map to walk.
 * @param _fnc
 * Function to apply to each map. This should have 'int' return
 * value, which indicates whether to keep going or not. Non-zero return value
 * will stop the walk, and can be retrieved with MAP_TILES_WALK_RETVAL. Two
 * arguments are automatically supplied to the function: the first one is the
 * tiled map, and the second one is either whatever map was supplied through
 * the _m parameter, or a map that is tiled up/down to that map. This means that
 * you can use the second 'map' parameter to do things like non-recursive
 * distance calculation (since you need the specific map level for that, but you
 * can still re-use the X/Y map tile coordinates).
 * @param ...
 * Additional arguments supplied to the function.
 */
#define MAP_TILES_WALK_START(_m, _fnc, ...) \
{ \
    int __ret, __idx, __tile_id, __z; \
    mapstruct *__tile; \
\
    __ret = 0; \
\
    if ((_m)->in_memory == MAP_IN_MEMORY) { \
        MAP_TILES_WALK_INTERNAL(_m, _fnc, ##__VA_ARGS__); \
    } \
\
    for (__tile_id = TILED_UP; \
            __ret == 0 && __tile_id <= TILED_DOWN; \
            __tile_id++) { \
        for (__tile = (_m)->tile_map[__tile_id], \
                __z = __tile_id == TILED_UP ? 1 : -1; \
                __ret == 0 && __tile != NULL && \
                __tile->in_memory == MAP_IN_MEMORY && \
                MAP_TILE_IS_SAME_LEVEL(_m, __z); \
                __tile = __tile->tile_map[__tile_id], \
                __z += __tile_id == TILED_UP ? 1 : -1) { \
            MAP_TILES_WALK_INTERNAL(__tile, _fnc, ##__VA_ARGS__); \
        } \
    }

/**
 * Acquires the return value from the user-supplied function.
 */
#define MAP_TILES_WALK_RETVAL __ret

/**
 * Ends map tile walking block.
 */
#define MAP_TILES_WALK_END \
}

/**
 * This is used by get_rangevector() to determine where the other
 * creature is.
 */
typedef struct rv_vector_s {
    /** The distance away */
    unsigned int distance;

    /** X distance away */
    int distance_x;

    /** Y distance away */
    int distance_y;

    /** Z distance away */
    int distance_z;

    /** Atrinik direction scheme that the creature should head */
    int direction;

    /** Part of the object that is closest. */
    object *part;
} rv_vector;

/**
 * @defgroup range_vector_flags Range Vector Flags
 * Range vector flags, used by functions like get_rangevector() and
 * get_rangevector_from_mapcoords().
 *@{*/
/**
 * Calculate Manhattan distance.
 */
#define RV_MANHATTAN_DISTANCE  0x00
/**
 * Calculate Euclidian distance.
 */
#define RV_EUCLIDIAN_DISTANCE  0x01
/**
 * Calculate diagonal distance.
 */
#define RV_DIAGONAL_DISTANCE   0x02
/**
 * Do not perform distance calculation.
 */
#define RV_NO_DISTANCE         (RV_EUCLIDIAN_DISTANCE | RV_DIAGONAL_DISTANCE)

/**
 * If set, will ignore tail parts of a multi-part object in range vector
 * calculations.
 */
#define RV_IGNORE_MULTIPART    0x04

/**
 * If not set, only immediately adjacent tiled maps are searched. If set, a
 * depth-first search is performed on the tiled maps, up to some limit.
 */
#define RV_RECURSIVE_SEARCH    0x08
/**
 * Do not load any maps when attempting to calculate the range vector.
 */
#define RV_NO_LOAD             0x10
/*@}*/

/**
 * @defgroup direction_constants Direction constants
 * The direction constants.
 *@{*/
/** North. */
#define NORTH 1
/** Northeast. */
#define NORTHEAST 2
/** East. */
#define EAST 3
/** Southeast. */
#define SOUTHEAST 4
/** South. */
#define SOUTH 5
/** Southwest. */
#define SOUTHWEST 6
/** West. */
#define WEST 7
/** Northwest. */
#define NORTHWEST 8
/** Maximum direction number. */
#define NUM_DIRECTION 8
/*@}*/

/** Check if 'pl' cannot see 'ob' due to it being hidden by plugin. */
#define OBJECT_IS_HIDDEN(pl, ob) (HAS_EVENT((ob), EVENT_ASK_SHOW) && trigger_event(EVENT_ASK_SHOW, (pl), (ob), NULL, NULL, 0, 0, 0, 0) == 1)

/**
 * Maximum darkness of building walls.
 */
#define MAP_BUILDING_DARKNESS_WALL 4
/**
 * Darkness of building interiors.
 */
#define MAP_BUILDING_DARKNESS 3

#endif

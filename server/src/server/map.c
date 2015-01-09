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
 * Map related functions. */

#include <global.h>
#include <loader.h>

int global_darkness_table[MAX_DARKNESS + 1] = {
    0, 20, 40, 80, 160, 320, 640, 1280
};

/**
 * Used to get the reverse direction for all the tiled maps.
 *
 * For example: TILED_NORTH -> TILED_SOUTH
 */
int map_tiled_reverse[TILED_NUM] = {
    TILED_SOUTH, /* TILED_NORTH */
    TILED_WEST, /* TILED_EAST */
    TILED_NORTH, /* TILED_SOUTH */
    TILED_EAST, /* TILED_WEST */
    TILED_SOUTHWEST, /* TILED_NORTHEAST */
    TILED_NORTHWEST, /* TILED_SOUTHEAST */
    TILED_NORTHEAST, /* TILED_SOUTHWEST */
    TILED_SOUTHEAST, /* TILED_NORTHWEST */
    TILED_DOWN, /* TILED_UP */
    TILED_UP /* TILED_DOWN */
};

static int map_tiled_coords[TILED_NUM][3] = {
    {0, -1, 0},
    {1, 0, 0},
    {0, 1, 0},
    {-1, 0, 0},
    {1, -1, 0},
    {1, 1, 0},
    {-1, 1, 0},
    {-1, -1, 0},
    {0, 0, 1},
    {0, 0, -1},
};

#define DEBUG_OLDFLAGS 1

static void load_objects(mapstruct *m, FILE *fp, int mapflags);
static void save_objects(mapstruct *m, FILE *fp, FILE *fp2);
static void allocate_map(mapstruct *m);
static void free_all_objects(mapstruct *m);

/**
 * Try loading the connected map tile with the given number.
 * @param orig_map Base map.
 * @param tile_num Tile number to connect to.
 * @return NULL if loading or tiling fails, loaded neighbor map otherwise. */
static inline mapstruct *load_and_link_tiled_map(mapstruct *orig_map, int tile_num)
{
    mapstruct *map = ready_map_name(orig_map->tile_path[tile_num], MAP_NAME_SHARED | (MAP_UNIQUE(orig_map) ? MAP_PLAYER_UNIQUE : 0));

    if (!map || map != orig_map->tile_map[tile_num]) {
        logger_print(LOG(BUG), "Failed to connect map %s with tile #%d (%s).", STRING_SAFE(orig_map->path), tile_num, STRING_SAFE(orig_map->tile_path[tile_num]));
        FREE_AND_CLEAR_HASH(orig_map->tile_path[tile_num]);
        return NULL;
    }

    return map;
}

/**
 * Recursive part of the relative_tile_position() function.
 * @param map1
 * @param map2
 * @param x
 * @param y
 * @param z
 * @param id
 * @param level Recursion level.
 * @return
 * @todo A bidirectional breadth-first search would be more efficient. */
static int relative_tile_position_rec(mapstruct *map1, mapstruct *map2, int *x, int *y, int *z, uint32 id, int level, int flags)
{
    int i;
    map_exit_t *exit;
    mapstruct *m;

    if (map1 == map2) {
        return 1;
    }

    if (level <= 0) {
        return 0;
    }

    level--;
    map1->traversed = id;

    DL_FOREACH(map1->exits, exit)
    {
        m = exit_get_destination(exit->obj, NULL, NULL, 0);

        if (m == NULL) {
            continue;
        }

        if (m->traversed != id && (m == map2 || relative_tile_position_rec(m,
                map2, x, y, z, id, flags,
                flags & RV_RECURSIVE_SEARCH ? level : 1))) {
            *z += 1;
            return 1;
        }
    }

    if (flags & RV_RECURSIVE_SEARCH) {
        /* Depth-first search for the destination map */
        for (i = 0; i < TILED_NUM; i++) {
            if (map1->tile_path[i]) {
                if (map1->tile_map[i] == NULL ||
                        map1->tile_map[i]->in_memory != MAP_IN_MEMORY) {
                    if (flags & RV_NO_LOAD || !load_and_link_tiled_map(map1,
                            i)) {
                        continue;
                    }
                }

                if (map1->tile_map[i]->traversed != id && (map1->tile_map[i] ==
                        map2 || relative_tile_position_rec(map1->tile_map[i],
                        map2, x, y, z, id, level, flags))) {
                    switch (i) {
                    case TILED_NORTH:
                        *y -= MAP_HEIGHT(map1->tile_map[i]);
                        return 1;

                    case TILED_EAST:
                        *x += MAP_WIDTH(map1);
                        return 1;

                    case TILED_SOUTH:
                        *y += MAP_HEIGHT(map1);
                        return 1;

                    case TILED_WEST:
                        *x -= MAP_WIDTH(map1->tile_map[i]);
                        return 1;

                    case TILED_NORTHEAST:
                        *y -= MAP_HEIGHT(map1->tile_map[i]);
                        *x += MAP_WIDTH(map1);
                        return 1;

                    case TILED_SOUTHEAST:
                        *y += MAP_HEIGHT(map1);
                        *x += MAP_WIDTH(map1);
                        return 1;

                    case TILED_SOUTHWEST:
                        *y += MAP_HEIGHT(map1);
                        *x -= MAP_WIDTH(map1->tile_map[i]);
                        return 1;

                    case TILED_NORTHWEST:
                        *y -= MAP_HEIGHT(map1->tile_map[i]);
                        *x -= MAP_WIDTH(map1->tile_map[i]);
                        return 1;

                    case TILED_UP:
                        *z += 1;
                        return 1;

                    case TILED_DOWN:
                        *z -= 1;
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

/**
 * Find the distance between two map tiles on a tiled map.
 *
 * The distance from the topleft (0, 0) corner of map1 to the topleft
 * corner of map2 will be added to x and y.
 *
 * This function does not work well with asymmetrically tiled maps.
 *
 * It will also (naturally) perform bad on very large tilesets such as
 * the world map as it may need to load all tiles into memory before
 * finding a path between two tiles.
 *
 * We probably want to handle the world map as a special case,
 * considering that all tiles are of equal size, and that we might be
 * able to parse their coordinates from their names...
 * @param map1
 * @param map2
 * @param x
 * @param y
 * @param z
 * @return 1 if the two tiles are part of the same map, 0 otherwise. */
static int relative_tile_position(mapstruct *map1, mapstruct *map2, int *x, int *y, int *z, int flags)
{
    static uint32 traversal_id = 0;

    /* Save some time in the simplest cases ( very similar to on_same_map() )*/
    if (map1 == NULL || map2 == NULL) {
        return 0;
    }

    if (map1 == map2) {
        return 1;
    }

    /* Avoid overflow of traversal_id */
    if (traversal_id == 4294967295U) {
        mapstruct *m;

        logger_print(LOG(DEBUG), "resetting traversal id");

        for (m = first_map; m != NULL; m = m->next) {
            m->traversed = 0;
        }

        traversal_id = 0;
    }

    /* Recursive search */
    return relative_tile_position_rec(map1, map2, x, y, z, ++traversal_id, 2,
            flags);
}

/**
 * Check whether a specified map has been loaded already.
 * Returns the mapstruct which has a name matching the given argument.
 * @param name Shared string of the map path name.
 * @return ::mapstruct which has a name matching the given argument,
 * NULL if no such map. */
mapstruct *has_been_loaded_sh(shstr *name)
{
    mapstruct *map;

    if (!name || !*name) {
        return NULL;
    }

    if (*name != '/' && *name != '.') {
        logger_print(LOG(DEBUG), "Found map name without starting '/' or '.' (%s)", name);
        return NULL;
    }

    for (map = first_map; map; map = map->next) {
        if (name == map->path) {
            break;
        }
    }

    return map;
}

/**
 * Makes a path absolute outside the world of Atrinik.
 *
 * In other words, it prepends LIBDIR/MAPDIR/ to the given path and
 * returns the pointer to a static array containing the result.
 * @param name Path of the map.
 * @return The full path. */
char *create_pathname(const char *name)
{
    static char buf[MAX_BUF];

    if (*name == '/') {
        snprintf(buf, sizeof(buf), "%s%s", settings.mapspath, name);
    } else {
        snprintf(buf, sizeof(buf), "%s/%s", settings.mapspath, name);
    }

    return buf;
}

/**
 * This makes absolute path to the itemfile where unique objects will be
 * saved.
 *
 * Converts '/' to '@'.
 * @param s Path of the map for the items.
 * @return The absolute path. */
static char *create_items_path(shstr *s)
{
    static char buf[MAX_BUF];
    char *t;

    if (*s == '/') {
        s++;
    }

    snprintf(buf, sizeof(buf), "%s/unique-items/", settings.datapath);

    for (t = buf + strlen(buf); *s; s++, t++) {
        if (*s == '/') {
            *t = '@';
        } else {
            *t = *s;
        }
    }

    *t = '\0';
    return buf;
}

/**
 * Check if there is a wall on specified map at x, y.
 *
 * Caller should check for @ref P_PASS_THRU in the return value to see if
 * it can cross here.
 *
 * The @ref P_PLAYER_ONLY flag here is analyzed without checking the
 * caller type. That is possible because player movement related
 * functions should always used blocked().
 * @param m Map we're checking for.
 * @param x X position where to check.
 * @param y Y position where to check.
 * @return 1 if a wall is present at the given location. */
int wall(mapstruct *m, int x, int y)
{
    if (!(m = get_map_from_coord(m, &x, &y))) {
        return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);
    }

    return (GET_MAP_FLAGS(m, x, y) & (P_DOOR_CLOSED | P_PLAYER_ONLY | P_NO_PASS | P_PASS_THRU));
}

/**
 * Check if it's impossible to see through the given coordinate on the
 * given map.
 * @param m Map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return 1 if the given location blocks view. */
int blocks_view(mapstruct *m, int x, int y)
{
    mapstruct *nm;

    if (!(nm = get_map_from_coord(m, &x, &y))) {
        return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);
    }

    return (GET_MAP_FLAGS(nm, x, y) & P_BLOCKSVIEW);
}

/**
 * Check if given coordinates on the given map block magic.
 * @param m Map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return 1 if the given location blocks magic. */
int blocks_magic(mapstruct *m, int x, int y)
{
    if (!(m = get_map_from_coord(m, &x, &y))) {
        return (P_BLOCKSVIEW | P_NO_PASS | P_NO_MAGIC | P_OUT_OF_MAP);
    }

    return (GET_MAP_FLAGS(m, x, y) & P_NO_MAGIC) || (GET_MAP_SPACE_PTR(m, x, y)->extra_flags & MSP_EXTRA_NO_MAGIC);
}

/**
 * Check if specified object cannot move onto x, y on the given map and
 * terrain.
 * @param op Object.
 * @param m The map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param terrain Terrain type.
 * @return 0 if not blocked by anything, combination of
 * @ref map_look_flags otherwise. */
int blocked(object *op, mapstruct *m, int x, int y, int terrain)
{
    int flags;
    MapSpace *msp;

    flags = (msp = GET_MAP_SPACE_PTR(m, x, y))->flags;

    /* Flying objects can move over various terrains. */
    if (op && QUERY_FLAG(op, FLAG_FLYING)) {
        terrain |= TERRAIN_AIRBREATH | TERRAIN_WATERWALK | TERRAIN_FIREWALK | TERRAIN_CLOUDWALK;
    }

    /* First, look at the terrain. If we don't have a valid terrain flag,
     * this is forbidden to enter. */
    if (msp->move_flags & ~terrain) {
        return ((flags & (P_NO_PASS | P_IS_MONSTER | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU)) | P_NO_TERRAIN);
    }

    if (flags & P_IS_MONSTER) {
        return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_MONSTER | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));
    }

    if (flags & P_NO_PASS) {
        if (!(flags & P_PASS_THRU) || !op || !QUERY_FLAG(op, FLAG_CAN_PASS_THRU)) {
            return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));
        }
    }

    if (flags & P_IS_PLAYER) {
        if (!op || (m->map_flags & MAP_FLAG_PVP && !(flags & P_NO_PVP) && !(msp->extra_flags & MSP_EXTRA_NO_PVP))) {
            return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));
        }

        if (op->type != PLAYER) {
            return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));
        }
    }

    /* We have an object pointer - do some last checks */
    if (op) {
        /* Player only space and not a player - no pass and possible checker
         * here */
        if ((flags & P_PLAYER_ONLY) && op->type != PLAYER) {
            return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV | P_PLAYER_ONLY));
        }

        if (flags & P_CHECK_INV) {
            if (blocked_tile(op, m, x, y)) {
                return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV));
            }
        }
    }

    return (flags & (P_DOOR_CLOSED));
}

/**
 * Returns true if the given coordinate is blocked except by the object
 * passed is not blocking. This is used with multipart monsters - if we
 * want to see if a 2x2 monster can move 1 space to the left, we don't
 * want its own area to block it from moving there.
 * @param op The monster object.
 * @param xoff X position offset.
 * @param yoff Y position offset.
 * @return 0 if the space to check is not blocked by anything other than
 * the monster, return value of blocked() otherwise. */
int blocked_link(object *op, int xoff, int yoff)
{
    object *tmp, *tmp2;
    mapstruct *m;
    int xtemp, ytemp, flags;

    for (tmp = op; tmp; tmp = tmp->more) {
        /* We search for this new position */
        xtemp = tmp->arch->clone.x + xoff;
        ytemp = tmp->arch->clone.y + yoff;

        /* Check if match is a different part of us */
        for (tmp2 = op; tmp2; tmp2 = tmp2->more) {
            /* If this is true, we can be sure this position is valid */
            if (xtemp == tmp2->arch->clone.x && ytemp == tmp2->arch->clone.y) {
                break;
            }
        }

        /* If this is NULL, tmp will move in a new node */
        if (!tmp2) {
            xtemp = tmp->x + xoff;
            ytemp = tmp->y + yoff;

            /* If this new node is illegal, we can skip all */
            if (!(m = get_map_from_coord(tmp->map, &xtemp, &ytemp))) {
                return -1;
            }

            /* We use always head for tests - no need to copy any flags to the
             * tail */
            if ((flags = blocked(op, m, xtemp, ytemp, op->terrain_flag))) {
                if ((flags & P_DOOR_CLOSED) && (op->behavior & BEHAVIOR_OPEN_DOORS)) {
                    if (door_try_open(op, m, xtemp, ytemp, 0)) {
                        continue;
                    }
                }

                return flags;
            }
        }
    }

    return 0;
}

/**
 * Same as blocked_link(), but using an absolute coordinate (map, x, y).
 * @param op The monster object.
 * @param map The map.
 * @param x X coordinate on the map.
 * @param y Y coordinate on the map.
 * @return 0 if the space to check is not blocked by anything other than
 * the monster, return value of blocked() otherwise.
 * @todo This function should really be combined with the above to reduce
 * code duplication. */
int blocked_link_2(object *op, mapstruct *map, int x, int y)
{
    object *tmp, *tmp2;
    int xtemp, ytemp, flags;
    mapstruct *m;

    for (tmp = op; tmp; tmp = tmp->more) {
        /* We search for this new position */
        xtemp = x + tmp->arch->clone.x;
        ytemp = y + tmp->arch->clone.y;

        /* Check if match is a different part of us */
        for (tmp2 = op; tmp2; tmp2 = tmp2->more) {
            /* If this is true, we can be sure this position is valid */
            if (xtemp == tmp2->x && ytemp == tmp2->y) {
                break;
            }
        }

        /* If this is NULL, tmp will move in a new node */
        if (!tmp2) {
            /* If this new node is illegal, we can skip all */
            if (!(m = get_map_from_coord(map, &xtemp, &ytemp))) {
                return -1;
            }

            /* We use always head for tests - no need to copy any flags to the
             * tail */
            if ((flags = blocked(op, m, xtemp, ytemp, op->terrain_flag))) {
                if ((flags & P_DOOR_CLOSED) && (op->behavior & BEHAVIOR_OPEN_DOORS)) {
                    if (door_try_open(op, m, xtemp, ytemp, 1)) {
                        continue;
                    }
                }

                return flags;
            }
        }
    }

    return 0;
}

/**
 * This is used for any action which needs to browse through the objects
 * of the tile node, for special objects like inventory checkers.
 * @param op Object trying to move to map at x, y.
 * @param m Map we want to check.
 * @param x X position to check for.
 * @param y Y position to check for.
 * @return 1 if the tile is blocked, 0 otherwise. */
int blocked_tile(object *op, mapstruct *m, int x, int y)
{
    object *tmp;

    for (tmp = GET_MAP_OB(m, x, y); tmp; tmp = tmp->above) {
        /* This must be before the checks below. Code for inventory checkers. */
        if (tmp->type == CHECK_INV && tmp->last_grace) {
            if (QUERY_FLAG(tmp, FLAG_XRAYS) && op->type != PLAYER) {
                continue;
            }

            /* If last_sp is set, the player/monster needs an object,
             * so we check for it. If they don't have it, they can't
             * pass through this space. */
            if (tmp->last_sp) {
                if (check_inv(tmp, op) == NULL) {
                    return 1;
                }

                continue;
            } else {
                /* In this case, the player must not have the object -
                 * if they do, they can't pass through. */

                if (check_inv(tmp, op) != NULL) {
                    return 1;
                }

                continue;
            }
        }
    }

    return 0;
}

/**
 * Check if an archetype can fit to a position.
 * @param at Archetype to check.
 * @param op Object. If not NULL, will check for terrain as well.
 * @param m Map.
 * @param x X position.
 * @param y Y position.
 * @retval 0 No block.
 * @retval -1 Out of map.
 * @retval other Blocking flags from blocked(). */
int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y)
{
    archetype *tmp;
    mapstruct *mt;
    int xt, yt, t;

    if (op) {
        t = op->terrain_flag;
    } else {
        t = TERRAIN_ALL;
    }

    if (at == NULL) {
        if (!(m = get_map_from_coord(m, &x, &y))) {
            return -1;
        }

        return blocked(op, m, x, y, t);
    }

    for (tmp = at; tmp; tmp = tmp->more) {
        xt = x + tmp->clone.x;
        yt = y + tmp->clone.y;

        if (!(mt = get_map_from_coord(m, &xt, &yt))) {
            return -1;
        }

        if ((xt = blocked(op, mt, xt, yt, t))) {
            return xt;
        }
    }

    return 0;
}

/**
 * Loads (and parses) the objects into a given map from the specified
 * file pointer.
 * @param m Map being loaded.
 * @param fp File to read from.
 * @param mapflags The same as we get with load_original_map(). */
static void load_objects(mapstruct *m, FILE *fp, int mapflags)
{
    int i;
    void *mybuffer;
    object *op;

    op = get_object();

    /* To handle buttons correctly */
    op->map = m;
    mybuffer = create_loader_buffer(fp);

    while ((i = load_object(fp, op, mybuffer, LO_REPEAT, mapflags))) {
        if (i == LL_MORE) {
            logger_print(LOG(DEBUG), "object %s - its a tail!", query_short_name(op, NULL));
            continue;
        }

        /* If the archetype for the object is null, means that we
         * got an invalid object. Don't do anything with it - the game
         * will not be able to do anything with it either. */
        if (op->arch == NULL) {
            logger_print(LOG(DEBUG), "object %s (%d)- invalid archetype. (pos:%d,%d)", query_short_name(op, NULL), op->type, op->x, op->y);
            continue;
        }

        /* Do some safety for containers */
        if (op->type == CONTAINER) {
            /* Used for containers as link to players viewing it */
            op->attacked_by = NULL;
            op->attacked_by_count = 0;
            sum_weight(op);
        }

        if (op->type == MONSTER) {
            fix_monster(op);
        }

        /* Important pre set for the animation/face of a object */
        if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE)) {
            SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
        }

        insert_ob_in_map(op, m, op, INS_NO_MERGE | INS_NO_WALK_ON);

        /* auto_apply() will remove FLAG_AUTO_APPLY after first use */
        if (QUERY_FLAG(op, FLAG_AUTO_APPLY)) {
            auto_apply(op);
        } else if ((mapflags & MAP_ORIGINAL) && op->randomitems) {
            /* For fresh maps, create treasures */
            create_treasure(op->randomitems, op, op->type != TREASURE ? GT_APPLY : 0, op->level ? op->level : m->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
        }

        op = get_object();
        op->map = m;
    }

    delete_loader_buffer(mybuffer);
    object_destroy(op);

    m->in_memory = MAP_IN_MEMORY;
    check_light_source_list(m);
}

/**
 * This saves all the objects on the map in a non destructive fashion.
 * @param m Map to save.
 * @param fp File where regular objects are saved.
 * @param fp2 File to save unique objects. */
static void save_objects(mapstruct *m, FILE *fp, FILE *fp2)
{
    int x, y;
    object *ob, *next, *head, *tmp, *owner;
    uint8 unique;

    for (x = 0; x < MAP_WIDTH(m); x++) {
        for (y = 0; y < MAP_HEIGHT(m); y++) {
            unique = 0;

            for (ob = GET_MAP_OB(m, x, y); ob; ob = next) {
                next = ob->above;

                if (ob->type == PLAYER) {
                    continue;
                }

                head = HEAD(ob);

                if (QUERY_FLAG(head, FLAG_NO_SAVE)) {
                    object_remove(head, 0);
                    object_destroy(head);
                    continue;
                } else if (QUERY_FLAG(head, FLAG_SPAWN_MOB)) {
                    /* Try to find the spawn point information. */
                    for (tmp = head->inv; tmp; tmp = tmp->below) {
                        if (tmp->type == SPAWN_POINT_INFO) {
                            if (OBJECT_VALID(tmp->owner, tmp->ownercount) && tmp->owner->type == SPAWN_POINT) {
                                tmp->owner->last_sp = -1;
                                tmp->owner->speed_left += 1.0f;
                                tmp->owner->enemy = NULL;
                            }

                            break;
                        }
                    }

                    object_remove(head, 0);
                    object_destroy(head);
                    continue;
                } else if (head->type == SPAWN_POINT) {
                    if (OBJECT_VALID(head->enemy, head->enemy_count)) {
                        head->last_sp = -1;
                        head->speed_left += 1.0f;
                        head->enemy = NULL;
                    }
                }

                /* Do not save tail parts. */
                if (ob->head) {
                    continue;
                }

                owner = get_owner(head);

                if (owner) {
                    clear_owner(head);
                }

                if (QUERY_FLAG(head, FLAG_IS_FLOOR) && QUERY_FLAG(head, FLAG_UNIQUE)) {
                    unique = 1;
                }

                if (unique || QUERY_FLAG(head, FLAG_UNIQUE)) {
                    save_object(fp2, head);
                } else {
                    save_object(fp, head);
                }
            }
        }
    }
}

/**
 * Sets darkness for map, both light_value and darkness.
 *
 * Takes care of checking the passed value.
 * @param m Pointer to the map's structure.
 * @param value The darkness value. */
void set_map_darkness(mapstruct *m, int value)
{
    if (value < 0 || value > MAX_DARKNESS) {
        value = MAX_DARKNESS;
    }

    MAP_DARKNESS(m) = (sint32) value;
    m->light_value = (sint32) global_darkness_table[value];
}

/**
 * Allocates, initializes, and returns a pointer to a mapstruct.
 * @return The new map structure. */
mapstruct *get_linked_map(void)
{
    mapstruct *map = ecalloc(1, sizeof(mapstruct));

    if (first_map) {
        first_map->previous = map;
    }

    map->next = first_map;
    first_map = map;

    map->in_memory = MAP_SWAPPED;

    /* The maps used to pick up default x and y values from the
     * map archetype. Mimic that behavior. */
    MAP_WIDTH(map) = 16;
    MAP_HEIGHT(map) = 16;
    MAP_RESET_TIMEOUT(map) = 0;
    MAP_TIMEOUT(map) = MAP_DEFAULTTIMEOUT;
    set_map_darkness(map, MAP_DEFAULT_DARKNESS);

    MAP_ENTER_X(map) = 0;
    MAP_ENTER_Y(map) = 0;

    return map;
}

/**
 * This basically allocates the dynamic array of spaces for the
 * map.
 * @param m Map to allocate spaces for. */
static void allocate_map(mapstruct *m)
{
    m->in_memory = MAP_LOADING;

    if (m->spaces || m->bitmap) {
        logger_print(LOG(ERROR), "Callled with already allocated map (%s)", m->path);
        exit(1);
    }

    if (m->buttons) {
        logger_print(LOG(BUG), "Callled with already set buttons (%s)", m->path);
    }

    m->spaces = ecalloc(1, MAP_WIDTH(m) * MAP_HEIGHT(m) * sizeof(MapSpace));
    m->bitmap = emalloc(((MAP_WIDTH(m) + 31) / 32) * MAP_HEIGHT(m) * sizeof(*m->bitmap));
    m->path_nodes = emalloc(MAP_WIDTH(m) * MAP_HEIGHT(m) * sizeof(*m->path_nodes));
}

/**
 * Creates and returns a map of the specified size.
 *
 * Used in random maps code.
 * @param sizex X size of the map.
 * @param sizey Y size of the map.
 * @return The new map structure. */
mapstruct *get_empty_map(int sizex, int sizey)
{
    mapstruct *m = get_linked_map();
    m->width = sizex;
    m->height = sizey;
    allocate_map(m);

    m->in_memory = MAP_IN_MEMORY;

    return m;
}

void map_set_tile(mapstruct *m, int tile, const char *pathname)
{
    char *path;
    shstr *path_sh;
    mapstruct *neighbor;

    assert(m != NULL);
    assert(tile >= 0 && tile < TILED_NUM);
    assert(pathname != NULL);

    if (m->tile_path[tile] != NULL) {
        log(LOG(BUG), "Tile location %d duplicated (%s <-> %s)", tile, m->path,
                m->tile_path[tile]);
        FREE_AND_CLEAR_HASH(m->tile_path[tile]);
    }

    if (map_path_isabs(pathname)) {
        path_sh = add_string(pathname);
    } else {
        path = map_get_path(m, pathname, m->map_flags & MAP_FLAG_UNIQUE, NULL);
        path_sh = add_string(path);
        efree(path);
    }

    m->tile_path[tile] = path_sh;
    neighbor = has_been_loaded_sh(path_sh);

    /* If the neighboring map tile has been loaded, set up the map pointers. */
    if (neighbor != NULL && (neighbor->in_memory == MAP_IN_MEMORY ||
            neighbor->in_memory == MAP_LOADING)) {
        int dest_tile = map_tiled_reverse[tile];

        m->tile_map[tile] = neighbor;

        if (neighbor->tile_path[dest_tile] == NULL ||
                neighbor->tile_path[dest_tile] == m->path) {
            neighbor->tile_map[dest_tile] = m;
        }
    }
}

/**
 * Opens the file "filename" and reads information about the map
 * from the given file, and stores it in a newly allocated
 * mapstruct.
 * @param filename Map path.
 * @param flags One of (or combination of):
 * - @ref MAP_PLAYER_UNIQUE: We don't do any name changes.
 * - @ref MAP_BLOCK: We block on this load. This happens in all cases, no
 *   matter if this flag is set or not.
 * - @ref MAP_STYLE: Style map - don't add active objects, don't add to
 *   server managed map list.
 * @return The loaded map structure, NULL on failure. */
mapstruct *load_original_map(const char *filename, int flags)
{
    FILE *fp;
    mapstruct *m;
    char pathname[HUGE_BUF], split[MAX_BUF];
    const char *basename;
    size_t pos, coords_idx, coords_len;
    sint16 old_style_z;

    /* No sense in doing this all for random maps, it will all fail anyways. */
    if (!strncmp(filename, "/random/", 8)) {
        return NULL;
    }

    snprintf(pathname, sizeof(pathname), "%s%s",
            *filename != '/' && *filename != '.' ? "/" : "",
            flags & MAP_PLAYER_UNIQUE ? filename : create_pathname(filename));

    if (flags & MAP_PLAYER_UNIQUE && !path_exists(pathname)) {
        char *path;

        path = path_basename(pathname);
        string_replace_char(path, "$", '/');
        fp = fopen(create_pathname(path), "rb");
        efree(path);
    } else {
        fp = fopen(pathname, "rb");
    }

    if (!fp) {
        logger_print(LOG(BUG), "Can't open map file %s", pathname);
        return NULL;
    }

    m = get_linked_map();

    FREE_AND_COPY_HASH(m->path, filename);

    if (flags & MAP_PLAYER_UNIQUE) {
        m->map_flags |= MAP_FLAG_UNIQUE;
    }

    if (!load_map_header(m, fp)) {
        logger_print(LOG(BUG), "Failure loading map header for %s, flags=%d", filename, flags);
        delete_map(m);
        fclose(fp);
        return NULL;
    }

    basename = strrchr(filename, '/') + 1;

    if (basename - 1 == NULL) {
        basename = filename;
    }

    pos = coords_idx = 0;
    coords_len = 0;
    old_style_z = 0;

    while (string_get_word(basename, &pos, '_', split, sizeof(split), 0)) {
        if (strlen(split) == 1) {
            old_style_z = string_isdigit(split) ? atoi(split) :
                'a' - *split - 1;
        } else if (strlen(split) > 3) {
            /* TODO: remove this hack (avoids parsing old-style naming
             * convention like map_0101) */
            continue;
        }

        if (string_isdigit(split)) {
            coords_len += strlen(split) + 1;
            m->coords[coords_idx] = atoi(split);
            coords_idx++;
        }

        if (coords_idx >= arraysize(m->coords)) {
            break;
        }
    }

    if (coords_idx >= 2) {
        size_t i;
        char *cp, *cp2, path[HUGE_BUF];

        cp = string_sub(pathname, 0, -coords_len);

        for (i = 0; i < TILED_NUM; i++) {
            if (m->tile_path[i] != NULL) {
                continue;
            }

            snprintf(VS(path), "%s_%d_%d", cp,
                    m->coords[0] + map_tiled_coords[i][0],
                    m->coords[1] + map_tiled_coords[i][1]);

            if (m->coords[2] + map_tiled_coords[i][2] != 0) {
                snprintfcat(VS(path), "_%d",
                        m->coords[2] + map_tiled_coords[i][2]);
            }

            if (path_exists(path)) {
                cp2 = path_basename(path);
                map_set_tile(m, i, cp2);
                efree(cp2);
            }
        }

        efree(cp);
    } else {
        m->coords[2] = old_style_z;
    }

    if (m->level_max == 0 && m->coords[2] >= 0) {
        m->level_max = SINT8_MAX;
    } else if (m->level_max == 0 && m->level_min == 0) {
        m->level_min = m->level_max = m->coords[2];
    }

    allocate_map(m);

    m->in_memory = MAP_LOADING;
    load_objects(m, fp, (flags & (MAP_BLOCK | MAP_STYLE)) | MAP_ORIGINAL);
    fclose(fp);

    if (!MAP_DIFFICULTY(m)) {
        logger_print(LOG(BUG), "Map %s has difficulty 0. Changing to 1.", filename);
        MAP_DIFFICULTY(m) = 1;
    }

    set_map_reset_time(m);
    return m;
}

/**
 * Loads a map, which has been loaded earlier, from file.
 * @param m Map we want to reload.
 * @return The map object we load into (this can change from the passed
 * option if we can't find the original map). */
static mapstruct *load_temporary_map(mapstruct *m)
{
    FILE *fp;
    char buf[HUGE_BUF];

    if (!m->tmpname) {
        logger_print(LOG(BUG), "No temporary filename for map %s! Fallback to original!", m->path);
        snprintf(buf, sizeof(buf), "%s", m->path);
        delete_map(m);
        m = load_original_map(buf, 0);
        return m;
    }

    fp = fopen(m->tmpname, "rb");

    if (!fp) {
        if (!strncmp(m->path, "/random/", 8)) {
            return NULL;
        }

        logger_print(LOG(BUG), "Can't open temporary map %s! Fallback to original!", m->tmpname);
        snprintf(buf, sizeof(buf), "%s", m->path);
        delete_map(m);
        m = load_original_map(buf, 0);
        return m;
    }

    if (!load_map_header(m, fp)) {
        logger_print(LOG(BUG), "Error loading map header for %s (%s)! Fallback to original!", m->path, m->tmpname);
        snprintf(buf, sizeof(buf), "%s", m->path);
        delete_map(m);
        m = load_original_map(buf, 0);
        fclose(fp);
        return m;
    }

    allocate_map(m);

    m->in_memory = MAP_LOADING;
    load_objects (m, fp, 0);
    fclose(fp);
    return m;
}

/**
 * Goes through a map and removes any unique items on the map.
 * @param m The map to go through. */
static void delete_unique_items(mapstruct *m)
{
    int i, j, unique = 0;
    object *op, *next;

    for (i = 0; i < MAP_WIDTH(m); i++) {
        for (j = 0; j < MAP_HEIGHT(m); j++) {
            unique = 0;

            for (op = GET_MAP_OB(m, i, j); op; op = next) {
                next = op->above;

                if (QUERY_FLAG(op, FLAG_IS_FLOOR) && QUERY_FLAG(op, FLAG_UNIQUE)) {
                    unique = 1;
                }

                if (op->head == NULL && (QUERY_FLAG(op, FLAG_UNIQUE) || unique)) {
                    if (QUERY_FLAG(op, FLAG_IS_LINKED)) {
                        connection_object_remove(op);
                    }

                    object_remove(op, 0);
                    object_destroy(op);
                }
            }
        }
    }
}

/**
 * Loads unique objects from file(s) into the map which is in memory.
 * @param m The map to load unique items into. */
static void load_unique_objects(mapstruct *m)
{
    FILE *fp;
    int count;
    char firstname[HUGE_BUF];

    for (count = 0; count < 10; count++) {
        snprintf(firstname, sizeof(firstname), "%s.v%02d", create_items_path(m->path), count);
        fp = fopen(firstname, "rb");

        if (fp != NULL) {
            break;
        }
    }

    /* If we get here, we did not find any map. */
    if (fp == NULL) {
        return;
    }

    m->in_memory = MAP_LOADING;

    /* If we have loaded unique items from */
    if (m->tmpname == NULL) {
        delete_unique_items(m);
    }

    load_objects(m, fp, 0);
    fclose(fp);
}

/**
 * Saves a map to file.  If flag is set, it is saved into the same
 * file it was (originally) loaded from.  Otherwise a temporary
 * filename will be generated, and the file will be stored there.
 * The temporary filename will be stored in the mapstructure.
 * If the map is unique, we also save to the filename in the map
 * (this should have been updated when first loaded).
 * @param m The map to save.
 * @param flag Save flag.
 * @return  */
int new_save_map(mapstruct *m, int flag)
{
    FILE *fp, *fp2;
    char filename[MAX_BUF], buf[MAX_BUF];

    if (flag && !*m->path) {
        return -1;
    }

    if (flag || MAP_UNIQUE(m)) {
        if (!MAP_UNIQUE(m)) {
            snprintf(filename, sizeof(filename), "%s", create_pathname(m->path));
        } else {
            /* This ensures we always reload from original maps */
            if (MAP_NOSAVE(m)) {
                return 0;
            }

            snprintf(filename, sizeof(filename), "%s", m->path);
        }

        path_ensure_directories(filename);
    } else {
        if (m->tmpname == NULL) {
            char path[MAX_BUF];

            snprintf(path, sizeof(path), "%s/tmp", settings.datapath);
            m->tmpname = tempnam(path, NULL);
        }

        snprintf(filename, sizeof(filename), "%s", m->tmpname);
    }

    m->in_memory = MAP_SAVING;
    fp = fopen(filename, "w");

    if (!fp) {
        logger_print(LOG(ERROR), "Can't open file %s for saving.", filename);
        exit(1);
    }

    save_map_header(m, fp, flag);

    /* Save unique items into fp2 */
    fp2 = fp;

    if (!MAP_UNIQUE(m)) {
        snprintf(buf, sizeof(buf), "%s.v00", create_items_path(m->path));

        if ((fp2 = fopen(buf, "w")) == NULL) {
            logger_print(LOG(BUG), "Can't open unique items file %s", buf);
        }

        save_objects(m, fp, fp2);

        if (fp2) {
            if (ftell(fp2) == 0) {
                fclose(fp2);
                unlink(buf);
            } else {
                fclose(fp2);
                chmod(buf, SAVE_MODE);
            }
        }
    } else {
        /* Otherwise to the same file, like apartments */
        save_objects(m, fp, fp);
    }

    if (fp) {
        fclose(fp);
    }

    chmod(filename, SAVE_MODE);
    return 0;
}

/**
 * Remove and free all objects in the given map.
 * @param m The map. */
static void free_all_objects(mapstruct *m)
{
    int x, y;
    object *ob, *head;

    for (x = 0; x < MAP_WIDTH(m); x++) {
        for (y = 0; y < MAP_HEIGHT(m); y++) {
            while ((ob = GET_MAP_OB(m, x, y)) != NULL) {
                head = HEAD(ob);

                object_remove(head, 0);
                object_destroy(head);
            }
        }
    }
}

/**
 * Frees everything allocated by the given map structure.
 *
 * Don't free tmpname - our caller is left to do that.
 * @param m Map to free.
 * @param flag If set, free all objects on the map. */
void free_map(mapstruct *m, int flag)
{
    int i;

    if (!m->in_memory) {
        return;
    }

    remove_light_source_list(m);

    if (m->buttons) {
        free_objectlinkpt(m->buttons);
    }

    if (flag && m->spaces) {
        free_all_objects(m);
    }

    FREE_AND_NULL_PTR(m->name);
    FREE_AND_NULL_PTR(m->bg_music);
    FREE_AND_NULL_PTR(m->weather);
    FREE_AND_NULL_PTR(m->spaces);
    FREE_AND_NULL_PTR(m->msg);
    m->buttons = NULL;
    m->first_light = NULL;

    for (i = 0; i < TILED_NUM; i++) {
        /* Delete the backlinks in other tiled maps to our map */
        if (m->tile_map[i]) {
            if (m->tile_map[i]->tile_map[map_tiled_reverse[i]] && m->tile_map[i]->tile_map[map_tiled_reverse[i]] != m) {
                logger_print(LOG(BUG), "Freeing map %s linked to %s which links back to another map.", STRING_SAFE(m->path), STRING_SAFE(m->tile_map[i]->path));
            }

            m->tile_map[i]->tile_map[map_tiled_reverse[i]] = NULL;
            m->tile_map[i] = NULL;
        }

        FREE_AND_CLEAR_HASH(m->tile_path[i]);
    }

    if (m->events) {
        map_event *tmp, *next;

        for (tmp = m->events; tmp; tmp = next) {
            next = tmp->next;
            map_event_free(tmp);
        }

        m->events = NULL;
    }

    FREE_AND_NULL_PTR(m->bitmap);
    m->in_memory = MAP_SWAPPED;
}

/**
 * Deletes all the data on the map (freeing pointers) and then removes
 * this map from the global linked list of maps.
 * @param m The map to delete. */
void delete_map(mapstruct *m)
{
    if (!m) {
        return;
    }

    if (m->in_memory == MAP_IN_MEMORY) {
        /* Change to MAP_SAVING, even though we are not,
         * so that object_remove doesn't do as much work. */
        m->in_memory = MAP_SAVING;
        free_map(m, 1);
    } else {
        remove_light_source_list(m);
    }

    /* Remove m from the global map list */
    if (m->next) {
        m->next->previous = m->previous;
    }

    if (m->previous) {
        m->previous->next = m->next;
    } else {
        /* If there is no previous, we are first map */
        first_map = m->next;
    }

    /* tmpname can still be needed if the map is swapped out, so we don't
     * do it in free_map(). */
    FREE_AND_NULL_PTR(m->tmpname);
    FREE_AND_CLEAR_HASH(m->path);
    efree(m);
}

/**
 * Makes sure the given map is loaded and swapped in.
 * @param name Path name of the map.
 * @param flags Possible flags:
 * - @ref MAP_FLUSH: Flush the map - always load from the map directory,
 *   and don't do unique items or the like.
 *  - @ref MAP_PLAYER_UNIQUE: This is an unique map for each player.
 *    Don't do any more name translation on it.
 * @return Pointer to the given map. */
mapstruct *ready_map_name(const char *name, int flags)
{
    mapstruct *m;
    shstr *name_sh;

    if (!name) {
        return NULL;
    }

    /* Have we been at this level before? */
    if (flags & MAP_NAME_SHARED) {
        m = has_been_loaded_sh(name);
    } else {
        /* Create a temporary shared string for the name if not explicitly given
         * */
        name_sh = add_string(name);
        m = has_been_loaded_sh(name_sh);
        free_string_shared(name_sh);
    }

    /* Map is good to go, so just return it */
    if (m && (m->in_memory == MAP_LOADING || m->in_memory == MAP_IN_MEMORY)) {
        return m;
    }

    if (!(flags & MAP_PLAYER_UNIQUE) && string_startswith(name, settings.datapath)) {
        flags |= MAP_PLAYER_UNIQUE;
    }

    /* Unique maps always get loaded from their original location, and never
     * a temp location.  Likewise, if map_flush is set, or we have never loaded
     * this map, load it now.  I removed the reset checking from here -
     * it seems the probability of a player trying to enter a map that should
     * reset but hasn't yet is quite low, and removing that makes this function
     * a bit cleaner (and players probably shouldn't rely on exact timing for
     * resets in any case - if they really care, they should use the 'maps
     * command. */
    if (!m || (flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE))) {
        /* First visit or time to reset */
        if (m) {
            /* Doesn't make much difference */
            clean_tmp_map(m);
            delete_map(m);
        }

        /* Create and load a map */
        if (!(m = load_original_map(name, (flags & MAP_PLAYER_UNIQUE)))) {
            return NULL;
        }

        /* If a player unique map, no extra unique object file to load.
         * if from the editor, likewise. */
        if (!(flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE))) {
            load_unique_objects(m);
        }
    } else {
        /* If in this loop, we found a temporary map, so load it up. */
        m = load_temporary_map(m);

        if (m == NULL) {
            return NULL;
        }

        load_unique_objects(m);

        clean_tmp_map(m);
        m->in_memory = MAP_IN_MEMORY;
    }

    return m;
}

/**
 * Remove the temporary file used by the map.
 * @param m Map. */
void clean_tmp_map(mapstruct *m)
{
    if (m->tmpname == NULL) {
        return;
    }

    unlink(m->tmpname);

    efree(m->tmpname);
    m->tmpname = NULL;
}

/**
 * Free all allocated maps. */
void free_all_maps(void)
{
    int real_maps = 0;

    while (first_map) {
        /* I think some of the callers above before it gets here set this to be
         * saving, but we still want to free this data */
        if (first_map->in_memory == MAP_SAVING) {
            first_map->in_memory = MAP_IN_MEMORY;
        }

        delete_map(first_map);
        real_maps++;
    }
}

/**
 * This function updates various attributes about a specific space on the
 * map (what it looks like, whether it blocks magic, has a living
 * creatures, prevents people from passing through, etc).
 * @param m Map to update.
 * @param x X position on the given map.
 * @param y Y position on the given map. */
void update_position(mapstruct *m, int x, int y)
{
    object *tmp;
    int flags, move_flags;

#ifdef DEBUG_OLDFLAGS
    int oldflags;

    if (!((oldflags = GET_MAP_FLAGS(m, x, y)) & (P_NEED_UPDATE | P_FLAGS_UPDATE))) {
        logger_print(LOG(DEBUG), "called with P_NEED_UPDATE|P_FLAGS_UPDATE not set: %s (%d, %d)", m->path, x, y);
    }
#endif

    /* save our update flag */
    flags = oldflags & P_NEED_UPDATE;

    /* update our flags */
    if (oldflags & P_FLAGS_UPDATE) {
        move_flags = 0;

        /* This is a key function and highly often called - every saved tick is
         * good. */
        for (tmp = GET_MAP_OB (m, x, y); tmp; tmp = tmp->above) {
            if (QUERY_FLAG(tmp, FLAG_PLAYER_ONLY)) {
                flags |= P_PLAYER_ONLY;
            }

            if (tmp->type == CHECK_INV) {
                flags |= P_CHECK_INV;
            }

            if (QUERY_FLAG(tmp, FLAG_IS_PLAYER)) {
                flags |= P_IS_PLAYER;
            }

            if (QUERY_FLAG(tmp, FLAG_DOOR_CLOSED)) {
                flags |= P_DOOR_CLOSED;
            }

            if (QUERY_FLAG(tmp, FLAG_MONSTER)) {
                flags |= P_IS_MONSTER;
            }

            if (QUERY_FLAG(tmp, FLAG_NO_MAGIC)) {
                flags |= P_NO_MAGIC;
            }

            if (QUERY_FLAG(tmp, FLAG_BLOCKSVIEW)) {
                flags |= P_BLOCKSVIEW;
            }

            if (QUERY_FLAG(tmp, FLAG_WALK_ON)) {
                flags |= P_WALK_ON;
            }

            if (QUERY_FLAG(tmp, FLAG_WALK_OFF)) {
                flags |= P_WALK_OFF;
            }

            if (QUERY_FLAG(tmp, FLAG_FLY_ON)) {
                flags |= P_FLY_ON;
            }

            if (QUERY_FLAG(tmp, FLAG_FLY_OFF)) {
                flags |= P_FLY_OFF;
            }

            if (QUERY_FLAG(tmp, FLAG_NO_PASS)) {
                if (flags & P_NO_PASS) {
                    if (!QUERY_FLAG(tmp, FLAG_PASS_THRU)) {
                        flags &= ~P_PASS_THRU;
                    }
                } else {
                    flags |= P_NO_PASS;

                    if (QUERY_FLAG(tmp, FLAG_PASS_THRU)) {
                        flags |= P_PASS_THRU;
                    }
                }
            }

            if (QUERY_FLAG(tmp, FLAG_IS_FLOOR)) {
                move_flags |= tmp->terrain_type;
            }

            if (QUERY_FLAG(tmp, FLAG_NO_PVP)) {
                flags |= P_NO_PVP;
            }

            if (tmp->type == MAGIC_MIRROR) {
                flags |= P_MAGIC_MIRROR;
            }

            if (tmp->type == EXIT) {
                flags |= P_IS_EXIT;
            }

            if (QUERY_FLAG(tmp, FLAG_OUTDOOR)) {
                flags |= P_OUTDOOR;
            }
        }

#ifdef DEBUG_OLDFLAGS

        /* We don't want to rely on this function to have accurate flags, but
         * since we're already doing the work, we calculate them here.
         * if they don't match, logic is broken someplace. */
        if (((oldflags & ~(P_FLAGS_UPDATE | P_FLAGS_ONLY | P_NO_ERROR)) != flags) && (!(oldflags & P_NO_ERROR))) {
            logger_print(LOG(DEBUG), "updated flags do not match old flags: %s (%d,%d) old:%x != %x", m->path, x, y, (oldflags & ~P_NEED_UPDATE), flags);
        }
#endif

        SET_MAP_FLAGS(m, x, y, flags);
        SET_MAP_MOVE_FLAGS(m, x, y, move_flags);
    }

    /* Check if we must rebuild the map layers for client view */
    if ((oldflags & P_FLAGS_ONLY) || !(oldflags & P_NEED_UPDATE)) {
        return;
    }

    /* Clear out need update flag */
    SET_MAP_FLAGS(m, x, y, GET_MAP_FLAGS(m, x, y) & ~P_NEED_UPDATE);
}

/**
 * Updates the map's timeout.
 * @param map Map to update. */
void set_map_reset_time(mapstruct *map)
{
    uint32 timeout = MAP_RESET_TIMEOUT(map);

    if (timeout == 0) {
        timeout = MAP_DEFAULTRESET;
    }

    if (timeout >= MAP_MAXRESET) {
        timeout = MAP_MAXRESET;
    }

    MAP_WHEN_RESET(map) = seconds() + timeout;
}

/**
 * Get tiled map, loading it if necessary.
 * @param m Map.
 * @param tiled Tile ID to get.
 * @return Tiled map on success, NULL on failure.
 */
mapstruct *get_map_from_tiled(mapstruct *m, int tiled)
{
    assert(m != NULL);
    assert(tiled >= 0 && tiled < TILED_NUM);

    if (m->tile_path[tiled] == NULL) {
        return NULL;
    }

    if (m->tile_map[tiled] == NULL ||
            m->tile_map[tiled]->in_memory != MAP_IN_MEMORY) {
        if (!load_and_link_tiled_map(m, tiled)) {
            return NULL;
        }
    }

    return m->tile_map[tiled];
}

/**
 * Get real coordinates from map.
 *
 * Return NULL if no map is valid (coordinates out of bounds and no tiled
 * map), otherwise it returns the map the coordinates are really on, and
 * updates x and y to be the localized coordinates.
 * @param m Map to consider.
 * @param[out] x Will contain the real X position that was checked.
 * @param[out] y Will contain the real Y position that was checked.
 * @return Map that is at specified location. Will be NULL if not on any
 * map. */
mapstruct *get_map_from_coord(mapstruct *m, int *x, int *y)
{
    /* m should never be null, but if a tiled map fails to load below, it
     * could happen. */
    if (!m) {
        return NULL;
    }

    /* Simple case - coordinates are within this local map. */
    if (*x >= 0 && *x < MAP_WIDTH(m) && *y >= 0 && *y < MAP_HEIGHT(m)) {
        return m;
    }

    /* West, Northwest or Southwest (3, 7 or 6) */
    if (*x < 0) {
        /* Northwest */
        if (*y < 0) {
            if (!m->tile_path[7]) {
                return NULL;
            }

            if (!m->tile_map[7] || m->tile_map[7]->in_memory != MAP_IN_MEMORY) {
                if (!load_and_link_tiled_map(m, 7)) {
                    return NULL;
                }
            }

            *y += MAP_HEIGHT(m->tile_map[7]);
            *x += MAP_WIDTH(m->tile_map[7]);
            return get_map_from_coord(m->tile_map[7], x, y);
        }

        /* Southwest */
        if (*y >= MAP_HEIGHT(m)) {
            if (!m->tile_path[6]) {
                return NULL;
            }

            if (!m->tile_map[6] || m->tile_map[6]->in_memory != MAP_IN_MEMORY) {
                if (!load_and_link_tiled_map(m, 6)) {
                    return NULL;
                }
            }

            *y -= MAP_HEIGHT(m);
            *x += MAP_WIDTH(m->tile_map[6]);
            return get_map_from_coord(m->tile_map[6], x, y);
        }

        /* West */
        if (!m->tile_path[3]) {
            return NULL;
        }

        if (!m->tile_map[3] || m->tile_map[3]->in_memory != MAP_IN_MEMORY) {
            if (!load_and_link_tiled_map(m, 3)) {
                return NULL;
            }
        }

        *x += MAP_WIDTH(m->tile_map[3]);
        return get_map_from_coord(m->tile_map[3], x, y);
    }

    /* East, Northeast or Southeast (1, 4 or 5) */
    if (*x >= MAP_WIDTH(m)) {
        /* Northeast */
        if (*y < 0) {
            if (!m->tile_path[4]) {
                return NULL;
            }

            if (!m->tile_map[4] || m->tile_map[4]->in_memory != MAP_IN_MEMORY) {
                if (!load_and_link_tiled_map(m, 4)) {
                    return NULL;
                }
            }

            *y += MAP_HEIGHT(m->tile_map[4]);
            *x -= MAP_WIDTH(m);
            return get_map_from_coord(m->tile_map[4], x, y);
        }

        /* Southeast */
        if (*y >= MAP_HEIGHT(m)) {
            if (!m->tile_path[5]) {
                return NULL;
            }

            if (!m->tile_map[5] || m->tile_map[5]->in_memory != MAP_IN_MEMORY) {
                if (!load_and_link_tiled_map(m, 5)) {
                    return NULL;
                }
            }

            *y -= MAP_HEIGHT(m);
            *x -= MAP_WIDTH(m);
            return get_map_from_coord(m->tile_map[5], x, y);
        }

        /* East */
        if (!m->tile_path[1]) {
            return NULL;
        }

        if (!m->tile_map[1] || m->tile_map[1]->in_memory != MAP_IN_MEMORY) {
            if (!load_and_link_tiled_map(m, 1)) {
                return NULL;
            }
        }

        *x -= MAP_WIDTH(m);
        return get_map_from_coord(m->tile_map[1], x, y);
    }

    /* Because we have tested x above, we don't need to check for
     * Northwest, Southwest, Northeast and Northwest here again. */
    if (*y < 0) {
        if (!m->tile_path[0]) {
            return NULL;
        }

        if (!m->tile_map[0] || m->tile_map[0]->in_memory != MAP_IN_MEMORY) {
            if (!load_and_link_tiled_map(m, 0)) {
                return NULL;
            }
        }

        *y += MAP_HEIGHT(m->tile_map[0]);
        return get_map_from_coord(m->tile_map[0], x, y);
    }

    if (*y >= MAP_HEIGHT(m)) {
        if (!m->tile_path[2]) {
            return NULL;
        }

        if (!m->tile_map[2] || m->tile_map[2]->in_memory != MAP_IN_MEMORY) {
            if (!load_and_link_tiled_map(m, 2)) {
                return NULL;
            }
        }

        *y -= MAP_HEIGHT(m);
        return get_map_from_coord(m->tile_map[2], x, y);
    }

    return NULL;
}

/**
 * Same as get_map_from_coord(), but this version doesn't load tiled maps
 * into memory, if they are not already.
 * @param m Map to consider.
 * @param[out] x Will contain the real X position that was checked. If
 * coordinates are not in a map this is set to 0, or to -1 if there is a
 * tiled map but it's not loaded.
 * @param[out] y Will contain the real Y position that was checked.
 * @return Map that is at specified location. Will be NULL if not on any
 * map. */
mapstruct *get_map_from_coord2(mapstruct *m, int *x, int *y)
{
    if (!m) {
        *x = 0;
        return NULL;
    }

    /* Simple case - coordinates are within this local map. */
    if (*x >= 0 && *x < MAP_WIDTH(m) && *y >= 0 && *y < MAP_HEIGHT(m)) {
        return m;
    }

    /* West, Northwest or Southwest (3, 7 or 6) */
    if (*x < 0) {
        /* Northwest */
        if (*y < 0) {
            if (!m->tile_path[7]) {
                *x = 0;
                return NULL;
            }

            if (!m->tile_map[7] || m->tile_map[7]->in_memory != MAP_IN_MEMORY) {
                *x = -1;
                return NULL;
            }

            *y += MAP_HEIGHT(m->tile_map[7]);
            *x += MAP_WIDTH(m->tile_map[7]);

            return get_map_from_coord2(m->tile_map[7], x, y);
        }

        /* Southwest */
        if (*y >= MAP_HEIGHT(m)) {
            if (!m->tile_path[6]) {
                *x = 0;
                return NULL;
            }

            if (!m->tile_map[6] || m->tile_map[6]->in_memory != MAP_IN_MEMORY) {
                *x = -1;
                return NULL;
            }

            *y -= MAP_HEIGHT(m);
            *x += MAP_WIDTH(m->tile_map[6]);

            return get_map_from_coord2(m->tile_map[6], x, y);
        }

        /* West */
        if (!m->tile_path[3]) {
            *x = 0;
            return NULL;
        }

        if (!m->tile_map[3] || m->tile_map[3]->in_memory != MAP_IN_MEMORY) {
            *x = -1;
            return NULL;
        }

        *x += MAP_WIDTH(m->tile_map[3]);
        return get_map_from_coord2(m->tile_map[3], x, y);
    }

    /* East, Northeast or Southeast (1, 4 or 5) */
    if (*x >= MAP_WIDTH(m)) {
        /* Northeast */
        if (*y < 0) {
            if (!m->tile_path[4]) {
                *x = 0;
                return NULL;
            }

            if (!m->tile_map[4] || m->tile_map[4]->in_memory != MAP_IN_MEMORY) {
                *x = -1;
                return NULL;
            }

            *y += MAP_HEIGHT(m->tile_map[4]);
            *x -= MAP_WIDTH(m);

            return get_map_from_coord2(m->tile_map[4], x, y);
        }

        /* Southeast */
        if (*y >= MAP_HEIGHT(m)) {
            if (!m->tile_path[5]) {
                *x = 0;
                return NULL;
            }

            if (!m->tile_map[5] || m->tile_map[5]->in_memory != MAP_IN_MEMORY) {
                *x = -1;
                return NULL;
            }

            *y -= MAP_HEIGHT(m);
            *x -= MAP_WIDTH(m);

            return get_map_from_coord2(m->tile_map[5], x, y);
        }

        /* East */
        if (!m->tile_path[1]) {
            *x = 0;
            return NULL;
        }

        if (!m->tile_map[1] || m->tile_map[1]->in_memory != MAP_IN_MEMORY) {
            *x = -1;
            return NULL;
        }

        *x -= MAP_WIDTH(m);
        return get_map_from_coord2(m->tile_map[1], x, y);
    }

    /* Because we have tested x above, we don't need to check for
     * Northwest, Southwest, Northeast and Northwest here again. */
    if (*y < 0) {
        if (!m->tile_path[0]) {
            *x = 0;
            return NULL;
        }

        if (!m->tile_map[0] || m->tile_map[0]->in_memory != MAP_IN_MEMORY) {
            *x = -1;
            return NULL;
        }

        *y += MAP_HEIGHT(m->tile_map[0]);

        return get_map_from_coord2(m->tile_map[0], x, y);
    }

    if (*y >= MAP_HEIGHT(m)) {
        if (!m->tile_path[2]) {
            *x = 0;
            return NULL;
        }

        if (!m->tile_map[2] || m->tile_map[2]->in_memory != MAP_IN_MEMORY) {
            *x = -1;
            return NULL;
        }

        *y -= MAP_HEIGHT(m);
        return get_map_from_coord2(m->tile_map[2], x, y);
    }

    *x = 0;
    return NULL;
}

/**
 * This is used by get_player to determine where the other
 * creature is.  get_rangevector takes into account map tiling,
 * so you just can not look the the map coordinates and get the
 * right value.  distance_x/y are distance away, which
 * can be negative.  direction is the crossfire direction scheme
 * that the creature should head.  part is the part of the
 * monster that is closest.
 *
 * get_rangevector looks at op1 and op2, and fills in the
 * structure for op1 to get to op2.
 * We already trust that the caller has verified that the
 * two objects are at least on adjacent maps.  If not,
 * results are not likely to be what is desired.
 * if the objects are not on maps, results are also likely to
 * be unexpected
 *
 * Flags: 0x1 is don't translate for closest body part.
 *        0x2 is do recursive search on adjacent tiles.
 * + any flags accepted by get_rangevector_from_mapcoords() below.
 *
 * Returns TRUE if successful, or FALSE otherwise.
 * @todo Document. */
int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags)
{
    if (!get_rangevector_from_mapcoords(op1->map, op1->x, op1->y, op2->map, op2->x, op2->y, retval, flags | RV_NO_DISTANCE)) {
        return 0;
    }

    retval->part = op1;

    /* If this is multipart, find the closest part now */
    if (!(flags & RV_IGNORE_MULTIPART) && op1->more) {
        object *tmp, *best = NULL;
        int best_distance = retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y, tmpi;

        /* we just take the offset of the piece to head to figure
         * distance instead of doing all that work above again
         * since the distance fields we set above are positive in the
         * same axis as is used for multipart objects, the simply arithmetic
         * below works. */
        for (tmp = op1->more; tmp; tmp = tmp->more) {
            tmpi = (retval->distance_x - tmp->arch->clone.x) * (retval->distance_x - tmp->arch->clone.x) + (retval->distance_y - tmp->arch->clone.y) * (retval->distance_y - tmp->arch->clone.y);

            if (tmpi < best_distance) {
                best_distance = tmpi;
                best = tmp;
            }
        }

        if (best) {
            retval->distance_x -= best->arch->clone.x;
            retval->distance_y -= best->arch->clone.y;
            retval->part = best;
        }
    }

    retval->distance = isqrt(retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y);
    retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);

    return 1;
}

/**
 * This is the base for get_rangevector above, but can more generally compute
 * the
 * rangvector between any two points on any maps.
 *
 * The part field of the rangevector is always set to NULL by this function.
 * (Since we don't actually know about any objects)
 *
 * If the function fails (because of the maps being separate), it will return
 * FALSE and
 * the vector is not otherwise touched. Otherwise it will return TRUE.
 *
 * Calculates manhattan distance (dx+dy) per default. (fast)
 * - Flags:
 *   0x4 - calculate euclidean (straight line) distance (slow)
 *   0x8 - calculate diagonal  (max(dx + dy)) distance   (fast)
 *   0x8|0x04 - don't calculate distance (or direction) (fastest)
 * @todo Document. */
int get_rangevector_from_mapcoords(mapstruct *map1, int x, int y, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags)
{
    retval->part = NULL;
    retval->distance_z = 0;

    if (map1 == map2) {
        retval->distance_x = x2 - x;
        retval->distance_y = y2 - y;
    } else if (map1->tile_map[TILED_NORTH] == map2) {
        retval->distance_x = x2 - x;
        retval->distance_y = -(y + (MAP_HEIGHT(map2) - y2));
    } else if (map1->tile_map[TILED_EAST] == map2) {
        retval->distance_y = y2 - y;
        retval->distance_x = (MAP_WIDTH(map1) - x) + x2;
    } else if (map1->tile_map[TILED_SOUTH] == map2) {
        retval->distance_x = x2 - x;
        retval->distance_y = (MAP_HEIGHT(map1) - y) + y2;
    } else if (map1->tile_map[TILED_WEST] == map2) {
        retval->distance_y = y2 - y;
        retval->distance_x = -(x + (MAP_WIDTH(map2) - x2));
    } else if (map1->tile_map[TILED_NORTHEAST] == map2) {
        retval->distance_y = -(y + (MAP_HEIGHT(map2) - y2));
        retval->distance_x = (MAP_WIDTH(map1) - x) + x2;
    } else if (map1->tile_map[TILED_SOUTHEAST] == map2) {
        retval->distance_x = (MAP_WIDTH(map1) - x) + x2;
        retval->distance_y = (MAP_HEIGHT(map1) - y) + y2;
    } else if (map1->tile_map[TILED_SOUTHWEST] == map2) {
        retval->distance_y = (MAP_HEIGHT(map1) - y) + y2;
        retval->distance_x = -(x + (MAP_WIDTH(map2) - x2));
    } else if (map1->tile_map[TILED_NORTHWEST] == map2) {
        retval->distance_x = -(x + (MAP_WIDTH(map2) - x2));
        retval->distance_y = -(y + (MAP_HEIGHT(map2) - y2));
    } else if (map1->tile_map[TILED_UP] == map2) {
        retval->distance_x = x2 - x;
        retval->distance_y = y2 - y;
        retval->distance_z = 1;
    } else if (map1->tile_map[TILED_DOWN] == map2) {
        retval->distance_x = x2 - x;
        retval->distance_y = y2 - y;
        retval->distance_z = -1;
    } else {
        retval->distance_x = x2;
        retval->distance_y = y2;

        if (!(flags & RV_RECURSIVE_SEARCH)) {
            flags |= RV_NO_LOAD;
        }

        if (!relative_tile_position(map1, map2, &retval->distance_x,
                &retval->distance_y, &retval->distance_z, flags)) {
            log(LOG(INFO), "didn't find relative tile pos");
            return 0;
        }

        retval->distance_x -= x;
        retval->distance_y -= y;
    }

    switch (flags & (RV_EUCLIDIAN_DISTANCE | RV_DIAGONAL_DISTANCE)) {
    case RV_MANHATTAN_DISTANCE:
        retval->distance = abs(retval->distance_x) + abs(retval->distance_y);
        break;

    case RV_EUCLIDIAN_DISTANCE:
        retval->distance = isqrt(retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y);
        break;

    case RV_DIAGONAL_DISTANCE:
        retval->distance = MAX(abs(retval->distance_x), abs(retval->distance_y));
        break;

        /* No distance calc */
    case RV_NO_DISTANCE:
        return 1;
    }

    retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);
    return 1;
}

static int on_same_map_exits(mapstruct *map1, mapstruct *map2)
{
    map_exit_t *exit;
    mapstruct *m;
    int i;

    DL_FOREACH(map1->exits, exit)
    {
        m = exit_get_destination(exit->obj, NULL, NULL, 0);

        if (m == NULL) {
            continue;
        }

        if (m == map2) {
            return 1;
        }

        for (i = 0; i < TILED_NUM_DIR; i++) {
            if (m->tile_map[i] == map2) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Checks whether two objects are on the same map, taking map tiling
 * into account.
 * @param op1 First object to check for.
 * @param op2 Second object to check against the first.
 * @return 1 if both objects are on same map, 0 otherwise. */
int on_same_map(object *op1, object *op2)
{
    int i;

    if (op1->map == NULL || op2->map == NULL) {
        return 0;
    }

    if (op1->map == op2->map) {
        return 1;
    }

    for (i = 0; i < TILED_NUM; i++) {
        if (op1->map->tile_map[i] == NULL ||
                op1->map->tile_map[i]->in_memory != MAP_IN_MEMORY) {
            continue;
        }

        if (op1->map->tile_map[i] == op2->map) {
            return 1;
        }

        if (on_same_map_exits(op1->map->tile_map[i], op2->map)) {
            return 1;
        }
    }

    if (on_same_map_exits(op1->map, op2->map)) {
        return 1;
    }

    return 0;
}

/**
 * Count the players on a map, using the local map player list.
 * @param m The map.
 * @return Number of players on this map. */
int players_on_map(mapstruct *m)
{
    object *tmp;
    int count;

    for (count = 0, tmp = m->player_first; tmp; tmp = CONTR(tmp)->map_above) {
        count++;
    }

    return count;
}

/**
 * Returns true if square x, y has P_NO_PASS set, which is true for walls
 * and doors but not monsters.
 * @param m Map to check for
 * @param x X coordinate to check for
 * @param y Y coordinate to check for
 * @return Non zero if blocked, 0 otherwise. */
int wall_blocked(mapstruct *m, int x, int y)
{
    int r;

    if (!(m = get_map_from_coord(m, &x, &y))) {
        return 1;
    }

    r = GET_MAP_FLAGS(m, x, y) & (P_NO_PASS | P_PASS_THRU);

    return r;
}

int map_get_darkness(mapstruct *m, int x, int y, object **mirror)
{
    MapSpace *msp;
    uint8 outdoor;
    int darkness;

    if (mirror) {
        *mirror = NULL;
    }

    msp = GET_MAP_SPACE_PTR(m, x, y);
    outdoor = MAP_OUTDOORS(m) || (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->item_power == -2);

    if (((outdoor && !(msp->flags & P_OUTDOOR)) || (!outdoor && msp->flags & P_OUTDOOR)) && (!msp->map_info || !OBJECT_VALID(msp->map_info, msp->map_info_count) || msp->map_info->item_power < 0)) {
        darkness = msp->light_value + global_darkness_table[world_darkness];
    } else {
        /* Check if map info object bound to this tile has a darkness. */
        if (msp->map_info && OBJECT_VALID(msp->map_info, msp->map_info_count) && msp->map_info->item_power != -1) {
            int dark_value;

            dark_value = msp->map_info->item_power;

            if (dark_value < 0 || dark_value > MAX_DARKNESS) {
                dark_value = MAX_DARKNESS;
            }

            darkness = global_darkness_table[dark_value] + msp->light_value;
        } else {
            darkness = m->light_value + msp->light_value;
        }
    }

    if (msp->flags & P_MAGIC_MIRROR) {
        object *tmp;
        magic_mirror_struct *m_data;
        mapstruct *mirror_map;

        FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_SYS, -1, tmp)
        {
            if (tmp->type == MAGIC_MIRROR) {
                if (mirror) {
                    *mirror = tmp;
                }

                m_data = MMIRROR(tmp);

                if (m_data && (mirror_map = magic_mirror_get_map(tmp)) && !OUT_OF_MAP(mirror_map, m_data->x, m_data->y)) {
                    MapSpace *mirror_msp = GET_MAP_SPACE_PTR(mirror_map, m_data->x, m_data->y);

                    if ((MAP_OUTDOORS(mirror_map) && !(mirror_msp->flags & P_OUTDOOR)) || (!MAP_OUTDOORS(mirror_map) && mirror_msp->flags & P_OUTDOOR)) {
                        darkness = mirror_msp->light_value + global_darkness_table[world_darkness];
                    } else {
                        darkness = mirror_map->light_value + mirror_msp->light_value;
                    }
                }

                break;
            }
        }
        FOR_MAP_LAYER_END
    }

    return darkness;
}

int map_path_isabs(const char *path)
{
    if (path == NULL) {
        return 0;
    }

    if (*path == '/' || string_startswith(path, settings.datapath)) {
        return 1;
    }

    return 0;
}

char *map_get_path(mapstruct *m, const char *path, uint8 unique, const char *name)
{
    char *path_tmp, *ret;

    path_tmp = NULL;

    if (m != NULL && MAP_UNIQUE(m)) {
        if (path && map_path_isabs(path)) {
            if (unique) {
                char *dir, *cp;

                dir = path_dirname(m->path);
                cp = estrdup(path);
                string_replace_char(cp, "/", '$');

                ret = path_join(dir, cp);

                efree(cp);
                efree(dir);
            } else {
                return estrdup(path);
            }
        } else {
            char *file, *filedir, *joined;

            /* Demangle the original map path, and get the original
             * directory the map was in. */
            file = path_basename(m->path);
            string_replace_char(file, "$", '/');
            filedir = path_dirname(file);

            if (!path) {
                path = path_tmp = path_basename(file);
            }

            if (unique) {
                char *newpath, *dir;

                /* Construct the new path. */
                joined = path_join(filedir, path);
                newpath = path_normalize(joined);
                string_replace_char(newpath, "/", '$');

                /* We need the data directory the map is in. */
                dir = path_dirname(m->path);

                /* Construct the path pointing inside the data directory. */
                ret = path_join(dir, newpath);

                efree(newpath);
                efree(dir);
            } else {
                joined = path_join(filedir, path);
                ret = path_normalize(joined);
            }

            efree(joined);
            efree(filedir);
            efree(file);
        }
    } else {
        if (path && map_path_isabs(path)) {
            if (unique && name) {
                char *cp;

                cp = estrdup(path);
                string_replace_char(cp, "/", '$');

                ret = player_make_path(name, cp);

                efree(cp);
            } else {
                return estrdup(path);
            }
        } else if (m != NULL) {
            char *filedir, *joined;

            filedir = path_dirname(m->path);

            if (!path) {
                path = path_tmp = path_basename(m->path);
            }

            if (unique && name) {
                char *newpath;

                /* Construct the new path. */
                joined = path_join(filedir, path);
                newpath = path_normalize(joined);
                string_replace_char(newpath, "/", '$');

                ret = player_make_path(name, newpath);

                efree(newpath);
            } else {
                joined = path_join(filedir, path);
                ret = path_normalize(joined);
            }

            efree(joined);
            efree(filedir);
        } else {
            return estrdup(EMERGENCY_MAPPATH);
        }
    }

    if (path_tmp) {
        efree(path_tmp);
    }

    return ret;
}

/**
 * Force the reset of a map, removing any players on it, swapping it out
 * and reloading it.
 * @param m Map to reset.
 * @return Reset map on success, NULL on failure. */
mapstruct *map_force_reset(mapstruct *m)
{
    object *tmp, *next, **players;
    size_t players_num, i;
    shstr *path;
    int flags;

    /* Cannot reset no-save unique maps or random maps. */
    if (m == NULL || (MAP_UNIQUE(m) && MAP_NOSAVE(m)) || strncmp(m->path, "/random/", 8) == 0) {
        return NULL;
    }

    players = NULL;
    players_num = 0;

    for (tmp = m->player_first; tmp; tmp = next) {
        next = CONTR(tmp)->map_above;

        leave_map(tmp);
        players = erealloc(players, sizeof(*players) * (players_num + 1));
        players[players_num] = tmp;
        players_num++;
    }

    m->reset_time = seconds();
    m->map_flags |= MAP_FLAG_FIXED_RTIME;
    /* Store the path, so we can load it after swapping is done. */
    path = add_refcount(m->path);
    flags = MAP_NAME_SHARED | (MAP_UNIQUE(m) ? MAP_PLAYER_UNIQUE : 0);

    if (m->in_memory == MAP_IN_MEMORY) {
        swap_map(m, 1);
    }

    clean_tmp_map(m);

    m = ready_map_name(path, flags);
    free_string_shared(path);

    for (i = 0; i < players_num; i++) {
        insert_ob_in_map(players[i], m, NULL, INS_NO_MERGE);
    }

    if (players) {
        efree(players);
    }

    return m;
}

/**
 * Internal function used by map_redraw().
 * @param tiled Tiled map.
 * @param map Map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param layer Layer to redraw, defaults to all.
 * @param sub_layer Sub-layer to redraw, defaults to all.
 * @return 0.
 */
static int map_redraw_internal(mapstruct *tiled, mapstruct *map, int x, int y,
        int layer, int sub_layer)
{
    object *pl;
    rv_vector rv;
    int ax, ay, layer_start, layer_end, sub_layer_start, sub_layer_end,
            socket_layer;

    if (layer == -1) {
        layer_start = LAYER_FLOOR;
        layer_end = NUM_LAYERS;
    } else {
        layer_start = layer_end = layer;
    }

    if (sub_layer == -1) {
        sub_layer_start = 0;
        sub_layer_end = NUM_SUB_LAYERS - 1;
    } else {
        sub_layer_start = sub_layer_end = sub_layer;
    }

    for (pl = tiled->player_first; pl != NULL; pl = CONTR(pl)->map_above) {
        if (!get_rangevector_from_mapcoords(pl->map, pl->x, pl->y, map, x, y,
                &rv, RV_NO_DISTANCE)) {
            continue;
        }

        ax = CONTR(pl)->socket.mapx_2 + rv.distance_x;
        ay = CONTR(pl)->socket.mapy_2 + rv.distance_y;

        if (ax < 0 || ax >= CONTR(pl)->socket.mapx ||
                ay < 0 || ay >= CONTR(pl)->socket.mapy) {
            continue;
        }

        for (layer = layer_start; layer <= layer_end; layer++) {
            for (sub_layer = sub_layer_start; sub_layer <= sub_layer_end;
                    sub_layer++) {
                socket_layer = NUM_LAYERS * sub_layer + layer - 1;

                CONTR(pl)->socket.lastmap.cells[ax][ay].faces[socket_layer] = 0;
            }
        }
    }

    return 0;
}

/**
 * Force redrawing of all objects on the specified tile, for all players that
 * can see it.
 * @param m Map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param layer Layer to redraw, -1 for all.
 * @param sub_layer Sub-layer to redraw, -1 for all.
 */
void map_redraw(mapstruct *m, int x, int y, int layer, int sub_layer)
{
    assert(m != NULL);
    assert(x >= 0 && x < m->width);
    assert(y >= 0 && y < m->height);
    assert(layer >= -1 && layer <= NUM_LAYERS);
    assert(sub_layer >= -1 && sub_layer < NUM_SUB_LAYERS);

    MAP_TILES_WALK_START(m, map_redraw_internal, x, y, layer, sub_layer)
    {
    }
    MAP_TILES_WALK_END
}

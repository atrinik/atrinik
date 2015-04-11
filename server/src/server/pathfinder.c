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
 * Pathfinding related code.
 *
 * Good pathfinding resources:
 * - Amit's Thoughts on Path-Finding and A-Star
 * http://theory.stanford.edu/~amitp/GameProgramming/
 * - Smart Moves: Intelligent Pathfinding
 * http://www.gamasutra.com/view/feature/3317/smart_move_intelligent_.php
 *
 * Possible enhancements (profile and identify need before spending time on):
 *  - Replace the open list with a binary heap or skip list. Will enhance
 * insertion performance.
 *
 * About monster-to-player paths:
 *  - use smaller max-number-of-nodes value (we don't want a mob running away
 * across the map)
 *
 * @author Alex Tokar - new A* algorithm and heuristics
 * @author Bjorn Axelsson (gecko@acc.umu.se) - original algorithm, functions and
 * structures
 */

#include <global.h>

/**
 * Selects algorithm to use for path-finding.
 *
 * 0 = BFS, 0.5 = A*, 1.0 = Dijkstra's
 */
#define ALGORITHM 0.5

/**
 * Greed modifier.
 */
#define GREED 1.

/**
 * Turns pathfinding profiling on/off.
 */
#define TIME_PATHFINDING 0

/**
 * Whether to visualize pathfinding.
 */
#define VISUALIZE_PATHFINDING 0

/**
 * Size of the pathfinder queue.
 */
#define PATHFINDER_QUEUE_SIZE 100

/**
 * Maximum number of node buffers.
 */
#define PATHFINDER_NODEBUF 1000

/**
 * Path cost when moving in a straight line.
 */
#define PATH_COST 1.0

/**
 * Path cost when moving diagonally.
 */
#define PATH_COST_DIAG 1.01

/**
 * Path cost per z level.
 */
#define PATH_COST_LEVEL 1000.

/**
 * The pathfinder queue.
 */
static struct {
    object *waypoint; ///< Waypoint object.
    tag_t wp_count; ///< Waypoint's ID.
} pathfinder_queue[PATHFINDER_QUEUE_SIZE];

/**
 * First in the queue.
 */
static int pathfinder_queue_first = 0;
/**
 * Last in the queue.
 */
static int pathfinder_queue_last = 0;

/**
 * The node buffers. Used to avoid lots of mallocs.
 */
static path_node_t pathfinder_nodebuf[PATHFINDER_NODEBUF];
/**
 * Next node buf.
 */
static int pathfinder_nodebuf_next = 0;

/**
 * Enqueue a waypoint for path computation.
 * @param waypoint Waypoint.
 * @return 1 on success, 0 on failure.
 */
static int pathfinder_queue_enqueue(object *waypoint)
{
    assert(waypoint != NULL);

    /* Queue full? */
    if (pathfinder_queue_last == pathfinder_queue_first - 1 ||
            (pathfinder_queue_first == 0 &&
            pathfinder_queue_last == PATHFINDER_QUEUE_SIZE - 1)) {
        return 0;
    }

    pathfinder_queue[pathfinder_queue_last].waypoint = waypoint;
    pathfinder_queue[pathfinder_queue_last].wp_count = waypoint->count;

    if (++pathfinder_queue_last >= PATHFINDER_QUEUE_SIZE) {
        pathfinder_queue_last = 0;
    }

    return 1;
}

/**
 * Get the first waypoint from the queue.
 * @param[out] count Waypoint's ID if there is a valid waypoint.
 * @return The waypoint, NULL if the queue is empty.
 */
static object *pathfinder_queue_dequeue(tag_t *count)
{
    object *waypoint;

    assert(count != NULL);

    /* Queue empty? */
    if (pathfinder_queue_last == pathfinder_queue_first) {
        return NULL;
    }

    waypoint = pathfinder_queue[pathfinder_queue_first].waypoint;
    *count = pathfinder_queue[pathfinder_queue_first].wp_count;

    if (++pathfinder_queue_first >= PATHFINDER_QUEUE_SIZE) {
        pathfinder_queue_first = 0;
    }

    return waypoint;
}

/**
 * Request a new path.
 * @param waypoint Waypoint.
 */
void path_request(object *waypoint)
{
    assert(waypoint != NULL);

    if (QUERY_FLAG(waypoint, FLAG_WP_PATH_REQUESTED)) {
        return;
    }

#ifdef DEBUG_PATHFINDING
    logger_print(LOG(DEBUG), "enqueuing path request for >%s< -> >%s<",
            waypoint->env->name, waypoint->name);
#endif

    if (pathfinder_queue_enqueue(waypoint)) {
        SET_FLAG(waypoint, FLAG_WP_PATH_REQUESTED);
        waypoint->owner = waypoint->env;
        waypoint->ownercount = waypoint->env->count;
    }
}

/**
 * Get the next (valid) waypoint for which a path is requested.
 * @return Waypoint, NULL if there isn't any left.
 */
object *path_get_next_request(void)
{
    object *waypoint;
    tag_t count;

    do {
        waypoint = pathfinder_queue_dequeue(&count);

        if (waypoint == NULL) {
            return NULL;
        }

        /* Verify the waypoint and its monster. */
        if (!OBJECT_VALID(waypoint, count) || !OBJECT_VALID(waypoint->owner,
                waypoint->ownercount) || !(QUERY_FLAG(waypoint, FLAG_CURSED) ||
                QUERY_FLAG(waypoint, FLAG_DAMNED)) || (QUERY_FLAG(waypoint,
                FLAG_DAMNED) && !OBJECT_VALID(waypoint->enemy,
                waypoint->enemy_count))) {
            waypoint = NULL;
        }
    } while (waypoint == NULL);

#ifdef DEBUG_PATHFINDING
    logger_print(LOG(DEBUG), "dequeued '%s' -> '%s'", waypoint->owner->name,
            waypoint->name);
#endif

    CLEAR_FLAG(waypoint, FLAG_WP_PATH_REQUESTED);
    return waypoint;
}

/**
 * Allocate and initialize a node.
 *
 * Also calculates the appropriate heuristics from 'start' and 'goal'
 * parameters.
 * @param map Map.
 * @param x X position.
 * @param y Y position.
 * @param cost Cost.
 * @param start Starting node.
 * @param goal Goal node.
 * @param parent Parent node.
 * @return New node.
 */
static path_node_t *path_node_new(mapstruct *map, sint16 x, sint16 y,
        double cost, path_node_t *start, path_node_t *goal, path_node_t *parent)
{
    path_node_t *node;
    rv_vector rv, rv2;
    int cross, straight, diagonal;

    assert(map != NULL);
    assert(!OUT_OF_MAP(map, x, y));
    assert(start != NULL);
    assert(goal != NULL);

    /* Out of memory? */
    if (pathfinder_nodebuf_next == PATHFINDER_NODEBUF) {
#ifdef DEBUG_PATHFINDING
        logger_print(LOG(DEBUG), "Out of static buffer memory");
#endif
        return NULL;
    }

    if (!get_rangevector_from_mapcoords(map, x, y, goal->map, goal->x, goal->y,
            &rv, RV_RECURSIVE_SEARCH | RV_NO_DISTANCE)) {
        return NULL;
    }

    if (!get_rangevector_from_mapcoords(start->map, start->x, start->y,
            goal->map, goal->x, goal->y, &rv2, RV_RECURSIVE_SEARCH |
            RV_NO_DISTANCE)) {
        return NULL;
    }

    cross = abs(rv.distance_x * rv2.distance_y - rv2.distance_x *
            rv.distance_y);
    straight = abs(abs(rv.distance_x) - abs(rv.distance_y));
    diagonal = MAX(abs(rv.distance_x), abs(rv.distance_y)) - straight;

    node = &pathfinder_nodebuf[pathfinder_nodebuf_next++];

    node->next = NULL;
    node->prev = NULL;
    node->parent = parent;
    node->map = map;
    node->x = x;
    node->y = y;
    node->cost = cost;
    node->flags = 0;
    node->distance_z = abs(rv.distance_z);
    node->heuristic = straight + PATH_COST_DIAG * diagonal + cross * 0.001 +
            abs(rv.distance_z) * PATH_COST_LEVEL;
    node->heuristic *= GREED;
    node->sum = (ALGORITHM * node->cost + (1 - ALGORITHM) *
            node->heuristic) / MAX(ALGORITHM, 1 - ALGORITHM);

    return node;
}

/**
 * Remove a node from a list.
 * @param node Node to remove.
 * @param[out] list List to remove from.
 */
static void path_node_remove(path_node_t *node, path_node_t **list)
{
    assert(node != NULL);
    assert(list != NULL);

    if (*list == NULL) {
        log(LOG(ERROR), "Removing node from an empty list: %s, %d, %d",
                node->map->path, node->x, node->y);
        return;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        *list = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    node->next = node->prev = NULL;
}

/**
 * Insert a node into a list.
 * @param node Node to insert.
 * @param[out] list List to insert into.
 */
static void path_node_insert(path_node_t *node, path_node_t **list)
{
    assert(node != NULL);
    assert(list != NULL);

    if (*list != NULL) {
        (*list)->prev = node;
    }

    node->next = *list;
    node->prev = NULL;

    *list = node;
}

/**
 * Insert a node in a sorted list (lowest path_node_t::sum first in list).
 * @param node Node to insert.
 * @param list List to insert into.
 * @todo Make more efficient by using skip list or heaps.
 */
static void path_node_insert_priority(path_node_t *node, path_node_t **list)
{
    path_node_t *tmp, *last, *insert_before;

    assert(node != NULL);
    assert(list != NULL);

    insert_before = NULL;

    /* Figure out which node to insert in front of. */
    for (tmp = *list; tmp != NULL; tmp = tmp->next) {
        last = tmp;

        if (node->sum <= tmp->sum) {
            insert_before = tmp;
            break;
        }
    }

    if (insert_before == *list) {
        /* Insert first. */
        path_node_insert(node, list);
    } else if (insert_before == NULL) {
        /* Insert last. */
        node->next = NULL;
        node->prev = last;
        last->next = node;
    } else {
        /* Insert in middle. */
        node->next = insert_before;
        node->prev = insert_before->prev;
        insert_before->prev = node;

        if (node->prev != NULL) {
            node->prev->next = node;
        }
    }
}

/**
 * Check if the specified tile is blocked.
 * @param op Object.
 * @param map Map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return 0 if the tile is not blocked, non-zero otherwise.
 */
static int tile_is_blocked(object *op, mapstruct *map, int x, int y)
{
    int block;

    if (op->type == PLAYER && CONTR(op)->tcl) {
        block = 0;
    } else {
        block = object_blocked(op, map, x, y);
    }

    return block;
}

/**
 * Generate a string representation of a path.
 *
 * Ideas on how to store paths:
 * - a) Store path as real waypoint objects (might be a lot of objects...)
 * - b) Store path as field in waypoints
 *   - b1) Linked list in i.e. ob->enemy (needs special free() call when
 * removing object).
 *   - b2) In ASCII in waypoint->msg (will even be saved out).
 *     - b2.1) Direction list (e.g. 1234155532, compact but fragile)
 *     - b2.2) Map / coordinate list: (/dev/testmaps:13,12 14,12 ...)
 * (human-readable (and editable), complex parsing)
 *             Approx: 600 steps in one 4096 bytes msg field
 *     - b2.2.1) Hex (/dev/testmaps/xxx D,C E,C ...) (harder to read and write,
 * more compact)
 *               Approx: 1000 steps in one 4096 bytes msg field
 *
 * @param path Path node.
 * @return Encoded path as a shared string.
 */
shstr *path_encode(path_node_t *path)
{
    mapstruct *last_map;
    StringBuffer *sb;
    path_node_t *tmp;
    char *cp;
    shstr *ret;

    assert(path != NULL);

    last_map = NULL;
    sb = stringbuffer_new();

    for (tmp = path; tmp != NULL; tmp = tmp->next) {
        if (tmp->map != last_map) {
            if (last_map != NULL) {
                stringbuffer_append_char(sb, '\n');
            }

            stringbuffer_append_string(sb, tmp->map->path);
            last_map = tmp->map;
        }

        stringbuffer_append_printf(sb, " %d,%d,%d", tmp->x, tmp->y, tmp->flags);
    }

    cp = stringbuffer_finish(sb);
    ret = add_string(cp);
    efree(cp);

    return ret;
}

/**
 * Get the next location from a textual path description (generated by
 * path_encode()) starting from the character index indicated by off.
 *
 * Example text path description:
 * - /dev/testmaps/testmap_waypoints 17,7 17,10 18,11 19,10 20,10
 * - /dev/testmaps/testmap_waypoints2 8,22
 * - /dev/testmaps/testmap_waypoints3 0,22 1,23
 * - /dev/testmaps/testmap_waypoints4 1,1 2,2
 * @param buf Buffer.
 * @param off Offset.
 * @param mappath Map path.
 * @param map Map. Should be initialized with whatever map the object we are
 * working on currently lives on (to handle paths without map strings).
 * @param x X position.
 * @param y Y position.
 * @param flags Flags.
 * @return If a location is found, will return 1 and update map, x, y and off
 * (off will be set to the index to use for the next call to this function).
 * Otherwise 0 will be returned and the values of map, x and y will be undefined
 * and off will not be touched. */
int path_get_next(shstr *buf, sint16 *off, shstr **mappath, mapstruct **map,
        int *x, int *y, uint32 *flags)
{
    const char *coord_start, *coord_end, *map_def;

    assert(buf != NULL);
    assert(off != NULL);
    assert(mappath != NULL);
    assert(map != NULL && *map != NULL);
    assert(x != NULL);
    assert(y != NULL);
    assert(flags != NULL);

    map_def = coord_start = buf + *off;

    if (string_isempty(*mappath)) {
        /* Scan backwards from requested offset to previous linebreak or start
         * of string */
        for (map_def = coord_start; map_def > buf && *(map_def - 1) != '\n';
                map_def--) {
        }
    }

    /* Extract map path if any at the current position (this part is only used
     * when we go between map tiles, or when we extract the first step). */
    if (!isdigit(*map_def)) {
        /* Temporary buffer for map path extraction */
        char map_name[HUGE_BUF];
        const char *mapend;

        /* Find the end of the map path */
        mapend = strchr(map_def, ' ');

        if (mapend == NULL) {
            log(LOG(BUG), "No delimeter after map name in path description "
                    "'%s' off %d", buf, *off);
            return 0;
        }

        strncpy(map_name, map_def, mapend - map_def);
        map_name[mapend - map_def] = '\0';

        /* Store the new map path in the given shared string */
        FREE_AND_COPY_HASH(*mappath, map_name);

        /* Adjust coordinate pointer to point after map path */
        if (!isdigit(*coord_start)) {
            coord_start = mapend + 1;
        }
    }

    /* Select the map we are aiming at. */
    if (*mappath) {
        /* We assume map name is already normalized. */
        if (*map == NULL || (*map)->path != *mappath) {
            *map = ready_map_name(*mappath, MAP_NAME_SHARED);
        }
    }

    if (*map == NULL) {
        log(LOG(BUG), "Couldn't load map from description '%s' off %d", buf,
                *off);
        return 0;
    }

    /* Get the requested coordinate pair. */
    coord_end = coord_start + strcspn(coord_start, " \n");

    if (coord_end == coord_start || sscanf(coord_start, "%d,%d,%d", x, y,
            flags) != 3) {
        log(LOG(BUG), "Illegal coordinate pair in '%s' off %d", buf, *off);
        return 0;
    }

    /* Adjust coordinates to be on the safe side */
    *map = get_map_from_coord(*map, x, y);

    if (*map == NULL) {
        log(LOG(BUG), "Location (%d, %d) is out of map", *x, *y);
        return 0;
    }

    /* Adjust the offset */
    *off = coord_end - buf + (*coord_end ? 1 : 0);

    return 1;
}

/**
 * Compress a path by removing redundant segments.
 *
 * Current implementation removes segments that can be traversed by walking in a
 * single direction.
 *
 * Something advanced could be to use a hughes transform / or something smart
 * with cross products.
 *
 * Rules:
 *  - always leave first and last path nodes
 *  - if the movement direction of node n to n+1 is the same
 *    as for n-1 to n, then remove node n.
 * @param path Path node.
 * @return 'path' with redundant segments removed.
 */
path_node_t *path_compress(path_node_t *path)
{
    path_node_t *tmp, *next;
    int last_dir;
    rv_vector v;
#ifdef DEBUG_PATHFINDING
    int removed_nodes = 0, total_nodes = 2;
#endif

    /* Guarantee at least length 3 */
    if (path == NULL || path->next == NULL) {
        return path;
    }

    next = path->next;

    get_rangevector_from_mapcoords(path->map, path->x, path->y, next->map,
            next->x, next->y, &v, RV_EUCLIDIAN_DISTANCE);
    last_dir = v.direction;

    for (tmp = next; tmp != NULL && tmp->next != NULL; tmp = next) {
        next = tmp->next;

#ifdef DEBUG_PATHFINDING
        total_nodes++;
#endif
        get_rangevector_from_mapcoords(tmp->map, tmp->x, tmp->y, next->map,
                next->x, next->y, &v, RV_EUCLIDIAN_DISTANCE);

        if (last_dir == v.direction) {
            path_node_remove(tmp, &path);
#ifdef DEBUG_PATHFINDING
            removed_nodes++;
#endif
        } else {
            last_dir = v.direction;
        }
    }

#ifdef DEBUG_PATHFINDING
    logger_print(LOG(DEBUG), "removed %d nodes of %d (%.0f%%)", removed_nodes,
            total_nodes, (float) removed_nodes * 100.0 / (float) total_nodes);
#endif

    return path;
}

/**
 * Build a visualization hash table out of a list of visited/closed nodes.
 * @param[out] visualization Hash table to use. Must be initialized to NULL.
 * @param[out] visualizer List of the visited/closed nodes.
 */
void path_visualize(path_visualization_t **visualization,
        path_visualizer_t **visualizer)
{
    path_visualizer_t *node, *tmp;
    path_visualization_t *visualization_node;

    assert(visualization != NULL);
    assert(visualizer != NULL);

    DL_FOREACH_SAFE(*visualizer, node, tmp)
    {
        HASH_FIND(hh, *visualization, &node->map->path, sizeof(shstr *),
                visualization_node);

        if (visualization_node == NULL) {
            visualization_node = ecalloc(1, sizeof(*visualization_node));
            FREE_AND_ADD_REF_HASH(visualization_node->path, node->map->path);
            HASH_ADD(hh, *visualization, path, sizeof(shstr *),
                    visualization_node);
        }

        DL_DELETE(*visualizer, node);
        DL_APPEND(visualization_node->nodes, node);
    }
}

/**
 * Find a path for op from location on map1 to location on map2.
 * @param op Object.
 * @param map1 From map.
 * @param x1 From X position.
 * @param y1 From Y position.
 * @param map2 To map.
 * @param x2 To X position.
 * @param y2 To Y position.
 * @param visualizer[out] Visualizer pointer where to store visited/closed
 * nodes. Can be NULL, otherwise the pointer MUST be initialized to NULL.
 * @return Found path.
 */
path_node_t *path_find(object *op, mapstruct *map1, int x, int y,
        mapstruct *map2, int x2, int y2, path_visualizer_t **visualizer)
{
    path_node_t *open_list, *found_path, *visited, *node, *new_node, *best;
    path_node_t start, goal;
    static uint32 traversal_id = 0;
    int i, nx, ny, is_diagonal, node_x, node_y;
    mapstruct *m, *node_map;
    double cost;
    rv_vector rv;
    uint32 node_id;
#if VISUALIZE_PATHFINDING
    path_visualizer_t *visualizer_tmp;
#endif
#if TIME_PATHFINDING
    int searched;
    PERF_TIMER_DECLARE(1);
#endif

    start.x = x;
    start.y = y;
    start.map = map1;

    goal.x = x2;
    goal.y = y2;
    goal.map = map2;

    /* Avoid overflow of traversal_id */
    if (traversal_id == UINT32_MAX) {
        DL_FOREACH(first_map, m)
        {
            m->pathfinding_id = 0;
        }

        traversal_id = 0;
    }

#if TIME_PATHFINDING
    searched = 0;
    PERF_TIMER_START(1);
#endif

    traversal_id++;
    pathfinder_nodebuf_next = 0;
    node_id = 0;
    found_path = NULL;

#if VISUALIZE_PATHFINDING
    visualizer_tmp = NULL;

    if (visualizer == NULL) {
        visualizer = &visualizer_tmp;
    }
#endif

    /* The initial tile. */
    open_list = best = path_node_new(map1, x, y, 0.0, &start, &goal, NULL);

    if (open_list == NULL) {
        return NULL;
    }

    while (open_list != NULL && pathfinder_nodebuf_next < PATHFINDER_NODEBUF) {
        node = open_list;
        path_node_remove(node, &open_list);

        if (node->heuristic <= 1.2) {
            if (visualizer != NULL) {
                PATHFINDING_SET_CLOSED(node->map, node->x, node->y,
                        traversal_id, visualizer);
                PATHFINDING_SET_CLOSED(goal.map, goal.x, goal.y,
                        traversal_id, visualizer);
            }

            for ( ; node != NULL; node = node->parent) {
                path_node_insert(node, &found_path);
            }

            break;
        }

        /* Close this tile. */
        PATHFINDING_SET_CLOSED(node->map, node->x, node->y, traversal_id,
                visualizer);

        node_map = node->map;
        node_x = node->x;
        node_y = node->y;

        if (GET_MAP_FLAGS(node_map, node_x, node_y) & P_IS_EXIT &&
                op->behavior & BEHAVIOR_EXITS) {
            object *tmp;

            for (tmp = GET_MAP_OB(node_map, node_x, node_y); tmp != NULL;
                    tmp = tmp->above) {
                if (tmp->type == EXIT) {
                    m = exit_get_destination(tmp, &nx, &ny, 1);

                    /* Do not enter exits that have worse z distance than the
                     * current node. */
                    if (m != NULL && get_rangevector_from_mapcoords(m, node_x,
                            node_y, goal.map, goal.x, goal.y, &rv,
                            RV_RECURSIVE_SEARCH) && abs(rv.distance_z) <=
                            node->distance_z) {
                        node_map = m;
                        node_x = nx;
                        node_y = ny;

                        /* Add exit flag to the node with the exit, to indicate
                         * that the path user needs to use an exit on that tile
                         * (possibly having to apply it, in case it's not a
                         * portal or the like). */
                        node->flags |= PATH_NODE_EXIT;

                        /* Close the tile that the exit leads to. */
                        PATHFINDING_SET_CLOSED(node_map, node_x, node_y,
                                traversal_id, visualizer);

                        break;
                    }
                }
            }
        }

        for (i = 1; i <= SIZEOFFREE1; i++) {
            nx = node_x + freearr_x[i];
            ny = node_y + freearr_y[i];
            is_diagonal = nx != node_x && ny != node_y;

            m = get_map_from_coord(node_map, &nx, &ny);

            if (m == NULL) {
                continue;
            }

            /* Skip closed tiles. */
            if (PATHFINDING_QUERY_CLOSED(m, nx, ny, traversal_id)) {
                continue;
            }

            /* Skip blocked tiles. */
            if (tile_is_blocked(op, m, nx, ny) != 0) {
                continue;
            }

            /* If the object can't use secret passages and they're a player or a
             * that is not chasing an enemy, and this tile is a secret passage,
             * skip it. */
            if (!(GET_MAP_FLAGS(m, nx, ny) & P_DOOR_CLOSED) &&
                    (op->type != PLAYER || !CONTR(op)->tcl) &&
                    !(op->behavior & BEHAVIOR_SECRET_PASSAGES) &&
                    (op->type == PLAYER || !OBJECT_VALID(op->enemy,
                    op->enemy_count)) && blocks_view(m, nx, ny)) {
                continue;
            }

            /* Calculate the cost. */
            cost = node->cost + (is_diagonal ? PATH_COST_DIAG : PATH_COST);
            cost += GET_MAP_MOVE_FLAGS(m, nx, ny) * 0.001;

            if (op->behavior & BEHAVIOR_STEALTH) {
                cost += GET_MAP_LIGHT(m, nx, ny) * 0.001;
            }

            /* Get the visited path node on this tile, if any. */
            visited = PATHFINDING_NODE_GET(m, nx, ny, traversal_id);

            /* If we have visited this node previously, and the cost is not any
             * better, skip it. */
            if (visited != NULL && cost >= visited->cost) {
                continue;
            }

            if (visited != NULL) {
                /* Remove previously visited node from the open list. */
                path_node_remove(visited, &open_list);

                if (visited == best) {
                    best = NULL;
                }
            } else {
#if TIME_PATHFINDING
                searched++;
#endif
            }

            new_node = path_node_new(m, nx, ny, cost, &start, &goal, node);

            if (new_node == NULL) {
                continue;
            }

            path_node_insert_priority(new_node, &open_list);
            PATHFINDING_NODE_SET(m, nx, ny, traversal_id, new_node, visualizer);

            if (best == NULL || new_node->sum < best->sum) {
                best = new_node;
            }
        }
    }

    if (found_path == NULL) {
        for (node = best; node != NULL; node = node->parent) {
            path_node_insert(node, &found_path);
        }
    }

#if TIME_PATHFINDING
    PERF_TIMER_STOP(1);
    log(LOG(DEVEL), "Pathfinding took %f seconds (searched %d nodes)",
            PERF_TIMER_GET(1), searched);
#endif

#if VISUALIZE_PATHFINDING
    {
        char path[HUGE_BUF];
        FILE *fp;

        snprintf(path, sizeof(path), "%s/pathfinding/%u.json",
                settings.datapath, traversal_id);
        path_ensure_directories(path);

        fp = fopen(path, "w");

        if (fp == NULL) {
            log(LOG(BUG), "Could not open %s for writing.", path);
        } else {
            path_visualization_t *visualization, *curr, *tmp;
            path_visualizer_t *visualizer_node, *visualizer_node_tmp;
            StringBuffer *sb;
            char *cp;

            visualization = NULL;
            path_visualize(&visualization, visualizer);

            fprintf(fp, "{\"start\": {\"map\": \"%s\", \"x\": %d, \"y\": %d},\n"
                    "\"goal\": {\"map\": \"%s\", \"x\": %d, \"y\": %d},\n"
                    "\"nodes\": {\n", start.map->path, start.x, start.y,
                    goal.map->path, goal.x, goal.y);

            HASH_ITER(hh, visualization, curr, tmp)
            {
                m = has_been_loaded_sh(curr->path);
                fprintf(fp, "\"%s\": {\"walked\": [\n", m->path);

                DL_FOREACH_SAFE(curr->nodes, visualizer_node,
                        visualizer_node_tmp)
                {
                    fprintf(fp, "{\"id\": %u, \"x\": %d, \"y\": %d, "
                            "\"closed\": %s, \"exit\": %s", visualizer_node->id,
                            visualizer_node->x, visualizer_node->y,
                            visualizer_node->closed ? "true" : "false",
                            GET_MAP_FLAGS(m, visualizer_node->x,
                            visualizer_node->y) & P_IS_EXIT ? "true" : "false");

                    if (visualizer_node->node == NULL) {
                        fprintf(fp, ", \"cost\": NaN, \"heuristic\": NaN, "
                                "\"sum\": NaN");
                    } else {
                        fprintf(fp, ", \"cost\": %f, \"heuristic\": %f, "
                                "\"sum\": %f", visualizer_node->node->cost,
                                visualizer_node->node->heuristic,
                                visualizer_node->node->sum);
                    }

                    fprintf(fp, "}");

                    if (visualizer_node_tmp != NULL) {
                        fprintf(fp, ",");
                    }

                    fprintf(fp, "\n");

                    DL_DELETE(curr->nodes, visualizer_node);
                    efree(visualizer_node);
                }

                sb = stringbuffer_new();

                for (x = 0; x < m->width; x++) {
                    for (y = 0; y < m->height; y++) {
                        if (!tile_is_blocked(op, m, x, y)) {
                            continue;
                        }

                        if (stringbuffer_length(sb) != 0) {
                            stringbuffer_append_char(sb, ',');
                        }

                        stringbuffer_append_printf(sb, "\n{\"x\": %d, "
                                "\"y\": %d}", x, y);
                    }
                }

                cp = stringbuffer_finish(sb);
                fprintf(fp, "],\n\"walls\": [%s\n]}%s\n", cp,
                        tmp != NULL ? "," : "");
                efree(cp);

                HASH_DEL(visualization, curr);
                FREE_ONLY_HASH(curr->path);
                efree(curr);
            }

            fprintf(fp, "},\n\"path\": [\n");

            for (node = found_path; node != NULL; node = node->next) {
                m = NULL;

                if (node->flags & PATH_NODE_EXIT) {
                    object *exit;

                    for (exit = GET_MAP_OB(node->map, node->x, node->y);
                            exit != NULL; exit = exit->above) {
                        if (exit->type == EXIT) {
                            m = exit_get_destination(exit, &nx, &ny, 1);
                        }
                    }
                }

                fprintf(fp, "{\"map\": \"%s\", \"x\": %d, \"y\": %d, "
                        "\"flags\": %d}%s\n", node->map->path, node->x, node->y,
                        node->flags, node->next != NULL || m != NULL ? "," :
                            "");

                if (m != NULL) {
                    fprintf(fp, "{\"map\": \"%s\", \"x\": %d, \"y\": %d}%s\n",
                            m->path, nx, ny, node->next != NULL ? "," : "");

                }
            }

            fprintf(fp, "],\n\"time_taken\": ");

#if TIME_PATHFINDING
            fprintf(fp, "%f", PERF_TIMER_GET(1));
#else
            fprintf(fp, "NaN");
#endif

            fprintf(fp, ",\n\"num_searched\": ");

#if TIME_PATHFINDING
            fprintf(fp, "%d", searched);
#else
            fprintf(fp, "NaN");
#endif

            fprintf(fp, "\n}\n");

            fclose(fp);

            log(LOG(DEVEL), "Generated pathfinding visualization for '%s': %s",
                    op->name, path);
        }
    }
#endif

    return found_path;
}

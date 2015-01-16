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
 * Pathfinding header file
 */

#ifndef PATHFINDER_H
#define PATHFINDER_H

/**
 * @defgroup PATH_NODE_xxx Path node flags
 * Flags for individual tiles in the path.
 *@{*/
/**
 * The path node has an exit.
 */
#define PATH_NODE_EXIT 0x01
/*@}*/

/**
 * Path node.
 */
typedef struct path_node {
    struct path_node *next; ///< Next node in a linked list.
    struct path_node *prev; ///< Previous node in a linked list
    struct path_node *parent; ///< Node this was reached from.

    struct mapdef *map; ///< Pointer to the map.
    sint16 x; ///< X position on the map for this node.
    sint16 y; ///< Y position on the map for this node.
    uint8 flags; ///< A combination of @ref PATH_NODE_xxx.
    int distance_z; ///< Z distance from this node to the goal.

    double cost; ///< Cost of reaching this node (distance from origin).
    double heuristic; ///< Estimated cost of reaching the goal from this node.
    double sum; ///< Sum of ::cost and ::heuristic.
} path_node_t;

/**
 * Used for visualization of path nodes; represents one node.
 */
typedef struct path_visualizer {
    struct path_visualizer *next; ///< Next 'node'.
    struct path_visualizer *prev; ///< Previous 'node'.

    mapstruct *map; ///< Map.
    sint16 x; ///< X position.
    sint16 y; ///< Y position.

    path_node_t *node; ///< The actual node. Can be NULL.

    uint32 id; ///< UID of the node; can be used for insertion order sorting.
    bool closed; ///< Whether the node is closed or just visited.
} path_visualizer_t;

/**
 * Contains all the maps that were visited in a hash table, for the purposes of
 * path visualization.
 */
typedef struct path_visualization {
    shstr *path; ///< Map path. Used as key for the hash table.
    path_visualizer_t *nodes; ///< Visited nodes on this map.
    UT_hash_handle hh; ///< Hash handle.
} path_visualization_t;

#define PATHFINDING_CHECK_ID(m, id) \
    if ((m)->pathfinding_id != (id)) { \
        (m)->pathfinding_id = (id); \
        memset((m)->bitmap, 0, ((MAP_WIDTH(m) + 31) / 32) * MAP_HEIGHT(m) * \
                sizeof(*(m)->bitmap)); \
        memset((m)->path_nodes, 0, MAP_WIDTH(m) * MAP_HEIGHT(m) * \
                sizeof(*(m)->path_nodes)); \
    }

#define PATHFINDING_VISUALIZER_APPEND(visualizer, _m, _x, _y, _closed, _node) \
    if (visualizer != NULL) { \
        path_visualizer_t *__tmp; \
 \
        __tmp = ecalloc(1, sizeof(*__tmp)); \
        __tmp->map = (_m); \
        __tmp->x = (_x); \
        __tmp->y = (_y); \
        __tmp->closed = (_closed); \
        __tmp->node = (_node); \
        __tmp->id = node_id++; \
        DL_APPEND(*visualizer, __tmp); \
    }

#define PATHFINDING_SET_CLOSED(m, x, y, id, visualizer) \
    { \
        PATHFINDING_CHECK_ID(m, id); \
        PATHFINDING_VISUALIZER_APPEND(visualizer, m, x, y, true, NULL); \
        (m)->bitmap[(x) / 32 + ((MAP_WIDTH(m) + 31) / 32) * (y)] |= \
                (1U << ((x) % 32)); \
    }

#define PATHFINDING_QUERY_CLOSED(m, x, y, id) \
    ((m)->pathfinding_id == (id) && ((m)->bitmap[(x) / 32 + \
            ((MAP_WIDTH(m) + 31) / 32) * (y)] & (1U << ((x) % 32))))

#define PATHFINDING_NODE_SET(m, x, y, id, node, visualizer) \
    { \
        PATHFINDING_CHECK_ID(m, id); \
        PATHFINDING_VISUALIZER_APPEND(visualizer, m, x, y, false, node); \
        (m)->path_nodes[(x) + MAP_WIDTH(m) * (y)] = node; \
    }

#define PATHFINDING_NODE_GET(m, x, y, id) \
    ((m)->pathfinding_id != (id) ? NULL : (m)->path_nodes[(x) + \
            MAP_WIDTH(m) * (y)])

/**
 * Pseudo-flag used to mark waypoints as "has requested path".
 *
 * Reuses a non-saved flag.
 */
#define FLAG_WP_PATH_REQUESTED FLAG_PARALYZED

/* Uncomment this to enable some verbose pathfinding debug messages */
/* #define DEBUG_PATHFINDING */

/**
 * Enable more intelligent use of CPU time for path finding?
 */
#define LEFTOVER_CPU_FOR_PATHFINDING

#endif

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
 * Pathfinding header file */

#ifndef PATHFINDER_H
#define PATHFINDER_H

/** Path node */
typedef struct astar_node
{
    /** Next node in linked list */
    struct astar_node *next;

    /** Previous node in linked list */
    struct astar_node *prev;

    /** Node this was reached from */
    struct astar_node *parent;

    /** Pointer to the map */
    struct mapdef *map;

    /** X-Position in the map for this node */
    sint16 x;

    /** Y-Position in the map for this object */
    sint16 y;

    /** Cost of reaching this node (distance from origin) */
    uint16 cost;

    /** Estimated cost of reaching the goal from this node */
    float heuristic;
} path_node;

/**
 * Pseudo-flag used to mark waypoints as "has requested path".
 *
 * Reuses a non-saved flag. */
#define FLAG_WP_PATH_REQUESTED FLAG_PARALYZED

/* Uncomment this to enable some verbose pathfinding debug messages */
/* #define DEBUG_PATHFINDING */

/** Enable more intelligent use of CPU time for path finding? */
#define LEFTOVER_CPU_FOR_PATHFINDING

#endif

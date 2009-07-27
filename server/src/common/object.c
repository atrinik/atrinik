/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

#include <global.h>
#ifndef WIN32 /* ---win32 exclude headers */
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#endif /* win32 */

#include <funcpoint.h>
#include <skillist.h>
#include <loader.h>

#ifdef MEMPOOL_OBJECT_TRACKING
/* for debugging only! */
static struct mempool_chunk *used_object_list = NULL;
#define MEMPOOL_OBJECT_FLAG_FREE 1
#define MEMPOOL_OBJECT_FLAG_USED 2
#endif

/* List of active objects that need to be processed */
object *active_objects;
/* List of objects that have been removed
 * during the last server timestep */
struct mempool_chunk *removed_objects;
/* The removedlist is not ended by NULL, but by a pointer to the end_marker */
/* only used as an end marker for the lists */
static struct mempool_chunk end_marker;
/* see walk_off/walk_on functions  */
static int static_walk_semaphore=FALSE;

/* container for objects without real maps or envs */
object void_container;

/* The Life Cycle of an Object:
 *
 - expand_mempool(): Allocated from system memory and put into the freelist of the object pool.
 - get_object():     Removed from freelist & put into removedlist (since it is not inserted anywhere yet).
 - insert_ob_in_(map/ob)(): Filled with data & inserted into (any) environment
 ... end of timestep
 - object_gc():      Removed from removedlist, but not freed (since it sits in an env).
 ...
 - remove_ob():      Removed from environment
 - Sits in removedlist until the end of this server timestep
 ... end of timestep
 - object_gc():      Freed and moved to freelist
 (attrsets are freed and given back to their respective pools too). */

/* This Table is sorted in 64er blocks of the same base material, defined in
 * materialtype. Entrys, used for random selections should start from down of
 * a table section. Unique materials should start from above the 64 block down.
 * The M_RANDOM_xx value will always counted from down. */

/* this IS extrem ugly - i will move it ASAP to a data file, which can be used
 * from editor too! */
material_real_struct material_real[NROFMATERIALS * NROFMATERIALS_REAL + 1] = {
    /* undefined Material - for stuff we don't need material information about */
    {"", 100,100,       0,0,0,      M_NONE,         RACE_TYPE_NONE},

        /* PAPERS */
    {"paper ",       90,    80,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    81,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    82,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    83,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    84,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    85,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    86,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    87,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    88,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"paper ",       90,    89,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   90,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   91,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   92,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   93,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   94,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},
    {"parchment ",   100,   95,       0,0,0,      M_PAPER,         RACE_TYPE_NONE},

    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* IRON (= Metal) */
    {"iron ",                100,    80,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"hardened iron ",       95,     81,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"forged iron ",         90,     82,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"black iron ",          90,     83,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"shear iron ",          90,     84,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"steel ",               90,     85,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"hardened steel ",      85,     86,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"forged steel ",        85,     87,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"shear steel ",         85,     88,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"diamant steel ",       85,     89,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"darksteel ",           70,     90,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"forged darksteel ",    60,     91,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"silksteel ",           50,     92,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"forged silksteel ",    40,     93,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"meteoric steel ",      40,     94,   0,0,0,          M_IRON,         RACE_TYPE_NONE},
    {"forged meteoric steel ",40,     95,   0,0,0,          M_IRON,         RACE_TYPE_NONE},


    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* Crystals/breakable/glass */
    {"glass ",       100,80,       0,0,0,	M_GLASS, RACE_TYPE_NONE}, /* 129 */
    {"zircon ",		100,80,       0,0,0,	M_GLASS, RACE_TYPE_NONE}, /* 130 */
    {"pearl ",       75,83,       0,0,0,    M_GLASS, RACE_TYPE_NONE}, /* 131 */
    {"emerald ",     75,85,       0,0,0,    M_GLASS, RACE_TYPE_NONE}, /* 132 */
    {"sapphire ",    50,92,       0,0,0,    M_GLASS, RACE_TYPE_NONE}, /* 133 */
    {"ruby ",        30,93,       0,0,0,    M_GLASS, RACE_TYPE_NONE}, /* 134 */
    {"diamond ",     10,95,       0,0,0,    M_GLASS, RACE_TYPE_NONE}, /* 135 */
    {"jasper ", 75,80,       0,0,0,      M_GLASS,         RACE_TYPE_NONE}, /* 136 */
    {"jade ", 75,80,       0,0,0,      M_GLASS,         RACE_TYPE_NONE}, /* 137 */
    {"aquamarine ", 80,80,       0,0,0,      M_GLASS,         RACE_TYPE_NONE}, /* 138 */
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* LEATHER */
    {"soft leather ",				100,	80,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"hardened leather ",			70,		81,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"stone leather ",				70,		82,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"dark leather ",				35,     83,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"silk leather ",				30,		84,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"wyvern leather ",				50,		85,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"ankheg leather ",				50,		86,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"griffon leather ",			40,     87,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"hellcat leather ",			40,		88,		  0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"gargoyle leather ",			35,		89,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"diamant leather ",			35,		90,       0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"chimera leather ",			30,		91,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"manticore leather ",			25,		92,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"cockatrice leather ",			20,		93,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"basilisk leather ",			20,		94,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},
    {"dragon leather ",				20,		95,        0,0,0,      M_LEATHER,         RACE_TYPE_NONE},

    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* WOOD */
    {"pine ",        100,80,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,81,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,82,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,83,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,84,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,85,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,86,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,87,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,88,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"pine ",        100,89,       0,0,0,         M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,90,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,91,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,92,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,93,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,94,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"oak ",             80,95,       0,0,0,          M_WOOD,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* ORGANIC */
    {"animal ",       100,80,       0,0,0,      M_ORGANIC,       RACE_TYPE_NONE}, /* 321 used for misc organics */
    {"dragon ",      50,96,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE}, /* 322 */
    {"chitin "				, 50,82,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE}, /* 323 */
    {"scale "				, 50,80,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 324 */
    {"white dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 325 */
    {"blue dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 326 */
    {"red dragonscale "		, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 327 */
    {"yellow dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 328 */
    {"grey dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 329 */
    {"black dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 330 */
    {"orange dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 331 */
    {"green dragonscale "	, 10,100,       0,0,0,      M_ORGANIC,         RACE_TYPE_NONE},/* 332 */
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* STONE */
    {"flint ",       100,80,       0,0,0,      M_STONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* CLOTH */
    {"wool ",       100,80,       0,0,0,      M_CLOTH,         RACE_TYPE_NONE},
    {"linen ",      90,80,        0,0,0,      M_CLOTH,         RACE_TYPE_NONE},
    {"silk ",       25,95,        0,0,0,      M_CLOTH,         RACE_TYPE_NONE},
    {"elven hair ",  25,95,        0,0,0,      M_CLOTH,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* ADAMANT (= magic metals) */
    {"magic silk ",  1,99,       0,0,0,      M_ADAMANT,         RACE_TYPE_NONE},
    {"mithril ",     1,99,       0,0,0,      M_ADAMANT,         RACE_TYPE_NONE},
    {"adamant ",     10,99,      0,0,0,      M_ADAMANT,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* Liquid */
	/* some like ice... the kind of liquid/potions in game don't depend
	 * or even handle the liquid base type */
    {"",       100,80,       0,0,0,      M_LIQUID,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* Soft Metal */
    {"tin ",         100,80,       0,0,0,	M_SOFT_METAL,	RACE_TYPE_NONE}, /* 641 */
    {"brass ",		 100,80,       0,0,0,	M_SOFT_METAL,   RACE_TYPE_NONE}, /* 642 */
    {"copper ",      100,80,       0,0,0,   M_SOFT_METAL,   RACE_TYPE_NONE}, /* 643 */
    {"bronze ",      100,80,       0,0,0,   M_SOFT_METAL,   RACE_TYPE_NONE}, /* 644 */
    {"silver ",      50, 90,       0,0,0,   M_SOFT_METAL,   RACE_TYPE_NONE}, /* 645 */
    {"gold ",        20, 95,       0,0,0,   M_SOFT_METAL,   RACE_TYPE_NONE}, /* 646 */
    {"platinum ",    10, 99,       0,0,0,   M_SOFT_METAL,   RACE_TYPE_NONE}, /* 647 */
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* Bone */
    {"",        100,80,       0,0,0,      M_BONE,         RACE_TYPE_NONE}, /* for misc bones*/
    {"human ",			100,80,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"elven ",			100,80,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"dwarven ",		100,80,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    /* Ice */
	/* not sure about the sense to put here different elements in...*/
    {"",         100,80,       0,0,0,      M_ICE,         RACE_TYPE_NONE}, /* water */
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
    {"", 0,0,       0,0,0,      M_NONE,         RACE_TYPE_NONE},
};

materialtype material[NROFMATERIALS] = {
  /*  		  P  M  F  E  C  C  A  D  W  G  P S P T F  C D D C C G H B  I *
   *		  H  A  I  L  O  O  C  R  E  H  O L A U E  A E E H O O O L  N *
   *		  Y  G  R  E  L  N  I  A  A  O  I O R R A  N P A A U D L I  T *
   *		  S  I  E  C  D  F  D  I  P  S  S W A N R  C L T O N   Y N  R *
   *		  I  C     T     U     N  O  T  O   L      E E H S T P   D  N */
  {"paper", 	{15,10,17, 9, 5, 7,13, 0,20,15, 0,0,0,0,0,10,0,0,0,0,0,0,0,0}},
  {"metal", 	{ 2,12, 3,12, 2,10, 7, 0,20,15, 0,0,0,0,0,10,0,0,0,0,0,0,0,0}},
  {"crystal",   {14,11, 8, 3,10, 5, 1, 0,20,15, 0,0,0,0,0, 0,0,0,0,0,0,0,0,0}},
  {"leather", 	{ 5,10,10, 3, 3,10,10, 0,20,15, 0,0,0,0,0,12,0,0,0,0,0,0,0,0}},
  {"wood", 	    {10,11,13, 2, 2,10, 9, 0,20,15, 0,0,0,0,0,12,0,0,0,0,0,0,0,0}},
  {"organics", 	{ 3,12, 9,11, 3,10, 9, 0,20,15, 0,0,0,0,0, 0,0,0,0,0,0,0,0,0}},
  {"stone", 	{ 2, 5, 2, 2, 2, 2, 1, 0,20,15, 0,0,0,0,0, 5,0,0,0,0,0,0,0,0}},
  {"cloth", 	{14,11,13, 4, 4, 5,10, 0,20,15, 0,0,0,0,0, 5,0,0,0,0,0,0,0,0}},
  {"magic material", 	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0,0, 0,0,0,0,0,0,0,0,0}},
  {"liquid", 	{ 0, 8, 9, 6,17, 0,15, 0,20,15,12,0,0,0,0,11,0,0,0,0,0,0,0,0}},
  {"soft metal",{ 6,12, 6,14, 2,10, 1, 0,20,15, 0,0,0,0,0,10,0,0,0,0,0,0,0,0}},
  {"bone", 	    {10, 9, 4, 5, 3,10,10, 0,20,15, 0,0,0,0,0, 2,0,0,0,0,0,0,0,0}},
  {"ice", 	    {14,11,16, 5, 0, 5, 6, 0,20,15, 0,0,0,0,0, 7,0,0,0,0,0,0,0,0}}
};

int freearr_x[SIZEOFFREE] = {
	0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
   	0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1};

int freearr_y[SIZEOFFREE] = {
	0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
   	-3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3};

int maxfree[SIZEOFFREE] = {
	0, 9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 27, 30, 31, 32, 33, 36, 37, 39, 39, 42, 43, 44, 45,
  	48, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49};

int freedir[SIZEOFFREE]= {
  	0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 2, 2, 3, 4, 4, 4, 5, 6, 6, 6, 7, 8, 8, 8,
  	1, 2, 2, 2, 2, 2, 3, 4, 4, 4, 4, 4, 5, 6, 6, 6, 6, 6, 7, 8, 8, 8, 8, 8};

/** Basic pooling memory management system **/
/* TODO: move out into mempool.c & mempool.h files.
 * TODO: increase efficiency by replacing nrof_used with nrof_allocated
 * TODO: poolify objlinks */

/* A pool definition in the mempools[] array and an entry in the mempool id enum
 * is needed for each type of struct we want to use the pooling memory management for. */

struct mempool mempools[NROF_MEMPOOLS] = {
    #ifdef MEMPOOL_TRACKING
    {NULL, 10, sizeof(struct puddle_info), 0, 0, NULL, NULL, "puddles", MEMPOOL_ALLOW_FREEING, NULL},
    #endif

    {NULL, OBJECT_EXPAND, sizeof(object), 0, 0, (chunk_constructor)initialize_object, (chunk_destructor)destroy_object, "objects", 0, NULL},

    {NULL, 5, sizeof(player), 0, 0, NULL, NULL, "players", MEMPOOL_BYPASS_POOLS, NULL},
    /* Actually, we will later set up the destructor to point towards free_player() */
};

/* The mempool system never frees memory back to the system, but is extremely efficient
 * when it comes to allocating and returning pool chunks. Always use the get_poolchunk()
 * and return_poolchunk() functions for getting and returning memory chunks. expand_mempool() is
 * used internally.
 *
 * Be careful if you want to use the internal chunk or pool data, its semantics and
 * format might change in the future. */

/* Initialize the mempools lists and related data structures */
void init_mempools()
{
    int i;

    /* Initialize end-of-list pointers and a few other values*/
    for (i = 0; i < NROF_MEMPOOLS; i++)
	{
        mempools[i].first_free = &end_marker;
        mempools[i].nrof_used = 0;
        mempools[i].nrof_free = 0;
#ifdef MEMPOOL_TRACKING
        mempools[i].first_puddle_info = NULL;
#endif
    }
    removed_objects = &end_marker;

    /* Set up container for "loose" objects */
    initialize_object(&void_container);
	void_container.type = TYPE_VOID_CONTAINER;
}

/* A tiny little function to set up the constructors/destructors to functions that may
 * reside outside the library */
void setup_poolfunctions(mempool_id pool, chunk_constructor constructor, chunk_destructor destructor)
{
    if (pool >= NROF_MEMPOOLS)
        LOG(llevBug, "BUG: setup_poolfunctions for illegal memory pool %d\n", pool);

    mempools[pool].constructor = constructor;
    mempools[pool].destructor = destructor;
}

/* Expands the memory pool with MEMPOOL_EXPAND new chunks. All new chunks
 * are put into the pool's freelist for future use.
 * expand_mempool is only meant to be used from get_poolchunk(). */
static void expand_mempool(mempool_id pool)
{
    uint32 i;
	struct mempool_chunk *first, *ptr;
    int chunksize_real;
#ifdef MEMPOOL_OBJECT_TRACKING
	static uint32 real_id = 1;
#endif
#ifdef MEMPOOL_TRACKING
	struct puddle_info *p;
#endif

    if (pool >= NROF_MEMPOOLS)
        LOG(llevBug, "BUG: expand_mempool for illegal memory pool %d\n", pool);

#if 0
    if (mempools[pool].nrof_free > 0)
        LOG(llevBug, "BUG: expand_mempool called with chunks still available in pool\n");
#endif

    chunksize_real = sizeof(struct mempool_chunk) + mempools[pool].chunksize;
    first = (struct mempool_chunk *)calloc(mempools[pool].expand_size,chunksize_real);

    if (first == NULL)
        LOG(llevError, "ERROR: expand_mempool(): Out Of Memory.\n");

    mempools[pool].first_free = first;
    mempools[pool].nrof_free += mempools[pool].expand_size;

    /* Set up the linked list */
    ptr = first;
    for (i = 0; i < mempools[pool].expand_size - 1; i++)
	{
        ptr = ptr->next = (struct mempool_chunk *)(((char *)ptr) + chunksize_real);
		/* and the last element */
#ifdef MEMPOOL_OBJECT_TRACKING
		/* secure */
		ptr->obj_next = ptr->obj_prev = 0;
		ptr->pool_id = pool;
		/* this is a real, unique object id  allows tracking beyond get/free objects */
		ptr->id = real_id++;
		ptr->flags |= MEMPOOL_OBJECT_FLAG_FREE;
#endif
	}

    ptr->next = &end_marker;

#ifdef MEMPOOL_TRACKING
    /* Track the allocation of puddles? */
	p = get_poolchunk(POOL_PUDDLE);
    p->first_chunk = first;
    p->next = mempools[pool].first_puddle_info;
    mempools[pool].first_puddle_info = p;
#endif
}

/* Get a chunk from the selected pool. The pool will be expanded if necessary. */
void *get_poolchunk(mempool_id pool)
{
    struct mempool_chunk *new_obj;

    if (pool >= NROF_MEMPOOLS)
        LOG(llevBug, "BUG: get_poolchunk for illegal memory pool %d\n", pool);

    if (mempools[pool].flags & MEMPOOL_BYPASS_POOLS)
        new_obj = calloc(1, sizeof(struct mempool_chunk) + mempools[pool].chunksize);
    else
	{
        if (mempools[pool].nrof_free == 0)
            expand_mempool(pool);

        new_obj = mempools[pool].first_free;
        mempools[pool].first_free = new_obj->next;
        mempools[pool].nrof_free--;
    }

    mempools[pool].nrof_used++;
    new_obj->next = NULL;

    if (mempools[pool].constructor)
        mempools[pool].constructor(MEM_USERDATA(new_obj));

#ifdef MEMPOOL_OBJECT_TRACKING
	/* that should never happen! */
	if (new_obj->obj_prev || new_obj->obj_next)
		LOG(llevDebug, "WARNING: DEBUG_OBJ::get_poolchunk() object >%d< is in used_object list!!\n", new_obj->id);

	/* put it in front of the used object list */
	new_obj->obj_next = used_object_list;
	if (new_obj->obj_next)
		new_obj->obj_next->obj_prev = new_obj;

	used_object_list = new_obj;
	new_obj->flags &=~MEMPOOL_OBJECT_FLAG_FREE;
	new_obj->flags |=MEMPOOL_OBJECT_FLAG_USED;
#endif

    return MEM_USERDATA(new_obj);
}

/* Return a chunk to the selected pool. Don't return memory to the wrong pool!
 * Returned memory will be reused, so be careful about those stale pointers */
void return_poolchunk(void *data, mempool_id pool)
{
    struct mempool_chunk *old = MEM_POOLDATA(data);

    if (CHUNK_FREE(data))
        LOG(llevBug, "BUG: return_poolchunk on already free chunk\n");

    if(pool >= NROF_MEMPOOLS)
        LOG(llevBug, "BUG: return_poolchunk for illegal memory pool %d\n", pool);

#ifdef MEMPOOL_OBJECT_TRACKING
	if (old->obj_next)
		old->obj_next->obj_prev = old->obj_prev;

	if (old->obj_prev)
		old->obj_prev->obj_next = old->obj_next;
	else
		used_object_list = old->obj_next;

	/* secure */
	old->obj_next = old->obj_prev = 0;
	old->flags &= ~MEMPOOL_OBJECT_FLAG_USED;
	old->flags |= MEMPOOL_OBJECT_FLAG_FREE;
#endif

    if (mempools[pool].destructor)
        mempools[pool].destructor(data);

    if (mempools[pool].flags & MEMPOOL_BYPASS_POOLS)
        free(old);
    else
	{
        old->next = mempools[pool].first_free;
        mempools[pool].first_free = old;
        mempools[pool].nrof_free++;
    }

    mempools[pool].nrof_used--;
}

#ifdef MEMPOOL_OBJECT_TRACKING
/* this is time consuming DEBUG only
 * function. Mainly, it checks the different memory parts
 * and controls they are was they are - if a object claims its
 * in a inventory we check the inventory - same for map.
 * If we have detached but not deleted a object - we will find it here. */
void check_use_object_list(void)
{
    struct mempool_chunk *chunk;

	for (chunk = used_object_list; chunk; chunk = chunk->obj_next)
	{
#ifdef MEMPOOL_TRACKING
		/* ignore for now */
		if (chunk->pool_id == POOL_PUDDLE)
		{
		}
		else
#endif
		if (chunk->pool_id == POOL_OBJECT)
		{
			object *tmp2 ,*tmp = MEM_USERDATA(chunk);

			/*LOG(llevDebug, "DEBUG_OBJ:: object >%s< (%d)\n",  query_name(tmp), chunk->id);*/

			if (QUERY_FLAG(tmp, FLAG_REMOVED))
				LOG(llevDebug, "VOID:DEBUG_OBJ:: object >%s< (%d) has removed flag set!\n", query_name(tmp), chunk->id);

			/* we are on a map */
			if (tmp->map)
			{
				if (tmp->map->in_memory != MAP_IN_MEMORY)
					LOG(llevDebug, "BUG:DEBUG_OBJ:: object >%s< (%d) has invalid map! >%d<!\n", query_name(tmp), tmp->map->name ? tmp->map->name : "NONE", chunk->id);
				else
				{
					for (tmp2= get_map_ob(tmp->map, tmp->x, tmp->y); tmp2; tmp2 = tmp2->above)
					{
						if (tmp2 == tmp)
							goto goto_object_found;
					}

					LOG(llevDebug, "BUG:DEBUG_OBJ:: object >%s< (%d) has invalid map! >%d<!\n", query_name(tmp), tmp->map->name ? tmp->map->name : "NONE", chunk->id);
				}
			}
			else if (tmp->env)
			{
				/* object claims to be here... lets check it IS here */
				for (tmp2 = tmp->env->inv; tmp2; tmp2 = tmp2->below)
				{
					if (tmp2 == tmp)
						goto goto_object_found;
				}

				LOG(llevDebug, "BUG:DEBUG_OBJ:: object >%s< (%d) has invalid env >%d<!\n", query_name(tmp), query_name(tmp->env), chunk->id);
			}
			/* where we are ? */
			else
				LOG(llevDebug, "BUG:DEBUG_OBJ:: object >%s< (%d) has no env/map\n", query_name(tmp), chunk->id);
		}
		else if (chunk->pool_id == POOL_PLAYER)
		{
			player *tmp = MEM_USERDATA(chunk);
			/*LOG(llevDebug, "DEBUG_OBJ:: player >%s< (%d)\n", tmp->ob ? query_name(tmp->ob) : "NONE", chunk->id);*/
		}
		else
			LOG(llevDebug, "BUG:DEBUG_OBJ: wrong pool ID! (%d - %d)", chunk->pool_id, chunk->id);

		goto_object_found:;
	}
}
#endif

#ifdef MEMPOOL_TRACKING
/* A Linked-List Memory Sort
 * by Philip J. Erdelsky <pje@efgh.com>
 * http://www.alumni.caltech.edu/~pje/
 * (Public Domain)
 *
 * The function sort_linked_list() will sort virtually any kind of singly-linked list, using a comparison
 * function supplied by the calling program. It has several advantages over qsort().
 *
 * The function sorts only singly linked lists. If a list is doubly linked, the backward pointers can be
 * restored after the sort by a few lines of code.
 *
 * Each element of a linked list to be sorted must contain, as its first members, one or more pointers.
 * One of the pointers, which must be in the same relative position in each element, is a pointer to the
 * next element. This pointer is <end_marker> (usually NULL) in the last element.
 *
 * The index is the position of this pointer in each element.
 * It is 0 for the first pointer, 1 for the second pointer, etc.
 *
 * Let n = compare(p,q,pointer) be a comparison function that compares two elements p and q as follows:
 * void *pointer;  user-defined pointer passed to compare() by linked_list_sort()
 * int n;          result of comparing *p and *q
 *                      >0 if *p is to be after *q in sorted order
 *                      <0 if *p is to be before *q in sorted order
 *                       0 if the order of *p and *q is irrelevant
 *
 *
 * The fourth argument (pointer) is passed to compare() without change. It can be an invaluable feature if
 * two or more comparison methods share a substantial amount of code and differ only in one or more parameter
 * values.
 *
 * The last argument (pcount) is of type (unsigned long *).
 * If it is not NULL, then *pcount is set equal to the number of records in the list.
 *
 * It is permissible to sort an empty list. If first == end_marker, the returned value will also be end_marker.  */
void *sort_singly_linked_list(void *p, unsigned index, int (*compare)(void *, void *, void *), void *pointer, unsigned long *pcount, void *end_marker)
{
	unsigned base;
	unsigned long block_size;

	struct record
	{
		struct record *next[1];
		/* other members not directly accessed by this function */
	};

	struct tape
	{
		struct record *first, *last;
		unsigned long count;
	} tape[4];

	/* Distribute the records alternately to tape[0] and tape[1]. */
	tape[0].count = tape[1].count = 0L;
	tape[0].first = NULL;
	base = 0;
	while (p != end_marker)
	{
		struct record *next = ((struct record *)p)->next[index];
		((struct record *)p)->next[index] = tape[base].first;
		tape[base].first = ((struct record *)p);
		tape[base].count++;
		p = next;
		base ^= 1;
	}

	/* If the list is empty or contains only a single record, then
	 * tape[1].count == 0L and this part is vacuous. */
	for (base = 0, block_size = 1L; tape[base+1].count != 0L; base ^= 2, block_size <<= 1)
	{
		int dest;
		struct tape *tape0, *tape1;
		tape0 = tape + base;
		tape1 = tape + base + 1;
		dest = base ^ 2;
		tape[dest].count = tape[dest+1].count = 0;

		for (; tape0->count != 0; dest ^= 1)
		{
			unsigned long n0, n1;
			struct tape *output_tape = tape + dest;
			n0 = n1 = block_size;
			while (1)
			{
				struct record *chosen_record;
				struct tape *chosen_tape;

				if (n0 == 0 || tape0->count == 0)
				{
					if (n1 == 0 || tape1->count == 0)
						break;

					chosen_tape = tape1;
					n1--;
				}
				else if (n1 == 0 || tape1->count == 0)
				{
					chosen_tape = tape0;
					n0--;
				}
				else if ((*compare)(tape0->first, tape1->first, pointer) > 0)
				{
					chosen_tape = tape1;
					n1--;
				}
				else
				{
					chosen_tape = tape0;
					n0--;
				}

				chosen_tape->count--;
				chosen_record = chosen_tape->first;
				chosen_tape->first = chosen_record->next[index];

				if (output_tape->count == 0)
					output_tape->first = chosen_record;
				else
					output_tape->last->next[index] = chosen_record;

				output_tape->last = chosen_record;
				output_tape->count++;
			}
		}
	}

	if (tape[base].count > 1L)
		tape[base].last->next[index] = end_marker;

	if (pcount != NULL)
		*pcount = tape[base].count;

	return tape[base].first;
}

/* Comparision function for sort_singly_linked_list */
static int sort_puddle_by_nrof_free(void *a, void *b, void *args)
{
	(void) args;

    if (((struct puddle_info *)a)->nrof_free < ((struct puddle_info *)b)->nrof_free)
        return -1;
    else if (((struct puddle_info *)a)->nrof_free > ((struct puddle_info *)b)->nrof_free)
        return 1;
    else
        return 0;
}

/* Go through the freelists and free puddles with no used chunks.
 * This function is quite slow and dangerous to call.
 * The idea is that it should be called occasionally when CPU usage is low
 *
 * Complexity of this function is O(N (M log M)) where
 * N is number of pools and M is number of puddles in pool  */
void free_empty_puddles(mempool_id pool)
{
    int chunksize_real = sizeof(struct mempool_chunk) + mempools[pool].chunksize;
    int freed = 0;

    struct mempool_chunk *last_free, *chunk;
    struct puddle_info *puddle, *next_puddle;

    if (mempools[pool].flags & MEMPOOL_BYPASS_POOLS)
        return;

    /* Free empty puddles and setup puddle-local freelists */
    for (puddle = mempools[pool].first_puddle_info, mempools[pool].first_puddle_info = NULL; puddle != NULL; puddle = next_puddle)
	{
      	uint32 ii;
      	next_puddle = puddle->next;

      	/* Count free chunks in puddle, and set up a local freelist */
      	puddle->first_free = puddle->last_free = NULL;
      	puddle->nrof_free = 0;

      	for (ii = 0; ii < mempools[pool].expand_size; ii++)
		{
			chunk = (struct mempool_chunk *)((char *)(puddle->first_chunk) + chunksize_real * ii);
			/* Find free chunks. (Notice special case for objects here. Yuck!) */
			if ((pool == POOL_OBJECT && OBJECT_FREE((object *)MEM_USERDATA(chunk))) || (pool != POOL_OBJECT && CHUNK_FREE((object *)MEM_USERDATA(chunk))))
			{
				if (puddle->nrof_free == 0)
				{
					puddle->first_free = chunk;
					puddle->last_free = chunk;
					chunk->next = NULL;
				}
				else
				{
					chunk->next = puddle->first_free;
					puddle->first_free = chunk;
				}

				puddle->nrof_free ++;
			}
		}

      	/* Can we actually free this puddle? */
      	if (puddle->nrof_free == mempools[pool].expand_size)
		{
          	/* Yup. Forget about it. */
          	free(puddle->first_chunk);
          	return_poolchunk(puddle, POOL_PUDDLE);
          	mempools[pool].nrof_free -= mempools[pool].expand_size;
          	freed++;
      	}
		else
		{
          	/* Nope keep this puddle: put it back into the tracking list */
          	puddle->next = mempools[pool].first_puddle_info;
          	mempools[pool].first_puddle_info = puddle;
      	}
    }

    /* Sort the puddles by amount of free chunks. It will let us set up the freelist so that
     * the chunks from the fullest puddles are used first.
     * This should (hopefully) help us free some of the lesser-used puddles earlier. */
    mempools[pool].first_puddle_info = sort_singly_linked_list(mempools[pool].first_puddle_info, 0, sort_puddle_by_nrof_free, NULL, NULL, NULL);

    /* Finally: restore the global freelist */
    mempools[pool].first_free = &end_marker;
    last_free = &end_marker;
    LOG(llevDebug, "%s free in puddles: ", mempools[pool].chunk_description);

    for (puddle = mempools[pool].first_puddle_info; puddle != NULL; puddle = puddle->next)
	{
        if (puddle->nrof_free > 0)
		{
            if (mempools[pool].first_free == &end_marker)
                mempools[pool].first_free = puddle->first_free;
            else
                last_free->next = puddle->first_free;

            puddle->last_free->next = &end_marker;
            last_free = puddle->last_free;
        }

        LOG(llevDebug, "%d ", puddle->nrof_free);
    }

    LOG(llevDebug, "\n");

    LOG(llevInfo, "Freed %d %s puddles\n", freed, mempools[pool].chunk_description);
}
#endif

/** Object management functions **/

/* Put an object in the list of removal candidates.
 * If the object has still FLAG_REMOVED set at the end of the
 * server timestep it will be freed */
void mark_object_removed(object *ob)
{
    struct mempool_chunk *mem = MEM_POOLDATA(ob);

    if (OBJECT_FREE(ob))
        LOG(llevBug, "BUG: mark_object_removed() called for free object\n");

    SET_FLAG(ob, FLAG_REMOVED);

    /* Don't mark objects twice */
    if (mem->next != NULL)
        return;

    mem->next = removed_objects;
    removed_objects = mem;
}

/* Go through all objects in the removed list and free the forgotten ones */
void object_gc()
{
    struct mempool_chunk *current, *next;
    object *ob;

    while ((next = removed_objects) != &end_marker)
	{
		/* destroy_object() may free some more objects (inventory items) */
        removed_objects = &end_marker;
        while (next != &end_marker)
		{
            current = next;
            next = current->next;
            current->next = NULL;

            ob = (object *)MEM_USERDATA(current);

            if (QUERY_FLAG(ob, FLAG_REMOVED))
			{
                if (OBJECT_FREE(ob))
                    LOG(llevBug, "BUG: Freed object in remove list: %s\n", STRING_OBJ_NAME(ob));
                else
                    return_poolchunk(ob, POOL_OBJECT);
            }
        }
    }
}

/* Moved this out of define.h and in here, since this is the only file
 * it is used in.  Also, make it an inline function for cleaner
 * design.
 *
 * Function examines the 2 objects given to it, and returns true if
 * they can be merged together.
 *
 * Note that this function appears a lot longer than the macro it
 * replaces - this is mostly for clarity - a decent compiler should hopefully
 * reduce this to the same efficiency.
 *
 * Check nrof variable *before* calling CAN_MERGE()
 *
 * Improvements made with merge:  Better checking on potion, and also
 * check weight */
int CAN_MERGE(object *ob1, object *ob2)
{
	/* just some quick hack */
	if (ob1->type == MONEY && ob1->type == ob2->type && ob1->arch == ob2->arch)
		return 1;

    /* Gecko: Moved out special handling of event obejct nrof */
	/* important: don't merge objects with glow_radius set - or we come
	 * in heavy side effect situations. Because we really not know what
	 * our calling function will do after this merge (and the calling function
	 * then must first find out a merge has happen or not). The sense of stacks
	 * are to store inactive items. Because glow_radius items can be active even
	 * when not apllied, merging is simply wrong here. MT. */
    if (((!ob1->nrof || !ob2->nrof) && ob1->type != TYPE_EVENT_OBJECT) || ob1->glow_radius || ob2->glow_radius)
        return 0;

	/* just a brain dead long check for things NEVER NEVER should be different
	 * this is true under all circumstances for all objects. */
    if (ob1->type != ob2->type || ob1 == ob2 || ob1->arch != ob2->arch || ob1->sub_type1 != ob2->sub_type1 || ob1->material!=ob2->material || ob1->material_real != ob2->material_real || ob1->magic != ob2->magic || ob1->item_quality!=ob2->item_quality ||ob1->item_condition!=ob2->item_condition || ob1->item_race!=ob2->item_race || ob1->speed != ob2->speed || ob1->value !=ob2->value || ob1->weight != ob2->weight)
        return 0;

    /* Gecko: added bad special check for event objects
     * Idea is: if inv is identical events only then go ahead and merge)
     * This goes hand in hand with the event keeping addition in get_split_ob() */
    if (ob1->inv || ob2->inv)
	{
        object *tmp1, *tmp2;

        if (!ob1->inv || !ob2->inv)
            return 0;

        /* Check that all inv objects are event objects */
        for (tmp1 = ob1->inv, tmp2 = ob2->inv; tmp1 && tmp2; tmp1 = tmp1->below, tmp2 = tmp2->below)
            if (tmp1->type != TYPE_EVENT_OBJECT || tmp2->type != TYPE_EVENT_OBJECT)
                return 0;

		/* Same number of events */
        if (tmp1 || tmp2)
            return 0;

        for (tmp1 = ob1->inv; tmp1; tmp1 = tmp1->below)
		{
            for (tmp2 = ob2->inv; tmp2; tmp2 = tmp2->below)
                if (CAN_MERGE(tmp1, tmp2))
                    break;

			/* Couldn't find something to merge event from ob1 with? */
            if (!tmp2)
                return 0;
        }
    }

	/* check the refcount pointer */
	if (ob1->name != ob2->name || ob1->title != ob2->title || ob1->race != ob2->race || ob1->slaying != ob2->slaying || ob1->msg != ob2->msg)
		return 0;

	/* compare the static arrays/structs */
	if ((memcmp(&ob1->stats, &ob2->stats, sizeof(living)) != 0) || (memcmp(&ob1->resist, &ob2->resist, sizeof(ob1->resist)) != 0) || (memcmp(&ob1->attack, &ob2->attack, sizeof(ob1->attack)) != 0) || (memcmp(&ob1->protection, &ob2->protection, sizeof(ob1->protection)) != 0))
		return 0;

	/* we ignore REMOVED and BEEN_APPLIED */
	if (ob1->randomitems != ob2->randomitems || ob1->other_arch != ob2->other_arch || (ob1->flags[0] | 0x300) != (ob2->flags[0] | 0x300) || ob1->flags[1] != ob2->flags[1] || ob1->flags[2] != ob2->flags[2] || ob1->flags[3] != ob2->flags[3] || ob1->path_attuned != ob2->path_attuned || ob1->path_repelled != ob2->path_repelled || ob1->path_denied != ob2->path_denied || ob1->terrain_type != ob2->terrain_type || ob1->terrain_flag != ob2->terrain_flag || ob1->weapon_speed != ob2->weapon_speed || ob1->magic != ob2->magic || ob1->item_level != ob2->item_level || ob1->item_skill != ob2->item_skill || ob1->glow_radius != ob2->glow_radius  || ob1->level != ob2->level)
		return 0;

	/* face can be difficult - but inv_face should never different or obj is different! */
	if (ob1->face != ob2->face || ob1->inv_face != ob2->inv_face || ob1->animation_id != ob2->animation_id || ob1->inv_animation_id != ob2->inv_animation_id)
		return 0;

	/* some stuff we should not need to test:
	 * carrying: because container merge isa big nono - and we tested ->inv before. better no double use here.
     * weight_limit: same reason like carrying - add when we double use for stacking items
	 * last_heal;
	 * last_sp;
	 * last_grace;
	 * sint16 last_eat;
	 * will_apply;
	 * run_away;
	 * stealth;
	 * hide;
     * move_type;
	 * layer;				this *can* be different for real same item - watch it
     * anim_speed;			this can be interesting... */

	/* can merge! */
	return 1;
}

/* merge_ob(op,top):
 *
 * This function goes through all objects below and including top, and
 * merges op to the first matching object.
 * If top is NULL, it is calculated.
 * Returns pointer to object if it succeded in the merge, otherwise NULL */
object *merge_ob(object *op, object *top)
{
	if (!op->nrof)
		return 0;

	if (top == NULL)
		for (top = op; top != NULL && top->above != NULL; top = top->above);

	for (; top != NULL; top = top->below)
	{
		if (top == op)
			continue;

		if (CAN_MERGE(op,top))
		{
			top->nrof += op->nrof;
			/* Don't want any adjustements now */
			op->weight = 0;
			/* this is right: no check off */
			remove_ob(op);
			return top;
		}
	}

	return NULL;
}

/* sum_weight() is a recursive function which calculates the weight
 * an object is carrying.  It goes through in figures out how much
 * containers are carrying, and sums it up. */
signed long sum_weight(object *op)
{
	sint32 sum;
	object *inv;

	for (sum = 0, inv = op->inv; inv != NULL; inv = inv->below)
	{
		if (inv->inv)
			sum_weight(inv);

		sum += inv->carrying + (inv->nrof ? inv->weight * (int) inv->nrof : inv->weight);
	}

	/* because we avoid calculating for EVERY item in the loop above
	 * the weight adjustment for magic containers, we can run here in some
	 * rounding problems... in the worst case, we can remove a item from the
	 * container but we are not able to put it back because rounding.
	 * well, a small prize for saving *alot* of muls in player houses for example. */
	if (op->type == CONTAINER && op->weapon_speed != 1.0f)
		sum = (sint32) ((float)sum * op->weapon_speed);

	op->carrying = sum;

	return sum;
}

/* add_weight(object, weight) adds the specified weight to an object,
 * and also updates how much the environment(s) is/are carrying. */
void add_weight (object *op, sint32 weight)
{
	while (op != NULL)
	{
		/* only *one* time magic can effect the weight of objects */
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
			weight = (sint32) ((float)weight * op->weapon_speed);

	    op->carrying += weight;
		op = op->env;
	}
}

/* sub_weight() recursively (outwards) subtracts a number from the
 * weight of an object (and what is carried by it's environment(s)). */
void sub_weight (object *op, sint32 weight)
{
	while (op != NULL)
	{
		/* only *one* time magic can effect the weight of objects */
		if (op->type == CONTAINER && op->weapon_speed != 1.0f)
			weight = (sint32) ((float)weight * op->weapon_speed);

		op->carrying -= weight;
		op = op->env;
  	}
}

/* Eneq(@csd.uu.se): Since we can have items buried in a character we need
 * a better check.  We basically keeping traversing up until we can't
 * or find a player. */

/* this function was wrong used in the past. Its only senseful for fix_player() - for
 * example we remove a active force from a player which was inserted in a special
 * force container (for example a exp object). For inventory views, we DON'T need
 * to update the item then! the player only sees his main inventory and *one* container.
 * is this object in a closed container, the player will never notice any change. */
object *is_player_inv(object *op)
{
    for (; op != NULL && op->type != PLAYER; op = op->env)
      	if (op->env == op)
			op->env = NULL;

    return op;
}

/* Used by: Server DM commands: dumpbelow, dump.
 *	Some error messages.
 * The result of the dump is stored in the static global errmsg array. */
void dump_object2(object *op)
{
  	char *cp;

	if (op->arch != NULL)
	{
		strcat(errmsg, "arch ");
		strcat(errmsg, op->arch->name ? op->arch->name : "(null)");
		strcat(errmsg, "\n");

		if ((cp = get_ob_diff(op, &empty_archetype->clone)) != NULL)
			strcat(errmsg,cp);

		strcat(errmsg,"end\n");
	}
	else
	{
		strcat(errmsg, "Object ");

		if (op->name == NULL)
			strcat(errmsg, "(null)");
		else
			strcat(errmsg, op->name);

		strcat(errmsg, "\n");
		strcat(errmsg, "end\n");
	}
}

/* Dumps an object.  Returns output in the static global errmsg array. */
void dump_object(object *op)
{
  	if (op == NULL)
	{
    	strcpy(errmsg, "[NULL pointer]");
    	return;
  	}

  	errmsg[0] = '\0';
  	dump_object2(op);
}

/* GROS - Dumps an object. Return the result into a string
 * Note that no checking is done for the validity of the target string, so
 * you need to be sure that you allocated enough space for it. */
void dump_me(object *op, char *outstr)
{
    char *cp;

    if (op == NULL)
    {
        strcpy(outstr, "[NULL pointer]");
        return;
    }

    outstr[0] = '\0';

    if (op->arch != NULL)
    {
        strcat(outstr, "arch ");
        strcat(outstr, op->arch->name ? op->arch->name : "(null)");
        strcat(outstr, "\n");

        if ((cp = get_ob_diff(op, &empty_archetype->clone)) != NULL)
            strcat(outstr, cp);

        strcat(outstr, "end\n");
    }
    else
    {
        strcat(outstr, "Object ");

        if (op->name == NULL)
            strcat(outstr, "(null)");
        else
            strcat(outstr, op->name);

        strcat(outstr, "\n");
        strcat(outstr, "end\n");
    }
}

/* Gecko: we could at least search through the active, friend and player lists here... */

/* Returns the object which has the count-variable equal to the argument. */
object *find_object(int i)
{
	(void) i;

  	return NULL;

#if 0
  	object *op;

  	for (op = objects; op != NULL; op = op->next)
    	if (op->count == (tag_t) i)
      		break;

 	return op;
#endif
}

/* Returns the first object which has a name equal to the argument.
 * Used only by the patch command, but not all that useful.
 * Enables features like "patch <name-of-other-player> food 999" */
object *find_object_name(char *str)
{
	(void) str;

    return NULL;

	/* if find_string() can't find the string -
	 * then its impossible that op->name will match.
	 * if we get a string - its the hash table
	 * ptr for this string. */

#if 0
  	const char *name = find_string(str);
  	object *op;

  	if (name == NULL)
	  	return NULL;

  	for (op = objects; op != NULL; op = op->next)
  	{
		if (op->name == name)
			break;
  	}

  	return op;
#endif
}

void free_all_object_data()
{
#ifdef MEMORY_DEBUG
    object *op, *next;

    for (op = free_objects; op != NULL; )
	{
		next = op->next;
		free(op);
		nrofallocobjects--;
		nroffreeobjects--;
		op = next;
    }
#endif

    LOG(llevDebug, "%d allocated objects, %d free objects\n", mempools[POOL_OBJECT].nrof_used, mempools[POOL_OBJECT].nrof_free);
}

/* Returns the object which this object marks as being the owner.
 * A id-scheme is used to avoid pointing to objects which have been
 * freed and are now reused.  If this is detected, the owner is
 * set to NULL, and NULL is returned.
 * (This scheme should be changed to a refcount scheme in the future) */
object *get_owner(object *op)
{
  	if (!op || op->owner == NULL)
    	return NULL;

  	if (!OBJECT_FREE(op) && op->owner->count == op->ownercount)
    	return op->owner;

  	op->owner = NULL, op->ownercount = 0;
  	return NULL;
}

void clear_owner(object *op)
{
    if (!op)
		return;

#if 0
    if (op->owner && op->ownercount == op->owner->count)
		op->owner->refcount--;
#endif

    op->owner = NULL;
    op->ownercount = 0;
}


/* Sets the owner of the first object to the second object.
 * Also checkpoints a backup id-scheme which detects freeing (and reusage)
 * of the owner object.
 * See also get_owner() */
static void set_owner_simple(object *op, object *owner)
{
    /* next line added to allow objects which own objects */
    /* Add a check for ownercounts in here, as I got into an endless loop
     * with the fireball owning a poison cloud which then owned the
     * fireball.  I believe that was caused by one of the objects getting
     * freed and then another object replacing it.  Since the ownercounts
     * didn't match, this check is valid and I believe that cause is valid. */
    while (owner->owner && owner != owner->owner && owner->ownercount == owner->owner->count)
		owner = owner->owner;

    /* IF the owner still has an owner, we did not resolve to a final owner.
     * so lets not add to that. */
    if (owner->owner)
		return;

    op->owner = owner;

    op->ownercount = owner->count;
    /*owner->refcount++;*/
}

static void set_skill_pointers(object *op, object *chosen_skill, object *exp_obj)
{
    op->chosen_skill = chosen_skill;
    op->exp_obj = exp_obj;

    /* unfortunately, we can't allow summoned monsters skill use
     * because we will need the chosen_skill field to pick the
     * right skill/stat modifiers for calc_skill_exp(). See
     * hit_player() in server/attack.c -b.t. */
    CLEAR_FLAG(op, FLAG_CAN_USE_SKILL);
    CLEAR_FLAG(op, FLAG_READY_SKILL);
}

/* Sets the owner and sets the skill and exp pointers to owner's current
 * skill and experience objects. */
void set_owner(object *op, object *owner)
{
    if (owner == NULL || op == NULL)
		return;

    set_owner_simple(op, owner);

    if (owner->type == PLAYER && owner->chosen_skill)
        set_skill_pointers(op, owner->chosen_skill, owner->chosen_skill->exp_obj);
    else if (op->type != PLAYER)
		CLEAR_FLAG(op, FLAG_READY_SKILL);
}

/* Set the owner to clone's current owner and set the skill and experience
 * objects to clone's objects (typically those objects that where the owner's
 * current skill and experience objects at the time when clone's owner was
 * set - not the owner's current skill and experience objects).
 *
 * Use this function if player created an object (e.g. fire bullet, swarm
 * spell), and this object creates further objects whose kills should be
 * accounted for the player's original skill, even if player has changed
 * skills meanwhile. */
void copy_owner(object *op, object *clone)
{
    object *owner = get_owner(clone);
    if (owner == NULL)
	{
		/* players don't have owners - they own themselves.  Update
	 	 * as appropriate. */
		if (clone->type == PLAYER)
			owner = clone;
		else
			return;
    }

    set_owner_simple(op, owner);

    if (clone->chosen_skill)
        set_skill_pointers(op, clone->chosen_skill, clone->exp_obj);
    else if (op->type != PLAYER)
		CLEAR_FLAG(op, FLAG_READY_SKILL);
}

/* initialize_object() frees everything allocated by an object, and also
 * initializes all variables and flags to default settings. */
void initialize_object(object *op)
{
    /* the memset will clear all these values for us, but we need
     * to reduce the refcount on them. */
    FREE_ONLY_HASH(op->name);
    FREE_ONLY_HASH(op->title);
    FREE_ONLY_HASH(op->race);
    FREE_ONLY_HASH(op->slaying);
    FREE_ONLY_HASH(op->msg);

	/* Using this memset is a lot easier (and probably faster)
     * than explicitly clearing the fields. */
    memset(op, 0, sizeof(object));

    /* Set some values that should not be 0 by default */
	/* control the facings 25 animations */
	op->anim_enemy_dir = -1;
	/* the same for movement */
	op->anim_moving_dir = -1;
	op->anim_enemy_dir_last = -1;
	op->anim_moving_dir_last = -1;
	op->anim_last_facing = 4;
	op->anim_last_facing_last = -1;

    op->face = blank_face;
    op->attacked_by_count= -1;

    /* give the object a new (unique) count tag */
    op->count= ++ob_count;
}

/* copy object first frees everything allocated by the second object,
 * and then copies the contends of the first object into the second
 * object, allocating what needs to be allocated. */
void copy_object(object *op2, object *op)
{
  	int is_removed = QUERY_FLAG(op, FLAG_REMOVED);

	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);

  	(void) memcpy((void *)((char *) op + offsetof(object, name)), (void *)((char *) op2 + offsetof(object, name)), sizeof(object) - offsetof(object, name));

    if (is_removed)
        SET_FLAG(op, FLAG_REMOVED);

	ADD_REF_NOT_NULL_HASH(op->name);
	ADD_REF_NOT_NULL_HASH(op->title);
	ADD_REF_NOT_NULL_HASH(op->race);
	ADD_REF_NOT_NULL_HASH(op->slaying);
	ADD_REF_NOT_NULL_HASH(op->msg);

 	if (QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		SET_FLAG(op, FLAG_KNOWN_MAGICAL);
		SET_FLAG(op, FLAG_KNOWN_CURSED);
	}

 	/* Only alter speed_left when we sure we have not done it before */
  	if (op->speed < 0 && op->speed_left == op->arch->clone.speed_left)
	  	op->speed_left += (RANDOM() % 90) / 100.0f;

  	update_ob_speed(op);
}

/* Same as above, but not touching the active list */
void copy_object_data(object *op2, object *op)
{
  	int is_removed = QUERY_FLAG(op, FLAG_REMOVED);

	FREE_ONLY_HASH(op->name);
	FREE_ONLY_HASH(op->title);
	FREE_ONLY_HASH(op->race);
	FREE_ONLY_HASH(op->slaying);
	FREE_ONLY_HASH(op->msg);

  	(void) memcpy((void *)((char *) op + offsetof(object, name)), (void *)((char *) op2 + offsetof(object, name)), sizeof(object) - offsetof(object, name));

    if (is_removed)
        SET_FLAG(op, FLAG_REMOVED);

	ADD_REF_NOT_NULL_HASH(op->name);
	ADD_REF_NOT_NULL_HASH(op->title);
	ADD_REF_NOT_NULL_HASH(op->race);
	ADD_REF_NOT_NULL_HASH(op->slaying);
	ADD_REF_NOT_NULL_HASH(op->msg);

	if (QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		SET_FLAG(op, FLAG_KNOWN_MAGICAL);
		SET_FLAG(op, FLAG_KNOWN_CURSED);
	}
}

/* get_object() grabs an object from the list of unused objects, makes
 * sure it is initialised, and returns it.
 * If there are no free objects, expand_objects() is called to get more. */
object *get_object()
{
    object *new_obj = (object *)get_poolchunk(POOL_OBJECT);

	mark_object_removed(new_obj);
    return new_obj;
}

/* If an object with the IS_TURNABLE() flag needs to be turned due
 * to the closest player being on the other side, this function can
 * be called to update the face variable, _and_ how it looks on the map. */
void update_turn_face(object *op)
{
    if (!QUERY_FLAG(op, FLAG_IS_TURNABLE) || op->arch == NULL)
		return;

	SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
    update_object(op, UP_OBJ_FACE);
}

/* Updates the speed of an object.  If the speed changes from 0 to another
 * value, or vice versa, then add/remove the object from the active list.
 * This function needs to be called whenever the speed of an object changes. */
void update_ob_speed(object *op)
{
    extern int arch_init;

    /* No reason putting the archetypes objects on the speed list,
     * since they never really need to be updated. */
    if (OBJECT_FREE(op) && op->speed)
	{
		dump_object(op);
		LOG(llevBug, "BUG: Object %s is freed but has speed.\n:%s\n", op->name, errmsg);
		op->speed = 0;
    }

    if (arch_init)
		return;

	/* these are special case objects - they have speed set, but should not put
	 * on the active list. */
	if (op->type == SPAWN_POINT_MOB)
		return;

    if (FABS(op->speed)>MIN_ACTIVE_SPEED)
	{
		/* If already on active list, don't do anything */
		if (op->active_next || op->active_prev || op == active_objects)
			return;

		/* process_events() expects us to insert the object at the beginning
		 * of the list. */
		/*LOG(-1, "SPEED: add object to speed list: %s (%d,%d)\n", query_name(op), op->x, op->y);*/
		op->active_next = active_objects;

		if (op->active_next != NULL)
			op->active_next->active_prev = op;

		active_objects = op;
		op->active_prev = NULL;
    }
    else
	{
		/* If not on the active list, nothing needs to be done */
		if (!op->active_next && !op->active_prev && op != active_objects)
			return;

		/*LOG(-1, "SPEED: remove object from speed list: %s (%d,%d)\n", query_name(op), op->x, op->y);*/
		if (op->active_prev == NULL)
		{
			active_objects = op->active_next;
			if (op->active_next != NULL)
				op->active_next->active_prev = NULL;
		}
		else
		{
			op->active_prev->active_next = op->active_next;
			if (op->active_next)
				op->active_next->active_prev = op->active_prev;
		}

		op->active_next = NULL;
		op->active_prev = NULL;
    }
}


/* OLD NOTES
 * update_object() updates the array which represents the map.
 * It takes into account invisible objects (and represent squares covered
 * by invisible objects by whatever is below them (unless it's another
 * invisible object, etc...)
 * If the object being updated is beneath a player, the look-window
 * of that player is updated (this might be a suboptimal way of
 * updating that window, though, since update_object() is called _often_)
 *
 * action is a hint of what the caller believes need to be done.
 * For example, if the only thing that has changed is the face (due to
 * an animation), we don't need to call update_position until that actually
 * comes into view of a player.  OTOH, many other things, like addition/removal
 * of walls or living creatures may need us to update the flags now.
 * current action are:
 * UP_OBJ_INSERT: op was inserted
 * UP_OBJ_REMOVE: op was removed
 * UP_OBJ_CHANGE: object has somehow changed.  In this case, we always update
 *  as that is easier than trying to look at what may have changed.
 * UP_OBJ_FACE: only the objects face has changed. */
/* i want use this function as one and only way to decide what we update in a tile
 * and how - and what not. As smarter this function is, as better. This function MUST
 * be called now from everything what does a noticable change to a object. We can
 * pre-decide its needed to call but normally we must call this. */
void update_object(object *op, int action)
{
	MapSpace *msp;
    int flags, newflags;

	/*LOG(-1, "update_object: %s (%d,%d) - action %x\n", op->name, op->x, op->y, action);*/
    if (op == NULL)
	{
        /* this should never happen */
        LOG(llevError, "ERROR: update_object() called for NULL object.\n");
		return;
    }

    if (op->env != NULL || !op->map || op->map->in_memory == MAP_SAVING)
		return;

    /* make sure the object is within map boundaries */
#if 0
    if (op->x < 0 || op->x >= MAP_WIDTH(op->map) || op->y < 0 || op->y >= MAP_HEIGHT(op->map))
	{
        LOG(llevError, "ERROR: update_object() called for object out of map!\n");
		return;
    }
#endif

	/* no need to change anything except the map update counter */
	if (action == UP_OBJ_FACE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FACE - %s\n", query_name(op));
#endif
	 	INC_MAP_UPDATE_COUNTER(op->map, op->x, op->y);
		return;
    }

	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);
    newflags = msp->flags;
	flags = newflags & ~P_NEED_UPDATE;

	/* always resort layer - but not always flags */
    if (action == UP_OBJ_INSERT)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_INS - %s\n", query_name(op));
#endif
		/* force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* handle lightning system */
		if (op->glow_radius)
			adjust_light_source(op->map, op->x, op->y, op->glow_radius);

		/* this is handled a bit more complex, we must always loop the flags! */
        if (QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PASS_THRU))
			newflags |= P_FLAGS_UPDATE;
		/* floors define our node - force a update */
		else if (QUERY_FLAG(op, FLAG_IS_FLOOR))
		{
			newflags |= P_FLAGS_UPDATE;
			msp->light_value += op->last_sp;
		}
		/* ok, we don't have to use flag loop - we can set it by hand! */
		else
		{
			if (op->type == CHECK_INV)
				newflags |= P_CHECK_INV;
			else if(op->type == MAGIC_EAR)
				newflags|= P_MAGIC_EAR;

			if (QUERY_FLAG(op, FLAG_ALIVE))
				newflags |= P_IS_ALIVE;

	        if (QUERY_FLAG(op, FLAG_IS_PLAYER))
				newflags |= P_IS_PLAYER;

	        if (QUERY_FLAG(op, FLAG_PLAYER_ONLY))
				newflags |= P_PLAYER_ONLY;

	        if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
				newflags |= P_BLOCKSVIEW;

			if (QUERY_FLAG(op, FLAG_NO_MAGIC))
				newflags |= P_NO_MAGIC;

			if (QUERY_FLAG(op, FLAG_NO_CLERIC))
				newflags |= P_NO_CLERIC;

			if (QUERY_FLAG(op, FLAG_WALK_ON))
				newflags |= P_WALK_ON;

			if (QUERY_FLAG(op, FLAG_FLY_ON))
				newflags |= P_FLY_ON;

			if (QUERY_FLAG(op, FLAG_WALK_OFF))
				newflags |= P_WALK_OFF;

			if (QUERY_FLAG(op, FLAG_FLY_OFF))
				newflags |= P_FLY_OFF;

			if (QUERY_FLAG(op, FLAG_DOOR_CLOSED))
				newflags |= P_DOOR_CLOSED;

			if (QUERY_FLAG(op, FLAG_CAN_REFL_SPELL))
				newflags |= P_REFL_SPELLS;

			if (QUERY_FLAG(op, FLAG_CAN_REFL_MISSILE))
				newflags |= P_REFL_MISSILE;
		}
    }
	else if (action == UP_OBJ_REMOVE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug,"UO_REM - %s\n", query_name(op));
#endif
		/* force layer rebuild */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;

		/* we don't handle floor tile light/darkness setting here -
		 * we assume we don't remove a floor tile ever before dropping
		 * the map. */

		/* handle lightning system */
		if (op->glow_radius)
			adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));

		/* we must rebuild the flags when one of this flags is touched from our object */
		if (QUERY_FLAG(op, FLAG_ALIVE) || QUERY_FLAG(op, FLAG_IS_PLAYER) || QUERY_FLAG(op, FLAG_BLOCKSVIEW) || QUERY_FLAG(op, FLAG_DOOR_CLOSED) || QUERY_FLAG(op, FLAG_PASS_THRU) || QUERY_FLAG(op, FLAG_NO_PASS) || QUERY_FLAG(op, FLAG_PLAYER_ONLY) || QUERY_FLAG(op, FLAG_NO_MAGIC) || QUERY_FLAG(op, FLAG_NO_CLERIC) || QUERY_FLAG(op, FLAG_WALK_ON) || QUERY_FLAG(op, FLAG_FLY_ON) || QUERY_FLAG(op, FLAG_WALK_OFF) || QUERY_FLAG(op, FLAG_FLY_OFF) || QUERY_FLAG(op, FLAG_CAN_REFL_SPELL) || QUERY_FLAG(op, FLAG_CAN_REFL_MISSILE) || QUERY_FLAG(op,	FLAG_IS_FLOOR) || op->type == CHECK_INV || op->type == MAGIC_EAR )
			newflags |= P_FLAGS_UPDATE;
    }
	else if (action == UP_OBJ_FLAGS)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FLAGS - %s\n", query_name(op));
#endif
		/* force flags rebuild but no tile counter*/
		newflags |= P_FLAGS_UPDATE;
    }
	else if (action == UP_OBJ_FLAGFACE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_FLAGFACE - %s\n", query_name(op));
#endif
		/* force flags rebuild */
		newflags |= P_FLAGS_UPDATE;
		msp->update_tile++;
    }
	else if (action == UP_OBJ_LAYER)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_LAYER - %s\n", query_name(op));
#endif
		/* rebuild layers - most common when we change visibility of the object */
		newflags |= P_NEED_UPDATE;
		msp->update_tile++;
    }
	else if (action == UP_OBJ_ALL)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UO_ALL - %s\n", query_name(op));
#endif
		/* force full tile update */
		newflags |= (P_FLAGS_UPDATE | P_NEED_UPDATE);
		msp->update_tile++;
	}
    else
	{
		LOG(llevError, "ERROR: update_object called with invalid action: %d\n", action);
		return;
    }

    if (flags != newflags)
	{
		/* rebuild flags */
		if (newflags & (P_FLAGS_UPDATE))
		{
			msp->flags |= (newflags | P_NO_ERROR | P_FLAGS_ONLY);
			update_position(op->map, op->x, op->y);
		}
		else
			msp->flags |= newflags;
    }

    if (op->more != NULL)
		update_object(op->more, action);
}

/* search a player inventory for quest item placeholder.
 * this function is NOT called very often - even it takes some
 * cycles when we examine a group. */
static int find_quest_item_inv(object *target, object *obj)
{
	object *tmp;

	for (tmp = target->inv; tmp; tmp = tmp->below)
	{
		if (tmp->inv)
		{
			if (find_quest_item_inv(tmp->inv, obj))
				return 1;
		}

		if (tmp->type == obj->type && tmp->name == obj->name && tmp->arch->name == obj->arch->name)
			return 1;
	}

	return 0;
}

static inline int find_quest_item(object *target, object *obj)
{
	object *tmp;

	if (!target || target->type != PLAYER)
		return 0;

	tmp = present_in_ob(TYPE_QUEST_CONTAINER, target);

	if (tmp)
	{
		for (tmp = tmp->inv; tmp; tmp = tmp->below)
		{
			if (tmp->name == obj->name && !strcmp(tmp->race, obj->arch->name))
				return 1;
		}
	}

	return find_quest_item_inv(target, obj);
}

/* we give a player a one drop item. This also
 * adds this item to the quest_container - instead to quest
 * items which will be added when the next quest step is triggered.
 * (most times from a quest script) */
static inline int add_one_drop_quest_item(object *target, object *obj)
{
	object *tmp, *q_tmp;

	if (!target || target->type != PLAYER)
		return 0;

	if (!(tmp = present_in_ob(TYPE_QUEST_CONTAINER, target)))
	{
		/* create and insert a quest container in player */
		tmp = get_object();
		copy_object(get_archetype("quest_container"), tmp);
		insert_ob_in_ob(tmp, target);
	}

	q_tmp = get_object();
	/* copy without put on active list */
	copy_object_data(obj, q_tmp);
	/* just to be on the secure side ... */
	q_tmp->speed = 0.0f;
	CLEAR_FLAG(q_tmp, FLAG_ANIMATE);
	CLEAR_FLAG(q_tmp, FLAG_ALIVE);
	/* we are storing the arch name of quest dummy items in race */
	FREE_AND_COPY_HASH(q_tmp->race, obj->arch->name);
	/* dummy copy in quest container */
	insert_ob_in_ob(q_tmp, tmp);
	SET_FLAG(obj, FLAG_IDENTIFIED);
	/* now the player can use this item normal,
	 * even trade and sell it */
	CLEAR_FLAG(obj, FLAG_QUEST_ITEM);
	CLEAR_FLAG(obj, FLAG_SYS_OBJECT);
	/* real object to player */
	insert_ob_in_ob(obj, target);

	return 1;
}

/* Drops the inventory of ob into ob's current environment. */
/* Makes some decisions whether to actually drop or not, and/or to
 * create a corpse for the stuff */
void drop_ob_inv(object *ob)
{
    object *corpse = NULL;
    object *enemy = NULL;
    object *tmp_op = NULL;
    object *tmp = NULL;

	/* we don't handle players here */
    if (ob->type == PLAYER)
	{
        LOG(llevBug, "BUG: drop_ob_inv() - try to drop items of %s\n", ob->name);
        return;
    }

	/* TODO */
    if (ob->env == NULL && (ob->map == NULL || ob->map->in_memory != MAP_IN_MEMORY))
	{
        LOG(llevDebug, "BUG: drop_ob_inv() - can't drop inventory of objects not in map yet: %s (%x)\n", ob->name, ob->map);
        return;
    }

    /* create race corpse and/or drop stuff to floor */
    if (ob->enemy && ob->enemy->type == PLAYER)
        enemy = ob->enemy;
    else
        enemy = get_owner(ob->enemy);

    if ((QUERY_FLAG(ob, FLAG_CORPSE) && !QUERY_FLAG(ob, FLAG_STARTEQUIP)) || QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
    {
        racelink *race_corpse = find_racelink(ob->race);
        if (race_corpse)
        {
            corpse = arch_to_object(race_corpse->corpse);
            corpse->x = ob->x;
			corpse->y = ob->y;
			corpse->map = ob->map;
            corpse->weight = ob->weight;
        }
    }

    tmp_op = ob->inv;

    while (tmp_op != NULL)
    {
        tmp = tmp_op->below;
		/* Inv-no check off / This will be destroyed in next loop of object_gc() */
        remove_ob(tmp_op);
        /* if we recall spawn mobs, we don't want drop their items as free.
         * So, marking the mob itself with "FLAG_STARTEQUIP" will kill
         * all inventory and not dropping it on the map.
         * This also happens when a player slays a to low mob/non exp mob.
         * Don't drop any sys_object in inventory... I can't think about
         * any use... when we do it, a disease needle for example
         * is dropping his disease force and so on. */

        if (QUERY_FLAG(tmp_op, FLAG_QUEST_ITEM))
        {
            /* legal, non freed enemy */
            if (enemy && enemy->type == PLAYER && enemy->count == ob->enemy_count)
            {
                /* this is the new quest item & one drop quest item code!
                 * Dropping quest items to the ground in a corpse can invoke
                 * alot of problems and glitches. The only way to avoid it is,
                 * to move the quest item HERE in the inventory of the player or
                 * group. For one drop quests it is the really only usable way. */
                /* first: if the player has this item (normally from killing the
                 * quest mob before) we free the quest item here and stop.
                 * otherwise, move the item in the players inventory! */
                if (!find_quest_item(enemy, tmp_op))
                {
                    char auto_buf[HUGE_BUF];

                    /* first, lets check what we have: quest or one drop quest */
					/* marks one drop quest items */
                    if (QUERY_FLAG(tmp_op, FLAG_SYS_OBJECT))
                    {
                        add_one_drop_quest_item(enemy, tmp_op);
                        sprintf(auto_buf, "You solved the one drop quest %s!\n", query_name(tmp_op, NULL));
                        (*draw_info_func)(NDI_UNIQUE | NDI_NAVY, 0, enemy, auto_buf);
                    }
                    else
                    {
                        insert_ob_in_ob(tmp_op,enemy);
                        sprintf(auto_buf, "You found the quest item %s!\n", query_name(tmp_op, NULL));
                        (*draw_info_func)(NDI_UNIQUE | NDI_NAVY, 0, enemy, auto_buf);
                    }

                    (*esrv_send_item_func)(enemy, tmp_op);
                }
            }
        }
		else if(!(QUERY_FLAG(ob, FLAG_STARTEQUIP) || (tmp_op->type != RUNE && (QUERY_FLAG(tmp_op, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp_op, FLAG_STARTEQUIP) || QUERY_FLAG(tmp_op, FLAG_NO_DROP)))))
        {
            tmp_op->x = ob->x, tmp_op->y = ob->y;

            /* if we have a corpse put the item in it */
            if (corpse)
                insert_ob_in_ob(tmp_op, corpse);
            else
            {
                /* don't drop traps from a container to the floor.
                 * removing the container where a trap is applied will
                 * neutralize the trap too
				 * Also not drop it in env - be safe here */
                if (tmp_op->type != RUNE)
				{
					if (ob->env)
					{
						insert_ob_in_ob(tmp_op, ob->env);
						/* this should handle in future insert_ob_in_ob() */
						if (ob->env->type == PLAYER)
							(*esrv_send_item_func)(ob->env, tmp_op);
						else if (ob->env->type == CONTAINER)
							(*esrv_send_item_func)(ob->env, tmp_op);
					}
					/* Insert in same map as the env */
					else
	                    insert_ob_in_map(tmp_op, ob->map, NULL, 0);
				}
            }
        }
        tmp_op = tmp;
    }

    if (corpse)
    {
        /* drop the corpse when something is in OR corpse_forced is set */
        /* i changed this to drop corpse always even they have no items
         * inside (player get confused when corpse don't drop. To avoid
         * clear corpses, change below "||corpse " to "|| corpse->inv" */
        if (QUERY_FLAG(ob, FLAG_CORPSE_FORCED) || corpse)
        {
            /* ok... we have a corpse AND we insert something in.
             * now check enemy and/or attacker to find a player.
             * if there is one - personlize this corpse container.
             * this gives the player the chance to grap this stuff first
             * - and looter will be stopped. */

            if (enemy && enemy->type == PLAYER)
            {
                if (enemy->count == ob->enemy_count)
                    FREE_AND_ADD_REF_HASH(corpse->slaying, enemy->name);
            }
			/* && no player */
            else if (QUERY_FLAG(ob, FLAG_CORPSE_FORCED))
            {
                /* normallly only player drop corpse. But in some cases
                 * npc can do it too. Then its smart to remove that corpse fast.
                 * It will not harm anything because we never deal for NPC with
                 * bounty. */
                corpse->stats.food = 3;
            }

			/* change sub_type to mark this corpse */
            if (corpse->slaying)
			{
				if (CONTR(enemy)->party_number != -1)
                {
                    corpse->stats.maxhp = CONTR(enemy)->party_number;
                    corpse->sub_type1 = ST1_CONTAINER_CORPSE_party;
                }
                else
                	corpse->sub_type1 = ST1_CONTAINER_CORPSE_player;
			}

			if (ob->env)
			{
				insert_ob_in_ob(corpse, ob->env);

				/* this should handle in future insert_ob_in_ob() */
				if (ob->env->type == PLAYER)
					(*esrv_send_item_func)(ob->env, corpse);
				else if (ob->env->type == CONTAINER)
					(*esrv_send_item_func)(ob->env, corpse);
			}
			else
				insert_ob_in_map(corpse, ob->map, NULL, 0);
        }
		/* disabled */
        else
        {
            /* if we are here, our corpse mob had something in inv but its nothing to drop */

			/* no check off - not put in the map here */
            if (!QUERY_FLAG(corpse, FLAG_REMOVED))
                remove_ob(corpse);
        }
    }
}

/* destroy_object() frees everything allocated by an object, removes
 * it from the list of used objects, and puts it on the list of
 * free objects. This function is called automatically to free unused objects
 * (it is called from return_poolchunk() during garbage collection in object_gc() ).
 * The object must have been removed by remove_ob() first for
 * this function to succeed. */
void destroy_object(object *ob)
{
	if (OBJECT_FREE(ob))
	{
		dump_object(ob);
		LOG(llevBug, "BUG: Trying to destroy freed object.\n%s\n", errmsg);
		return;
	}

    if (!QUERY_FLAG(ob, FLAG_REMOVED))
	{
		dump_object(ob);
		LOG(llevBug, "BUG: Destroy object called with non removed object\n:%s\n", errmsg);
    }

  	/* This should be very rare... */
  	if (QUERY_FLAG(ob, FLAG_IS_LINKED))
	  	remove_button_link(ob);

  	if (QUERY_FLAG(ob, FLAG_FRIENDLY))
	  	remove_friendly_object(ob);

  	if (ob->type == CONTAINER && ob->attacked_by)
	  	container_unlink_func(NULL, ob);

  	/* Make sure to get rid of the inventory, too. It will be destroy()ed at the next gc */
  	/* TODO: maybe destroy() it here too? */
  	remove_ob_inv(ob);

  	/* Remove object from the active list */
  	ob->speed = 0;
  	update_ob_speed(ob);
 	/*LOG(llevDebug, "FO: a:%s %x >%s< (#%d)\n", ob->arch ? (ob->arch->name ? ob->arch->name : "") : "", ob->name, ob->name ? ob->name : "", ob->name ? query_refcount(ob->name) : 0);*/

  	/* Free attached attrsets */
  	if (ob->custom_attrset)
	{
		/*LOG(llevDebug, "destroy_object() custom attrset found in object %s (type %d)\n", STRING_OBJ_NAME(ob), ob->type);*/

      	switch (ob->type)
		{
          	case PLAYER:
			/* Players are changed into DEAD_OBJECTs when they logout */
          	case DEAD_OBJECT:
              return_poolchunk(ob->custom_attrset, POOL_PLAYER);
              break;

          default:
              LOG(llevBug, "BUG: destroy_object() custom attrset found in unsupported object %s (type %d)\n", STRING_OBJ_NAME(ob), ob->type);
      	}
      	ob->custom_attrset = NULL;
  	}

	FREE_AND_CLEAR_HASH2(ob->name);
	FREE_AND_CLEAR_HASH2(ob->title);
	FREE_AND_CLEAR_HASH2(ob->race);
	FREE_AND_CLEAR_HASH2(ob->slaying);
	FREE_AND_CLEAR_HASH2(ob->msg);

	/* mark object as "do not use" and invalidate all references to it */
  	ob->count = 0;
}

/* Drop op's inventory on the floor and remove op from the map.
 * Used mainly for physical destruction of normal objects and mobs */
void destruct_ob(object *op)
{
	if (op->inv)
	    drop_ob_inv(op);

    remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_DEFAULT);
}

/* remove_ob(op):
 *   This function removes the object op from the linked list of objects
 *   which it is currently tied to.  When this function is done, the
 *   object will have no environment.  If the object previously had an
 *   environment, the x and y coordinates will be updated to
 *   the previous environment.
 *   if we want remove alot of players inventory items, set
 *   FLAG_NO_FIX_PLAYER to thje player first and call fix_player()
 *   explicit then. */
void remove_ob(object *op)
{
	MapSpace *msp;
    object *otmp;

    if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		/*dump_object(op)*/;
		LOG(llevBug, "BUG: Trying to remove removed object.:%s map:%s (%d,%d)\n", query_name(op, NULL), op->map ? (op->map->path ? op->map->path : "op->map->path == NULL") : "op->map == NULL", op->x, op->y);
		return;
    }

	/* check off is handled outside here */
    if (op->more != NULL)
		remove_ob(op->more);

    mark_object_removed(op);
	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);

    /* In this case, the object to be removed is in someones
     * inventory. */
    if (op->env != NULL)
	{
		/* this is not enough... when we for example remove money from a pouch
		 * which is in a sack (which is itself in the player inv) then the weight
		 * of the sack is not calculated right. This is only a temporary effect but
		 * we need to fix it here a recursive ->env chain. */
		if (op->nrof)
			sub_weight(op->env, op->weight * op->nrof);
		else
			sub_weight(op->env, op->weight + op->carrying);

		/* NO_FIX_PLAYER is set when a great many changes are being
		 * made to players inventory.  If set, avoiding the call to save cpu time.
		 * the flag is set from outside... perhaps from a drop_all() function. */
		if ((otmp = is_player_inv(op->env)) != NULL && CONTR(otmp) && !QUERY_FLAG(otmp, FLAG_NO_FIX_PLAYER))
		    fix_player(otmp);

		if (op->above != NULL)
			op->above->below = op->below;
		else
			op->env->inv = op->below;

		if (op->below != NULL)
			op->below->above = op->above;

		/* we set up values so that it could be inserted into
		 * the map, but we don't actually do that - it is up
		 * to the caller to decide what we want to do. */
		op->x = op->env->x, op->y = op->env->y;

		#ifdef POSITION_DEBUG
		op->ox = op->x, op->oy = op->y;
		#endif

		op->map = op->env->map;
		op->above = NULL, op->below = NULL;
		op->env = NULL;
		return;
    }

    /* If we get here, we are removing it from a map */
    if (!op->map)
	{
		LOG(llevBug, "BUG: remove_ob(): object %s (%s) not on map or env.\n", query_short_name(op, NULL), op->arch ? (op->arch->name ? op->arch->name : "<nor arch name!>") : "<no arch!>");
		return;
	}

	/* lets first unlink this object from map*/

	/* if this is the base layer object, we assign the next object to be it if it is from same layer type */
	msp = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	if (op->layer)
	{
		if (GET_MAP_SPACE_LAYER(msp, op->layer - 1) == op)
		{
			/* well, don't kick the inv objects of this layer to normal layer */
		    if (op->above && op->above->layer == op->layer && GET_MAP_SPACE_LAYER(msp, op->layer + 6) != op->above)
				SET_MAP_SPACE_LAYER(msp, op->layer - 1, op->above);
			else
				SET_MAP_SPACE_LAYER(msp, op->layer - 1, NULL);
		}
		/* inv layer? */
		else if (GET_MAP_SPACE_LAYER(msp, op->layer + 6) == op)
		{
		    if (op->above && op->above->layer == op->layer)
				SET_MAP_SPACE_LAYER(msp, op->layer + 6, op->above);
			else
				SET_MAP_SPACE_LAYER(msp, op->layer + 6, NULL);
		}
	}

    /* link the object above us */
    if (op->above)
		op->above->below = op->below;
	/* assign below as last one */
	else
		SET_MAP_SPACE_LAST(msp, op->below);

    /* Relink the object below us, if there is one */
    if (op->below)
		op->below->above = op->above;
	/* first object goes on above it. */
    else
		SET_MAP_SPACE_FIRST(msp, op->above);

    op->above = NULL;
    op->below = NULL;

	/* this is triggered when a map is swaped out and the objects on it get removed too */
	if (op->map->in_memory == MAP_SAVING)
		return;

	/* we updated something here - mark this tile as changed! */
	msp->update_tile++;

	/* some player only stuff.
	 * we adjust the ->player map variable and the local map player chain. */
	if (op->type == PLAYER)
	{
		struct pl_player *pltemp = CONTR(op);

		/* now we remove us from the local map player chain */
		if (pltemp->map_below)
			CONTR(pltemp->map_below)->map_above = pltemp->map_above;
		else
			op->map->player_first = pltemp->map_above;

		if (pltemp->map_above)
			CONTR(pltemp->map_above)->map_below = pltemp->map_below;

		pltemp->map_below = pltemp->map_above = NULL;

		/* thats always true when touching the players map pos. */
		pltemp->update_los = 1;

		/* a open container NOT in our player inventory = unlink (close) when we move */
		if (pltemp->container && pltemp->container->env != op)
			container_unlink_func(pltemp, NULL);
	}

    update_object(op, UP_OBJ_REMOVE);

    op->env = NULL;
}

/* delete and remove recursive the inventory of an object. */
void remove_ob_inv(object *op)
{
	object *tmp, *tmp2;

	for (tmp = op->inv; tmp; tmp = tmp2)
	{
		/* save ptr, gets NULL in remove_ob */
		tmp2 = tmp->below;

		if (tmp->inv)
			remove_ob_inv(tmp);

		/* no map, no check off */
		remove_ob(tmp);
	}
}


/* insert_ob_in_map (op, map, originator, flag):
 * This function inserts the object in the two-way linked list
 * which represents what is on a map.
 * The second argument specifies the map, and the x and y variables
 * in the object about to be inserted specifies the position.
 *
 * originator: Player, monster or other object that caused 'op' to be inserted
 * into 'map'.  May be NULL.
 *
 * flag is a bitmask about special things to do (or not do) when this
 * function is called.  see the object.h file for the INS_ values.
 * Passing 0 for flag gives proper default values, so flag really only needs
 * to be set if special handling is needed.
 *
 * Return value:
 *   NULL if 'op' was destroyed
 *   just 'op' otherwise
 *   When a trap (like a trapdoor) has moved us here, op will returned true.
 *   The caller function must handle it and controlling ->map, ->x and ->y of op
 *
 * I reworked the FLY/MOVE_ON system - it should now very solid and faster. MT-2004.
 * Notice that the FLY/WALK_OFF stuff is removed from remove_ob() and must be called
 * explicit when we want make a "move/step" for a object which can trigger it. */
object *insert_ob_in_map(object *op, mapstruct *m, object *originator, int flag)
{
    object *tmp = NULL, *top;
	MapSpace *mc;
    int x, y, lt, layer, layer_inv;

	/* some tests to check all is ok... some cpu ticks
	 * which tracks we have problems or not */
    if (OBJECT_FREE(op))
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert freed object %s in map %s!\n:%s\n", query_name(op, NULL), m->name, errmsg);
		return NULL;
    }

    if (m == NULL)
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert object %s in null-map!\n%s\n", query_name(op, NULL), errmsg);
		return NULL;
    }

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		dump_object(op);
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert non removed object %s in map %s.\n%s\n", query_name(op, NULL), m->name, errmsg);
		return NULL;
    }

	/* tail, but no INS_TAIL_MARKER: we had messed something outside! */
	if (op->head && !(flag & INS_TAIL_MARKER))
	{
		LOG(llevBug, "BUG: insert_ob_in_map(): inserting op->more WITHOUT INS_TAIL_MARKER! OB:%s (ARCH: %s) (MAP: %s (%d,%d))\n", query_name(op, NULL), op->arch->name, m->path, op->x, op->y);
		return NULL;
	}

    if (op->more)
	{
		if (insert_ob_in_map(op->more, op->more->map, originator, flag | INS_TAIL_MARKER) == NULL)
		{
			if (!op->head)
				LOG(llevBug, "BUG: insert_ob_in_map(): inserting op->more killed op %s in map %s\n", query_name(op, NULL), m->name);

			return NULL;
		}
    }

    CLEAR_FLAG(op, FLAG_REMOVED);

#ifdef POSITION_DEBUG
    op->ox = op->x;
    op->oy = op->y;
#endif

	/* this is now a key part of this function, because
	 * we adjust multi arches here when they cross map boarders! */
    x = op->x;
    y = op->y;
	op->map = m;

    if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevBug, "BUG: insert_ob_in_map(): Trying to insert object %s outside the map %s (%d,%d).\n\n", query_name(op, NULL), op->map->path, op->x, op->y);
		return NULL;
    }

	/* x and y will only change when we change the map too - so check the map */
    if (op->map != m)
	{
		op->map = m;
		op->x = x;
		op->y = y;
    }

	/* hm, i not checked this, but is it not smarter to remove op instead and return? MT */
    if (op->nrof && !(flag & INS_NO_MERGE))
	{
		for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		{
			if (CAN_MERGE(op,tmp))
			{
				op->nrof+=tmp->nrof;
				/* a bit tricky remove_ob() without check off.
				 * technically, this happens: arrow x/y is falling on the stack
				 * of perhaps 10 arrows. IF a teleporter is called, the whole 10
				 * arrows are teleported.Thats a right effect. */
				remove_ob(tmp);
			}
		}
    }

	/* we need this for FLY/MOVE_ON/OFF */
	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	/* nothing on the floor can be applied */
    CLEAR_FLAG(op, FLAG_APPLIED);
	/* or locked */
    CLEAR_FLAG(op, FLAG_INV_LOCKED);

	/* map layer system */
	/* we don't test for sys object because we ALWAYS set the layer of a sys object
	 * to 0 when we load a sys_object (0 is default, but server loader will warn when
	 * we set a layer != 0). We will do the check in the arch load and
	 * in the map editor, so we don't need to mess with it anywhere at runtime.
	 * Note: even the inserting process is more complicate and more code as the crossfire
	 * on, we should speed up things alot - with more object more cpu time we will safe.
	 * Also, see that we don't need to access in the inserting or sorting the old objects.
	 * no FLAG_xx check or something - all can be done by the cpu in cache. */
	/* for fast access - we will not change the node here */
	mc = GET_MAP_SPACE_PTR(op->map, op->x, op->y);

	/* so we have a non system object */
	if (op->layer)
	{
		layer = op->layer - 1;
		layer_inv = layer + 7;

		/* not invisible? */
		if (!QUERY_FLAG(op, FLAG_IS_INVISIBLE))
		{
			/* have we another object of this layer? */
			if ((top = GET_MAP_SPACE_LAYER(mc, layer)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, layer_inv)) == NULL)
			{
				/* no, we are the first of this layer - lets search something above us we can chain with.*/
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (top = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
			}

			/* now, if top != NULL, thats the object BEFORE we want chain. This can be
			 * the base layer, the inv base layer or a object from a upper layer.
			 * If it NULL, we are the only object in this tile OR
			 * ->last holds the object BEFORE ours. */
			/* we always go in front */
			SET_MAP_SPACE_LAYER(mc, layer, op);
			/* easy - we chain our object before this one */
			if (top)
			{
				if (top->below)
					top->below->above = op;
				/* if no object before, we are new starting object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				op->below = top->below;
				top->below = op;
				op->above = top;
			}
			/* we are first object here or one is before us  - chain to it */
			else
			{
				if ((top = GET_MAP_SPACE_LAST(mc)) != NULL)
				{
					top->above = op;
					op->below = top;

				}
				/* a virgin! we set first and last object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				SET_MAP_SPACE_LAST(mc, op);
			}
		}
		/* invisible object */
		else
		{
			int tmp_flag;

			/* check the layers */
			if ((top = GET_MAP_SPACE_LAYER(mc, layer_inv)) != NULL)
				tmp_flag = 1;
			else if ((top = GET_MAP_SPACE_LAYER(mc, layer)) != NULL)
			{
				/* in this case, we have 1 or more normal objects in this layer,
				 * so we must skip all of them. easiest way is to get a upper layer
				 * valid object. */
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (tmp = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (tmp = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
				tmp_flag = 2;
			}
			else
			{
				/* no, we are the first of this layer - lets search something above us we can chain with.*/
				for (lt = op->layer; lt < MAX_ARCH_LAYERS && (top = GET_MAP_SPACE_LAYER(mc, lt)) == NULL && (top = GET_MAP_SPACE_LAYER(mc, lt + 7)) == NULL; lt++);
				tmp_flag = 3;
			}

			/* in all cases, we are the new inv base layer */
			SET_MAP_SPACE_LAYER(mc, layer_inv, op);
			/* easy - we chain our object before this one - well perhaps */
			if (top)
			{
				/* ok, now the tricky part.
				 * if top is set, this can be...
				 * - the inv layer of same layer id (and tmp_flag will be 1)
				 * - the normal layer of same layer id (tmp_flag == 2)
				 * - the inv OR normal layer of a upper layer (tmp_flag == 3)
				 * if tmp_flag = 1, its easy - just we get in front of top and use
				 * the same links.
				 * if tmp_flag = 2 AND tmp is set, tmp is the object we chain before.
				 * is tmp is NULL, we get ->last and chain after it.
				 * if tmp_flag = 3, we chain all times to top (before). */
				if (tmp_flag == 2)
				{
					if (tmp)
					{
						/* we can't be first, there is always one before us */
						tmp->below->above = op;
						op->below = tmp->below;
						/* and one after us... */
						tmp->below = op;
						op->above = tmp;
					}
					else
					{
						/* there is one before us, so this is always valid */
						tmp = GET_MAP_SPACE_LAST(mc);
						/* new last object */
						SET_MAP_SPACE_LAST(mc, op);
						op->below = tmp;
						tmp->above = op;
					}
				}
				/* tmp_flag 1 & tmp_flag 3 are done the same way */
				else
				{
					if (top->below)
						top->below->above = op;
					/* if no object before ,we are new starting object */
					else
						SET_MAP_SPACE_FIRST(mc, op);

					op->below = top->below;
					top->below = op;
					op->above = top;
				}
			}
			/* we are first object here or one is before us  - chain to it */
			else
			{
				/* there is something down we don't care what */
				if ((top = GET_MAP_SPACE_LAST(mc)) != NULL)
				{
					/* just chain to it */
					top->above = op;
					op->below = top;

				}
				/* a virgin! we set first and last object */
				else
					SET_MAP_SPACE_FIRST(mc, op);

				SET_MAP_SPACE_LAST(mc, op);
			}
		}
	}
	/* op->layer == 0 - lets just put this object in front of all others */
	else
	{
		/* is there some else? */
		if ((top = GET_MAP_SPACE_FIRST(mc)) != NULL)
		{
			/* easy chaining */
			top->below = op;
			op->above = top;
		}
		/* no ,we are last object too */
		else
			SET_MAP_SPACE_LAST(mc, op);

		SET_MAP_SPACE_FIRST(mc, op);
	}

	/* lets set some specials for our players
	 * we adjust the ->player map variable and the local
	 * map player chain. */
	if (op->type == PLAYER)
	{
		CONTR(op)->socket.update_tile = 0;
		/* thats always true when touching the players map pos. */
		CONTR(op)->update_los = 1;

		if (op->map->player_first)
		{
			CONTR(op->map->player_first)->map_below = op;
			CONTR(op)->map_above = op->map->player_first;
		}
		op->map->player_first = op;

	}

	/* we updated something here - mark this tile as changed! */
	mc->update_tile++;
    /* updates flags (blocked, alive, no magic, etc) for this map space */
    update_object(op, UP_OBJ_INSERT);

	/* check walk on/fly on flag if not canceld AND there is some to move on.
	 * Note: We are first inserting the WHOLE object/multi arch - then we check all
	 * part for traps. This ensures we don't must do nasty hacks with half inserted/removed
	 * objects - for example when we hit a teleporter trap.
	 * Check only for single tiles || or head but ALWAYS for heads. */
	if (!(flag & INS_NO_WALK_ON) && (mc->flags & (P_WALK_ON | P_FLY_ON) || op->more) && !op->head)
	{
		int event;

		/* we want reuse mc here... bad enough we need to check it double for multi arch */
		if (QUERY_FLAG(op, FLAG_FLY_ON))
		{
			/* we are flying but no fly event here */
			if (!(mc->flags & P_FLY_ON))
				goto check_walk_loop;
		}
		/* we are not flying - check walking only */
		else
		{
			if (!(mc->flags & P_WALK_ON))
				goto check_walk_loop;
		}

		if ((event = check_walk_on(op, originator, MOVE_APPLY_MOVE)))
		{
			/* don't return NULL - we are valid but we was moved */
			if (event == CHECK_WALK_MOVED)
				return op;
			/* CHECK_WALK_DESTROYED */
			else
				return NULL;
		}

		/* TODO: check event */
		check_walk_loop:
        for (tmp = op->more; tmp != NULL; tmp = tmp->more)
		{
			mc = GET_MAP_SPACE_PTR(tmp->map, tmp->x, tmp->y);

			/* object is flying/levitating */
			/* trick: op is single tile OR always head! */
			if (QUERY_FLAG(op, FLAG_FLY_ON))
			{
				/* we are flying but no fly event here */
				if (!(mc->flags & P_FLY_ON))
					continue;
			}
			/* we are not flying - check walking only */
			else
			{
				if (!(mc->flags & P_WALK_ON))
					continue;
			}

			if ((event = check_walk_on(tmp, originator, MOVE_APPLY_MOVE)))
			{
				/* don't return NULL - we are valid but we was moved */
				if (event == CHECK_WALK_MOVED)
					return op;
				/* CHECK_WALK_DESTROYED */
				else
					return NULL;
			}
		}
    }

    return op;
}

/* this function inserts an object in the map, but if it
 *  finds an object of its own type, it'll remove that one first.
 *  op is the object to insert it under:  supplies x and the map. */
void replace_insert_ob_in_map(char *arch_string, object *op)
{
    object *tmp;
    object *tmp1;

    /* first search for itself and remove any old instances */
    for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		if (!strcmp(tmp->arch->name, arch_string))
		{
			/* no move off here... should be ok, this is a technical function */
			remove_ob(tmp);
			tmp->speed = 0;
			/* Remove it from active list */
			update_ob_speed(tmp);
		}
    }

    tmp1 = arch_to_object(find_archetype(arch_string));

    tmp1->x = op->x; tmp1->y = op->y;
    insert_ob_in_map(tmp1, op->map, op, 0);
}

/* get_split_ob(ob,nr) splits up ob into two parts.  The part which
 * is returned contains nr objects, and the remaining parts contains
 * the rest (or is removed and freed if that number is 0).
 * On failure, NULL is returned, and the reason put into the
 * global static errmsg array. */
object *get_split_ob(object *orig_ob, int nr)
{
    object *newob;
    object *tmp, *event;
    int is_removed = (QUERY_FLAG(orig_ob, FLAG_REMOVED) != 0);

    if ((int) orig_ob->nrof < nr)
	{
		sprintf(errmsg, "There are only %d %ss.", orig_ob->nrof ? orig_ob->nrof : 1, query_name(orig_ob, NULL));
		return NULL;
    }

    newob = get_object();
    copy_object(orig_ob, newob);

    /* Gecko: copy inventory (event objects) */
    for (tmp = orig_ob->inv; tmp; tmp = tmp->below)
	{
        if (tmp->type == TYPE_EVENT_OBJECT)
		{
            event = get_object();
            copy_object(tmp, event);
            insert_ob_in_ob(event, newob);
        }
    }

/*    if(QUERY_FLAG(orig_ob, FLAG_UNPAID) && QUERY_FLAG(orig_ob, FLAG_NO_PICK))*/
/*		;*/ /* clone objects .... */
/*	else*/
	orig_ob->nrof -= nr;

    if (orig_ob->nrof < 1)
	{
		if (!is_removed)
			remove_ob(orig_ob);

		check_walk_off(orig_ob, NULL, MOVE_APPLY_VANISHED);
    }
    else if (!is_removed)
	{
		if (orig_ob->env != NULL)
			sub_weight(orig_ob->env, orig_ob->weight * nr);

		if (orig_ob->env == NULL && orig_ob->map->in_memory != MAP_IN_MEMORY)
		{
			strcpy(errmsg, "Tried to split object whose map is not in memory.");
			LOG(llevDebug, "Error, Tried to split object whose map is not in memory.\n");
			return NULL;
		}
    }

    newob->nrof = nr;
    return newob;
}

/* decrease_ob_nr(object, number) decreases a specified number from
 * the amount of an object.  If the amount reaches 0, the object
 * is subsequently removed and freed.
 *
 * Return value: 'op' if something is left, NULL if the amount reached 0 */
object *decrease_ob_nr(object *op, int i)
{
    object *tmp;
    player *pl;

	/* objects with op->nrof require this check */
    if (i == 0)
        return op;

    if (i > (int)op->nrof)
        i = (int)op->nrof;

    if (QUERY_FLAG(op, FLAG_REMOVED))
        op->nrof -= i;
    else if (op->env != NULL)
    {
		/* is this object in the players inventory, or sub container
		 * therein? */
        tmp = is_player_inv(op->env);
		/* nope.  Is this a container the player has opened?
		 * If so, set tmp to that player.
		 * IMO, searching through all the players will mostly
		 * likely be quicker than following op->env to the map,
		 * and then searching the map for a player. */

		/* TODO: this is another nasty "search all players"...
		 * i skip this to rework as i do the container patch -
		 * but we can do this now much smarter !
		 * MT -08.02.04  */
		if (!tmp)
		{
			for (pl = first_player; pl; pl = pl->next)
				if (pl->container == op->env)
					break;

			if (pl)
				tmp = pl->ob;
			else
				tmp = NULL;
		}

        if (i < (int)op->nrof)
		{
            sub_weight (op->env, op->weight * i);
            op->nrof -= i;

            if (tmp)
			{
                (*esrv_send_item_func)(tmp, op);
                (*esrv_update_item_func)(UPD_WEIGHT, tmp, tmp);
            }
        }
		else
		{
            remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
            op->nrof = 0;

            if (tmp)
			{
                (*esrv_del_item_func)(CONTR(tmp), op->count,op->env);
                (*esrv_update_item_func)(UPD_WEIGHT, tmp, tmp);
            }
        }
    }
    else
    {
		object *above = op->above;

        if (i < (int)op->nrof)
            op->nrof -= i;
        else
		{
            remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
            op->nrof = 0;
        }

		/* Since we just removed op, op->above is null */
        for (tmp = above; tmp != NULL; tmp = tmp->above)
		{
            if (tmp->type == PLAYER)
			{
                if (op->nrof)
                    (*esrv_send_item_func)(tmp, op);
                else
                    (*esrv_del_item_func)(CONTR(tmp), op->count, op->env);
            }
		}
    }

    if (op->nrof)
        return op;
    else
        return NULL;
}

/* insert_ob_in_ob(op, environment):
 *   This function inserts the object op in the linked list
 *   inside the object environment.
 *
 * Eneq(@csd.uu.se): Altered insert_ob_in_ob to make things picked up enter
 * the inventory at the last position or next to other objects of the same
 * type.
 * Frank: Now sorted by type, archetype and magic!
 *
 * The function returns now pointer to inserted item, and return value can
 * be != op, if items are merged. -Tero */
object *insert_ob_in_ob(object *op, object *where)
{
  	object *tmp, *otmp;

  	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
    	dump_object(op);
    	LOG(llevBug, "BUG: Trying to insert (ob) inserted object.\n%s\n", errmsg);
    	return op;
  	}

  	if (where == NULL)
	{
    	dump_object(op);
    	LOG(llevBug, "BUG: Trying to put object in NULL.\n%s\n", errmsg);
    	return op;
  	}

	if (where->head)
	{
		LOG(llevBug, "BUG: Tried to insert object wrong part of multipart object.\n");
		where = where->head;
	}

  	if (op->more)
	{
    	LOG(llevError, "ERROR: Tried to insert multipart object %s (%d)\n", query_name(op, NULL), op->count);
    	return op;
  	}

  	CLEAR_FLAG(op, FLAG_REMOVED);

  	if (op->nrof)
	{
		for (tmp = where->inv; tmp != NULL; tmp = tmp->below)
		{
			if (CAN_MERGE(tmp, op))
			{
				/* return the original object and remove inserted object
				 * (client needs the original object) */
				tmp->nrof += op->nrof;

				/* Weight handling gets pretty funky.  Since we are adding to
				 * tmp->nrof, we need to increase the weight. */
				add_weight(where, op->weight * op->nrof);

				/* Make sure we get rid of the old object */
				SET_FLAG(op, FLAG_REMOVED);

				op = tmp;
				/* and fix old object's links (we will insert it further down)*/
				remove_ob(op);
				/* Just kidding about previous remove */
				CLEAR_FLAG(op, FLAG_REMOVED);
				break;
			}
		}

		/* I assume stackable objects have no inventory
		 * We add the weight - this object could have just been removed
		 * (if it was possible to merge).  calling remove_ob will subtract
		 * the weight, so we need to add it in again, since we actually do
		 * the linking below */
		add_weight(where, op->weight * op->nrof);
  	}
	else
    	add_weight(where, (op->weight + op->carrying));

	SET_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	op->map = NULL;
	op->env = where;
	op->above = NULL;
	op->below = NULL;
	op->x = 0, op->y = 0;

#ifdef POSITION_DEBUG
  	op->ox = 0, op->oy = 0;
#endif

  	/* Client has no idea of ordering so lets not bother ordering it here.
   	 * It sure simplifies this function... */
  	if (where->inv==NULL)
      	where->inv = op;
  	else
	{
     	op->below = where->inv;
      	op->below->above = op;
      	where->inv = op;
  	}

	/* check for event object and set the owner object
	 * event flags. */
	if (op->type == TYPE_EVENT_OBJECT && op->sub_type1)
		where->event_flags |= (1U << (op->sub_type1 - 1));

	/* if player, adjust one drop items and fix player if not
	 * marked as no fix. */
	otmp = is_player_inv(where);
	if (otmp && CONTR(otmp) != NULL)
	{
		if (QUERY_FLAG(op, FLAG_ONE_DROP))
			SET_FLAG(op, FLAG_STARTEQUIP);

		if (!QUERY_FLAG(otmp, FLAG_NO_FIX_PLAYER))
			fix_player(otmp);
	}

	return op;
}

/* Checks if any objects which has the WALK_ON() (or FLY_ON() if the
 * object is flying) flag set, will be auto-applied by the insertion
 * of the object into the map (applying is instantly done).
 * Any speed-modification due to SLOW_MOVE() of other present objects
 * will affect the speed_left of the object.
 *
 * originator: Player, monster or other object that caused 'op' to be inserted
 * into 'map'.  May be NULL.
 *
 * Return value: 1 if 'op' was destroyed, 0 otherwise.
 *
 * 4-21-95 added code to check if appropriate skill was readied - this will
 * permit faster movement by the player through this terrain. -b.t.
 *
 * MSW 2001-07-08: Check all objects on space, not just those below
 * object being inserted.  insert_ob_in_map may not put new objects
 * on top. */
int check_walk_on(object *op, object *originator, int flags)
{
    object *tmp;
	/* when TRUE, this function is root call for static_walk_semaphore setting */
	int local_walk_semaphore = FALSE;
    tag_t tag;
    int fly;

    if (QUERY_FLAG(op, FLAG_NO_APPLY))
		return 0;

	fly = QUERY_FLAG(op, FLAG_FLYING);

	if (fly)
		flags |= MOVE_APPLY_FLY_ON;
	else
		flags |= MOVE_APPLY_WALK_ON;

    tag = op->count;

	/* This flags ensures we notice when a moving event has appeared!
	 * Because the functions who set/clear the flag can be called recursive
	 * from this function and walk_off() we need a static, global semaphor
	 * like flag to ensure we don't clear the flag except in the mother call. */
	if(!static_walk_semaphore)
	{
		local_walk_semaphore = TRUE;
		static_walk_semaphore = TRUE;
		CLEAR_FLAG(op, FLAG_OBJECT_WAS_MOVED);
	}

    for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		/* Can't apply yourself */
		if (tmp == op)
			continue;

		if (fly ? QUERY_FLAG(tmp, FLAG_FLY_ON) : QUERY_FLAG(tmp, FLAG_WALK_ON))
		{
			/* apply_func must handle multi arch parts....
			 * NOTE: move_apply() can be heavy recursive and recall
			 * this function too.*/
			move_apply_func(tmp, op, originator,flags);

			/* this means we got killed, removed or whatever! */
			if (was_destroyed(op, tag))
			{
				if (local_walk_semaphore)
					static_walk_semaphore = FALSE;

				return CHECK_WALK_DESTROYED;
			}

			/* and here a remove_ob() or insert_xx() was triggered - we MUST stop now */
			if (QUERY_FLAG(op, FLAG_OBJECT_WAS_MOVED))
			{
				if (local_walk_semaphore)
					static_walk_semaphore = FALSE;

				return CHECK_WALK_MOVED;
			}
		}
    }

	if (local_walk_semaphore)
		static_walk_semaphore = FALSE;

	return CHECK_WALK_OK;
}

/* Different to check_walk_on() this must be called explicit and its
 * handles muti arches at once.
 * There are some flags notifiying move_apply() about the kind of event
 * we have. */
int check_walk_off(object *op, object *originator, int flags)
{
	MapSpace *mc;
	object *tmp, *part;
	/* when TRUE, this function is root call for static_walk_semaphore setting */
	int local_walk_semaphore = FALSE;
	int fly;
	tag_t tag;

	/* no map, no walk off - item can be in inventory and/or ... */
	if (!op || !op->map)
		return CHECK_WALK_OK;

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		LOG(llevBug, "BUG: check_walk_off: object %s is not removed when called\n", query_name(op, NULL));
		return CHECK_WALK_OK;
	}

	if (QUERY_FLAG(op, FLAG_NO_APPLY))
		return CHECK_WALK_OK;

    tag = op->count;
	fly = QUERY_FLAG(op, FLAG_FLYING);

	if (fly)
		flags |= MOVE_APPLY_FLY_OFF;
	else
		flags |= MOVE_APPLY_WALK_OFF;

	/* check single and multi arches */
	for (part = op; part; part = part->more)
	{
		mc = GET_MAP_SPACE_PTR(part->map, part->x, part->y);

		 /* no event on this tile */
		if (!(mc->flags & (P_WALK_OFF | P_FLY_OFF)))
			continue;

		/* This flags ensures we notice when a moving event has appeared!
		 * Because the functions who set/clear the flag can be called recursive
		 * from this function and walk_off() we need a static, global semaphor
		 * like flag to ensure we don't clear the flag except in the mother call. */
		if (!static_walk_semaphore)
		{
			local_walk_semaphore = TRUE;
			static_walk_semaphore = TRUE;
			CLEAR_FLAG(op, FLAG_OBJECT_WAS_MOVED);
		}

		/* ok, check objects here... */
	    for (tmp = mc->first; tmp != NULL; tmp = tmp->above)
		{
			/* its the ob part in this space... better not >1 part in same space of same arch */
			if (tmp == part)
				continue;

			/* event */
			if (fly ? QUERY_FLAG(tmp, FLAG_FLY_OFF) : QUERY_FLAG(tmp, FLAG_WALK_OFF))
			{
				move_apply_func(tmp, part, originator, flags);

				if (OBJECT_FREE(part) || tag != op->count)
				{
					if (local_walk_semaphore)
						static_walk_semaphore = FALSE;

					return CHECK_WALK_DESTROYED;
				}

				/* and here a insert_xx() was triggered - we MUST stop now */
				if (!QUERY_FLAG(part, FLAG_REMOVED) || QUERY_FLAG(part, FLAG_OBJECT_WAS_MOVED))
				{
					if (local_walk_semaphore)
						static_walk_semaphore = FALSE;

					return CHECK_WALK_MOVED;
				}
			}
		}

		if (local_walk_semaphore)
		{
			local_walk_semaphore = FALSE;
			static_walk_semaphore = FALSE;
		}

	}

	if (local_walk_semaphore)
		static_walk_semaphore = FALSE;

    return CHECK_WALK_OK;
}

/* present_arch(arch, map, x, y) searches for any objects with
 * a matching archetype at the given map and coordinates.
 * The first matching object is returned, or NULL if none. */
object *present_arch(archetype *at, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevError, "ERROR: Present_arch called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		if (tmp->arch == at)
			return tmp;

	return NULL;
}

/* present(type, map, x, y) searches for any objects with
 * a matching type variable at the given map and coordinates.
 * The first matching object is returned, or NULL if none. */
object *present(unsigned char type, mapstruct *m, int x, int y)
{
	object *tmp;

	if (!(m = out_of_map(m, &x, &y)))
	{
		LOG(llevError, "ERROR: Present called outside map.\n");
		return NULL;
	}

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		if (tmp->type == type)
			return tmp;

	return NULL;
}

/* present_in_ob(type, object) searches for any objects with
 * a matching type variable in the inventory of the given object.
 * The first matching object is returned, or NULL if none. */
object *present_in_ob(unsigned char type, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		if (tmp->type == type)
			return tmp;

	return NULL;
}

/* present_arch_in_ob(archetype, object) searches for any objects with
 * a matching archetype in the inventory of the given object.
 * The first matching object is returned, or NULL if none. */
object *present_arch_in_ob(archetype *at, object *op)
{
	object *tmp;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
		if (tmp->arch == at)
			return tmp;

	return NULL;
}

/* set_cheat(object) sets the cheat flag (WAS_WIZ) in the object and in
 * all it's inventory (recursively).
 * If checksums are used, a player will get set_cheat called for
 * him/her-self and all object carried by a call to this function. */
void set_cheat(object *op)
{
  	object *tmp;
  	SET_FLAG(op, FLAG_WAS_WIZ);

  	if (op->inv)
    	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
      		set_cheat(tmp);
}

/* find_free_spot(archetype, map, x, y, start, stop) will search for
 * a spot at the given map and coordinates which will be able to contain
 * the given archetype.  start and stop specifies how many squares
 * to search (see the freearr_x/y[] definition).
 * It returns a random choice among the alternatives found.
 * start and stop are where to start relative to the free_arr array (1,9
 * does all 4 immediate directions).  This returns the index into the
 * array of the free spot, -1 if no spot available (dir 0 = x,y)
 * Note - this only checks to see if there is space for the head of the
 * object - if it is a multispace object, this should be called for all
 * pieces. */
int find_free_spot(archetype *at, object *op, mapstruct *m, int x, int y, int start, int stop)
{
  	int i, index = 0;
	static int altern[SIZEOFFREE];

	for (i = start; i < stop; i++)
	{
		if (!arch_blocked(at, op, m, x + freearr_x[i], y + freearr_y[i]))
			altern[index++] = i;
		else if (wall(m, x + freearr_x[i], y + freearr_y[i]) && maxfree[i] < stop)
			stop = maxfree[i];
	}

	if (!index)
		return -1;
	return
		altern[RANDOM() % index];
}

/* find_first_free_spot(archetype, mapstruct, x, y) works like
 * find_free_spot(), but it will search max number of squares.
 * But it will return the first available spot, not a random choice.
 * Changed 0.93.2: Have it return -1 if there is no free spot available. */
int find_first_free_spot(archetype *at, mapstruct *m, int x, int y)
{
  	int i;
  	for (i = 0; i < SIZEOFFREE; i++)
	{
    	if (!arch_blocked(at, NULL, m, x + freearr_x[i], y + freearr_y[i]))
      		return i;
  	}

  	return -1;
}

int find_first_free_spot2(archetype *at, mapstruct *m, int x, int y, int start, int range)
{
	int i;
	for (i = start; i < range; i++)
	{
		if (!arch_blocked(at, NULL, m, x + freearr_x[i], y + freearr_y[i]))
			return i;
	}

	return -1;
}

/* find_dir(map, x, y, exclude) will search some close squares in the
 * given map at the given coordinates for live objects.
 * It will not considered the object given as exlude among possible
 * live objects.
 * It returns the direction toward the first/closest live object if finds
 * any, otherwise 0. */
int find_dir(mapstruct *m, int x, int y, object *exclude)
{
	int i, xt, yt, max = SIZEOFFREE;
	mapstruct *mt;
	object *tmp;

	if (exclude && exclude->head)
		exclude = exclude->head;

	for (i = 1; i < max; i++)
	{
		xt = x + freearr_x[i];
		yt = y + freearr_y[i];

		if (wall(m, xt, yt))
			max=maxfree[i];
		else
		{
			if (!(mt = out_of_map(m, &xt, &yt)))
				continue;

			tmp = GET_MAP_OB(mt, xt, yt);

			while (tmp != NULL && ((tmp != NULL && !QUERY_FLAG(tmp, FLAG_MONSTER) && tmp->type != PLAYER) || (tmp == exclude || (tmp->head && tmp->head == exclude))))
				tmp = tmp->above;

			if (tmp != NULL)
				return freedir[i];
		}
	}

	return 0;
}

/* find_dir_2(delta-x,delta-y) will return a direction in which
 * an object which has subtracted the x and y coordinates of another
 * object, needs to travel toward it. */
int find_dir_2(int x, int y)
{
	int q;

	if (!y)
		q= -300 * x;
	else
		q = x * 100 / y;

	if (y > 0)
	{
		if (q < -242)
			return 3;

		if (q < -41)
			return 2;

		if (q < 41)
			return 1;

		if (q < 242)
			return 8;

		return 7;
	}

	if (q < -242)
		return 7;

	if (q < -41)
		return 6;

	if (q < 41)
		return 5;

	if (q < 242)
		return 4;

	return 3;
}

/* absdir(int): Returns a number between 1 and 8, which represent
 * the "absolute" direction of a number (it actually takes care of
 * "overflow" in previous calculations of a direction). */
int absdir(int d)
{
  	while (d < 1)
		d += 8;

  	while (d > 8)
		d -= 8;

  	return d;
}

/* dirdiff(dir1, dir2) returns how many 45-degrees differences there is
 * between two directions (which are expected to be absolute (see absdir()) */
int dirdiff(int dir1, int dir2)
{
	int d;
	d = abs(dir1 - dir2);

	if (d > 4)
		d = 8 - d;

	return d;
}

/* can_pick(picker, item): finds out if an object is possible to be
 * picked up by the picker.  Returnes 1 if it can be
 * picked up, otherwise 0.
 *
 * Cf 0.91.3 - don't let WIZ's pick up anything - will likely cause
 * core dumps if they do.
 *
 * Add a check so we can't pick up invisible objects (0.93.8) */
int can_pick(object *who,object *item)
{
  	return ((who->type == PLAYER && QUERY_FLAG(item, FLAG_NO_PICK) && QUERY_FLAG(item, FLAG_UNPAID)) || (item->weight > 0 && !QUERY_FLAG(item, FLAG_NO_PICK) && (!IS_INVISIBLE(item, who) || QUERY_FLAG(who, FLAG_SEE_INVISIBLE)) && (who->type == PLAYER || item->weight < who->weight / 3)));
}

/* create clone from object to another */
object *ObjectCreateClone(object *asrc)
{
    object *dst = NULL, *tmp, *src, *part, *prev, *item;

    if (!asrc)
		return NULL;

    src = asrc;
    if (src->head)
        src = src->head;

    prev = NULL;
    for (part = src; part; part = part->more)
	{
        tmp = get_object();
        copy_object(part, tmp);
        tmp->x -= src->x;
        tmp->y -= src->y;

        if (!part->head)
		{
            dst = tmp;
            tmp->head = NULL;
        }
		else
            tmp->head = dst;

        tmp->more = NULL;

        if (prev)
            prev->more = tmp;

        prev = tmp;
    }

    /*** copy inventory ***/
    for (item = src->inv; item; item = item->below)
	{
		(void) insert_ob_in_ob(ObjectCreateClone(item), dst);
    }

    return dst;
}

int was_destroyed(object *op, tag_t old_tag)
{
    /* checking for OBJECT_FREE isn't necessary, but makes this function more
     * robust */
    /* Gecko: redefined "destroyed" a little broader: included removed objects.
     * -> need to make sure this is never a problem with temporarily removed objects */
    return (QUERY_FLAG(op, FLAG_REMOVED) || (op->count != old_tag) || OBJECT_FREE(op));
}

/* GROS - Creates an object using a string representing its content.
 * Basically, we save the content of the string to a temp file, then call
 * load_object on it. I admit it is a highly inefficient way to make things,
 * but it was simple to make and allows reusing the load_object function.
 * Remember not to use load_object_str in a time-critical situation.
 * Also remember that multiparts objects are not supported for now. */
object* load_object_str(char *obstr)
{
    object *op;
    FILE *tempfile;
	void *mybuffer;
    char filename[MAX_BUF];

    sprintf(filename, "%s/cfloadobstr2044", settings.tmpdir);
    tempfile = fopen(filename, "w+");
    if (tempfile == NULL)
    {
        LOG(llevError, "ERROR: load_object_str(): Unable to access load object temp file\n");
        return NULL;
    }

    fprintf(tempfile, "%s", obstr);
    op = get_object();
    rewind(tempfile);
	mybuffer = create_loader_buffer(tempfile);
    load_object(tempfile, op, mybuffer, LO_REPEAT, 0);
	delete_loader_buffer(mybuffer);
    LOG(llevDebug, "load str completed, object=%s\n", query_name(op, NULL));
    fclose(tempfile);
    return op;
}

int auto_apply(object *op)
{
	object *tmp = NULL, *tmp2;
	int i, level;

	/* because auto_apply will be done only *one* time
	 * when a new, base map is loaded, we always clear
	 * the flag now. */
	CLEAR_FLAG(op, FLAG_AUTO_APPLY);

	switch (op->type)
	{
		case SHOP_FLOOR:
			if (op->randomitems == NULL)
				return 0;

			do
			{
				/* let's give it 10 tries */
				i = 10;
				level = get_enviroment_level(op);
				while ((tmp = generate_treasure(op->randomitems, level)) == NULL && --i);
					if (tmp == NULL)
						return 0;

				if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
					tmp = NULL;
			} while (!tmp);

			tmp->x = op->x, tmp->y = op->y;
			SET_FLAG(tmp, FLAG_UNPAID);

			/* If this shop floor doesn't have FLAG_CURSED, generate shop-clone items */
			if (!QUERY_FLAG(op, FLAG_CURSED))
			{
				SET_FLAG(tmp, FLAG_NO_PICK);
			}

			insert_ob_in_map(tmp, op->map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
			identify(tmp);

			break;

		case TREASURE:
			level = get_enviroment_level(op);
			create_treasure(op->randomitems, op, op->map ? GT_ENVIRONMENT : 0, level, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

			/* If we generated on object and put it in this object inventory,
			 * move it to the parent object as the current object is about
			 * to disappear.  An example of this item is the random_* stuff
			 * that is put inside other objects.
			 * i fixed this - old part only copied one object instead all. */
			for (tmp = op->inv; tmp; tmp = tmp2)
			{
				tmp2 = tmp->below;
				remove_ob(tmp);

				if (op->env)
					insert_ob_in_ob(tmp, op->env);
			}
			/* no move off needed */
			remove_ob(op);
			break;
	}

	return tmp ? 1 : 0;
}

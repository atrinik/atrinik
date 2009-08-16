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

/*
 * The mapstruct is allocated each time a new map is opened.
 * It contains pointers (very indirectly) to all objects on the map.
 */

#ifndef MAP_H
#define MAP_H

#define MAX_DARKNESS 7 /* number of darkness level. Add +1 for "total dark" */
extern int	global_darkness_table[MAX_DARKNESS+1];

#define MAP_PLAYER_MAP	1    /* for exit objects: this is a player unique map */
#define MAX_ARCH_LAYERS 7	/* thats our 7 logical layers.
* ! for first and last object, we will use 2 more fake layers!
*/
#define MAP_LAYERS		4	/* thats our 4 physical layers we really show */

/* This is when the map will reset */
#define MAP_WHEN_RESET(m)	((m)->reset_time)

#define MAP_RESET_TIMEOUT(m)	((m)->reset_timeout)
#define MAP_DIFFICULTY(m)		((m)->difficulty)
#define MAP_TIMEOUT(m)			((m)->timeout)
#define MAP_SWAP_TIME(m)		((m)->swap_time)
#define MAP_OUTDOORS(m)			((m)->map_flags & MAP_FLAG_OUTDOOR)
#define MAP_UNIQUE(m)			((m)->map_flags & MAP_FLAG_UNIQUE)
#define MAP_FIXED_RESETTIME(m)	((m)->map_flags & MAP_FLAG_FIXED_RTIME)

#define MAP_NOSAVE(m)			((m)->map_flags & MAP_FLAG_NO_SAVE)

#define MAP_NOMAGIC(m)			((m)->map_flags & MAP_FLAG_NOMAGIC)
#define MAP_NOPRIEST(m)			((m)->map_flags & MAP_FLAG_NOPRIEST)
#define MAP_NOHARM(m)			((m)->map_flags & MAP_FLAG_NOHARM)
#define MAP_NOSUMMON(m)			((m)->map_flags & MAP_FLAG_NOSUMMON)
#define MAP_FIXEDLOGIN(m)		((m)->map_flags & MAP_FLAG_FIXED_LOGIN)
#define MAP_PERMDEATH(m)		((m)->map_flags & MAP_FLAG_PERMDEATH)
#define MAP_ULTRADEATH(m)		((m)->map_flags & MAP_FLAG_ULTRADEATH)
#define MAP_ULTIMATEDEATH(m)	((m)->map_flags & MAP_FLAG_ULTIMATEDEATH)
#define MAP_PVP(m)				((m)->map_flags & MAP_FLAG_PVP)
#define MAP_PLUGINS(m)			((m)->map_flags & MAP_FLAG_PLUGINS)

/* mape darkness used to enforce the MAX_DARKNESS value.
 * but IMO, if it is beyond max value, that should be fixed
 * on the map or in the code. */
#define MAP_DARKNESS(m)	   	(m)->darkness

#define MAP_WIDTH(m)		(m)->width
#define MAP_HEIGHT(m)		(m)->height
/* Convenient function - total number of spaces is used
 * in many places. */
#define MAP_SIZE(m)		((m)->width * (m)->height)

#define MAP_ENTER_X(m)		(m)->enter_x
#define MAP_ENTER_Y(m)		(m)->enter_y

/* options passed to ready_map_name and load_original_map */
#define MAP_FLUSH	    	0x1
#define MAP_PLAYER_UNIQUE   0x2
#define MAP_BLOCK	    	0x4
#define MAP_STYLE	    	0x8
#define MAP_ARTIFACT		0x20
#define MAP_NAME_SHARED		0x40 /* indicates that the name string is a shared string */
#define MAP_ORIGINAL		0x80 /* original map. generate treasures */

/* Values for in_memory below.  Should probably be an enumerations */
#define MAP_IN_MEMORY 1
#define MAP_SWAPPED 2
#define MAP_LOADING 3
#define MAP_SAVING 4

/* new macros for map layer system */
#define GET_MAP_SPACE_PTR(M_,X_,Y_)		(&((M_)->spaces[(X_) + (M_)->width * (Y_)]))

#define GET_MAP_SPACE_FIRST(M_)			( (M_)->first )
#define GET_MAP_SPACE_LAST(M_)			( (M_)->last )
#define GET_MAP_SPACE_LAYER(M_,L_)		( (M_)->layer[L_] )
#define GET_MAP_SPACE_CL(M_,L_)			( (M_)->client_mlayer[L_]==-1?NULL:(M_)->layer[(M_)->client_mlayer[L_]])
#define GET_MAP_SPACE_CL_INV(M_,L_)		( (M_)->client_mlayer_inv[L_]==-1?NULL:(M_)->layer[(M_)->client_mlayer_inv[L_]])

#define SET_MAP_SPACE_FIRST(M_,O_)			( (M_)->first = (O_ ))
#define SET_MAP_SPACE_LAST(M_,O_)			( (M_)->last = (O_))
#define SET_MAP_SPACE_LAYER(M_,L_,O_)		( (M_)->layer[L_] = (O_))
#define SET_MAP_SPACE_CLID(M_,L_,O_)			( (M_)->client_mlayer[L_] = (sint8) (O_))
#define SET_MAP_SPACE_CLID_INV(M_,L_,O_)		( (M_)->client_mlayer_inv[L_] = (sint8) (O_))


#define GET_MAP_UPDATE_COUNTER(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].update_tile)

#define INC_MAP_UPDATE_COUNTER(M,X,Y)	((M)->spaces[((X) + (M)->width * (Y))].update_tile++)

#define GET_MAP_MOVE_FLAGS(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].move_flags )
#define SET_MAP_MOVE_FLAGS(M,X,Y,C)	( (M)->spaces[(X) + (M)->width * (Y)].move_flags = C )
#define GET_MAP_FLAGS(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].flags )
#define SET_MAP_FLAGS(M,X,Y,C)	( (M)->spaces[(X) + (M)->width * (Y)].flags = C )
#define GET_MAP_LIGHT(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].light )
#define SET_MAP_LIGHT(M,X,Y,L)	( (M)->spaces[(X) + (M)->width * (Y)].light = (sint8) L )

#define GET_MAP_OB(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].first )
#define GET_MAP_OB_LAST(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].last )
#define GET_MAP_OB_LAYER(_M_,_X_,_Y_,_Z_)	( (_M_)->spaces[(_X_) + (_M_)->width * (_Y_)].layer[_Z_] )
#define get_map_ob	GET_MAP_OB

#define SET_MAP_OB(M,X,Y,tmp)	( (M)->spaces[(X) + (M)->width * (Y)].first = (tmp) )
#define SET_MAP_OB_LAST(M,X,Y,tmp)	( (M)->spaces[(X) + (M)->width * (Y)].last = (tmp) )
#define SET_MAP_OB_LAYER(_M_,_X_,_Y_,_Z_,tmp)	( (_M_)->spaces[(_X_) + (_M_)->width * (_Y_)].layer[_Z_] = (tmp) )
#define set_map_ob	SET_MAP_OB

#define SET_MAP_DAMAGE(M,X,Y,tmp)	( (M)->spaces[(X) + (M)->width * (Y)].last_damage = (uint16) (tmp) )
#define GET_MAP_DAMAGE(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].last_damage )

#define SET_MAP_RTAG(M,X,Y,tmp)	( (M)->spaces[(X) + (M)->width * (Y)].round_tag = (uint32) (tmp) )
#define GET_MAP_RTAG(M,X,Y)	( (M)->spaces[(X) + (M)->width * (Y)].round_tag )

/* These are the 'face flags' we grap out of the flags object structure 1:1.
 * I user a macro to get them from the object, doing a fast AND to mask the bigger
 * object flags to a uint8. I had to change to object flag order for it, but it
 * increase the server->client protocol ALOT - we don't need to collect anything. */
#define FFLAG_SLEEP     0x01        /* object sleeps */
#define FFLAG_CONFUSED  0x02        /* object is confused */
#define FFLAG_PARALYZED 0x04        /* object is paralyzed */
#define FFLAG_SCARED    0x08        /* object is scared - it will run away */
#define FFLAG_BLINDED   0x10        /* object is blinded */
#define FFLAG_INVISIBLE 0x20        /* object is invisible (can seen with "see invisible" on)*/
#define FFLAG_ETHEREAL  0x40        /* object is etheral */

#define FFLAG_PROBE     0x80        /* object is probed !Flag is set by map2 cmd! */


/* You should really know what you are doing before using this - you
 * should almost always be using out_of_map instead, which takes into account
 * map tiling. */
#define OUT_OF_REAL_MAP(M,X,Y) ((X)<0 || (Y)<0 || (X)>=(M)->width || (Y)>=(M)->height)

/* These are used in the MapLook flags element.  They are not used in
 * in the object flags structure. */

#define P_BLOCKSVIEW	0x01
#define P_NO_MAGIC      0x02	/* Spells (some) can't pass this object */
#define P_NO_PASS       0x04	/* Nothing can pass (wall() is true) */
#define P_IS_PLAYER		0x08	/* there is one or more player on this tile */

#define P_IS_ALIVE      0x10	/* something alive is on this space */
#define P_NO_CLERIC     0x20	/* no clerical spells cast here */
#define P_PLAYER_ONLY	0x40   /* Only players are allowed to enter this space. This excludes mobs,
* pets and golems but also spell effects and throwed/fired items.
* it works like a no_pass for players only (pass_thru don't work for it).
			*/
#define P_DOOR_CLOSED   0x80	 /* a closed door is blocking this space - if we want approach, we must first
			* check its possible to open it.
			*/


#define P_CHECK_INV		0x100   /* we have something like inventory checker in this tile node.
			* if set, me must blocked_tile(), to see what happens to us
			*/
#define P_IS_PVP		0x200	/* This is ARENA flag - NOT PvP area flags - area flag is in mapheader */
#define P_PASS_THRU		0x400	/* same as NO_PASS - but objects with PASS_THRU set can cross it.
			* Note: If a node has NO_PASS and P_PASS_THRU set, there are 2 objects
			* in the node, one with pass_thru and one with real no_pass - then
			* no_pass will overrule pass_thru
			*/

#define P_MAGIC_EAR		0x800   /* we have a magic ear in this map tile... later we should add a map
			* pointer where we attach as chained list this stuff - no search
			* or flags then needed.
			*/
#define P_WALK_ON		0x1000	/* this 4 flags are for moving objects and what happens when they enter */
#define P_WALK_OFF		0x2000  /* or leave a map tile */
#define P_FLY_OFF		0x4000
#define P_FLY_ON		0x8000

#define P_REFL_SPELLS	0x10000  /* something on the tile reflect spells */
#define P_REFL_MISSILE	0x20000 /* something on the tile reflect missiles */


#define P_OUT_OF_MAP	0x4000000 /* of course not set for map tiles but from blocked_xx()
			* function where the out_of_map() fails to grap a valid
			* map or tile.
			*/
			/* these are special flags to control how and what the update_position()
			* functions updates the map space. */
#define P_FLAGS_ONLY	0x8000000	/* skip the layer update, do flags only */
#define P_FLAGS_UPDATE	0x10000000	/* if set, update the flags by looping the map objects */
#define P_NEED_UPDATE	0x20000000	/* resort the layer when updating */
#define P_NO_ERROR      0x40000000	/* Purely temporary - if set, update_position
			* does not complain if the flags are different. */

#define P_NO_TERRAIN    0x80000000 /* DON'T USE THIS WITH SET_MAP_FLAGS... this is just to mark for return
			* values of blocked...
			*/

#ifdef WIN32
#pragma pack(push,1)
#endif

			typedef struct MapSpace_s
		{
			/* start of the objects in this map tile */
			object  *first;

			/* array of visible layer objects + for invisible (*2)*/
			object	*layer[MAX_ARCH_LAYERS * 2];

			/* last object in this list */
			object  *last;

			/* used to create chained light source list.*/
			struct MapSpace_s *prev_light;

			struct MapSpace_s *next_light;

			/* tag for last_damage */
			uint32  round_tag;

			/* counter for update tile */
			uint32  update_tile;

			/* light source counter - as higher as brighter light source here */
			sint32	light_source;

			/* how much light is in this tile. 0 = total dark
			* 255+ = full daylight. */
			sint32  light_value;

			/* flags about this space (see the P_ values above) */
			int		flags;

			/* last_damage tmp backbuffer */
			uint16  last_damage;

			/* terrain type flags (water, underwater,...) */
			uint16  move_flags;

			/* index for layer[] - this will send to player */
			sint8	client_mlayer[MAP_LAYERS];

			/* same for invisible objects */
			sint8	client_mlayer_inv[MAP_LAYERS];

			/* How much light this space provides */
			uint8	light;
		} MapSpace;

#ifdef WIN32
#pragma pack(pop)
#endif

			/* map flags for global map settings - used in ->map_flags */
#define MAP_FLAG_NOTHING			0
#define MAP_FLAG_OUTDOOR			1		/* map is outdoor map - daytime effects are on */
#define MAP_FLAG_UNIQUE				2		/* special unique map - see docs */
#define MAP_FLAG_FIXED_RTIME		4		/* if true, reset time is not affected by
			* players entering/exiting map
			*/
#define MAP_FLAG_NOMAGIC			8		/* no sp based spells */
#define MAP_FLAG_NOPRIEST			16		/* no grace baes spells allowed */
#define MAP_FLAG_NOHARM				32		/* allow only no attack, no debuff spells
			* this is city default setting - heal for example
			* is allowed on you and others but no curse or
			* fireball or abusing stuff like darkness or create walls
			*/
#define MAP_FLAG_NOSUMMON			64		/* don't allow any summon/pet summon spell.
			* this includes "call summons" for calling pets from other maps
			*/
#define MAP_FLAG_FIXED_LOGIN		128		/* when set, a player login on this map will forced
			* to default enter_x/enter_y of this map.
			* this avoid map stucking and treasure camping
			*/
#define MAP_FLAG_PERMDEATH			256		/* this map is a perm death. */
#define MAP_FLAG_ULTRADEATH			1024	/* this map is a ultra death map */
#define MAP_FLAG_ULTIMATEDEATH		2048	/* this map is a ultimate death map */
#define MAP_FLAG_PVP				4096	/* PvP is possible on this map */
#define MAP_FLAG_NO_SAVE			8192	/* don't save maps - atm only used with unique maps */
#define MAP_FLAG_PLUGINS			16384   /* Call plugin map events for this map */


#define SET_MAP_TILE_VISITED(m, x, y, id) { \
    if((m)->pathfinding_id != (id)) { \
        (m)->pathfinding_id = (id); \
        memset((m)->bitmap, 0, ((MAP_WIDTH(m)+31)/32) * MAP_HEIGHT(m) * sizeof(uint32)); } \
    (m)->bitmap[(x)/32 + ((MAP_WIDTH(m)+31)/32)*(y)] |= (1U << ((x) % 32)); }
#define QUERY_MAP_TILE_VISITED(m, x, y, id) \
    ((m)->pathfinding_id == (id) && ((m)->bitmap[(x)/32 + ((MAP_WIDTH(m)+31)/32)*(y)] & (1U << ((x) % 32))))

			/* In general, code should always use the macros
			* above (or functions in map.c) to access many of the
			* values in the map structure.  Failure to do this will
			* almost certainly break various features.  You may think
			* it is safe to look at width and height values directly
			* (or even through the macros), but doing so will completely
			* break map tiling. */
			typedef struct mapdef
		{
			/* Next map, linked list */
			struct mapdef *next;

			/* Name of map as given by its creator */
			char *name;

			/* Background music of the map */
			char *bg_music;

			/* Name of temporary file */
			char *tmpname;

			/* Message map creator may have left */
			char *msg;

			/* The following two are used by the pathfinder algorithm in pathfinder.c */

			/* Bitmap used for marking visited tiles in pathfinding */
			uint32 *bitmap;

			/* For which traversal is the above valid */
			uint32 pathfinding_id;

			/* Array of spaces on this map */
			MapSpace *spaces;

			/* list of tiles spaces with light sources in */
			MapSpace *first_light;

			/* Linked list of linked lists of buttons */
			oblinkpt *buttons;

			/* Filename of the map (shared string now) */
			const char *path;

			/* path to adjoining maps (shared strings) */
			const char *tile_path[TILED_MAPS];

			/* Next map, linked list */
			struct mapdef *tile_map[TILED_MAPS];

			/* chained list of player on this map */
			object *player_first;

			/* indicates the base light value in this map.
			* this value is only used when the map is not marked
			* as outdoor. */
			int darkness;

			/* the real light_value, build out from darkness and
			* possible other factors. */
			int light_value;

			/* mag flags for various map settings */
			uint32 map_flags;

			/* when this map should reset */
			uint32 reset_time;

			/* How many seconds must elapse before this map
			* should be reset */
			uint32 reset_timeout;

			/* to identify maps for fixed_login */
			uint32 map_tag;

			/* swapout is set to this */
			sint32 timeout;

			/* When it reaches 0, the map will be swapped out */
			sint32 swap_time;

			/* If not true, the map has been freed and must
			* be loaded before used.  The map,omap and map_ob
			* arrays will be allocated when the map is loaded */
			uint32 in_memory;

			/* Used by relative_tile_position() to mark visited maps */
			uint32 traversed;

			/* This is a counter - used for example from NPC's which have
			* a global function. If this counter is != 0, map will not swap
			* and the npc/object with perm_load flag will stay in game. */
			int perm_load;

			/* What level the player should be to play here */
			int difficulty;

			/* Width and height of map. */
			int height;

			int width;

			/* enter_x and enter_y are default entrance location */
			int enter_x;

			/* on the map if none are set in the exit */
			int enter_y;

			/* Compression method used */
			int compressed;
		} mapstruct;

		/* This is used by get_rangevector to determine where the other
		 * creature is.  get_rangevector takes into account map tiling,
		 * so you just can not look the the map coordinates and get the
		 * righte value.  distance_x/y are distance away, which
		 * can be negativbe.  direction is the crossfire direction scheme
		 * that the creature should head.  part is the part of the
		 * monster that is closest.
		 * Note: distance should be always >=0. I changed it to UINT. MT */
		typedef struct rv_vector_s
		{
			unsigned int distance;

			int	distance_x;

			int	distance_y;

			int	direction;

			object *part;
		} rv_vector;

		/* constants for the flags for get_rangevector() and get_rangevector_from_mapcoords() */
#define RV_IGNORE_MULTIPART   	0x01
#define RV_RECURSIVE_SEARCH    	0x02

#define RV_MANHATTAN_DISTANCE 	0x00
#define RV_EUCLIDIAN_DISTANCE 	0x04
#define RV_DIAGONAL_DISTANCE  	0x08
#define RV_NO_DISTANCE        	(0x08 | 0x04)

		extern int map_tiled_reverse[TILED_MAPS];

#endif

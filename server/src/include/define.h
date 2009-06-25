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
* the Free Software Foundation; either version 3 of the License, or     *
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

/* This file is really too large.  With all the .h files
 * around, this file should be better split between them - things
 * that deal with objects should be in objects.h, things dealing
 * with players in player.h, etc.  As it is, everything just seems
 * to be dumped in here.
 */

#ifndef DEFINE_H
#define DEFINE_H

/*
 * Crossfire requires ANSI-C, but some compilers "forget" to define it.
 * Thus the prototypes made by cextract don't get included correctly.
 */
#if !defined(__STDC__)
/* Removed # from start of following line.  makedepend was picking it up.
 * The following should still hopefully result in an error.
 */
error - Your ANSI C compiler should be defining __STDC__;
#endif

#ifndef WIN32 /* ---win32 exclude unix configuration part */
#include <autoconf.h>
#endif

#define FONTDIR ""
#define FONTNAME ""

/* Decstations have trouble with fabs()... */
#define FABS(x) ((x)<0?-(x):(x))
#define abs(x) ((x)<0?-(x):(x))

#ifdef __NetBSD__
#include <sys/param.h>
#endif
#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#define MAX_STAT		30	/* The maximum legal value of any stat */
#define MIN_STAT		1	/* The minimum legal value of any stat */

/* Used for all kinds of things */
#define MAX_BUF				256
#define VERY_BIG_BUF		1024
/* Used for messages - some can be quite long */
#define HUGE_BUF			4096
/* Database buf - this must be quite big,
 * because we store all kind of stuff in database. */
#define DB_BUF				1097152
/* Max length a packet could be */
#define SOCKET_BUFLEN		4096

#define FONTSIZE		3000	/* Max chars in font */

#define MAX_ANIMATIONS		256

#define MAX_NAME 16
#define BIG_NAME 32
#define MAX_EXT_TITLE 98

/* modes for cast_identify() */
#define IDENTIFY_MODE_NORMAL 0
#define IDENTIFY_MODE_ALL 1
#define IDENTIFY_MODE_MARKED 2

/* spell list commands for client spell list */
#define SPLIST_MODE_ADD    0
#define SPLIST_MODE_REMOVE 1
#define SPLIST_MODE_UPDATE 2

/* thats for golem control function send to client */
#define GOLEM_CTR_RELEASE  0
#define GOLEM_CTR_ADD	   1

/* this marks no skill used */
#define CHOSEN_SKILL_NO (99999)

#define PORTAL_DESTINATION_NAME "Town portal destination"

/* LOS (loc.c) defines */
#define BLOCKED_LOS_VISIBLE		0x00	/* its visible */
#define BLOCKED_LOS_IGNORE		0x01	/* ignore this tile for blocksview/visible changes! */
#define BLOCKED_LOS_BLOCKSVIEW	0x02	/* visible but will block all behind */
#define BLOCKED_LOS_BLOCKED		0x04	/* sight is blocked */
#define BLOCKED_LOS_OUT_OF_MAP	0x08	/* tile is not visible because not part of legal map */

/* TYPE DEFINES */
/* Only add new values to this list if somewhere in the program code,
 * it is actually needed.  Just because you add a new monster does not
 * mean it has to have a type defined here.  That only needs to happen
 * if in some .c file, it needs to do certain special actions based on
 * the monster type, that can not be handled by any of the numerous
 * flags
 * Also, if you add new entries, try and fill up the holes in this list.
 */

/* type 0 will be undefined and shows a non valid type information */

#define PLAYER		            1
#define BULLET		            2
#define ROD		                3
#define TREASURE	            4
#define POTION		            5
#define FOOD		            6
#define POISON		            7
#define BOOK		            8
#define CLOCK		            9
#define FBULLET		            10
#define FBALL		            11
#define LIGHTNING	            12
#define ARROW		            13
#define BOW		                14
#define WEAPON		            15
#define ARMOUR		            16
#define PEDESTAL	            17
#define ALTAR		            18
#define CONFUSION	            19
#define LOCKED_DOOR	            20
#define SPECIAL_KEY	            21
#define MAP		                22
#define DOOR		            23
#define KEY		                24
#define MMISSILE	            25
#define TIMED_GATE	            26
#define TRIGGER		            27
#define GRIMREAPER	            28
#define MAGIC_EAR	            29
#define TRIGGER_BUTTON	        30
#define TRIGGER_ALTAR	        31
#define TRIGGER_PEDESTAL        32
#define SHIELD		            33
#define HELMET		            34
#define HORN		            35
#define MONEY		            36
#define CLASS                   37  /* object for applying character class modifications to someone */
#define GRAVESTONE	            38
#define AMULET		            39
#define PLAYERMOVER	            40
#define TELEPORTER	            41
#define CREATOR		            42
#define SKILL		            43	/* Skills are similar to abilites, but
				                     * not related to spells.  by njw@cs.city.ac.u */
#define EXPERIENCE	            44	/* An experience 'object'. Needed for multi-exp/skills
				                     * hack. -b.t. thomas@astro.psu.edu */
#define EARTHWALL	            45
#define GOLEM		            46
#define BOMB		            47
#define THROWN_OBJ	            48
#define BLINDNESS	            49
#define GOD		                50

/*  peterm:  detector is an object which notices the presense of
	 another object and is triggered like buttons.  */
#define DETECTOR	            51
#define SPEEDBALL	            52
#define DEAD_OBJECT	            53
#define DRINK		            54
#define MARKER                  55 /* inserts an invisible, weightless
			                        force into a player with a specified string. */
#define HOLY_ALTAR	            56
#define PLAYER_CHANGER          57

/* warning - don't use battleground! i will integrate this as map flag
 * bound to normal floor. The extended map flags of daimonin will allow
 * to remove this stuff.
 */
#define BATTLEGROUND            58      /* battleground, by Andreas Vogl */

#define PEACEMAKER              59  /* Object owned by a player which can convert
			                           a monster into a peaceful being incapable of attack.  */
#define GEM		                60
#define FIRECHEST	            61
#define FIREWALL	            62
#define ANVIL                   63
#define CHECK_INV	            64	/* by b.t. thomas@nomad.astro.psu.edu */
#define MOOD_FLOOR	            65	/* by b.t. thomas@nomad.astro.psu.edu
				                     * values of last_sp set how to change:
				                     * 0 = furious,	all monsters become aggressive
				                     * 1 = angry, all but friendly become aggressive
				                     * 2 = calm, all aggressive monsters calm down
				                     * 3 = sleep, all monsters fall asleep
				                     * 4 = charm, monsters become pets */
#define EXIT		            66
#define TYPE_AGE_FORCE			67	/* this object is a age force */
#define SHOP_FLOOR	            68
#define SHOP_MAT	            69
#define RING		            70

#define FLOOR                   71 /* this is a floor tile -> native layer 0 */

#define FLESH		            72	/* animal 'body parts' -b.t. */
#define INORGANIC	            73	/* metals and minerals */

#define TYPE_LIGHT_APPLY        74  /* new light source for player */

#define LIGHTER		            75
#define TRAP_PART	            76	/* Needed by set traps skill -b.t. */

#define WALL                    77 /* this is a wall. put it always in layer 1 if not set is_floor */
#define LIGHT_SOURCE            78 /* thats now light sources on the map (invisible lights) */

#define MISC_OBJECT             79 /* misc. objects are for objects without a function
                                      in the engine. Like statues, clocks, chairs,...
                                      If perhaps we create a function where we can sit
                                      on chairs, we create a new type and remove all
                                      chairs from here. */
#define MONSTER                 80 /* yes, thats a real, living creature */
#define SPAWN_POINT             81 /* a spawn point or monster generator object */
#define TYPE_LIGHT_REFILL       82  /* refilling item for TYPE_LIGHT_APPLY */

#define SPAWN_POINT_MOB			83 /* inactive default mob inside spawn point inv.
									* This object is somewhat special because its generated
									* ITS IS ONLY A COPY AND NOT A REAL MONSTER NOR A ACTIVE OBJECT.
									*/
#define SPAWN_POINT_INFO		84	/* this is put inside a mob, created from a spawn point.
									 * It is used to track back the mob to the spawn point.
									 */
#define SPELLBOOK	            85

#define ORGANIC					86 /* body parts which can't be eaten - dragon scales for example */

#define CLOAK		            87
#define CONE		            88
#define AURA                    89  /* aura spell object */

#define SPINNER		            90
#define GATE		            91
#define BUTTON		            92
#define CF_HANDLE		        93
#define PIT		                94 /* PIT are called HOLE in crossfire) - normal hole is type EXIT */
#define TRAPDOOR	            95
#define WORD_OF_RECALL	        96
#define PARAIMAGE	            97
#define SIGN		            98
#define BOOTS		            99
#define GLOVES		            100
#define TYPE_BASE_INFO			101	/* This object holds the real base stats of mobs or other active objects */
#define TYPE_RANDOM_DROP		102	/* only true for spawn points atm: if found, there is a random check against
									 * carrying. If greater as carrying - all ->inv is droped.
									 */
#define CONVERTER	            103
#define BRACERS		            104
#define POISONING	            105 /* thats the poison force... if a player applies for example
									 * a POISON object (poison food), this force is inserted in
									 * the player and does the poison effect until it wear out
									 * or get removed
									 */
#define SAVEBED		            106
#define POISONCLOUD	            107
#define FIREHOLES	            108
#define WAND		            109
#define ABILITY		            110
#define SCROLL		            111
#define DIRECTOR	            112
#define GIRDLE		            113
#define FORCE		            114
#define POTION_EFFECT           115    /* a force, holding the effect of a potion */
#define TYPE_JEWEL				116	   /* to make it different from GEM - thats needed to have a better */
#define TYPE_NUGGET				117    /* use from the artifacts file */
#define TYPE_EVENT_OBJECT		118	   /* event/script object */
#define TYPE_WAYPOINT_OBJECT	119	   /* waypoint object */
#define TYPE_QUEST_CONTAINER	120	   /* used to store quest infos in players */
#define CLOSE_CON	            121    /* Eneq(@csd.uu.se): Id for close_container archetype. */
#define CONTAINER	            122
#define ARMOUR_IMPROVER         123
#define WEAPON_IMPROVER         124

#define TYPE_WEALTH				125		/* this is a "raw" (abstract) wealth object. When generated
									     * its tranformed in real money depending on the enviroment
										 * where its generated. (most times by treasure list and
										 * tranformed to money by using level of mob or map to generating
										 * a fitting amount of money basing on the base setting).
										 */
/* unused: 126 - 129
 * type 125 was MONEY_CHANGER
 */
#define SKILLSCROLL	            130	/* can add a skill to player's inventory -bt.*/
#define DEEP_SWAMP	            138
#define IDENTIFY_ALTAR	        139
#define CANCELLATION	        141
#define MENU		            150 /* Mark Wedel (mark@pyramid.com) Shop inventories */
#define BALL_LIGHTNING          151 /* peterm:  ball lightning and color spray */
#define SWARM_SPELL             153
#define RUNE                    154

#define POWER_CRYSTAL           156
#define CORPSE                  157

#define DISEASE                 158
#define SYMPTOM                 159

#define TYPE_VOID_CONTAINER		255 /* pure internal system object */
/* END TYPE DEFINE */

/* START SUB TYPE 1 DEFINE */
/* SPECIAL FLAGS */
/* because we use now a complexer, client controled fire & throw system,
 * we must mark different types/sub_types items for throwing like potions,
 * dust, weapons, special ammun, etc.
 */

#define ST1_MISSILE_THROW 128
/* These are very special sub_types - used by different types.
 * The reason is, that different items can be missiles - amuns & throw items.
 * First, we have the amun types - arrows for bows, or bolds for xbows.
 * They need a firing weapon and get fired indirekt. Daimonin don't allow
 * to throw amun - its really senseless to try to throw a bolt and hope
 * to do any serious damage with it.
 * For throwing we have some more types.
 * First, we have the 'real' throw weapons - like darts or shurikens.
 * Second, there are weapons which can be used as hand weapons but
 * also as throw weapons. A hammer like mjoellnir or a simple spear, for example.
 * Third, there are special throw items. ATM, we have
 * Potions - like firestorm
 * Dust - like dust of blinding... etc.
 * Dusts are also from ' type POTION' but use a different base arch.
 * These special items will handled different - we allow the player to throws
 * ANY dust or potion. But only when the potion/dust is from sub_type _missile_xx
 * we include the skill id and only these items will do a range effect.
 * Every potion/dust will have a level (like a spell or rod). If this level is
 * 40% higher as our throw skill (and more as 5 level higher), then throwing
 * will fail, like we try to use a to high item device.
 * If we apply a throw potion/dust, the effect will triggered (a firestorm potion
 * will explode) but no skill id will be inserted in the firestorm objects.
 * So, the player and perhaps some mobs will take damage - but it will not give
 * any exp when this will happen. MNT-11-2002
 * have not included this 40% throw thing - perhaps later. MT-2003
 */
#define ST1_MISSILE_BOW     1 /* ammunition for bows = arrows */
#define ST1_MISSILE_CBOW    2 /* bolts */
#define ST1_MISSILE_SSTONE  3 /* sling stones */
 /* these are our special throw weapons - they are called AMMUNITION too */
#define ST1_MISSILE_IMPACT	(ST1_MISSILE_THROW | 0)  /* impact */
#define ST1_MISSILE_SLASH	(ST1_MISSILE_THROW | 1)  /* slash */
#define ST1_MISSILE_PIERCE	(ST1_MISSILE_THROW | 2)  /* pierce */
#define ST1_MISSILE_CLEAVE	(ST1_MISSILE_THROW | 3)  /* cleave */
#define ST1_MISSILE_BOULDER	(ST1_MISSILE_THROW | 4)  /* special case - impact damage too */
                                                     /* boulders get picked by giants and thrown by them */

/* some fancy macros to determintate the kidn of hand weapon */
#define WEAPON_IS_1H 0
#define WEAPON_IS_2H 1
#define WEAPON_IS_POLE 2

/* definitions for weapontypes */
/* one hand weapons - allows shields in second hand */
#define WEAP_1H_IMPACT	0  /* impact damage: clubs, maces, hammers, etc. */
#define WEAP_1H_SLASH	1  /* slash: swords, scimitar */
#define WEAP_1H_PIERCE	2  /* pierce: daggers, rapier */
#define WEAP_1H_CLEAVE	3  /* cleave: axes */

/* two-hand weapons. you need base 1h skill + two-hand mastery for use */
/* exp goes always in 1h skill - mastery skills are indirect skills */
#define WEAP_2H_IMPACT	4  /* impact damage: clubs, maces, hammers, etc. */
#define WEAP_2H_SLASH	5  /* slash */
#define WEAP_2H_PIERCE	6  /* pierce */
#define WEAP_2H_CLEAVE	7  /* cleave */

/* same like 2h but for polearms */
#define WEAP_POLE_IMPACT	8  /* impact damage: clubs, maces, hammers, etc. */
#define WEAP_POLE_SLASH	    9  /* slash -swords */
#define WEAP_POLE_PIERCE	10  /* pierce: rapier */
#define WEAP_POLE_CLEAVE	11  /* cleave: axes */

#define RANGE_WEAP_BOW	    0  /* range weapons - bows */
#define RANGE_WEAP_XBOWS	1  /* crossbows */
#define RANGE_WEAP_SLINGS	2  /* slings */
#define RANGE_WEAP_FIREARMS	3  /* firearms - not implemented */

/* some skills are auto-used, some should not be able to use with fire xxx (use_skill) */
#define ST1_SKILL_NORMAL    0   /* normal skill but not direct usable with use_skill() */
#define ST1_SKILL_USE       1   /* skill can be used with fire and use_skill() */
#define ST1_SKILL_INDIRECT  2   /* skill is used indirect by server */

/* spellbooks can be different types: mages, cleric and so on.
 * to remove identification from arch name, i use ST1 for it MT
 */
#define ST1_SPELLBOOK_CLERIC 1
#define ST1_SPELLBOOK_MAGE	 2

/* container can be different items: normal containers, mob corpse or player corpse.
 * all containers sub_type >=64 are "personlized" - in the slaying field of this containers
 * is not the key but the name of the player which can as only person access the container.
 * is the container sub_type >= 128 then its a group container - the group name will open/close it.
 * if the container sub_type >=192 then the clan name will open it.
 */

#define ST1_CONTAINER_NORMAL				0
#define ST1_CONTAINER_CORPSE				1
#define ST1_CONTAINER_DEAD_PL				2

#define ST1_CONTAINER_NORMAL_player			64
#define ST1_CONTAINER_CORPSE_player			65

#define ST1_CONTAINER_NORMAL_party			128
#define ST1_CONTAINER_CORPSE_party			129

#define ST1_CONTAINER_NORMAL_clan			192
#define ST1_CONTAINER_CORPSE_clan			193

/* sub type for EXIT objects - if set, a teleport sound is played */
#define ST1_EXIT_SOUND_NO					0
#define ST1_EXIT_SOUND						1

/* sub types for doors */
#define ST1_DOOR_NORMAL 0
#define ST1_DOOR_CURTAIN 1 /* make different sound */

/* END SUB TYPE 1 DEFINE */

/* definitions for detailed pickup descriptions.
 *   The objective is to define intelligent groups of items that the
 *   user can pick up or leave as he likes. */

/* high bit as flag for new pickup options */
#define PU_NOTHING		0x00000000

#define PU_DEBUG		0x10000000
#define PU_INHIBIT		0x20000000
#define PU_STOP			0x40000000
#define PU_NEWMODE		0x80000000

#define PU_RATIO		0x0000000F

#define PU_FOOD			0x00000010
#define PU_DRINK		0x00000020
#define PU_VALUABLES		0x00000040
#define PU_BOW			0x00000080

#define PU_ARROW		0x00000100
#define PU_HELMET		0x00000200
#define PU_SHIELD		0x00000400
#define PU_ARMOUR		0x00000800

#define PU_BOOTS		0x00001000
#define PU_GLOVES		0x00002000
#define PU_CLOAK		0x00004000
#define PU_KEY			0x00008000

#define PU_MISSILEWEAPON	0x00010000
#define PU_ALLWEAPON		0x00020000
#define PU_MAGICAL		0x00040000
#define PU_POTION		0x00080000


/* Instead of using arbitrary constants for indexing the
 * freearr, add these values.  <= SIZEOFFREE1 will get you
 * within 1 space.  <= SIZEOFFREE2 wll get you withing
 * 2 spaces, and the entire array (<= SIZEOFFREE) is
 * three spaces
 */
#define SIZEOFFREE1 8
#define SIZEOFFREE2 24
#define SIZEOFFREE 49

#define PATH_NULL	0x00000000      /* 0 */
#define PATH_PROT	0x00000001      /* 1 */
#define PATH_FIRE	0x00000002      /* 2 */
#define PATH_FROST	0x00000004   /* 4 */
#define PATH_ELEC	0x00000008      /* 8 */
#define PATH_MISSILE	0x00000010   /* 16 */
#define PATH_SELF	0x00000020      /* 32 */
#define PATH_SUMMON	0x00000040   /* 64 */
#define PATH_ABJURE	0x00000080  /* 128 */
#define PATH_RESTORE	0x00000100  /* 256 */
#define PATH_DETONATE	0x00000200  /* 512 */
#define PATH_MIND	0x00000400    /* 1024 */
#define PATH_CREATE	0x00000800   /* 2048 */
#define PATH_TELE	0x00001000     /* 4096 */
#define PATH_INFO	0x00002000     /* 8192 */
#define PATH_TRANSMUTE	0x00004000   /* 16384 */
#define PATH_TRANSFER	0x00008000  /*  32768 */
#define PATH_TURNING	0x00010000  /* 65536 */
#define PATH_WOUNDING	0x00020000  /* 131072 */
#define PATH_DEATH	0x00040000  /* 262144 */
#define PATH_LIGHT	0x00080000  /* 524288 */

#define NRSPELLPATHS	20

#define NROFREALSPELLS	20	/* Number of different spells */

/* Terrain type flags
 * These values are used from terrain_typ and terrain_flag
 * Arches without terrain flags become AIRBREATH as default.
 * These values also define the enviroment of the map tile position
 */

#define TERRAIN_NOTHING         0
#define TERRAIN_AIRBREATH       1   /* thats default - walk on earth */
#define TERRAIN_WATERWALK       2   /* walk on water - deep water too */
#define TERRAIN_WATERBREATH     4   /* you can move underwater as on land */
#define TERRAIN_FIREWALK        8   /* walk over lava or fire */
#define TERRAIN_FIREBREATH      16  /* you can move in fire environment (fire elemental dimension, hell,..) */
#define TERRAIN_CLOUDWALK       32  /* move "on clouds" in the air - is not flying. Similiar to the story of the magic bean*/

/* Flag structure now changed.
 * Each flag is now a bit offset, starting at zero.  The macros
 * will update/read the appropriate flag element in the object
 * structure.
 *
 * Hopefully, since these offsets are integer constants set at run time,
 * the compiler will reduce the macros something as simple as the
 * old system was.
 *
 * Flags now have FLAG as the prefix.  This to be clearer, and also
 * to make sure F_ names are not still being used anyplace.
 *
 * The macros below assume that the flag size for each element is 32
 * bits.  IF it is smaller, bad things will happen.  See structs.h
 * for more info.
 *
 * All functions should use the macros below.  In process of converting
 * to the new system, I find several files that did not use the previous
 * macros.
 *
 * If any FLAG's are added, be sure to add them to the flag_links structure
 * in common/loader.c, if necessary.
 *
 * flags[0] is 0 to 31
 * flags[1] is 32 to 63
 * flags[2] is 64 to 95
 * flags[3] is 96 to 127
 */
/* Basic routines to do above */
#define SET_FLAG(xyz, p) \
	((xyz)->flags[p/32] |= (1U << (p % 32)))
#define CLEAR_FLAG(xyz, p) \
	((xyz)->flags[p/32] &= ~(1U << (p % 32)))
#define QUERY_FLAG(xyz, p) \
	((xyz)->flags[p/32] & (1U << (p % 32)))

/* this is rarely used but needed for some flags, which are
 * used for intern handling like INVISIBLE or WALK_OFF. Because
 * some core functions like remove_ob() use this, it will be better
 * we set this ONE time outside instead of every time in remove_ob():
 * we skip the call for the head in this way.
 */
#define SET_MULTI_FLAG(xyz, p) \
	{object * _tos_;for(_tos_=xyz;_tos_;_tos_=_tos_->more) ((_tos_)->flags[p/32] |= (1U << (p % 32)));}
#define CLEAR_MULTI_FLAG(xyz, p) \
	{object * _tos_;for(_tos_=xyz;_tos_;_tos_=_tos_->more) ((_tos_)->flags[p/32] &= ~(1U << (p % 32)));}

/* convenience macros to determine what kind of things we are dealing with */

#define IS_WEAPON(op) \
	(op->type == ARROW || op->type == BOW || op->type == WEAPON)

#define IS_ARMOR(op) \
	(op->type == ARMOUR || op->type == SHIELD || op->type == HELMET || \
	 op->type == CLOAK || op->type == BOOTS || op->type == GLOVES || \
	 op->type == BRACERS || op->type == GIRDLE)

#define IS_LIVE(__op__) ( (__op__)->type == PLAYER || QUERY_FLAG((__op__),FLAG_MONSTER) || \
						 (QUERY_FLAG((__op__), FLAG_ALIVE) && !QUERY_FLAG((__op__), FLAG_GENERATOR)))

#define IS_ARROW(op) \
	(op->type==ARROW || op->type==MMISSILE || op->type==BULLET)

/* the flags */

/* used in blocked() when we only want know about blocked by something */
#define TERRAIN_ALL		0xffff

/* NOTE: you MUST set the FLAG_xx to V_xxx array in loader.l too when
 * you change something here! Search for NUM_FLAGS in loader.l for more.
 */
/* WARNING: The first 8 bit are used from the map2 cmd as direct mapped data.
 * The order must stay as it is here!
 */
#define FLAG_SLEEP			0 /* NPC is sleeping */
#define FLAG_CONFUSED		1 /* confused... random dir when moving and problems to do actions */
#define FLAG_PARALYZED      2 /* Object is paralyzed */
#define FLAG_SCARED			3 /* Monster is scared (for players: this is feared) */
#define FLAG_BLIND			4 /* If set, object cannot see (the map) with eyes */
#define FLAG_IS_INVISIBLE	5 /* only THIS invisible can be seen with seen_invisible */
#define FLAG_IS_ETHEREAL	6 /* object is etheral  - means transparent and special protected */
#define FLAG_IS_GOOD		7 /* NOT USED from map2. alignment flag */

#define FLAG_NO_PICK	 	8 /* Object can't be picked up */
#define FLAG_WALK_ON	 	9 /* Applied when it's walked upon */
#define FLAG_NO_PASS		10 /* Nothing can pass (wall() is true) */
#define FLAG_ANIMATE		11 /* The object looks at archetype for faces */
#define FLAG_SLOW_MOVE		12 /* Uses the stats.exp/1000 to slow down */
                               /* I use this flag now in 2 ways: for objects like floors
							    * it force objects to move slow. For monsters with speed,
								* it force monsters to move slow! (slow spell or snared!)
								*/
#define FLAG_FLYING			13 /* Not affected by WALK_ON or SLOW_MOVE) */
#define FLAG_MONSTER		14 /* A object with this flag is used like a object with
                                * type == MONSTER. SO, we can use type GOLEMS objects
								* for example in attack functions like MONSTER without
								* checking all possible different type defines.
								*/
#define FLAG_FRIENDLY		15 /* Will help players */
							   /*
                                *  REMOVED and BEEN_APPLIED are direct used from CAN_MERGE - change it too when
								* you move this flag!
								*/
#define FLAG_REMOVED	 	16 /* Object is not in any map or invenory */
#define FLAG_BEEN_APPLIED	17 /* The object has been applied */
#define FLAG_AUTO_APPLY		18 /* Will be applied when created */
#define FLAG_TREASURE		19 /* Will generate treasure when applied */
#define FLAG_IS_NEUTRAL		20 /* alignment of this object: we need the explicit neutral setting for items */
#define FLAG_SEE_INVISIBLE 	21 /* Will see invisible player */
#define FLAG_CAN_ROLL		22 /* Object can be rolled */
#define FLAG_GENERATOR		23 /* Will generate type ob->stats.food */

#define FLAG_IS_TURNABLE 	24 /* Object can change face with direction */
#define FLAG_WALK_OFF		25 /* Object is applied when left */
#define FLAG_FLY_ON			26 /* As WALK_ON, but only with FLAG_FLYING */
#define FLAG_FLY_OFF		27 /* As WALK_OFF, but only with FLAG_FLYING */
#define FLAG_IS_USED_UP		28 /* When (--food<0) the object will exit */
#define FLAG_IDENTIFIED		29 /* Not implemented yet */
#define FLAG_REFLECTING		30 /* Object reflects from walls (lightning) */
#define FLAG_CHANGING		31 /* Changes to other_arch when anim is done*/

/* Start of values in flags[1] */
#define FLAG_SPLITTING		32 /* Object splits into stats.food other objs */
#define FLAG_HITBACK		33 /* Object will hit back when hit */
#define FLAG_STARTEQUIP		34 /* Object was given to player at start */
#define FLAG_BLOCKSVIEW		35 /* Object blocks view */
#define FLAG_UNDEAD			36 /* Monster is undead */
#define UNUSED_FLAG2	 	37 /* used to be FREED */
#define FLAG_UNAGGRESSIVE	38 /* Monster doesn't attack players */
#define FLAG_REFL_MISSILE	39 /* object will give missile reflection */

#define FLAG_REFL_SPELL		40 /* object will give spell reflection */
#define FLAG_NO_MAGIC		41 /* Spells (some) can't pass this object */
#define FLAG_NO_FIX_PLAYER	42 /* fix_player() won't be called */
#define FLAG_IS_EVIL		43 /* alignment flags */
#define FLAG_TEAR_DOWN		44 /* at->faces[hp*animations/maxhp] at hit */
#define FLAG_RUN_AWAY		45 /* Object runs away from nearest player
								* but can still attack at a distance
								*/
#define FLAG_PASS_THRU		46 /* Objects with can_pass_thru can pass
							    * thru this object as if it wasn't there
								*/
#define FLAG_CAN_PASS_THRU	47 /* Can pass thru... */

#define FLAG_PICK_UP		48 /* Can pick up */
#define FLAG_UNIQUE			49 /* Item is really unique (UNIQUE_ITEMS) */
#define FLAG_NO_DROP		50 /* Object can't be dropped */
#define FLAG_INDESTRUCTIBLE	51/* every item with quality use up. if this is set, it don't use up by physical forces */
#define FLAG_CAST_SPELL		52 /* (Monster) can learn and cast spells */
#define FLAG_USE_SCROLL		53 /* (Monster) can read scroll */
#define FLAG_USE_RANGE		54 /* (Monster) can apply and use range items */
#define FLAG_USE_BOW		55 /* (Monster) can apply and fire bows */

#define FLAG_USE_ARMOUR		56 /* (Monster) can wear armour/shield/helmet */
#define FLAG_USE_WEAPON		57 /* (Monster) can wield weapons */
#define FLAG_USE_RING		58 /* (Monster) can use rings, boots, gauntlets, etc */
#define FLAG_READY_RANGE	59 /* (Monster) has a range item readied... 8) */
#define FLAG_READY_BOW		60 /* not implemented yet */
#define FLAG_XRAYS			61 /* X-ray vision */
#define FLAG_NO_APPLY		62 /* Avoids step_on/fly_on to this object */
#define FLAG_IS_FLOOR		63 /* Can't see what's underneath this object */

/* Start of values in flags[2] */
#define FLAG_LIFESAVE		64 /* Saves a players' life once, then destr. */
#define FLAG_IS_MAGICAL     65 /* item is magic - intern used.
                                * player use FLAG_KNOWN_MAGICAL
                                */
#define FLAG_ALIVE	 		66 /* Object can fight (or be fought) */
#define FLAG_STAND_STILL	67 /* NPC will not (ever) move */
#define FLAG_RANDOM_MOVE	68 /* NPC will move randomly */
#define FLAG_ONLY_ATTACK	69 /* NPC will evaporate if there is no enemy */
#define FLAG_WIZ	 		70 /* Object has special privilegies */
#define FLAG_STEALTH		71 /* Will wake monsters with less range */

#define FLAG_WIZPASS		72 /* The wizard can go through walls */
#define FLAG_IS_LINKED		73 /* The object is linked with other objects */
#define FLAG_CURSED			74 /* The object is cursed */
#define FLAG_DAMNED			75 /* The object is _very_ cursed */
#define FLAG_SEE_ANYWHERE	76 /* The object will be visible behind walls (disabled MT2003 - read los.c) */
#define FLAG_KNOWN_MAGICAL	77 /* The object is known to be magical */
#define FLAG_KNOWN_CURSED	78 /* The object is known to be cursed */
#define FLAG_CAN_USE_SKILL	79 /* The monster can use skills */

#define FLAG_IS_THROWN		80 /* Object is designed to be thrown. */
#define FLAG_VUL_SPHERE		81
#define FLAG_PROOF_SPHERE	82
#define FLAG_IS_MALE		83 /* gender flags. it effects only player & mobs */
#define FLAG_IS_FEMALE		84 /* is not female nor male, it is a neuter */
#define FLAG_APPLIED	 	85 /* Object is ready for use by living */
#define FLAG_INV_LOCKED		86 /* Item will not be dropped from inventory */
#define FLAG_IS_WOODED		87 /* Item is wooded terrain */

#define FLAG_IS_HILLY		88 /* Item is hilly/mountain terrain */
#define FLAG_READY_SKILL	89 /* (Monster or Player) has a skill readied */
#define FLAG_READY_WEAPON	90 /* (Monster or Player) has a weapon readied */
#define FLAG_NO_SKILL_IDENT	91 /* If set, item cannot be identified w/ a skill */
#define FLAG_WAS_WIZ	 	92 /* Player was once a wiz */
#define FLAG_SEE_IN_DARK	93 /* if set ob not effected by darkness */
#define FLAG_IS_CAULDRON	94 /* container can make alchemical stuff */
#define FLAG_DUST			95 /* item is a 'powder', effects throwing */

/* Start of values in flags[3] */
#define FLAG_NO_STEAL			96 /* Item can't be stolen */
#define FLAG_ONE_HIT			97 /* Monster can only hit once before going
									* away (replaces ghosthit)
									*/
#define FLAG_CLIENT_SENT		98 /* THIS IS A DEBUG FLAG ONLY.  We use it to
									* detect cases were the server is trying
									* to send an upditem when we have not
									* actually sent the item.
									*/
#define FLAG_BERSERK            99	/* monster will attack closest living object */
#define FLAG_NO_ATTACK          100 /* object will not attack */
#define FLAG_INVULNERABLE       101 /* monster can't be damaged */

#define FLAG_QUEST_ITEM			102	/* this is a special quest object */

#define FLAG_IS_TRAPED			103	/* object is traped - most common a container with
									 * a known trap inside. This info so useful for client
									 * below and inventory look.
									 */

#define FLAG_VUL_ELEMENTAL		104	/* Thats the item damage flags. Every flag determinate */
#define FLAG_PROOF_ELEMENTAL	105 /* that a worn or wielded item can be damaged from this */
#define FLAG_VUL_MAGIC			106 /* effect. If the effect for a item is set, then he can */
#define FLAG_PROOF_MAGIC		107 /* be "proofed" against it. It the proof flag is set, the */
#define FLAG_VUL_PHYSICAL		108 /* item don't get damage from this effect anymore */
#define FLAG_PROOF_PHYSICAL		109
#define FLAG_SYS_OBJECT			110 /* thats old invisible - now sys_object (which are invisible) */
#define FLAG_USE_FIX_POS		111 /* when putting a object on map - do it exactly on position */

#define FLAG_UNPAID	 			112 /* Object hasn't been paid for yet */
#define FLAG_IS_AGED			113 /* if set, object falls under heavy ageing effects */
#define FLAG_MAKE_INVISIBLE		114	/* if a applyable item has this set, he makes the wearer invisible */
#define FLAG_MAKE_ETHEREAL		115 /* same as make_invisibile but for ethereal */
#define FLAG_IS_PLAYER			116 /* object "is player". */
#define FLAG_IS_NAMED			117	/* object name is "unique"- for artifacts like Stormbringer.
									 * Unique object normally don't have a race or material
									 * (no "elven iron Stormbringer")
									 */
#define FLAG_SPAWN_MOB			118 /* monster with this flag are created by spawn point
									 * and have a spawn info object inside inventory
									 */
#define FLAG_NO_TELEPORT		119	/* objects with this flags will not be teleported
									 * from teleporters. Except, they are in a inventory
									 * of a teleporter object.
									 */
#define FLAG_CORPSE				120 /* if set, this object (usally mob) will drop corpse using race name->
                                     * all item of the mob will put in the corpse and/or if slaying of corpse
                                     * is set only the player which killed the mob can access the corpse until
                                     * it decayed - then items drop on ground and all can grap it */
#define FLAG_CORPSE_FORCED		121 /* normally, corpses will only be placed when the mob has some items to drop.
									 * this flag will drop a corpse even the corpse is empty */
#define FLAG_PLAYER_ONLY		122 /* if a item with this flag is placed in a tile, this tile can't be entered
									 * from anything ecept a player
									 */
#define FLAG_NO_CLERIC			123
#define FLAG_ONE_DROP			124 /* if this flag is set, the item marked with it will flaged
									 * start equipment when a player gets it (item is inserted
									 * in player inventory and/or touched by a player)
									 */
#define FLAG_PERM_CURSED		125  /* object will set to cursed when monster or player applies it.
									  * remove curse will remove cursed 1 but not this flag.
									  */
#define FLAG_PERM_DAMNED		126	  /* same as perm_cursed but for damned */

#define FLAG_DOOR_CLOSED		127	  /* this object works like a closed door. Main function
									   * is to trigger the right map flags, so a moving objects
									   * know that spot is blocked by a door and he must open it first->
									   */
#define FLAG_WAS_REFLECTED		128	  /* object was reflected (arrow, throw object...) */
#define FLAG_IS_MISSILE			129	  /* object is used as missile (arrow, potion, magic bullet, ...) */
#define FLAG_CAN_REFL_MISSILE	130		/* Arrows WILL reflect from object (most times) */
#define FLAG_CAN_REFL_SPELL		131		/* Spells WILL reflect from object (most times) */

#define FLAG_IS_ASSASSINATION	132		/* If a attacking force and slaying is set, this is 3 times damage */
#define FLAG_OBJECT_WAS_MOVED	133		/* internal used from remove_ob() und insert_xx() */
#define FLAG_NO_SAVE			134		/* don't save this object - remove it before we save */

/* flag 37 is still free (old FREED flag). Let it free for secure reason for some time */

#define NUM_FLAGS		134 /* Should always be equal to the last defined flag */
#define NUM_FLAGS_32	5	/* the number of uint32 we need to store all flags */

/* macros for invisible test. the first tests only system objects */
#define IS_SYS_INVISIBLE(__ob_)			QUERY_FLAG(__ob_, FLAG_SYS_OBJECT)
#define IS_INVISIBLE(__ob_,__player_)	(QUERY_FLAG(__ob_, FLAG_SYS_OBJECT)||(QUERY_FLAG(__ob_, FLAG_IS_INVISIBLE) && !QUERY_FLAG(__player_, FLAG_SEE_INVISIBLE)))


/* Values can go up to 127 before the size of the flags array in the
 * object structure needs to be enlarged.
 */

#define NROFNEWOBJS(xyz)	((xyz)->stats.food)

#define SLOW_PENALTY(xyz)	((xyz)->stats.exp)/1000.0
#define SET_SLOW_PENALTY(xyz,fl)	(xyz)->stats.exp= (sint32) ((fl)*1000.0)
#define SET_GENERATE_TYPE(xyz,va)	(xyz)->stats.sp=(va)
#define GENERATE_TYPE(xyz)	((xyz)->stats.sp)
#define GENERATE_SPEED(xyz)	((xyz)->stats.maxsp) /* if(!RANDOM()%<speed>) */

/* Note: These values are only a default value, resizing can change them */
#define INV_SIZE		12	/* How many items can be viewed in inventory */
#define LOOK_SIZE		6	/* ditto, but for the look-window */
#define MAX_INV_SIZE		40	/* For initializing arrays */
#define MAX_LOOK_SIZE		40	/* ditto for the look-window */

#define EXIT_PATH(xyz)		(xyz)->slaying
#define EXIT_LEVEL(xyz)		(xyz)->stats.food
#define EXIT_X(xyz)		(xyz)->stats.hp
#define EXIT_Y(xyz)		(xyz)->stats.sp

#define F_BUY 0
#define F_SELL 1
#define F_TRUE 2	/* True value of item, unadjusted */

#define DIRX(xyz)	freearr_x[(xyz)->direction]
#define DIRY(xyz)	freearr_y[(xyz)->direction]

#define D_LOCK(xyz)	(xyz)->contr->freeze_inv=(xyz)->contr->freeze_look=1;
#define D_UNLOCK(xyz)	(xyz)->contr->freeze_inv=(xyz)->contr->freeze_look=0;

#define ARMOUR_SPEED(xyz)	(xyz)->last_sp
#define ARMOUR_SPELLS(xyz)	(xyz)->last_heal

/* GET_?_FROM_DIR if used only for positional firing where dir is X and Y
   each of them signed char, concatenated in a int16 */
#define GET_X_FROM_DIR(dir) (signed char) (  dir & 0xFF )
#define GET_Y_FROM_DIR(dir) (signed char) ( (dir & 0xFF00) >> 8)
#define SET_DIR_FROM_XY(X,Y) (signed char)X + ( ((signed char)Y)<<8)
#define FIRE_DIRECTIONAL 0
#define FIRE_POSITIONAL  1

/******************************************************************************/
/* Monster Movements added by kholland@sunlab.cit.cornell.edu                 */
/******************************************************************************/
/* if your monsters start acting wierd, mail me                               */
/******************************************************************************/
/* the following definitions are for the attack_movement variable in monsters */
/* if the attack_variable movement is left out of the monster archetype, or is*/
/* set to zero                                                                */
/* the standard mode of movement from previous versions of crossfire will be  */
/* used. the upper four bits of movement data are not in effect when the monst*/
/* er has an enemy. these should only be used for non agressive monsters.     */
/* to program a monsters movement add the attack movement numbers to the movem*/
/* ment numbers example a monster that moves in a circle until attacked and   */
/* then attacks from a distance:                                              */
/*                                                      CIRCLE1 = 32          */
/*                                              +       DISTATT = 1           */
/*                                      -------------------                   */
/*                      attack_movement = 33                                  */
/******************************************************************************/
#define DISTATT  1 /* move toward a player if far, but mantain some space,  */
                   /* attack from a distance - good for missile users only  */
#define RUNATT   2 /* run but attack if player catches up to object         */
#define HITRUN   3 /* run to then hit player then run away cyclicly         */
#define WAITATT  4 /* wait for player to approach then hit, move if hit     */
#define RUSH     5 /* Rush toward player blindly, similiar to dumb monster  */
#define ALLRUN   6 /* always run never attack good for sim. of weak player  */
#define DISTHIT  7 /* attack from a distance if hit as recommended by Frank */
#define WAIT2    8 /* monster does not try to move towards player if far    */
                   /* maintains comfortable distance                        */
#define PETMOVE 16 /* if the upper four bits of move_type / attack_movement */
                   /* are set to this number, the monster follows a player  */
                   /* until the owner calls it back or off                  */
                   /* player followed denoted by 0b->owner                  */
                   /* the monster will try to attack whatever the player is */
                   /* attacking, and will continue to do so until the owner */
                   /* calls off the monster - a key command will be         */
                   /* inserted to do so                                     */
#define CIRCLE1 32 /* if the upper four bits of move_type / attack_movement */
                   /* are set to this number, the monster will move in a    */
                   /* circle until it is attacked, or the enemy field is    */
                   /* set, this is good for non-aggressive monsters and NPC */
#define CIRCLE2 48 /* same as above but a larger circle is used             */
#define PACEH   64 /* The Monster will pace back and forth until attacked   */
                   /* this is HORIZONTAL movement                           */
#define PACEH2  80 /* the monster will pace as above but the length of the  */
                   /* pace area is longer and the monster stops before      */
                   /* changing directions                                   */
                   /* this is HORIZONTAL movement                           */
#define RANDO   96 /* the monster will go in a random direction until       */
                   /* it is stopped by an obstacle, then it chooses another */
                   /* direction.                                            */
#define RANDO2 112 /* constantly move in a different random direction       */
#define PACEV  128 /* The Monster will pace back and forth until attacked   */
                   /* this is VERTICAL movement                             */
#define PACEV2 144 /* the monster will pace as above but the length of the  */
                   /* pace area is longer and the monster stops before      */
                   /* changing directions                                   */
                   /* this is VERTICAL movement                             */
#define WPOINT 176 /* The monster uses waypoints (if it has any)            */
#define LO4     15 /* bitmasks for upper and lower 4 bits from 8 bit fields */
#define HI4    240

/*
 * Use of the state-variable in player objects:
 */

#define ST_PLAYING				0
#define ST_ROLL_STAT			1
#define ST_CHANGE_CLASS			2
#define ST_GET_NAME				3
#define ST_GET_PASSWORD			4
#define ST_CONFIRM_PASSWORD     5

#define BLANK_FACE_NAME "blank.111"
#define NEXT_ITEM_FACE_NAME "next_item.101"
#define PREVIOUS_ITEM_FACE_NAME "prev_item.101"

/*
 * Defines for the luck/random functions to make things more readable
 */

#define PREFER_HIGH	1
#define PREFER_LOW	0

/* socket defines */
#define SockList_AddChar(_sl_,_c_)		(_sl_)->buf[(_sl_)->len++]=(_c_)
#define SockList_AddShort(_sl_, _data_) (_sl_)->buf[(_sl_)->len++]= ((_data_)>>8)&0xff; \
										(_sl_)->buf[(_sl_)->len++] = (_data_) & 0xff

#define SockList_AddInt(_sl_, _data_)	(_sl_)->buf[(_sl_)->len++]=((_data_)>>24)&0xff; \
										(_sl_)->buf[(_sl_)->len++]= ((_data_)>>16)&0xff; \
										(_sl_)->buf[(_sl_)->len++]= ((_data_)>>8)&0xff; \
										(_sl_)->buf[(_sl_)->len++] = (_data_) & 0xff

/* Basically does the reverse of SockList_AddInt, but on
 * strings instead.  Same for the GetShort, but for 16 bits.
 */
#define GetInt_String(_data_) (((_data_)[0]<<24) + ((_data_)[1]<<16) + ((_data_)[2]<<8) + (_data_)[3])
#define GetShort_String(_data_) (((_data_)[0]<<8)+(_data_)[1])

/* Simple function we use below to keep adding to the same string
 * but also make sure we don't overwrite that string.
 */
static inline void safe_strcat(char *dest, const char *orig, int *curlen, int maxlen)
{
    if (*curlen == (maxlen-1)) return;
    strncpy(dest+*curlen, orig, maxlen-*curlen-1);
    dest[maxlen-1]=0;
    *curlen += strlen(orig);
    if (*curlen>(maxlen-1)) *curlen=maxlen-1;
}


#define DESCRIBE_PATH(retbuf, variable, name) \
    if(variable) { \
      int i,j=0; \
      strcat(retbuf,"(" name ": "); \
      for(i=0; i<NRSPELLPATHS; i++) \
        if(variable & (1<<i)) { \
          if (j) \
            strcat(retbuf,", "); \
          else \
            j = 1; \
          strcat(retbuf, spellpathnames[i]); \
        } \
      strcat(retbuf,")"); \
    }


#define DESCRIBE_PATH_SAFE(retbuf, variable, name, len, maxlen) \
    if(variable) { \
      int i,j=0; \
      safe_strcat(retbuf,"(" name ": ", len, maxlen); \
      for(i=0; i<NRSPELLPATHS; i++) \
        if(variable & (1<<i)) { \
          if (j) \
            safe_strcat(retbuf,", ", len, maxlen); \
          else \
            j = 1; \
          safe_strcat(retbuf, spellpathnames[i], len, maxlen); \
        } \
      safe_strcat(retbuf,")", len, maxlen); \
    }

/* Flags for apply_special() */
enum apply_flag {
  /* Basic flags, always use one of these */
	AP_NULL			= 0,
	AP_APPLY		= 1,
	AP_UNAPPLY		= 2,

        AP_BASIC_FLAGS		= 15,

  /* Optional flags, for bitwise or with a basic flag */
        AP_NO_MERGE		= 16,
	AP_IGNORE_CURSE		= 32
};

/* Cut off point of when an object is put on the active list or not */
#define MIN_ACTIVE_SPEED	0.00001f

/* Bresenham line drawing algorithm. Implemented as macros for
 * flexibility and speed. */

/* Bresenham init */
/* dx & dy are input only and will not be changed.
 * All other parameters are the outputs which will be initialized */
#define BRESENHAM_INIT(dx, dy, fraction, stepx, stepy, dx2, dy2) \
    { \
        (dx2) = (dx) << 1; \
        (dy2) = (dy) << 1; \
        if ((dy) < 0) { (dy2) = -(dy2);  (stepy) = -1; } else { (stepy) = 1; } \
        if ((dx) < 0) { (dx2) = -(dx2);  (stepx) = -1; } else { (stepx) = 1; } \
        if((dx2) > (dy2)) (fraction) = (dy2) - (dx)*(stepx); else (fraction) = (dx2) - (dy)*(stepy); \
    }

/* Bresenham line stepping macro */
/* x,y are input-output and will be always be changed
 * fraction is also input-output, but should be initialized with
 * BRESENHAM_INIT.
 * stepx, stepy, dx2 and dy2 are input only and should also
 * be initialized by BRESENHAM_INIT
 */
#define BRESENHAM_STEP(x,y,fraction,stepx,stepy,dx2,dy2) \
    if ((dx2) > (dy2)) { \
        if ((fraction) >= 0) { \
            (y) += (stepy); \
            (fraction) -= (dx2); \
        } \
        (x) += (stepx); \
        (fraction) += (dy2); \
    } else { \
        if ((fraction) >= 0) { \
            (x) += (stepx); \
            (fraction) -= (dy2); \
        } \
        (y) += (stepy); \
        (fraction) += (dx2); \
    }

/*
 * random() is much better than rand().  If you have random(), use it instead.
 * You shouldn't need to change any of this
 *
 * 0.93.3: It looks like linux has random (previously, it was set below
 * to use rand).  Perhaps old version of linux lack rand?  IF you run into
 * problems, add || defined(linux) the #if immediately below.
 *
 * 0.94.2 - you probably shouldn't need to change any of the rand stuff
 * here.
 */

#ifdef HAVE_SRANDOM
#define RANDOM() random()
#define SRANDOM(xyz) srandom(xyz)
#else
#  ifdef HAVE_SRAND48
#  define RANDOM() lrand48()
#  define SRANDOM(xyz) srand48(xyz)
#  else
#    ifdef HAVE_SRAND
#      define RANDOM() rand()
#      define SRANDOM(xyz) srand(xyz)
#    else
#      error "Could not find a usable random routine"
#    endif
#  endif
#endif

#define PLUGINS
#endif /* DEFINE_H */

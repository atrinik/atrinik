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

#ifndef ITEM_H
#define ITEM_H

/*
 *  These should probably be in a separate file
 */

/*
 *  Use static buffer for object names. Item names are changing so
 *  often that mallocing them it just a waste of time. Also there is
 *  probably some upper limits for names that client can show, Note
 *  that total number of items is small (<100) so this don't even
 *  waste too much memory
 */
#define NAME_LEN	128
#define copy_name(t,f) strncpy(t, f, NAME_LEN-1)

/*
 *  item structure keeps all information what player
 *  (= client) knows about items in its inventory
 */
typedef struct item_struct {
    struct item_struct *next;	/* next item in inventory */
    struct item_struct *prev;	/* previous item in inventory */
    struct item_struct *env;	/* which items inventory is this item */
    struct item_struct *inv;	/* items inventory */
    char d_name[NAME_LEN];	/* item's full name w/o status information */
    char s_name[NAME_LEN];	/* item's singular name as sent to us */
    char p_name[NAME_LEN];	/* item's plural name as sent to us */
    char flags[NAME_LEN];	/* item's status information */
    sint32 tag;			/* item identifier (0 = free) */
    sint32 nrof;		/* number of items */
    float weight;		/* how much item weights */
    sint16 face;		/* index for face array */
    uint16 animation_id;	/* Index into animation array */
    uint16 anim_speed;		/* how often to animate */
    uint16 anim_state;		/* last face in sequence drawn */
    uint16 last_anim;		/* how many ticks have passed since we last animated */
    uint16 magical:1;		/* item is magical */
    uint16 cursed:1;		/* item is cursed */
    uint16 damned:1;		/* item is damned */
    uint16 unpaid:1;		/* item is unpaid */
    uint16 locked:1;		/* item is locked */
    uint16 traped:1;		/* item is traped */
    uint16 applied:1;		/* item is applied */
    uint16 open:1;		/* container is open */
    uint16 inv_updated:1;	/* item's inventory is updated, this is set
				   when item's inventory is modified, draw
				   routines can use this to redraw things */
    uint8 apply_type;		/* how item is applied (worn/wield/etc) */
    uint32 flagsval;		/* unmodified flags value as sent from the server*/
    uint8   type;		/* Item type for ordering */
    uint8 itype;
    uint8 stype;
    uint8 item_qua;
    uint8 item_con;
    uint8 item_skill;
    uint8 item_level;
	uint8 direction;
} item;


#define TYPE_PLAYER		            1
#define TYPE_BULLET		            2
#define TYPE_ROD		                3
#define TYPE_TREASURE	            4
#define TYPE_POTION		            5
#define TYPE_FOOD		            6
#define TYPE_POISON		            7
#define TYPE_BOOK		            8
#define TYPE_CLOCK		            9
#define TYPE_FBULLET		            10
#define TYPE_FBALL		            11
#define TYPE_LIGHTNING	            12
#define TYPE_ARROW		            13
#define TYPE_BOW		                14
#define TYPE_WEAPON		            15
#define TYPE_ARMOUR		            16
#define TYPE_PEDESTAL	            17
#define TYPE_ALTAR		            18
#define TYPE_CONFUSION	            19
#define TYPE_LOCKED_DOOR	            20
#define TYPE_SPECIAL_KEY	            21
#define TYPE_MAP		                22
#define TYPE_DOOR		            23
#define TYPE_KEY		                24
#define TYPE_MMISSILE	            25
#define TYPE_TIMED_GATE	            26
#define TYPE_TRIGGER		            27
#define TYPE_GRIMREAPER	            28
#define TYPE_MAGIC_EAR	            29
#define TYPE_TRIGGER_BUTTON	        30
#define TYPE_TRIGGER_ALTAR	        31
#define TYPE_TRIGGER_PEDESTAL        32
#define TYPE_SHIELD		            33
#define TYPE_HELMET		            34
#define TYPE_HORN		            35
#define TYPE_MONEY		            36
#define TYPE_CLASS                   37  /* object for applying character class modifications to someone */
#define TYPE_GRAVESTONE	            38
#define TYPE_AMULET		            39
#define TYPE_PLAYERMOVER	            40
#define TYPE_TELEPORTER	            41
#define TYPE_CREATOR		            42
#define TYPE_SKILL		            43	/* Skills are similar to abilites, but
				                     * not related to spells.  by njw@cs.city.ac.u */
#define TYPE_EXPERIENCE	            44	/* An experience 'object'. Needed for multi-exp/skills
				                     * hack. -b.t. thomas@astro.psu.edu */
#define TYPE_EARTHWALL	            45
#define TYPE_GOLEM		            46
#define TYPE_BOMB		            47
#define TYPE_THROWN_OBJ	            48
#define TYPE_BLINDNESS	            49
#define TYPE_GOD		                50

/*  peterm:  detector is an object which notices the presense of
	 another object and is triggered like buttons.  */
#define TYPE_DETECTOR	            51
#define TYPE_SPEEDBALL	            52
#define TYPE_DEAD_OBJECT	            53
#define TYPE_DRINK		            54
#define TYPE_MARKER                  55 /* inserts an invisible, weightless
			                        force into a player with a specified string. */
#define TYPE_HOLY_ALTAR	            56
#define TYPE_PLAYER_CHANGER          57
#define TYPE_BATTLEGROUND            58      /* battleground, by Andreas Vogl */

#define TYPE_PEACEMAKER              59  /* Object owned by a player which can convert
			                           a monster into a peaceful being incapable of attack.  */
#define TYPE_GEM		                60
#define TYPE_FIRECHEST	            61
#define TYPE_FIREWALL	            62
#define TYPE_ANVIL                   63
#define TYPE_CHECK_INV	            64	/* by b.t. thomas@nomad.astro.psu.edu */
#define TYPE_MOOD_FLOOR	            65	/* by b.t. thomas@nomad.astro.psu.edu
				                     * values of last_sp set how to change:
				                     * 0 = furious,	all monsters become aggressive
				                     * 1 = angry, all but friendly become aggressive
				                     * 2 = calm, all aggressive monsters calm down
				                     * 3 = sleep, all monsters fall asleep
				                     * 4 = charm, monsters become pets */
#define TYPE_EXIT		            66
#define TYPE_ENCOUNTER	            67
#define TYPE_SHOP_FLOOR	            68
#define TYPE_SHOP_MAT	            69
#define TYPE_RING		            70

#define TYPE_FLOOR                   71 /* this is a floor tile -> native layer 0 */

#define TYPE_FLESH		            72	/* animal 'body parts' -b.t. */
#define TYPE_INORGANIC	            73	/* metals and minerals */

#define TYPE_LIGHT_APPLY            74 /* new apply item: light source for players */

#define TYPE_LIGHTER		            75
#define TYPE_TRAP_PART	            76	/* Needed by set traps skill -b.t. */

#define TYPE_WALL                    77 /* this is a wall. put it always in layer 1 if not set is_floor */
#define TYPE_LIGHT_SOURCE            78 /* torches, lamps, etc. *outdated* */
#define TYPE_MISC_OBJECT             79 /* misc. objects are for objects without a function
                                      in the engine. Like statues, clocks, chairs,...
                                      If perhaps we create a function where we can sit
                                      on chairs, we create a new type and remove all
                                      chairs from here. */
#define TYPE_MONSTER                 80 /* yes, thats a real, living creature */
#define TYPE_SPAWN_GENERATOR         81 /* a spawn point or monster generator object */

#define TYPE_SPELLBOOK	            85

#define TYPE_CLOAK		            87
#define TYPE_CONE		            88
#define TYPE_AURA                    89  /* aura spell object */

#define TYPE_SPINNER		            90
#define TYPE_GATE		            91
#define TYPE_BUTTON		            92
#define TYPE_CF_HANDLE		        93
#define TYPE_HOLE		            94
#define TYPE_TRAPDOOR	            95
#define TYPE_WORD_OF_RECALL	        96
#define TYPE_PARAIMAGE	            97
#define TYPE_SIGN		            98
#define TYPE_BOOTS		            99
#define TYPE_GLOVES		            100

#define TYPE_CONVERTER	            103
#define TYPE_BRACERS		            104
#define TYPE_POISONING	            105
#define TYPE_SAVEBED		            106
#define TYPE_POISONCLOUD	            107
#define TYPE_FIREHOLES	            108
#define TYPE_WAND		            109
#define TYPE_ABILITY		            110
#define TYPE_SCROLL		            111
#define TYPE_DIRECTOR	            112
#define TYPE_GIRDLE		            113
#define TYPE_FORCE		            114
#define TYPE_POTION_EFFECT           115    /* a force, holding the effect of a potion */
#define TYPE_CLOSE_CON	            121    /* Eneq(@csd.uu.se): Id for close_container archetype. */
#define TYPE_CONTAINER	            122
#define TYPE_ARMOUR_IMPROVER         123
#define TYPE_WEAPON_IMPROVER         124

/* unused: 125 - 129
 * type 125 was MONEY_CHANGER
 */
#define TYPE_SKILLSCROLL	            130	/* can add a skill to player's inventory -bt.*/
#define TYPE_DEEP_SWAMP	            138
#define TYPE_IDENTIFY_ALTAR	        139
#define TYPE_CANCELLATION	        141
#define TYPE_MENU		            150 /* Mark Wedel (mark@pyramid.com) Shop inventories */
#define TYPE_BALL_LIGHTNING          151 /* peterm:  ball lightning and color spray */
#define TYPE_SWARM_SPELL             153
#define TYPE_RUNE                    154

#define TYPE_POWER_CRYSTAL           156
#define TYPE_CORPSE                  157

#define TYPE_DISEASE                 158
#define TYPE_SYMPTOM                 159
/* END TYPE DEFINE */

#define F_ETHEREAL		0x0080
#define F_INVISIBLE		0x0100

/*
 *  A few macros to make clear interface
 *  These will change (especially update_item and add_new_item)
 */
#define delete_item(tag) remove_item(locate_item(tag))
#define delete_item_inventory(tag) remove_item_inventory(locate_item(tag))

extern void init_item_types ( void );
extern int locate_item_nr_from_tag (item *op, int tag);
extern int locate_item_tag_from_nr (item *op, int nr);
extern item *locate_item_from_inv (item *op, sint32 tag);
extern item *locate_item_from_item (item *op, sint32 tag);
extern uint8 get_type_from_name ( const char *name );
extern void update_item_sort ( item *it );
extern char *get_number ( int i );
extern void free_all_items ( item *op );
extern item *locate_item ( sint32 tag );
extern void remove_item ( item *op );
extern void remove_item_inventory ( item *op );
extern item *create_new_item ( item *env, sint32 tag ,int bflag);

extern void set_item_values (item *op, char *name, sint32 weight, uint16 face,
                      int flags, uint16 anim, uint16 animspeed,
                      sint32 nrof,uint8 itype, uint8 stype,uint8 q,uint8 c,uint8 s,uint8 l,uint8 dir);
                      extern void toggle_locked ( item *op );
extern void send_mark_obj ( item *op );
extern item *player_item ( void );
extern item *map_item ( void );
extern void update_item ( int tag, int loc, char *name, int weight, int face, int flags, int anim, int animspeed, int nrof ,uint8 type, uint8 subtype,uint8 quality,uint8 codition,uint8 skill,uint8 level,uint8 direction, int bflag);
extern void print_inventory ( item *op );
extern void animate_objects ( void );

extern void fire_command (char *buf);
extern void combat_command (char *buf);
extern void dump_inv (item *);
#endif /* ITEM_H */

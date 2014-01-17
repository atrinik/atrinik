/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Player related header file. */

#ifndef PLAYER_H
#define PLAYER_H

/**
 * @defgroup GENDER_xxx Gender IDs.
 * IDs of the various genders.
 *@{*/
/** Neuter: no gender. */
#define GENDER_NEUTER 0
/** Male. */
#define GENDER_MALE 1
/** Female. */
#define GENDER_FEMALE 2
/** Hermaphrodite: both genders. */
#define GENDER_HERMAPHRODITE 3
/** Total number of genders. */
#define GENDER_MAX 4
/*@}*/

#define PLAYER_DOLL_SLOT_COLOR "353734"

#define EXP_PROGRESS_BUBBLES 10

typedef struct Stat_struct
{
    sint8 Str, Dex, Con, Wis, Cha, Int, Pow;

    /** Weapon class. */
    sint16 wc;

    /** Armour class. */
    sint16 ac;

    /** Level. */
    uint32 level;

    /** Hit points. */
    sint32 hp;

    /** Max hit points */
    sint32 maxhp;

    /** Spell points. */
    sint32 sp;

    /** Max spell points. */
    sint32 maxsp;

    /** Total experience. */
    sint64 exp;

    /** How much food in stomach. */
    sint16 food;

    /** How much damage the player does when hitting. */
    sint16 dam;

    /** Player's speed; gets converted to a float for display. */
    sint32 speed;

    /** Weapon speed. */
    double weapon_speed;

    /** Contains fire on/run on flags. */
    uint16 flags;

    /** Protections. */
    sint16 protection[20];

    /** Ranged weapon damage. */
    sint16 ranged_dam;

    /** Ranged weapon wc. */
    sint16 ranged_wc;

    /** Ranged weapon speed. */
    sint32 ranged_ws;
} Stats;

/** The player structure. */
typedef struct Player_Struct
{
    /** Player object. */
    object *ob;

    /** Items below the player (pl.below->inv). */
    object *below;

    /** Inventory of an open container. */
    object *sack;

    /** Tag of the open container. */
    sint32 container_tag;

    /** Player's weight limit. */
    double weight_limit;

    /** Are we a DM? */
    int dm;

    /** Target. */
    int target_code;

    /** Target's color. */
    char target_color[COLOR_BUF];

    /** Target name. */
    char target_name[MAX_BUF];

    int warn_hp;

    /** Currently marked item. */
    int mark_count;

    /** HP regeneration. */
    float gen_hp;

    /** Mana regeneration. */
    float gen_sp;

    /** Skill cooldown time. */
    float action_timer;

    /** 1 if fire key is pressed. */
    uint8 fire_on;

    /** 1 if run key is on. */
    uint8 run_on;

    /** Player's carrying weight. */
    float real_weight;

    int warn_statdown;
    int warn_statup;

    /** Player stats. */
    Stats stats;

    /** HP of our target in percent. */
    char target_hp;

    /** Player's name. */
    char name[40];

    /** Rank and name of char. */
    char ext_title[MAX_BUF];

    /** Party name this player is member of. */
    char partyname[MAX_BUF];

    /**
     * Buffer for party name the player is joining, but has to enter
     * password first. */
    char partyjoin[MAX_BUF];

    /**
     * Which item is being dragged. */
    tag_t dragging_tag;

    /**
     * X position where the item was dragged from. */
    int dragging_startx;

    /**
     * Y position where the item was dragged from. */
    int dragging_starty;

    /** Which inventory widget has the focus. */
    int inventory_focus;

    /** Version of the server's socket. */
    int server_socket_version;

    size_t target_object_index;

    uint8 target_is_friend;

    /**
     * Player's gender. */
    uint8 gender;

    tag_t equipment[PLAYER_EQUIP_MAX];

    uint32 path_attuned;

    uint32 path_repelled;

    uint32 path_denied;

    player_state_t state;

    /**
     * Account name that we are logged into. */
    char account[MAX_BUF];

    /**
     * Password that was used to log in. */
    char password[MAX_BUF];

    /**
     * Current IP. */
    char host[MAX_BUF];

    /**
     * Last IP that the account was used from. */
    char last_host[MAX_BUF];

    /**
     * Last time the account was used. */
    time_t last_time;
} Client_Player;

#endif

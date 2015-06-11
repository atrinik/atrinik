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

typedef struct Stat_struct {
    int8_t Str, Dex, Con, Int, Pow;

    /** Weapon class. */
    int16_t wc;

    /** Armour class. */
    int16_t ac;

    /** Level. */
    uint32_t level;

    /** Hit points. */
    int32_t hp;

    /** Max hit points */
    int32_t maxhp;

    /** Spell points. */
    int32_t sp;

    /** Max spell points. */
    int32_t maxsp;

    /** Total experience. */
    int64_t exp;

    /** How much food in stomach. */
    int16_t food;

    /** How much damage the player does when hitting. */
    int16_t dam;

    /** Player's speed. */
    float speed;

    /** Weapon speed. */
    float weapon_speed;

    /** Contains fire on/run on flags. */
    uint16_t flags;

    /** Protections. */
    int8_t protection[CS_STAT_PROT_END - CS_STAT_PROT_START + 1];

    /** Ranged weapon damage. */
    int16_t ranged_dam;

    /** Ranged weapon wc. */
    int16_t ranged_wc;

    /** Ranged weapon speed. */
    float ranged_ws;
} Stats;

/** The player structure. */
typedef struct Player_Struct {
    /** Player object. */
    object *ob;

    /** Items below the player (pl.below->inv). */
    object *below;

    /** Inventory of an open container. */
    object *sack;

    /** Tag of the open container. */
    tag_t container_tag;

    /** Player's weight limit. */
    float weight_limit;

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
    tag_t mark_count;

    /** HP regeneration. */
    float gen_hp;

    /** Mana regeneration. */
    float gen_sp;

    /** Skill cooldown time. */
    float action_timer;

    /** 1 if fire key is pressed. */
    uint8_t fire_on;

    /** 1 if run key is on. */
    uint8_t run_on;

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
    widgetdata *inventory_focus;

    /** Version of the server's socket. */
    int server_socket_version;

    size_t target_object_index;

    uint8_t target_is_friend;

    /**
     * Player's gender. */
    uint8_t gender;

    tag_t equipment[PLAYER_EQUIP_MAX];

    uint32_t path_attuned;

    uint32_t path_repelled;

    uint32_t path_denied;

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

    /**
     * HTTP data URL.
     */
    char http_url[MAX_BUF];

    /**
     * If 1, the player is ready to engage in combat and will swing their
     * weapon at targeted enemies.
     */
    uint8_t combat;

    /**
     * If 1, the player will swing their weapon at their target, be it friend
     * or foe.
     */
    uint8_t combat_force;
} Client_Player;

#endif

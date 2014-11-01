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
 * Handles player related structures, enums and defines. */

#ifndef PLAYER_H
#define PLAYER_H

/** Level color structure. */
typedef struct _level_color
{
    /** Green level. */
    int green;

    /** Blue level. */
    int blue;

    /** Yellow level. */
    int yellow;

    /** Orange level. */
    int orange;

    /** Red level. */
    int red;

    /** Purple level. */
    int purple;
}_level_color;

/** Fire modes submitted from client. */
enum
{
    /** No fire mode. */
    FIRE_MODE_NONE = -1,
    /** Bow. */
    FIRE_MODE_BOW,
    /** Spell. */
    FIRE_MODE_SPELL,
    /** Wand. */
    FIRE_MODE_WAND,
    /** Skill. */
    FIRE_MODE_SKILL,
    /** Throwing. */
    FIRE_MODE_THROW
};

/**
 * @defgroup PLAYER_AFLAG_xxx Player animation flags
 *@{*/
/** If set, show fighting animation. */
#define PLAYER_AFLAG_FIGHT      1
/**
 * If set at the end of an animation, set fighting flag and clear this
 * flag. It is set in attack_ob_simple() when the player swings at an
 * enemy. */
#define PLAYER_AFLAG_ENEMY      2
/** Whether to add an extra frame to do one more swing animation. */
#define PLAYER_AFLAG_ADDFRAME   4
/*@}*/

/** Maximum quickslots allowed. */
#define MAX_QUICKSLOT 32

/** Maximum failures allowed when trying to reach destination path. */
#define PLAYER_PATH_MAX_FAILS 15

/** One path player is attempting to reach. */
typedef struct player_path
{
    /** Next path in linked list. */
    struct player_path *next;

    /** Destination map. */
    mapstruct *map;

    /** Destination X. */
    sint16 x;

    /** Destination Y. */
    sint16 y;

    /**
     * How many times we failed trying to reach this destination. If more
     * than @ref PLAYER_PATH_MAX_FAILS, will abort trying to reach the
     * destination. */
    uint8 fails;
} player_path;

#define SKILL_LEVEL(_pl, _skill) ((_pl)->skill_ptr[(_skill)] ? (_pl)->skill_ptr[(_skill)]->level : 1)

/** The player structure. */
typedef struct pl_player
{
    /** Pointer to previous player, NULL if this is first. */
    struct pl_player *prev;

    /** Pointer to next player, NULL if this is last. */
    struct pl_player *next;

    /** Socket information for this player. */
    socket_struct socket;

    /* Everything below will be cleared by memset() in get_player(). */

    /** Name of the map the player is on. */
    char maplevel[MAX_BUF];

    /** Skill used for fire mode. */
    char firemode_name[BIG_NAME * 2];

    /** Rank + name +" the xxxx" */
    char quick_name[BIG_NAME * 3];

    /** Map where player will respawn after death. */
    char savebed_map[MAX_BUF];

    /** Who killed this player. */
    char killer[BIG_NAME];

    /** Holds arbitrary input from client. */
    char write_buf[MAX_BUF];

    /** Player the DM is following. */
    char followed_player[BIG_NAME];

    /** DM command permissions. */
    char **cmd_permissions;

    /** Faction IDs. */
    shstr **faction_ids;

    /** Available region maps. */
    char **region_maps;

    /** Last map info name sent. */
    char map_info_name[HUGE_BUF];

    /** Last map info music sent. */
    char map_info_music[HUGE_BUF];

    /** Last map info weather sent. */
    char map_info_weather[MAX_BUF];

    /**
     * Last sent map. */
    struct mapdef *last_update;

    /** The object representing the player. */
    object *ob;

    /** Target object. */
    object *target_object;

    /** Pointers to applied items in the player's inventory. */
    object *equipment[PLAYER_EQUIP_MAX];

    /** Quick jump table to skill objects in the player's inventory. */
    object *skill_ptr[NROFSKILLS];

    /** Marked object. */
    object *mark;

    /** Pointer used from local map player chain. */
    object *map_below;

    /** Pointer used from local map player chain. */
    object *map_above;

    /** Current container being used. */
    object *container;

    /** First player accessing player::container. */
    object *container_above;

    /** Last player accessing player::container. */
    object *container_below;

    /**
     * Object defining player's class. Can be NULL. */
    object *class_ob;

    /** Player's quest container. */
    object *quest_container;

    /** For the client target HP marker. */
    char target_hp;

    /** X coordinate of respawn (savebed). */
    int bed_x;

    /** Y coordinate of respawn (savebed). */
    int bed_y;

    /**
     * Array showing what spaces the player can see. For maps smaller
     * than MAP_CLIENT_.., the upper left is used. */
    int blocked_los[MAP_CLIENT_X][MAP_CLIENT_Y];

    /** This is initialized from init_player_exp(). */
    int last_skill_index;

    /** Map update command. */
    int map_update_cmd;

    /** Tile for map update. */
    int map_update_tile;

    /** Last X position we sent to client. */
    int map_tile_x;

    /** Last Y position we sent to client. */
    int map_tile_y;

    /** Scroll X offset between 2 map updates. */
    int map_off_x;

    /** Scroll Y offset between 2 map updates. */
    int map_off_y;

    /** Number of player::cmd_permissions. */
    int num_cmd_permissions;

    /** Number of faction IDs. */
    int num_faction_ids;

    /** Number of available region maps. */
    int num_region_maps;

    /** Reputations with the various factions. */
    sint64 *faction_reputation;

    /** Fame rating in the world. */
    sint64 fame;

    uint32 action_attack;

    /** weapon_speed_left * 1000 and cast from float to int for client. */
    uint32 action_timer;

    /** Previous value of action timer sent to the client. */
    uint32 last_action_timer;

    /** Last speed value sent to client. */
    float last_speed;

    /** Last weapon speed value sent to client. */
    float last_weapon_speed;

    /** Last overall level sent to the client. */
    unsigned char last_level;

    /** Any bonuses/penalties to digestion. */
    signed char digestion;

    /** Penalty to sp regen from armour. */
    signed char gen_sp_armour;

    /** Bonuses to regeneration speed of hp. */
    signed char gen_hp;

    /** Bonuses to regeneration speed of sp. */
    signed char gen_sp;

    /** Player should keep moving in dir until run is off. */
    unsigned char run_on;

#ifdef AUTOSAVE
    /** Last tick the player was saved. */
    long last_save_tick;
#endif

    /** Last weight sent to the player. */
    long last_weight;

#ifdef SAVE_INTERVAL
    /** Last time the player was saved. */
    time_t last_save_time;
#endif

    /** The count of the container. */
    uint32 container_count;

    /** Count of marked object. */
    uint32 mark_count;

    /** Skill experience for all skills. */
    sint64 skill_exp[NROFSKILLS];

    /** Number of deaths. */
    uint64 stat_deaths;

    /** Number of monsters killed. */
    uint64 stat_kills_mob;

    /** Number of players killed in PvP. */
    uint64 stat_kills_pvp;

    /** Total damage taken. */
    uint64 stat_damage_taken;

    /** Total damage dealt. */
    uint64 stat_damage_dealt;

    /** HP regenerated. */
    uint64 stat_hp_regen;

    /** Mana regenerated. */
    uint64 stat_sp_regen;

    /** How many food points have been consumed. */
    uint64 stat_food_consumed;

    /** Number of food items consumed. */
    uint64 stat_food_num_consumed;

    /** Amount of HP healed using heal spells. */
    uint64 stat_damage_healed;

    /** Amount of HP healed using heal spells on friendly targets. */
    uint64 stat_damage_healed_other;

    /** Amount of HP healed by receiving healing from friendly creatures. */
    uint64 stat_damage_heal_received;

    /** Number of steps taken. */
    uint64 stat_steps_taken;

    /** Number of spells cast. */
    uint64 stat_spells_cast;

    /** Number of seconds played. */
    uint64 stat_time_played;

    /** Number of seconds spent AFK. */
    uint64 stat_time_afk;

    /** Cache for value of ::stat_time_played. */
    time_t last_stat_time_played;

    /** Number of arrows/bolts/etc fired. */
    uint64 stat_arrows_fired;

    /** Number of missiles thrown. */
    uint64 stat_missiles_thrown;

    /** Number of books read. */
    uint64 stat_books_read;

    /** Number of unique books read (the ones that give exp). */
    uint64 stat_unique_books_read;

    /** Number of potions used. */
    uint64 stat_potions_used;

    /** Number of scrolls used. */
    uint64 stat_scrolls_used;

    /** Total experience gained. */
    uint64 stat_exp_gained;

    /** Total experience lost. */
    uint64 stat_exp_lost;

    /** Total number of items dropped. */
    uint64 stat_items_dropped;

    /** Total number of items picked up. */
    uint64 stat_items_picked;

    /** Total number of unique corpses searched. */
    uint64 stat_corpses_searched;

    /** Number of traps found using the find traps skill. */
    uint64 stat_traps_found;

    /** Number of traps successfully disarmed. */
    uint64 stat_traps_disarmed;

    /** Number of traps sprung. */
    uint64 stat_traps_sprung;

    /** Number of times the player has enabled AFK mode. */
    uint64 stat_afk_used;

    /** Number of times the player has formed a party. */
    uint64 stat_formed_party;

    /** Number of times the player has joined a party. */
    uint64 stat_joined_party;

    /** Number of items the player has renamed an item. */
    uint64 stat_renamed_items;

    /** Number of times the player has used an emote command. */
    uint64 stat_emotes_used;

    /**
     * Number of times the player used inscription skill to write in a
     * book. */
    uint64 stat_books_inscribed;

    /** Count of target. */
    uint32 target_object_count;

    /** Last weight limit sent to client. */
    uint32 last_weight_limit;

    /** If true, update line of sight with update_los(). */
    uint32 update_los : 1;

    /** Is the player AFK? */
    uint32 afk : 1;

    /** Any numbers typed before a command. */
    uint32 count;

    /** Last ranged weapon speed sent. */
    sint32 last_ranged_ws;

    /**
     * Last attuned spell path sent to client. */
    uint32 last_path_attuned;

    /**
     * Last repelled spell path sent to client. */
    uint32 last_path_repelled;

    /**
     * Last denied spell path sent to client. */
    uint32 last_path_denied;

    /**
     * Last sent UIDs of player's equipment. */
    uint32 last_equipment[PLAYER_EQUIP_MAX];

    /** Last fire/run on flags sent to client. */
    uint16 last_flags;

    /** Remainder for HP regen. */
    uint16 gen_hp_remainder;

    /** Remainder for mana regen. */
    uint16 gen_sp_remainder;

    /** Regeneration speed of HP. */
    uint16 gen_client_hp;

    /** Regeneration speed of mana. */
    uint16 gen_client_sp;

    /** Last regeneration of HP sent to client. */
    uint16 last_gen_hp;

    /** Last regeneration of mana sent to client. */
    uint16 last_gen_sp;

    /** Table of last skill levels sent to client. */
    sint16 skill_level[NROFSKILLS];

    /** Some anim flags for special player animation handling. */
    uint16 anim_flags;

    /** Total item power of objects equipped. */
    sint16 item_power;

    /** Last ranged damage sent. */
    sint16 last_ranged_dam;

    /** Last ranged wc sent. */
    sint16 last_ranged_wc;

    /** Table of protections last sent to the client. */
    sint8 last_protection[NROFATTACKS];

    /** Last gender sent to the client. */
    uint8 last_gender;

    /** If 1, the player is not able to chat. */
    uint8 no_chat;

    /** Last HP sent to party members. */
    uint8 last_party_hp;

    /** Last SP sent to party members. */
    uint8 last_party_sp;

    /** If 1, collision is disabled for this player. */
    uint8 tcl;

    /** Whether god mode is on or off. */
    uint8 tgm;

    /** If 1, disable lighting. */
    uint8 tli;

    /** If 1, LoS is disabled and player can see through walls. */
    uint8 tls;

    /** If 1, normally invisible items can be seen. */
    uint8 tsi;

    /** Last stats sent to the client. */
    living last_stats;

    /** Pointer to the party this player is member of. */
    party_struct *party;

    /** Start of the movement path queue. */
    player_path *move_path;

    /** End of the movement path queue. */
    player_path *move_path_end;

    /**
     * Player name to reply to. */
    char player_reply[64];

    /** Auto-reply message when AFK */
    char afk_auto_reply[MAX_BUF];

} player;

#endif

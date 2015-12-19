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
 * Handles player related structures, enums and defines.
 */

#ifndef PLAYER_H
#define PLAYER_H

/** Level color structure. */
typedef struct _level_color {
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
} _level_color;

/** Fire modes submitted from client. */
enum {
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
 *@{
 */
/** If set, show fighting animation. */
#define PLAYER_AFLAG_FIGHT      1
/**
 * If set at the end of an animation, set fighting flag and clear this
 * flag. It is set in attack_ob_simple() when the player swings at an
 * enemy.
 */
#define PLAYER_AFLAG_ENEMY      2
/** Whether to add an extra frame to do one more swing animation. */
#define PLAYER_AFLAG_ADDFRAME   4
/*@}*/

/** Maximum quickslots allowed. */
#define MAX_QUICKSLOT 32

/** Maximum failures allowed when trying to reach destination path. */
#define PLAYER_PATH_MAX_FAILS 15

#define PLAYER_TESTING_NAME1 "Tester"
#define PLAYER_TESTING_NAME2 "Tester Testington"

#define ACCOUNT_TESTING_NAME "tester"

#define PLAYER_REGEN_HP_RATE 2000.0
#define PLAYER_REGEN_SP_RATE 1200.0

#define PLAYER_REGEN_MODIFIER 10.0
#define PLAYER_REGEN_MODIFIER_MAX 10.0

/** One path player is attempting to reach. */
typedef struct player_path {
    /** Next path in linked list. */
    struct player_path *next;

    /** Destination map. */
    mapstruct *map;

    /** Destination X. */
    int16_t x;

    /** Destination Y. */
    int16_t y;

    /**
     * How many times we failed trying to reach this destination. If more
     * than @ref PLAYER_PATH_MAX_FAILS, will abort trying to reach the
     * destination.
 */
    uint8_t fails;
} player_path;

#define SKILL_LEVEL(_pl, _skill) ((_pl)->skill_ptr[(_skill)] ? (_pl)->skill_ptr[(_skill)]->level : 1)

/**
 * Player faction structure. Holds information about the player's affiliation
 * with a particular faction.
 */
typedef struct player_faction {
    shstr *name; ///< Name of the faction.
    double reputation; ///< Reputation.
    UT_hash_handle hh; ///< Hash handle.
} player_faction_t;

/** The player structure. */
typedef struct pl_player {
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

    /** Last map info name sent. */
    char map_info_name[HUGE_BUF];

    /** Last map info music sent. */
    char map_info_music[HUGE_BUF];

    /** Last map info weather sent. */
    char map_info_weather[MAX_BUF];

    /**
     * Last sent map.
 */
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
     * than MAP_CLIENT_.., the upper left is used.
 */
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

    /** Fame rating in the world. */
    int64_t fame;

    uint32_t action_attack;

    /** weapon_speed_left * 1000 and cast from float to int for client. */
    float action_timer;

    /** Previous value of action timer sent to the client. */
    float last_action_timer;

    /** Last speed value sent to client. */
    double last_speed;

    /** Last weapon speed value sent to client. */
    double last_weapon_speed;

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
    unsigned int run_on : 1;

    /** Direction (minus one) the player should keep running in. */
    unsigned int run_on_dir : 3;

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

    /** Last tick the player was in combat. */
    long last_combat;

    /** The count of the container. */
    uint32_t container_count;

    /** Count of marked object. */
    uint32_t mark_count;

    /** Skill experience for all skills. */
    int64_t skill_exp[NROFSKILLS];

    /** Number of deaths. */
    uint64_t stat_deaths;

    /** Number of monsters killed. */
    uint64_t stat_kills_mob;

    /** Number of players killed in PvP. */
    uint64_t stat_kills_pvp;

    /** Total damage taken. */
    uint64_t stat_damage_taken;

    /** Total damage dealt. */
    uint64_t stat_damage_dealt;

    /** HP regenerated. */
    uint64_t stat_hp_regen;

    /** Mana regenerated. */
    uint64_t stat_sp_regen;

    /** How many food points have been consumed. */
    uint64_t stat_food_consumed;

    /** Number of food items consumed. */
    uint64_t stat_food_num_consumed;

    /** Amount of HP healed using heal spells. */
    uint64_t stat_damage_healed;

    /** Amount of HP healed using heal spells on friendly targets. */
    uint64_t stat_damage_healed_other;

    /** Amount of HP healed by receiving healing from friendly creatures. */
    uint64_t stat_damage_heal_received;

    /** Number of steps taken. */
    uint64_t stat_steps_taken;

    /** Number of spells cast. */
    uint64_t stat_spells_cast;

    /** Number of seconds played. */
    uint64_t stat_time_played;

    /** Number of seconds spent AFK. */
    uint64_t stat_time_afk;

    /** Cache for value of ::stat_time_played. */
    time_t last_stat_time_played;

    /** Number of arrows/bolts/etc fired. */
    uint64_t stat_arrows_fired;

    /** Number of missiles thrown. */
    uint64_t stat_missiles_thrown;

    /** Number of books read. */
    uint64_t stat_books_read;

    /** Number of unique books read (the ones that give exp). */
    uint64_t stat_unique_books_read;

    /** Number of potions used. */
    uint64_t stat_potions_used;

    /** Number of scrolls used. */
    uint64_t stat_scrolls_used;

    /** Total experience gained. */
    uint64_t stat_exp_gained;

    /** Total experience lost. */
    uint64_t stat_exp_lost;

    /** Total number of items dropped. */
    uint64_t stat_items_dropped;

    /** Total number of items picked up. */
    uint64_t stat_items_picked;

    /** Total number of unique corpses searched. */
    uint64_t stat_corpses_searched;

    /** Number of traps found using the find traps skill. */
    uint64_t stat_traps_found;

    /** Number of traps successfully disarmed. */
    uint64_t stat_traps_disarmed;

    /** Number of traps sprung. */
    uint64_t stat_traps_sprung;

    /** Number of times the player has enabled AFK mode. */
    uint64_t stat_afk_used;

    /** Number of times the player has formed a party. */
    uint64_t stat_formed_party;

    /** Number of times the player has joined a party. */
    uint64_t stat_joined_party;

    /** Number of items the player has renamed an item. */
    uint64_t stat_renamed_items;

    /** Number of times the player has used an emote command. */
    uint64_t stat_emotes_used;

    /**
     * Number of times the player used inscription skill to write in a
     * book.
 */
    uint64_t stat_books_inscribed;

    /** Count of target. */
    uint32_t target_object_count;

    /** Last weight limit sent to client. */
    uint32_t last_weight_limit;

    /** If true, update line of sight with update_los(). */
    uint32_t update_los : 1;

    /** Is the player AFK? */
    uint32_t afk : 1;

    /** Any numbers typed before a command. */
    uint32_t count;

    /** Last ranged weapon speed sent. */
    float last_ranged_ws;

    /**
     * Last attuned spell path sent to client.
 */
    uint32_t last_path_attuned;

    /**
     * Last repelled spell path sent to client.
 */
    uint32_t last_path_repelled;

    /**
     * Last denied spell path sent to client.
 */
    uint32_t last_path_denied;

    /**
     * Last sent UIDs of player's equipment.
 */
    uint32_t last_equipment[PLAYER_EQUIP_MAX];

    /** Last fire/run on flags sent to client. */
    uint16_t last_flags;

    /** Remainder for HP regen. */
    uint16_t gen_hp_remainder;

    /** Remainder for mana regen. */
    uint16_t gen_sp_remainder;

    /** Regeneration speed of HP. */
    uint16_t gen_client_hp;

    /** Regeneration speed of mana. */
    uint16_t gen_client_sp;

    /** Last regeneration of HP sent to client. */
    uint16_t last_gen_hp;

    /** Last regeneration of mana sent to client. */
    uint16_t last_gen_sp;

    /** Table of last skill levels sent to client. */
    int16_t skill_level[NROFSKILLS];

    /** Some anim flags for special player animation handling. */
    uint16_t anim_flags;

    /** Total item power of objects equipped. */
    int16_t item_power;

    /** Last ranged damage sent. */
    int16_t last_ranged_dam;

    /** Last ranged wc sent. */
    int16_t last_ranged_wc;

    /** Table of protections last sent to the client. */
    int8_t last_protection[NROFATTACKS];

    /** Last gender sent to the client. */
    uint8_t last_gender;

    /** If 1, the player is not able to chat. */
    uint8_t no_chat;

    /** Last HP sent to party members. */
    uint8_t last_party_hp;

    /** Last SP sent to party members. */
    uint8_t last_party_sp;

    /** If 1, collision is disabled for this player. */
    uint8_t tcl;

    /** Whether god mode is on or off. */
    uint8_t tgm;

    /** If 1, disable lighting. */
    uint8_t tli;

    /** If 1, LoS is disabled and player can see through walls. */
    uint8_t tls;

    /** If 1, normally invisible items can be seen. */
    uint8_t tsi;

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

    /** Last stats sent to the client. */
    living last_stats;

    /** Pointer to the party this player is member of. */
    party_struct *party;

    /** Start of the movement path queue. */
    player_path *move_path;

    /** End of the movement path queue. */
    player_path *move_path_end;

    /**
     * Player name to reply to.
 */
    char player_reply[64];

    /** Auto-reply message when AFK */
    char afk_auto_reply[MAX_BUF];

    object *talking_to; ///< Object the player is talking to.
    tag_t talking_to_count; ///< ID of ::talking_to.

    player_faction_t *factions;

    long item_power_effects; ///< Next time of item power effects.
} player;

#endif

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Player equipment.
 * @anchor PLAYER_EQUIP_xxx */
enum
{
	/** Armor. */
	PLAYER_EQUIP_MAIL,
	/** Gauntlets. */
	PLAYER_EQUIP_GAUNTLET,
	/** Bracers. */
	PLAYER_EQUIP_BRACER,
	/** Helmet. */
	PLAYER_EQUIP_HELM,
	/** Boots. */
	PLAYER_EQUIP_BOOTS,
	/** Cloak. */
	PLAYER_EQUIP_CLOAK,
	/** Girdle. */
	PLAYER_EQUIP_GIRDLE,
	/** Shield. */
	PLAYER_EQUIP_SHIELD,
	/** Right ring. */
	PLAYER_EQUIP_RRING,
	/** Left ring. */
	PLAYER_EQUIP_LRING,
	/** Amulet. */
	PLAYER_EQUIP_AMULET,
	/** Weapon. */
	PLAYER_EQUIP_WEAPON,
	/** Bow/crossbow/etc. */
	PLAYER_EQUIP_BOW,
	/** Skill item. */
	PLAYER_EQUIP_SKILL_ITEM,
	/** Wand/rod/horn/etc. */
	PLAYER_EQUIP_MAGIC_DEVICE,

	/** Maximum number of equipment. */
	PLAYER_EQUIP_MAX
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

/**
 * Enumerations of the READY_OBJ_xxx constants used by
 * @ref CLIENT_CMD_READY.
 * @anchor READY_OBJ_xxx */
enum
{
	READY_OBJ_ARROW,
	READY_OBJ_THROW,
	READY_OBJ_MAX
};

/** Minimum length a player name must have. */
#define PLAYER_NAME_MIN 2

/** Maximum length a player name can have. */
#define PLAYER_NAME_MAX 12

/** Minimum length a player password must have. */
#define PLAYER_PASSWORD_MIN 2

/** Maximum length a player password can have. */
#define PLAYER_PASSWORD_MAX 30

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

	/** For client: \<Rank\> \<Name\>\n\<Gender\> \<Race\> \<Profession\> */
	char ext_title[MAX_EXT_TITLE];

	/** How much HP the player gained on that level. */
	char levhp[MAXLEVEL + 1];

	/** How much SP the player gained on that level. */
	char levsp[MAXLEVEL + 1];

	/** How much grace the player gained on that level. */
	char levgrace[MAXLEVEL + 1];

	/** Who killed this player. */
	char killer[BIG_NAME];

	/** Holds arbitrary input from client. */
	char write_buf[MAX_BUF];

	/** The player's password. May be encrypted. */
	char password[PLAYER_PASSWORD_MAX + 1];

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

	/** The weapon in our hand. */
	object *selected_weapon;

	/**
	 * The hand-to-hand skill we use when we not using a weapon (like
	 * karate). */
	object *skill_weapon;

	/** Target object. */
	object *target_object;

	/** Pointers to applied items in the player's inventory. */
	object *equipment[PLAYER_EQUIP_MAX];

	/** Quick jump table to skill objects in the player's inventory. */
	object *skill_ptr[NROFSKILLS];

	/** The exp object table. */
	object *last_skill_ob[MAX_EXP_CAT];

	/** Experience objects. */
	object *exp_ptr[MAX_EXP_CAT];

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

	/** Readied objects (arrows, quivers, bolts, etc). */
	object *ready_object[READY_OBJ_MAX];

	/** UIDs of the readied objects. */
	tag_t ready_object_tag[READY_OBJ_MAX];

	/** For the client target HP marker. */
	char target_hp;

	/** Skill number of used weapon skill for fast access. */
	int set_skill_weapon;

	/** Skill number of used archery skill for fast access. */
	int set_skill_archery;

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

	/** weapon_speed_left * 1000 and cast from float to int for client. */
	int action_timer;

	/** Previous value of action timer sent to the client. */
	int last_action_timer;

	/** Last speed value sent to client. */
	float last_speed;

	/** Weapon speed index (mainly used for client). */
	char weapon_sp;

	/** Last weapon speed index. */
	char last_weapon_sp;

	/** Last experience category level sent to client. */
	char last_skill_level[MAX_EXP_CAT];

	/** The CS_STATS_ id for client STATS cmd. */
	uint8 last_skill_id[MAX_EXP_CAT];

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

	/** Bonuses to regeneration speed of grace. */
	signed char gen_grace;

	/** Input state of the player (name, password, etc). */
	unsigned char state;

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

	/** Used for action timer for spell casting. */
	uint32 action_casting;

	/** Used for action timer for ranged attacks. */
	uint32 action_range;

	/** Count of marked object. */
	uint32 mark_count;

	/** Last skill category experience sent to client. */
	sint64 last_skill_exp[MAX_EXP_CAT];

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

	/** Grace regenerated. */
	uint64 stat_grace_regen;

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

	/** Number of prayers cast. */
	uint64 stat_prayers_cast;

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
	uint32 update_los:1;

	/** If true, player is in combat mode. */
	uint32 combat_mode:1;

	/** If true, player is praying and regaining grace. */
	uint32 praying:1;

	/** Internal use by praying to send pray message to player. */
	uint32 was_praying:1;

	/** Is the player AFK? */
	uint32 afk:1;

	/** Update skill list when set. */
	uint32 update_skills:1;

	/** Any numbers typed before a command. */
	uint32 count;

	/** Last ranged weapon speed sent. */
	sint32 last_ranged_ws;

	/** Type of readied spell. */
	sint16 chosen_spell;

	/** Last fire/run on flags sent to client. */
	uint16 last_flags;

	/** Remainder for HP regen. */
	uint16 gen_hp_remainder;

	/** Remainder for mana regen. */
	uint16 gen_sp_remainder;

	/** Remainder for grace regen. */
	uint16 gen_grace_remainder;

	/** Regeneration speed of HP. */
	uint16 gen_client_hp;

	/** Regeneration speed of mana. */
	uint16 gen_client_sp;

	/** Regeneration speed of grace. */
	uint16 gen_client_grace;

	/** Last regeneration of HP sent to client. */
	uint16 last_gen_hp;

	/** Last regeneration of mana sent to client. */
	uint16 last_gen_sp;

	/** Last regeneration of grace sent to client. */
	uint16 last_gen_grace;

	/** Table of last skill levels sent to client. */
	sint16 skill_level[NROFSKILLS];

	/** How much our player is encumbered. */
	sint16 encumbrance;

	/** Some anim flags for special player animation handling. */
	uint16 anim_flags;

	/** Number of known spells.. */
	uint16 nrofknownspells;

	/** Spells known by the player. */
	sint16 known_spells[NROFREALSPELLS];

	/** Total item power of objects equipped. */
	sint16 item_power;

	/** IDs of spell quickslots. */
	sint16 spell_quickslots[MAX_QUICKSLOT];

	/** Last ranged damage sent. */
	sint16 last_ranged_dam;

	/** Last ranged wc sent. */
	sint16 last_ranged_wc;

	/** Table of protections last sent to the client. */
	sint8 last_protection[NROFATTACKS];

	/** If 1, the player is not able to shout. */
	uint8 no_shout;

	/** Last HP sent to party members. */
	uint8 last_party_hp;

	/** Last SP sent to party members. */
	uint8 last_party_sp;

	/** Last grace sent to party members. */
	uint8 last_party_grace;

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

	/** Can be less in case of poisoning. */
	living orig_stats;

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
} player;

#endif

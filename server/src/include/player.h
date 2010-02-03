/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

extern _level_color level_color[201];

/** Fire modes submited from client. */
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
	FIRE_MODE_THROW,
	/** Golem. */
	FIRE_MODE_SUMMON
};

/** The range types. */
typedef enum rangetype
{
	/** No range. */
	range_none = 0,
	/** Bow. */
	range_bow = 1,
	/** Magic. */
	range_magic = 2,
	/** A wand. */
	range_wand = 3,
	/** Rod. */
	range_rod = 4,
	/** Scroll. */
	range_scroll = 5,
	/** Horn. */
	range_horn = 6,
	/** Skill. */
	range_skill = 7
} rangetype;

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

#ifdef WIN32
#pragma pack(push,1)
#endif

/** The player structure. */
typedef struct pl_player
{
	/** Pointer to next player, NULL if this is last. */
	struct pl_player *next;

	/** Socket information for this player. */
	socket_struct socket;

	/**
	 * Last sent map. */
	struct mapdef *last_update;

	/** The object representing the player. */
	object *ob;

	/** Which golem is controlled. */
	object *golem;

	/** The weapon in our hand. */
	object *selected_weapon;

	/**
	 * The hand-to-hand skill we use when we not using a weapon (like
	 * karate). */
	object *skill_weapon;

	/** Target object. */
	object *target_object;

	/** Experience object pointer to man) defining exp object. */
	object *sp_exp_ptr;

	/** Experience object pointer to grace defining exp object. */
	object *grace_exp_ptr;

	/** Pointers to applied items in the player's inventory. */
	object *equipment[PLAYER_EQUIP_MAX];

	/** Quick jump table to skill objects in the player's inventory. */
	object *skill_ptr[NROFSKILLS];

	/** The exp object table. */
	object *last_skill_ob[MAX_EXP_CAT];

	/** Marked object. */
	object *mark;

	/** Quick jump to our age force. */
	object *age_force;

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

	/** For the client target HP marker. */
	int target_hp;

	/** Skill number of used weapon skill for fast access. */
	int set_skill_weapon;

	/** Skill number of used archery skill for fast access. */
	int set_skill_archery;

	/** X coordinate of respawn (savebed). */
	int bed_x;

	/** Y coordinate of respawn (savebed). */
	int bed_y;

	/** firemode_xxx are set from command_fire() */
	int firemode_type;

	/** ID of the object being thrown. */
	int firemode_tag1;

	/** ID of the object being used as ammunition for bow/crossbow/etc. */
	int firemode_tag2;

	/** Number of items the player has in his shop. */
	int shop_items_count;

	/**
	 * Array showing what spaces the player can see. For maps smaller
	 * than MAP_CLIENT_.., the upper left is used. */
	int blocked_los[MAP_CLIENT_X][MAP_CLIENT_Y];

	/** How much HP the player regenerates every tick. */
	int reg_hp_num;

	/** How much SP the player regenerates every tick. */
	int reg_sp_num;

	/** How much grace the player regenerates every tick. */
	int reg_grace_num;

	/** This is initialized from init_player_exp(). */
	int last_skill_index;

	/** weapon_speed_left * 1000 and cast from float to int for client. */
	float action_timer;

	/** Previous value of action timer sent to the client. */
	float last_action_timer;

	/** Last speed value sent to client. */
	float last_speed;

	/** Name of the map the player is on. */
	char maplevel[MAX_BUF];

	/** For the client target HP real % value. */
	char target_hp_p;

	/** Weapon speed index (mainly used for client). */
	char weapon_sp;

	/** Last weapon speed index. */
	char last_weapon_sp;

	/** Last experience category level sent to client. */
	char last_skill_level[MAX_EXP_CAT];

	/** The CS_STATS_ id for client STATS cmd. */
	char last_skill_id[MAX_EXP_CAT];

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

	/** 2 (seed) + 11 (crypted) + 1 (EOS) + 2 (safety) = 16 */
	char password[16];

	/** Player the DM Is following. */
	char followed_player[BIG_NAME];

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

	/** Player should fire object, not move. */
	unsigned char fire_on;

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

	/** Which golem is controlled - the ID count. */
	uint32 golem_count;

	/** Used for action timer for spell casting. */
	uint32 action_casting;

	/** Used for action timer for ranged attacks. */
	uint32 action_range;

	/** Count of marked object. */
	uint32 mark_count;

	/** Last skill category experience sent to client. */
	sint32 last_skill_exp[MAX_EXP_CAT];

	/** Skill experience for all skills. */
	sint32 skill_exp[NROFSKILLS];

	/** Count of target. */
	uint32 target_object_count;

	/** Last target search position. */
	uint32 target_map_pos;

	/** Last weight limit sent to client. */
	uint32 last_weight_limit;

	/**
	 * This flag is set when the player is loaded from file
	 * and not just created. It is used to overrule the "no save
	 * when exp is 0" rule - which can lead to inventory duping. */
	uint32 player_loaded:1;

	/** If true, update line of sight with update_los(). */
	uint32 update_los:1;

	/** If true, player is in combat mode. */
	uint32 combat_mode:1;

	/** If true, player is praying and regaining grace. */
	uint32 praying:1;

	/** Internal use by praying to send pray message to player. */
	uint32 was_praying:1;

	/**
	 * If true, no message to other players about entering the game, and
	 * no entry in /who list. */
	uint32 dm_stealth:1;

	/** If true, all maps are shown in daylight for the player. */
	uint32 dm_light:1;

	/** Player was removed from a map. Used by /resetmap command. */
	uint32 dm_removed_from_map:1;

	/** Is the player AFK? */
	uint32 afk:1;

	/** Is metaserver privacy activated? */
	uint32 ms_privacy:1;

	/** True if you know the spell of the wand */
	uint32 known_spell:1;

	/** Update skill list when set. */
	uint32 update_skills:1;

	/** Any numbers typed before a command. */
	uint32 count;

	/** Type of readied spell. */
	sint16 chosen_spell;

	/** Last fire/run on flags sent to client. */
	uint16 last_flags;

	/** Real tick counter for HP regenerations. */
	sint16 base_hp_reg;

	/** Real tick counter for mana regenerations. */
	sint16 base_sp_reg;

	/** Real tick counter for grace regenerations. */
	sint16 base_grace_reg;

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

	/** Condition adjusted damage sent to client. */
	sint16 client_dam;

	/** The age of our player. */
	sint16 age;

	/** Unnatural changes to our age. */
	sint16 age_add;

	/** Permanent age changes... Very bad, or good when younger */
	sint16 age_changes;

	/** Maximum age of our player. */
	sint16 age_max;

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

	/** Table of protections last sent to the client. */
	sint8 last_protection[NROFPROTECTIONS];

	/** Which range attack is being used by player. */
	rangetype shoottype;

	/** Can be less in case of poisoning. */
	living orig_stats;

	/** Last stats sent to the client. */
	living last_stats;

	/**
	 * Player shop structure, with linked list of items the player is
	 * selling. */
	player_shop *shop_items;

	/** Pointer to the party this player is member of. */
	partylist_struct *party;
} player;

#ifdef WIN32
#pragma pack(pop)
#endif

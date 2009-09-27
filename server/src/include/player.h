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

/**
 * @file
 * Handles player related structures, enums and defines. */

/** Level color structure. */
typedef struct _level_color
{
	/** Green level */
	int green;

	/** Blue level */
	int blue;

	/** Yellow level */
	int yellow;

	/** Orange level */
	int orange;

	/** Red level*/
	int red;

	/** Purple level */
	int purple;
}_level_color;

extern _level_color level_color[201];

/** Fire modes submited from client */
enum
{
	FIRE_MODE_NONE = -1,
	FIRE_MODE_BOW,
	FIRE_MODE_SPELL,
	FIRE_MODE_WAND,
	FIRE_MODE_SKILL,
	FIRE_MODE_THROW,
	FIRE_MODE_SUMMON
};

typedef enum rangetype
{
	range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
	range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
	range_skill = 7,range_potion = 8, range_dust = 9,
	range_size = 10
} rangetype;

/** Used for item damage system */
enum
{
	PLAYER_EQUIP_MAIL,
	PLAYER_EQUIP_GAUNTLET,
	PLAYER_EQUIP_BRACER,
	PLAYER_EQUIP_HELM,
	PLAYER_EQUIP_BOOTS,
	PLAYER_EQUIP_CLOAK,
	PLAYER_EQUIP_GIRDLE,
	PLAYER_EQUIP_SHIELD,

	/* this must 1 entry before LRING! */
	PLAYER_EQUIP_RRING,
	PLAYER_EQUIP_LRING,
	PLAYER_EQUIP_AMULET,
	PLAYER_EQUIP_WEAPON1,
	PLAYER_EQUIP_BOW,

	/* last index */
	PLAYER_EQUIP_MAX
};

/**
 * Party structure.
 * @todo Move this to a new file, party.h */
typedef struct party_struct
{
	/** ID of the party. */
	sint16 partyid;

	/** Name of the party leader */
	const char *partyleader;

	/** Password this party requires */
	char passwd[7];

	/** Name of the party */
	char *partyname;

#ifdef PARTY_KILL_LOG
	struct party_kill
	{
		/** Name of the killer */
		char killer[MAX_NAME + 1];

		char dead[MAX_NAME + 1];

		/** Exp gained */
		uint32 exp;
	} party_kills[PARTY_KILL_LOG];
#endif

	/** Total experience gained */
	uint32 total_exp;

	/** Total of kills */
	uint32 kills;

	/** Next party in the list */
	struct party_struct *next;
} partylist;

/* we can include more flags here... */
#define PLAYER_AFLAG_NO 0
#define PLAYER_AFLAG_FIGHT 1	/* if this flag is set, show player fight animation */
#define PLAYER_AFLAG_ENEMY 2	/* if this flag is set at END of a animation,
* set fight flag and clear this flag. It is set in hit_player()
* when the player swings to an enemy
*/
#define PLAYER_AFLAG_ADDFRAME 4	/* intern */

#ifdef WIN32
#pragma pack(push,1)
#endif

/** Player structure */
typedef struct pl_player
{
	/** Pointer to next player, NULL if this is last */
	struct pl_player *next;

	/** Socket information for this player */
	NewSocket socket;

	/** Name of the map the player is on */
	char maplevel[MAX_BUF];

	/**
	 * When moving on tiled maps, player can change
	 * map without triggering mapevents and new_map_cmd.
	 * This will break client cache and script events.
	 * This value will used as control value.*/
	struct mapdef *last_update;

	/** The object representing the player */
	object *ob;

	/** Which golem is controlled */
	object *golem;

	/** Which enemy we have. Needed to skip extra swing animation */
	object *anim_enemy;

	/** The weapon in our hand */
	object *selected_weapon;

	/**
	 * The hand-to-hand skill we use when we not using a weapon (like
	 * karate) */
	object *skill_weapon;

	/** Target */
	object *target_object;

	/** Experience object pointer to sp (mana) defining exp object */
	object *sp_exp_ptr;

	/** Experience object pointer to grace defining exp object */
	object *grace_exp_ptr;

	/** Pointers to applied items in the player's inventory */
	object *equipment[PLAYER_EQUIP_MAX];

	/** Quick jump table to skill objects in the player's inventory */
	object *skill_ptr[NROFSKILLS];

	/** The exp object table */
	object *last_skill_ob[MAX_EXP_CAT];

	/** Marked object */
	object *mark;

	/** Quick jump to our age force */
	object *age_force;

	/** Pointer used from local map player chain */
	object *map_below;

	/** Pointer used from local map player chain */
	object *map_above;

	/** Current container being used. */
	object *container;

	/** The count of the container */
	uint32 container_count;

	/**
	 * Pints to a PLAYER ob, accessing this container too!
	 * If this is NULL, we are the "last" one looking in ->container. */
	object *container_above;

	/**
	 * Same as container_above - if this is NULL, we are "last" looking
	 * the container */
	object *container_below;

	/**
	 * hm, this can be kicked now - i do it for a quick hack to
	 * implement the animations. use is_melee_range() instead. */
	uint32 anim_enemy_count;

	/** For the client target HP marker - special shadow */
	int target_hp;

	/** Skill number of used weapon skill for fast access */
	int set_skill_weapon;

	/** Skill number of used archery skill for fast access */
	int set_skill_archery;

	/** X coordinate of respawn (savebed) */
	int bed_x;

	/** Y coordinate of respawn (savebed) */
	int bed_y;

	/** Which golem is controlled - the ID count */
	uint32 golem_count;

	/** firemode_xxx are set from command_fire() */
	int firemode_type;

	int firemode_tag1;

	int firemode_tag2;

	/** Skill action timers - used for action delays like cast time */
	uint32 action_casting;

	uint32 action_range;

	/** weapon_speed_left * 1000 and cast from float to int for client */
	float action_timer;

	/** Previous value of action timer sent to the client */
	float last_action_timer;

	/** Count of mark object */
	uint32 mark_count;

	/** Shadow register. If != exp. obj update client */
	sint32 last_skill_exp[MAX_EXP_CAT];

	/** Shadow register for updating skill values to client */
	sint32 skill_exp[NROFSKILLS];

	/**
	 * Count of target */
	uint32 target_object_count;

	/** Last target search position */
	uint32 target_map_pos;

	float last_speed;

	/** Condition adjusted damage sent to client */
	sint16 client_dam;

	/** The age of our player */
	sint16 age;

	/** Unnatural changes to our age - can be removed by restoration */
	sint16 age_add;

	/** Permanent age changes... Very bad, or good when younger */
	sint16 age_changes;

	/** Maximum age of our player */
	sint16 age_max;

	/** Shadow register for updint skill levels to client */
	sint16 skill_level[NROFSKILLS];

	/** How much our player is encumbered */
	sint16 encumbrance;

	/** Some anim flags for special player animation handling */
	uint16 anim_flags;

	/** Index in the anim_flags array */
	uint16 nrofknownspells;

	/** Spells known by the player */
	sint16 known_spells[NROFREALSPELLS];

	/** For the client target HP real % value */
	char target_hp_p;

	/** Seapon speed index (mainly used for client) */
	char weapon_sp;

	/** Any bonuses/penalties to digestion */
	signed char digestion;

	/** Penalty to sp regen from armour */
	signed char gen_sp_armour;

	/** Bonuses to regeneration speed of hp */
	signed char gen_hp;

	/** Bonuses to regeneration speed of sp */
	signed char gen_sp;

	/** Bonuses to regeneration speed of grace */
	signed char gen_grace;

	/** How much HP the player regenerates every tick */
	int reg_hp_num;

	/** How much SP the player regenerates every tick */
	int reg_sp_num;

	/** How much grace the player regenerates every tick */
	int reg_grace_num;

	/** Real tick counter for hp regenerations */
	sint16 base_hp_reg;

	/** Real tick counter for sp regenerations */
	sint16 base_sp_reg;

	/** Real tick counter for grace regenerations */
	sint16 base_grace_reg;

	/** Bonuses to regeneration speed of hp */
	uint16 gen_client_hp;

	/** Bonuses to regeneration speed of sp */
	uint16 gen_client_sp;

	/** Bonuses to regeneration speed of grace */
	uint16 gen_client_grace;

	uint16 last_gen_hp;

	uint16 last_gen_sp;

	uint16 last_gen_grace;

	char last_weapon_sp;

	/** Shadow register client exp group for level */
	char last_skill_level[MAX_EXP_CAT];

	/** The CS_STATS_ id for client STATS cmd */
	char last_skill_id[MAX_EXP_CAT];

	char firemode_name[BIG_NAME * 2];

	/** Rank + name +" the xxxx" */
	char quick_name[BIG_NAME * 3];

	/** Map where player will respawn after death */
	char savebed_map[MAX_BUF];

	/* for smaller map sizes, only the the first elements are used (ie, upper left) */

	/* in fact we only need char size, but use int for faster access */
	int blocked_los[MAP_CLIENT_X][MAP_CLIENT_Y];

	/** For client: <Rank> <Name>\n<Gender> <Race> <Profession> */
	char ext_title[MAX_EXT_TITLE];

	/** How much HP the player gained on that level */
	char levhp[MAXLEVEL + 1];

	/** How much SP the player gained on that level */
	char levsp[MAXLEVEL + 1];

	/** How much grace the player gained on that level */
	char levgrace[MAXLEVEL + 1];

	/** shadow register for client update resistance table */
	sint8 last_protection[NROFPROTECTIONS];

	/**
	 * This flag is set when the player is loaded from file
	 * and not just created. It is used to overrule the "no save
	 * when exp is 0" rule - which can lead inventory duping. */
	uint32 player_loaded:1;

	/** If true, the player has set a name. */
	uint32 name_changed:1;

	/** If true, update_los() in draw(), and clear */
	uint32 update_los:1;

	/** If true, player is in combat mode */
	uint32 combat_mode:1;

	/** If true, player is praying and gaining fast grace */
	uint32 praying:1;

	/** Internal use by praying to send pray message to player */
	uint32 was_praying:1;

	/**
	 * If true, no message to other players about entering the game, and
	 * no entry in /who list. */
	uint32 dm_stealth:1;

	/** If true, all maps are shown in daylight for the player */
	uint32 dm_light:1;

	/** Internal dm flag: player was removed from a map. Used by /resetmap */
	uint32 dm_removed_from_map:1;

	/** True if you know the spell of the wand */
	uint32 known_spell:1;

	/** What was last updated with draw_stats() */
	uint32 last_known_spell:1;

	/** Update skill list when set */
	uint32 update_skills:1;

	/** Which range attack is being used by player */
	rangetype shoottype;

	/** What was last updated with draw_stats() */
	rangetype last_shoot;

	/** Type of readied spell */
	sint16 chosen_spell;

	/** Type of spell that the item fires */
	sint16 chosen_item_spell;

	/** fire/run on flags for last tick */
	uint16 last_flags;

	/** Any numbers typed before a command */
	uint32 count;

	/** This is initialized from init_player_exp() */
	int last_skill_index;

	unsigned char state;

	/** Which priority will be used in info_all */
	unsigned char listening;

	unsigned char fire_on;

	unsigned char run_on;

	/** Last weight limit transmitted to client */
	uint32 last_weight_limit;

	/** Can be less in case of poisoning */
	living orig_stats;

	/** Last stats drawn with draw_stats() */
	living last_stats;

	/** Same usage as last_stats */
	signed long last_value;

	long last_weight;

	unsigned char last_level;

	/** Who killed this player. */
	char killer[BIG_NAME];

	/**
	 * Last player that told you something, used for /reply command.
	 * @todo The client should handle this instead of the server. */
	char last_tell[MAX_NAME];

	char write_buf[MAX_BUF];

	/** 2 (seed) + 11 (crypted) + 1 (EOS) + 2 (safety) = 16 */
	char password[16];

#ifdef SAVE_INTERVAL
	time_t last_save_time;
#endif

#ifdef AUTOSAVE
	long last_save_tick;
#endif

	/** ID of the party the player is in. */
	sint16 party_number;

	/** Is the player AFK? */
	sint16 afk;

#ifdef SEARCH_ITEMS
	char search_str[MAX_BUF];
#endif

	int apartment_invite;

	/**
	 * Player shop structure, with linked list of items the player is
	 * selling. */
	player_shop *shop_items;

	/** Number of items the player has in his shop. */
	int shop_items_count;

	/* i disabled this now - search for last_used in the code.
	  * perhaps we need this in the future. */
#if 0
	/* Pointer to object last picked or applied */
	object *last_used;

	/* Safety measures to be sure it's the same */
	long last_used_id;
#endif
} player;

#ifdef WIN32
#pragma pack(pop)
#endif

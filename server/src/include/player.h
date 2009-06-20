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

typedef struct _level_color
{
	int green;
	int blue;
	int yellow;
	int orange;
	int red;
	int purple;
}_level_color;

extern _level_color level_color[201];

/* fire modes submited from client */
enum {
	FIRE_MODE_NONE = -1,
	FIRE_MODE_BOW,
	FIRE_MODE_SPELL,
	FIRE_MODE_WAND,
	FIRE_MODE_SKILL,
	FIRE_MODE_THROW,
	FIRE_MODE_SUMMON
};

typedef enum rangetype {
  	range_bottom = -1, range_none = 0, range_bow = 1, range_magic = 2,
  	range_wand = 3, range_rod = 4, range_scroll = 5, range_horn = 6,
  	range_skill = 7,range_potion = 8, range_dust = 9,
  	range_size = 10
} rangetype;


typedef enum usekeytype {
    key_inventory = 0,
    keyrings = 1,
    containers = 2
} usekeytype;

/* used for item damage system */
enum {
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
	PLAYER_EQUIP_MAX
	/* last index */
};

/* not really the player, but tied pretty closely */
typedef struct party_struct {
  	sint16 partyid;
  	const char *partyleader;
  	char passwd[7];
  	struct party_struct *next;
  	char *partyname;
#ifdef PARTY_KILL_LOG
  	struct party_kill
  	{
    	char killer[MAX_NAME + 1], dead[MAX_NAME + 1];
    	uint32 exp;
  	} party_kills[PARTY_KILL_LOG];
#endif
  	uint32 total_exp, kills;
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

/* slowly reworking this struct - some old values in - MT2003 */
typedef struct pl_player
{
	/* this is not cleared with memset - seek for offsetof((....,maplevel) */

	/* Pointer to next player, NULL if this is last */
	struct pl_player *next;

	/* Socket information for this player */
	NewSocket socket;

	/* all this is set to 0 with memset */

	/* Name of the map the player is on */
	char maplevel[MAX_BUF];

	/* when moving on tiled maps, player can change
	 * map without triggering mapevents and new_map_cmd.
	 * This will break client cache and script events.
	 * This value will used as control value.*/
	struct mapdef *last_update;

	/* The object representing the player */
	object *ob;

	/* Which golem is controlled */
	object *golem;

	/* which enemy we hack. needed to skip extra swing animation */
	object *anim_enemy;

	/* thats the weapon in our hand */
    object *selected_weapon;

	/* thats the hth skill we use when we not use a weapon (like karate) */
	object *skill_weapon;

	/* our target */
	object *target_object;

	/* exp object pointer to sp (mana) defining exp object */
	object *sp_exp_ptr;

	/* exp object pointer to grace (mana) defining exp object */
	object *grace_exp_ptr;

	/* pointers to applied items in the players inventory */
	object *equipment[PLAYER_EQUIP_MAX];

	/* quick jump table to skill objects in the players inv. */
	object *skill_ptr[NROFSKILLS];

	/* the exp object table */
	object *last_skill_ob[MAX_EXP_CAT];

	/* marked object */
	object *mark;

	/* quick jump to our age force */
	object *age_force;

	/* ptr used from local map player chain */
	object *map_below;

	object *map_above;

	/* Current container being used. */
	object *container;

	/* the count of the container */
	uint32 container_count;

	/* that points to a PLAYER ob, accessing this container too!
	 * if this is NULL, we are the "last" one looking in ->container. */
	object *container_above;

	/* same as above - if this is NULl, we are "last" looking the container */
	object *container_below;

	/* hm, this can be kicked now - i do it for a quick hack to
	 * implement the animations. use is_melee_range() instead. */
	uint32 anim_enemy_count;

	/* for the client target HP marker - special shadow */
	int target_hp;

	/* skill number of used weapon skill for fast access */
	int set_skill_weapon;

	/* same for archery */
	int set_skill_archery;

	/* x, y - coordinates of respawn (savebed) */
	int bed_x;

	int bed_y;

	/* Which golem is controlled - the id count */
	uint32 golem_count;

	/* firemode_xxx are set from command_fire() */
	int firemode_type;

	int firemode_tag1;

	int firemode_tag2;

	/* "skill action timers" - used for action delays like cast time */
	uint32 action_casting;

	uint32 action_range;

	/* weapon_speed_left * 1000 and cast from float to int for client */
	float action_timer;

	/* previous value sent to the client */
    float last_action_timer;

	/* count or mark object */
	uint32 mark_count;

	/* shadow register. if != exp. obj update client */
	sint32 last_skill_exp[MAX_EXP_CAT];

	/* shadow register for updating skill values to client */
	sint32 skill_exp[NROFSKILLS];

	/* count of target - NOTE: if we target ourself, this count it 0
	 * this will kick us out of enemy target routines - all functions
	 * who handle self target know it and use only target_object -
	 * for our own player object the pointer will never change for us. */
	uint32 target_object_count;

	/* last target search position */
	uint32 target_map_pos;

	/* Mode of player for pickup. */
	uint32 mode;

	float last_speed;

	/* condition adjusted damage send to client */
	sint16 client_dam;

	/* the age of our player */
    sint16 age;

	/* unnatural changes to our age - can be removed by restoration */
    sint16 age_add;

	/* permanent changes .... very bad (or good when younger) */
    sint16 age_changes;

	/* the age of our player */
    sint16 age_max;

	/* shadow register for updint skill levels to client */
	sint16 skill_level[NROFSKILLS];

	/*  How much our player is encumbered  */
	sint16 encumbrance;

	/* some anim flags for special player animation handling */
	uint16 anim_flags;

	/* Index in the above array */
	uint16 nrofknownspells;

	/* Spells known by the player */
	sint16 known_spells[NROFREALSPELLS];

	/* for the client target HP real % value*/
	char target_hp_p;

	/* weapon speed index (mainly used for client) */
	char weapon_sp;

	/* Any bonuses/penalties to digestion */
    signed char digestion;

	/* Penalty to sp regen from armour */
	signed char gen_sp_armour;

	/* Bonuses to regeneration speed of hp */
	signed char gen_hp;

	/* Bonuses to regeneration speed of sp */
	signed char gen_sp;

	/* Bonuses to regeneration speed of grace */
	signed char gen_grace;

	/* thats how much every reg tick we get */
	int reg_hp_num;

	int reg_sp_num;

	int reg_grace_num;

	/* our real tick counter for hp regenerations */
	sint16 base_hp_reg;

	/* our real tick counter for sp regenerations */
	sint16 base_sp_reg;

	/* our real tick counter for grace regenerations */
	sint16 base_grace_reg;

	/* send to client - shadow & prepared gen_xx values */

	/* Bonuses to regeneration speed of hp */
	uint16 gen_client_hp;

	/* Bonuses to regeneration speed of sp */
	uint16 gen_client_sp;

	/* Bonuses to regeneration speed of grace */
	uint16 gen_client_grace;

	uint16 last_gen_hp;

	uint16 last_gen_sp;

	uint16 last_gen_grace;

	char last_weapon_sp;

	/* shadow register client exp group for level */
	char last_skill_level[MAX_EXP_CAT];

	/* Thats the CS_STATS_ id for client STATS cmd */
	char last_skill_id[MAX_EXP_CAT];

	char firemode_name[BIG_NAME * 2];

	/* thats rank + name +" the xxxx" */
	char quick_name[BIG_NAME * 3];

	/* map where player will respawn after death */
	char savebed_map[MAX_BUF];

	/* for smaller map sizes, only the the first elements are used (ie, upper left) */

	/* in fact we only need char size, but use int for faster access */
	int blocked_los[MAP_CLIENT_X][MAP_CLIENT_Y];

	/* for client: <Rank> <Name>\n<Gender> <Race> <Profession> */
	char ext_title[MAX_EXT_TITLE];

	/* What the player gained on that level */
	char levhp[MAXLEVEL + 1];

	char levsp[MAXLEVEL + 1];

	char levgrace[MAXLEVEL + 1];

	/* shadow register for client update resistance table */
	sint8 last_protection[NROFPROTECTIONS];

	/* this flags is set when the player is loaded from file
	 * and not just created. It is used to overrule the "no save
	 * when exp is 0" rule - which can lead inventory duping. */
	uint32 player_loaded:1;

	/* If true, the player has set a name. */
	uint32 name_changed:1;

	/* If true, update_los() in draw(), and clear */
	uint32 update_los:1;

	/* if true, player is in combat mode, attacking with weapon */
	uint32 combat_mode:1;

	/* if true, player is praying and gaining fast grace */
	uint32 praying:1;

	/* internal used by praying to send pray msg to player */
	uint32 was_praying:1;

	/* some dm flags */

	/* 1 = no "XX enter the game" and no entry in /who */
	uint32 dm_stealth:1;

	/* 1 = all maps are shown in daylight for the dm */
	uint32 dm_light:1;

	/* internal dm flag: player was removed from a map */
	uint32 dm_removed_from_map:1;

	/* all values before this line are tested and proofed */

	/* True if you know the spell of the wand */
  	uint32 known_spell:1;

	/* What was last updated with draw_stats() */
  	uint32 last_known_spell:1;

	/* update skill list when set */
  	uint32 update_skills:1;
#ifdef EXPLORE_MODE
	/* if True, player is in explore mode */
  	uint32 explore:1;
#endif

	/* Which range-attack is being used by player */
  	rangetype shoottype;

	/* What was last updated with draw_stats() */
  	rangetype last_shoot;

	/* Method for finding keys for doors */
  	usekeytype usekeys;

	/* Type of readied spell */
  	sint16 chosen_spell;

	/* Type of spell that the item fires */
  	sint16 chosen_item_spell;

	/* fire/run on flags for last tick */
  	uint16 last_flags;

	/* Any numbers typed before a command */
  	uint32 count;

	/* this is init from init_player_exp() */
  	int last_skill_index;

	unsigned char state;

	/* Which priority will be used in info_all */
	unsigned char listening;

	unsigned char fire_on;

	unsigned char run_on;

	/* How long this player has been idle */
	unsigned char idle;

	/* Last weight limit transmitted to client */
	uint32  last_weight_limit;

	/* Can be less in case of poisoning */
	living orig_stats;

	/* Last stats drawn with draw_stats() */
	living last_stats;

	/* Same usage as last_stats */
	signed long last_value;

	long last_weight;

  	unsigned char last_level;

	/* Who killed this player. */
  	char killer[BIG_NAME];

  	char last_cmd;

	/* last player that told you something [mids 01/14/2002] */
	/* this is a typcial client part - no need to use the server
	 * to store or handle this! */
  	char last_tell[MAX_NAME];

  	char write_buf[MAX_BUF];

  	char input_buf[MAX_BUF];

	/* 2 (seed) + 11 (crypted) + 1 (EOS) + 2 (safety) = 16 */
  	char password[16];
#ifdef SAVE_INTERVAL
  	time_t last_save_time;
#endif

#ifdef AUTOSAVE
  	long last_save_tick;
#endif

  	sint16 party_number;

	sint16 afk;

	/* used when player wants to join a party
	 * but we will have to get password first
	 * so we have to remember which party to
	 * join */
  	sint16 party_number_to_join;

#ifdef SEARCH_ITEMS
  	char search_str[MAX_BUF];
#endif

	int apartment_invite;

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

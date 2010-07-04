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
 * Various skill related functions. */

/* define the following for skills utility debuging */
/* #define SKILL_UTIL_DEBUG */

#include <global.h>

/** Array of experience objects in the game. */
static object *exp_cat[MAX_EXP_CAT];
/** Current number of experience categories in the game. */
static int nrofexpcat = 0;

/** Table for stat modification of exp */
float stat_exp_mult[MAX_STAT + 1] =
{
	0.0f,   0.01f,  0.1f,   0.3f,   0.5f,
	0.6f,   0.7f,   0.8f,   0.85f,  0.9f,
	0.95f,  0.96f,  0.97f,  0.98f,  0.99f,
	1.0f,   1.01f,  1.02f,  1.03f,  1.04f,
	1.05f,  1.07f,  1.09f,  1.12f,  1.15f,
	1.2f,   1.3f,   1.4f,   1.5f,   1.7f,
	2.0f
};

/**
 * Used for calculating experience gained in calc_skill_exp(). */
static float lev_exp[MAXLEVEL + 1] =
{
	0.0f,     1.0f,     1.11f,    1.75f,    3.2f,
	5.5f,     10.0f,    20.0f,    35.25f,   66.1f,
	137.0f,   231.58f,  240.00f,  247.62f,  254.55f,
	260.87f,  266.67f,  272.00f,  276.92f,  281.48f,
	285.71f,  289.66f,  293.33f,  296.77f,  300.00f,
	303.03f,  305.88f,  308.57f,  311.11f,  313.51f,
	315.79f,  317.95f,  320.00f,  321.95f,  323.81f,
	325.58f,  327.27f,  328.89f,  330.43f,  331.91f,
	333.33f,  334.69f,  336.00f,  337.25f,  338.46f,
	339.62f,  340.74f,  341.82f,  342.86f,  343.86f,
	344.83f,  345.76f,  346.67f,  347.54f,  348.39f,
	349.21f,  350.00f,  350.77f,  351.52f,  352.24f,
	352.94f,  353.62f,  354.29f,  354.93f,  355.56f,
	356.16f,  356.76f,  357.33f,  357.89f,  358.44f,
	358.97f,  359.49f,  360.00f,  360.49f,  360.98f,
	361.45f,  361.90f,  362.35f,  362.79f,  365.22f,
	367.64f,  369.04f,  373.44f,  378.84f,  384.22f,
	389.59f,  395.96f,  402.32f,  410.67f,  419.01f,
	429.35f,  440.68f,  452.00f,  465.32f,  479.63f,
	494.93f,  510.23f,  527.52f,  545.81f,  562.09f,
	580.37f,  599.64f,  619.91f,  640.17f,  662.43f,
	685.68f,  709.93f,  773.17f,  852.41f,  932.65f,
	1013.88f, 1104.11f, 1213.35f, 1324.60f, 1431.86f,
	1542.13f
};

/** Skill category table. */
typedef struct _skill_name_table
{
	/** Name of the category. */
	char *name;
	/** ID of the category. */
	int id;
}_skill_name_table;

/** Array of the skill categories. */
static _skill_name_table skill_name_table[] =
{
	{"agility", CS_STAT_SKILLEXP_AGILITY},
	{"personality", CS_STAT_SKILLEXP_PERSONAL},
	{"mental", CS_STAT_SKILLEXP_MENTAL},
	{"physique", CS_STAT_SKILLEXP_PHYSIQUE},
	{"magic", CS_STAT_SKILLEXP_MAGIC},
	{"wisdom", CS_STAT_SKILLEXP_WISDOM},
	{"", -1}
};

static int item_skill_cs_stat[] =
{
	0,
	CS_STAT_SKILLEXP_AGILITY,
	CS_STAT_SKILLEXP_PERSONAL,
	CS_STAT_SKILLEXP_MENTAL,
	CS_STAT_SKILLEXP_PHYSIQUE,
	CS_STAT_SKILLEXP_MAGIC,
	CS_STAT_SKILLEXP_WISDOM
};

static int change_skill_to_skill(object *who, object *skl);
static int attack_melee_weapon(object *op, int dir);
static int attack_hth(object *pl, int dir, char *string);
static int do_skill_attack(object *tmp, object *op, char *string);

/**
 * Find and assign the skill experience stuff.
 * @param pl Player.
 * @param exp Experience object.
 * @param index Index. */
static void find_skill_exp_name(object *pl, object *exp, int index)
{
	int s;

	for (s = 0; skill_name_table[s].id != -1; s++)
	{
		if (!strcmp(skill_name_table[s].name, exp->name))
		{
			CONTR(pl)->last_skill_ob[index] = exp;
			CONTR(pl)->last_skill_id[index] = skill_name_table[s].id;
			CONTR(pl)->last_skill_index++;
			return;
		}
	}
}

/**
 * Find and return player skill exp level of given index item_skill
 * using CS_STAT_SKILLEXP_xxx
 * @param pl Player.
 * @param item_skill Item skill.
 * @return Found level, 0 on failure. */
int find_skill_exp_level(object *pl, int item_skill)
{
	int s;

	for (s = 0; s < CONTR(pl)->last_skill_index; s++)
	{
		if (CONTR(pl)->last_skill_id[s] == item_skill_cs_stat[item_skill])
		{
			return CONTR(pl)->last_skill_ob[s]->level;
		}
	}

	return 0;
}

/**
 * Find and return player skill exp level name of given index item_skill.
 * @param item_skill The skill experience category to look for.
 * @return Name of the skill experience category. */
char *find_skill_exp_skillname(int item_skill)
{
	if (!item_skill || item_skill > 6)
	{
		return skill_name_table[6].name;
	}

	return skill_name_table[item_skill - 1].name;
}

/**
 * Main skills use function similar in scope to cast_spell().
 *
 * We handle all requests for skill use outside of some combat here.
 * We require a separate routine outside of fire() so as to allow monsters
 * to utilize skills.
 * @param op The object actually using the skill.
 * @param dir The direction in which the skill is used.
 * @return 0 on failure of using the skill, non-zero otherwise. */
sint64 do_skill(object *op, int dir)
{
	sint64 success = 0;
	int skill = op->chosen_skill->stats.sp;

	switch (skill)
	{
		case SK_KARATE:
			attack_hth(op, dir, "karate-chopped");
			break;

		case SK_BOXING:
			attack_hth(op, dir, "punched");
			break;

		case SK_MELEE_WEAPON:
		case SK_SLASH_WEAP:
		case SK_CLEAVE_WEAP:
		case SK_PIERCE_WEAP:
			attack_melee_weapon(op, dir);
			break;

		case SK_FIND_TRAPS:
			success = find_traps(op, CONTR(op)->exp_ptr[EXP_AGILITY]->level);
			break;

		case SK_REMOVE_TRAP:
			success = remove_trap(op);
			break;

		case SK_THROWING:
			new_draw_info(NDI_UNIQUE, op, "This skill is not usable in this way.");
			break;

		case SK_USE_MAGIC_ITEM:
		case SK_MISSILE_WEAPON:
			new_draw_info(NDI_UNIQUE, op, "There is no special attack for this skill.");
			return success;
			break;

		case SK_PRAYING:
			new_draw_info(NDI_UNIQUE, op, "This skill is not usable in this way.");
			return success;
			break;

		case SK_SPELL_CASTING:
		case SK_BARGAINING:
			new_draw_info(NDI_UNIQUE, op, "This skill is already in effect.");
			return success;
			break;

		case SK_CONSTRUCTION:
			construction_do(op, dir);
			return success;

		default:
			LOG(llevDebug, "%s attempted to use unknown skill: %d\n", query_name(op, NULL), op->chosen_skill->stats.sp);
			return success;
			break;
	}

	/* For players we now update the speed_left from using the skill.
	 * Monsters have no skill use time because of the random nature in
	 * which use_monster_skill is called already simulates this. -b.t. */
	if (op->type == PLAYER)
	{
		op->speed_left -= get_skill_time(op,skill);
	}

	/* This is a good place to add experience for successfull use of skills.
	 * Note that add_exp() will figure out player/monster experience
	 * gain problems. */
	if (success && skills[skill].category != EXP_NONE)
	{
		add_exp(op, success, op->chosen_skill->stats.sp);
	}

	return success;
}

/**
 * Calculates amount of experience can be gained for
 * successfull use of a skill.
 * @param who Player/creature that used the skill.
 * @param op Object that was 'defeated'.
 * @param level Level of the skill. If -1, will get level of who's chosen
 * skill.
 * @return Experience for the skill use. */
sint64 calc_skill_exp(object *who, object *op, int level)
{
	int who_lvl = level;
	sint64 op_exp = 0;
	int op_lvl = 0;
	float exp_mul, max_mul, tmp;

	/* No exp for non players. */
	if (!who || who->type != PLAYER)
	{
		LOG(llevDebug, "DEBUG: calc_skill_exp() called with who != PLAYER or NULL (%s (%s)- %s)\n", query_name(who, NULL), !who ? "NULL" : "", query_name(op, NULL));
		return 0;
	}

	if (level == -1)
	{
		/* The related skill level */
		who_lvl = SK_level(who);
	}

	if (!op)
	{
		LOG(llevBug, "BUG: calc_skill_exp() called with op == NULL (%s - %s)\n", query_name(who, NULL), query_name(op, NULL));
		op_lvl = who->map->difficulty < 1 ? 1: who->map->difficulty;
		op_exp = 0;
	}
	/* All other items/living creatures */
	else
	{
		op_exp = op->stats.exp;
		op_lvl = op->level;
	}

	/* No exp for no level and no exp ;) */
	if (op_lvl < 1 || op_exp < 1)
	{
		return 0;
	}

	if (who_lvl < 2)
	{
		max_mul = 0.85f;
	}

	if (who_lvl < 3)
	{
		max_mul = 0.7f;
	}
	else if (who_lvl < 4)
	{
		max_mul = 0.6f;
	}
	else if (who_lvl < 5)
	{
		max_mul = 0.45f;
	}
	else if (who_lvl < 7)
	{
		max_mul = 0.35f;
	}
	else if (who_lvl < 8)
	{
		max_mul = 0.3f;
	}
	else
	{
		max_mul = 0.25f;
	}

	/* We first get a global level difference multiplicator */
	exp_mul = calc_level_difference(who_lvl, op_lvl);
	op_exp = (int) ((float) op_exp * lev_exp[op_lvl] * exp_mul);
	tmp = ((float) (new_levels[who_lvl + 1] - new_levels[who_lvl]) * 0.1f) * max_mul;

	if ((float) op_exp > tmp)
	{
		op_exp = (int) tmp;
	}

	return op_exp;
}

/**
 * This routine looks through all the assembled archetypes for experience
 * objects then copies their values into the ::exp_cat array. */
static void init_exp_obj()
{
	archetype *at;

	for (at = first_archetype; at; at = at->next)
	{
		if (at->clone.type == EXPERIENCE)
		{
			exp_cat[nrofexpcat] = get_object();
			exp_cat[nrofexpcat]->level = 1;
			exp_cat[nrofexpcat]->stats.exp = 0;
			copy_object(&at->clone, exp_cat[nrofexpcat], 0);
			/* avoid gc on these objects */
			insert_ob_in_ob(exp_cat[nrofexpcat], &void_container);
			nrofexpcat++;

			if (nrofexpcat == MAX_EXP_CAT)
			{
				LOG(llevSystem, "ERROR: Aborting! Reached limit of available experience\n");
				LOG(llevError, "ERROR: categories. Need to increase value of MAX_EXP_CAT.\n");
				exit(0);
			}
		}
	}

	if (!nrofexpcat)
	{
		LOG(llevError, "ERROR: Aborting! No experience objects found in archetypes.\n");
		exit(0);
	}
}

/**
 * Initialize the experience system. */
void init_new_exp_system()
{
	static int init_new_exp_done = 0;
	int i;

	if (init_new_exp_done)
	{
		return;
	}

	init_new_exp_done = 1;

	/* Locate the experience objects and create list of them */
	init_exp_obj();

	for (i = 0; i < NROFSKILLS; i++)
	{
		/* Link the skill archetype ptr to skill list for fast access.
		 * Now we can access the skill archetype by skill number or skill name. */
		if (!(skills[i].at = get_skill_archetype(i)))
		{
			LOG(llevError, "ERROR: Aborting! Skill #%d (%s) not found in archlist!\n", i, skills[i].name);
		}
	}
}

/**
 * Free all previously initialized experience objects. */
void free_exp_objects()
{
	int i;

	for (i = 0; i < MAX_EXP_CAT; i++)
	{
		if (!exp_cat[i])
		{
			continue;
		}

		SET_FLAG(exp_cat[i], FLAG_REMOVED);
		return_poolchunk(exp_cat[i], pool_object);
	}
}

/**
 * Dump debugging information about the skills. */
void dump_skills()
{
	int i;

	LOG(llevInfo, "exper_catgry \t str \t dex \t con \t wis \t cha \t int \t pow \n");

	for (i = 0; i < nrofexpcat; i++)
	{
		LOG(llevInfo, "%d-%s \t %d \t %d \t %d \t %d \t %d \t %d \t %d \n", i, exp_cat[i]->name, exp_cat[i]->stats.Str, exp_cat[i]->stats.Dex, exp_cat[i]->stats.Con, exp_cat[i]->stats.Wis, exp_cat[i]->stats.Cha, exp_cat[i]->stats.Int, exp_cat[i]->stats.Pow);
	}

	LOG(llevInfo, "\n");
	LOG(llevInfo, "%20s  %12s  %4s %4s %4s  %5s %5s %5s\n", "sk#       Skill name", "ExpCat", "Time", "Base", "xlvl", "Stat1", "Stat2", "Stat3");
	LOG(llevInfo, "%20s  %12s  %4s %4s %4s  %5s %5s %5s\n", "---       ----------", "------", "----", "----", "----", "-----", "-----", "-----");

	for (i = 0; i < NROFSKILLS; i++)
	{
		LOG(llevInfo, "%2d-%17s  %12s  %4d\n", i, skills[i].name, exp_cat[skills[i].category] != NULL ? exp_cat[skills[i].category]->name : "NONE", skills[i].time);
	}
}

/**
 * Check if object knows the specified skill.
 * @param op Object we're checking.
 * @param skillnr ID of the skill.
 * @return 1 if the object knows the skill, 0 otherwise. */
int check_skill_known(object *op, int skillnr)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && tmp->stats.sp == skillnr)
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Look up a skill by name.
 * @param string Name of the skill to look for.
 * @return ID of the skill if found, -1 otherwise. */
int lookup_skill_by_name(char *string)
{
	int skillnr = 0;
	size_t nmlen;
	char name[MAX_BUF];

	if (!string)
	{
		return -1;
	}

	strcpy(name, string);
	nmlen = strlen(name);

	for (skillnr = 0; skillnr < NROFSKILLS; skillnr++)
	{
		if (strlen(name) >= strlen(skills[skillnr].name))
		{
			if (!strncmp(name, skills[skillnr].name, MIN(strlen(skills[skillnr].name), nmlen)))
			{
				return skillnr;
			}
		}
	}

	return -1;
}

/**
 * Check skill for firing.
 * @param who Object.
 * @return 1 on success, 0 on failure. */
int check_skill_to_fire(object *who)
{
	int skillnr = 0;
	object *tmp;
	rangetype shoottype = range_none;

	if (who->type != PLAYER)
	{
		return 1;
	}

	switch ((shoottype = CONTR(who)->shoottype))
	{
		case range_bow:
			if (!(tmp = CONTR(who)->equipment[PLAYER_EQUIP_BOW]))
			{
				return 0;
			}

			if (tmp->sub_type == 2)
			{
				skillnr = SK_SLING_WEAP;
			}
			else if (tmp->sub_type == 1)
			{
				skillnr = SK_XBOW_WEAP;
			}
			else
			{
				skillnr = SK_MISSILE_WEAPON;
			}

			break;

		case range_none:
		case range_skill:
			return 1;
			break;

		case range_magic:
			if (spells[CONTR(who)->chosen_spell].type == SPELL_TYPE_PRIEST)
			{
				skillnr = SK_PRAYING;
			}
			else
			{
				skillnr = SK_SPELL_CASTING;
			}

			break;

		case range_scroll:
		case range_rod:
		case range_horn:
		case range_wand:
			skillnr = SK_USE_MAGIC_ITEM;
			break;

		default:
			LOG(llevBug, "BUG: bad call of check_skill_to_fire() from %s\n", query_name(who, NULL));
			return 0;
	}

	if (change_skill(who, skillnr))
	{
#ifdef SKILL_UTIL_DEBUG
		LOG(llevDebug, "check_skill_to_fire(): got skill:%s for %s\n", skills[skillnr].name, who->name);
#endif
		CONTR(who)->shoottype = shoottype;
		return 1;
	}

	return 0;
}

/**
 * When a player tried to use an object which requires a skill this
 * function is called.
 * @param who Player object.
 * @param item The object to apply.
 * @return 1 if it can be applied, 0 otherwise. */
int check_skill_to_apply(object *who, object *item)
{
	int skill = 0, tmp;
	/* perhaps we need a additional skill to use */
	int add_skill = NO_SKILL_READY;

	/* Only for players. */
	if (who->type != PLAYER)
	{
		return 1;
	}

	/* First figure out the required skills from the item */
	switch (item->type)
	{
		case WEAPON:
			tmp = item->sub_type;

			/* Polearm */
			if (tmp >= WEAP_POLE_IMPACT)
			{
				/* Select the right weapon type. */
				tmp = item->sub_type - WEAP_POLE_IMPACT;
				add_skill = SK_POLEARMS;
			}
			/* Two handed  */
			else if (tmp >= WEAP_2H_IMPACT)
			{
				/* Select the right weapon type. */
				tmp = item->sub_type - WEAP_2H_IMPACT;
				add_skill = SK_TWOHANDS;
			}

			if (tmp == WEAP_1H_IMPACT)
			{
				skill = SK_MELEE_WEAPON;
			}
			else if (tmp == WEAP_1H_SLASH)
			{
				skill = SK_SLASH_WEAP;
			}
			else if (tmp == WEAP_1H_CLEAVE)
			{
				skill = SK_CLEAVE_WEAP;
			}
			else if (tmp == WEAP_1H_PIERCE)
			{
				skill = SK_PIERCE_WEAP;
			}

			break;

		case BOW:
			tmp = item->sub_type;

			if (tmp == RANGE_WEAP_BOW)
			{
				skill = SK_MISSILE_WEAPON;
			}
			else if (tmp == RANGE_WEAP_XBOWS)
			{
				skill = SK_XBOW_WEAP;
			}
			else
			{
				skill = SK_SLING_WEAP;
			}

			break;

		case POTION:
			skill = SK_USE_MAGIC_ITEM;
			break;

		case SCROLL:
			skill = SK_LITERACY;
			break;

		case ROD:
			skill = SK_USE_MAGIC_ITEM;
			break;

		case WAND:
			skill = SK_USE_MAGIC_ITEM;
			break;

		case HORN:
			skill = SK_USE_MAGIC_ITEM;
			break;

		default:
			LOG(llevDebug, "Warning: bad call of check_skill_to_apply()\n");
			LOG(llevDebug, "No skill exists for item: %s\n", query_name(item, NULL));
			return 0;
	}

	/* This should not happen */
	if (skill == NO_SKILL_READY)
	{
		LOG(llevBug, "BUG: check_skill_to_apply() called for %s and item %s with skill NO_SKILL_READY\n", query_name(who, NULL), query_name(item, NULL));
	}

	/* Check the additional skill if there is one */
	if (add_skill != NO_SKILL_READY)
	{
		if (!change_skill(who, add_skill))
		{
			return 0;
		}

		change_skill(who, NO_SKILL_READY);
	}

	/* If this skill is ready, all is fine. if not, ready it, if it can't
	 * readied, we can't apply/use/do it. */
	if (!who->chosen_skill || (who->chosen_skill && who->chosen_skill->stats.sp != skill))
	{
		if (!change_skill(who, skill))
		{
			return 0;
		}
	}

	return 1;
}

/**
 * Makes various checks and initialization of player experience objects.
 * If things aren't cool then we change them here.
 * @param pl Player.
 * @return 1 on success, 0 on failure. */
int init_player_exp(object *pl)
{
	int i, j, exp_index = 0;
	object *tmp, *exp_ob[MAX_EXP_CAT];

	if (pl->type != PLAYER)
	{
		LOG(llevBug, "BUG: init_player_exp(): called non-player %s.\n", query_name(pl, NULL));
		return 0;
	}

	CONTR(pl)->last_skill_index = 0;

	/* First-pass find all current exp objects */
	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == EXPERIENCE)
		{
			exp_ob[exp_index] = tmp;
			CONTR(pl)->exp_ptr[tmp->sub_type] = tmp;
			find_skill_exp_name(pl, tmp, CONTR(pl)->last_skill_index);
			exp_index++;
		}
		else if (exp_index == MAX_EXP_CAT)
		{
			return 0;
		}
	}

	/* Second - if pl has wrong nrof experience objects
	 * then give player a complete roster. */

	/* No exp objects - situation for a new player
	 * or pre-skills/exp code player file */
	if (!exp_index)
	{
		for (j = 0; j < nrofexpcat; j++)
		{
			tmp = get_object();
			copy_object(exp_cat[j], tmp, 0);
			insert_ob_in_ob(tmp, pl);
			tmp->stats.exp = 0;
			exp_ob[j] = tmp;
			CONTR(pl)->exp_ptr[tmp->sub_type] = tmp;

			esrv_send_item(pl, tmp);
			exp_index++;
		}
	}

	/* Now we loop through one more time and set "apply" flag on valid
	 * experience objects. Fix_player() requires this, and we get the
	 * bonus of being able to ignore invalid experience objects in the
	 * player inventory (player file from another game set up?). Also, we
	 * reset the score (or "total" experience) of the player to be the
	 * sum of all valid experience objects. */
	pl->stats.exp = 0;

	for (i = 0; i < exp_index; i++)
	{
		if (!QUERY_FLAG(exp_ob[i], FLAG_APPLIED))
		{
			SET_FLAG(exp_ob[i], FLAG_APPLIED);
		}

		if (pl->stats.exp < exp_ob[i]->stats.exp)
		{
			pl->stats.exp = exp_ob[i]->stats.exp;
			pl->level = exp_ob[i]->level;
		}

		player_lvl_adj(NULL, exp_ob[i]);
	}

	return 1;
}

/**
 * Removes skill from a player skill list and unlinks the pointer to the
 * exp object.
 * @param skillop Skill object. */
void unlink_skill(object *skillop)
{
	object *op = skillop ? skillop->env : NULL;

	if (!op || op->type != PLAYER)
	{
		LOG(llevBug, "BUG: unlink_skill() called for non-player %s!\n", query_name(op, NULL));
		return;
	}

	send_skilllist_cmd(op, skillop, SPLIST_MODE_REMOVE);
	skillop->exp_obj = NULL;
}

/**
 * Linking skills with experience objects and creating a linked list of
 * skills for later fast access.
 * @param pl Player. */
void link_player_skills(object *pl)
{
	int i, cat = 0, sk_index = 0, exp_index = 0;
	object *tmp, *sk_ob[NROFSKILLS], *exp_ob[MAX_EXP_CAT];

	/* We're going to unapply all skills */
	pl->chosen_skill = NULL;
	CONTR(pl)->last_skill_index = 0;

	/* First find all exp and skill objects */
	for (tmp = pl->inv; tmp && sk_index < 1000; tmp = tmp->below)
	{
		if (tmp->type == EXPERIENCE)
		{
			CONTR(pl)->exp_ptr[tmp->sub_type] = tmp;
			exp_ob[tmp->sub_type] = tmp;
			find_skill_exp_name(pl, tmp, CONTR(pl)->last_skill_index);
			tmp->nrof = 1;
			exp_index++;
		}
		else if (tmp->type == SKILL)
		{
			/* For startup, lets unapply all skills */
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			tmp->nrof = 1;
			sk_ob[sk_index] = tmp;
			sk_index++;
		}
	}

	/* Ok, create linked list and link the associated skills to exp objects */
	for (i = 0; i < sk_index; i++)
	{
		cat = skills[sk_ob[i]->stats.sp].category;

		if (cat == EXP_NONE)
		{
			continue;
		}

		sk_ob[i]->exp_obj = exp_ob[cat];
	}
}

/**
 * Links a skill to exp object when applied or learned by a player.
 * @param pl Player.
 * @param skillop Skill object.
 * @return 1 if it can link, 0 otherwise. */
int link_player_skill(object *pl, object *skillop)
{
	object *tmp;
	int cat = skills[skillop->stats.sp].category;

	/* Ok the skill has an exp object, now find right one in pl inv */
	if (cat != EXP_NONE)
	{
		for (tmp = pl->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == EXPERIENCE && tmp->sub_type == cat)
			{
				skillop->exp_obj = tmp;
				break;
			}
		}

		return 1;
	}

	skillop->exp_obj = NULL;
	return 0;
}

/**
 * Player is trying to learn a skill. Success is based on Int.
 * @param pl Player object.
 * @param scroll Scroll object the player is learning this from, can be
 * NULL.
 * @param name Name of the skill to learn.
 * @param skillnr ID of the skill to learn.
 * @param scroll_flag Whether we're learning the skill from a scroll.
 * @retval 0 Player already knows the skill.
 * @retval 1 The player learns the skill.
 * @retval 2 Some failure. */
int learn_skill(object *pl, object *scroll, char *name, int skillnr, int scroll_flag)
{
	object *tmp, *tmp2;
	archetype *skill = NULL;

	if (scroll)
	{
		skill = find_archetype(scroll->slaying);
	}
	else if (name)
	{
		skill = find_archetype(name);
	}
	else
	{
		skill = skills[skillnr].at;
	}

	if (!skill)
	{
		return 2;
	}

	tmp = arch_to_object(skill);

	if (!tmp)
	{
		return 2;
	}

	/* Check if player already has it */
	for (tmp2 = pl->inv; tmp2; tmp2 = tmp2->below)
	{
		if (tmp2->type == SKILL)
		{
			if (tmp2->stats.sp == tmp->stats.sp)
			{
				new_draw_info_format(NDI_UNIQUE, pl, "You already know the skill '%s'!", tmp->name);
				return 0;
			}
		}
	}

	/* Now a random chance to learn, based on player Int */
	if (scroll_flag)
	{
		/* Failure */
		if (rndm(0, 99) > learn_spell[pl->stats.Int])
		{
			return 2;
		}
	}

	/* Everything is cool. Give 'em the skill */
	insert_ob_in_ob(tmp,pl);
	link_player_skill(pl, tmp);
	play_sound_player_only (CONTR(pl), SOUND_LEARN_SPELL,NULL, 0, 0, 0, 0);
	new_draw_info_format(NDI_UNIQUE, pl, "You have learned the skill %s!", tmp->name);

	send_skilllist_cmd(pl, tmp, SPLIST_MODE_ADD);
	esrv_send_item(pl, tmp);

	return 1;
}

/**
 * Similar to invoke command, it executes the skill in the
 * direction that the user is facing.
 * @param op Player trying to use a skill.
 * @param string Parameter for the skill to use.
 * @retval 0 Unable to change to the requested skill, or unable to use
 * the skill properly.
 * @retval 1 Skill correctly used. */
int use_skill(object *op, char *string)
{
	int sknum = -1;

	/* The skill name appears at the begining of the string,
	 * need to reset the string to next word, if it exists. */
	if (string && (sknum = lookup_skill_by_name(string)) >= 0)
	{
		size_t len;

		if (sknum == -1)
		{
			new_draw_info_format(NDI_UNIQUE, op, "Unable to find skill by name %s", string);
			return 0;
		}

		len = strlen(skills[sknum].name);

		/* All this logic goes and skips over the skill name to find any
		 * options given to the skill. */
		if (len >= strlen(string))
		{
			*string = '\0';
		}
		else
		{
			while (len--)
			{
				string++;
			}

			while (*string == ' ')
			{
				string++;
			}
		}

		if (strlen(string) == 0)
		{
			string = NULL;
		}
	}

#ifdef SKILL_UTIL_DEBUG
	LOG(llevDebug, "use_skill(): got skill: %s\n", sknum > -1 ? skills[sknum].name : "none");
#endif

	/* Change to the new skill, then execute it. */
	if (change_skill(op, sknum))
	{
		if (op->chosen_skill->sub_type != ST1_SKILL_USE)
		{
			new_draw_info(NDI_UNIQUE, op, "You can't use this skill in this way.");
		}
		else
		{
			if (do_skill(op, op->facing))
			{
				return 1;
			}
		}
	}

	return 0;
}

/**
 * This changes the object's skill.
 * @param who Living to change skill for.
 * @param sk_index ID of the skill.
 * @return 0 on failure, 1 on success. */
int change_skill(object *who, int sk_index)
{
	object *tmp;

	if (who->chosen_skill && who->chosen_skill->stats.sp == sk_index)
	{
		/* optimization for changing skill to current skill */
		if (who->type == PLAYER)
		{
			CONTR(who)->shoottype = range_skill;
		}

		return 1;
	}

	if (sk_index >= 0 && sk_index < NROFSKILLS && (tmp = find_skill(who, sk_index)) != NULL)
	{
		if (apply_special(who, tmp, AP_APPLY))
		{
			LOG(llevBug, "BUG: change_skill(): can't apply new skill (%s - %d)\n", who->name, sk_index);
			return 0;
		}

		return 1;
	}

	if (who->chosen_skill)
	{
		if (apply_special(who, who->chosen_skill, AP_UNAPPLY))
		{
			LOG(llevBug, "BUG: change_skill(): can't unapply old skill (%s - %d)\n", who->name, sk_index);
		}
	}

	if (sk_index >= 0)
	{
		new_draw_info_format(NDI_UNIQUE, who, "You have no knowledge of %s.", skills[sk_index].name);
	}

	return 0;
}

/**
 * Like change_skill(), but uses skill object instead of looking for the
 * skill number.
 * @param who Living to change skill for.
 * @param skl Skill object.
 * @return 0 on failure, 1 on success. */
static int change_skill_to_skill(object *who, object *skl)
{
	/* Quick sanity check */
	if (!skl)
	{
		return 1;
	}

	if (who->chosen_skill == skl)
	{
		/* Optimization for changing skill to current skill */
		if (who->type == PLAYER)
		{
			CONTR(who)->shoottype = range_skill;
		}

		return 0;
	}

	if (skl->env != who)
	{
		LOG(llevBug, "BUG: change_skill_to_skill: skill is not in players inventory (%s - %s)\n", query_name(who, NULL), query_name(skl, NULL));
		return 1;
	}

	if (apply_special(who, skl, AP_APPLY))
	{
		LOG(llevBug, "BUG: change_skill(): can't apply new skill (%s - %s)\n", query_name(who, NULL), query_name(skl, NULL));
		return 1;
	}

	return 0;
}

/**
 * This handles melee weapon attacks -b.t.
 * @param op Living thing attacking.
 * @param dir Attack direction.
 * @return 0 if no attack was done, nonzero otherwise. */
static int attack_melee_weapon(object *op, int dir)
{
	if (!QUERY_FLAG(op, FLAG_READY_WEAPON))
	{
		if (op->type == PLAYER)
		{
			new_draw_info(NDI_UNIQUE, op, "You have no ready weapon to attack with!");
		}

		return 0;
	}

	return skill_attack(NULL, op, dir, NULL);
}

/**
 * This handles all hand-to-hand attacks.
 * @param pl Object attacking.
 * @param dir Attack direction.
 * @param string Describes the attack ("karate-chop", "punch", ...).
 * @return 0 if no attack was done, nonzero otherwise. */
static int attack_hth(object *pl, int dir, char *string)
{
	object *enemy = NULL, *weapon;

	if (QUERY_FLAG(pl, FLAG_READY_WEAPON))
	{
		for (weapon = pl->inv; weapon; weapon = weapon->below)
		{
			if (weapon->type != WEAPON || !QUERY_FLAG(weapon, FLAG_APPLIED))
			{
				continue;
			}

			CLEAR_FLAG(weapon, FLAG_APPLIED);
			CLEAR_FLAG(pl, FLAG_READY_WEAPON);
			fix_player(pl);

			if (pl->type == PLAYER)
			{
				new_draw_info(NDI_UNIQUE, pl, "You unwield your weapon in order to attack.");
				esrv_update_item(UPD_FLAGS, pl, weapon);
			}

			break;
		}
	}

	return skill_attack(enemy, pl, dir, string);
}

/**
 * Core routine for use when we attack using the skills system. There
 * aren't too many changes from before, basically this is a 'wrapper' for
 * the old attack system. In essence, this code handles all skill-based
 * attacks, ie hth, missile and melee weapons should be treated here. If
 * an opponent is already supplied by move_player(), we move right onto
 * do_skill_attack(), otherwise we find if an appropriate opponent
 * exists.
 * @param tmp Targetted monster.
 * @param pl What is attacking.
 * @param dir Attack direction.
 * @param string Describes the attack ("karate-chop", "punch", ...).
 * @return 1 if the attack damaged the opponent. */
int skill_attack(object *tmp, object *pl, int dir, char *string)
{
	int xt, yt;
	mapstruct *m;

	if (!dir)
	{
		dir = pl->facing;
	}

	/* If we don't yet have an opponent, find if one exists, and attack.
	 * Legal opponents are the same as outlined in move_player() */
	if (tmp == NULL)
	{
		xt = pl->x + freearr_x[dir];
		yt = pl->y + freearr_y[dir];

		if (!(m = get_map_from_coord(pl->map, &xt,&yt)))
		{
			return 0;
		}

		for (tmp = get_map_ob(m, xt, yt); tmp; tmp = tmp->above)
		{
			if ((IS_LIVE(tmp) && (tmp->head == NULL ? tmp->stats.hp > 0 : tmp->head->stats.hp > 0)) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || tmp->type == DOOR)
			{
				if (pl->type == PLAYER && tmp->type == PLAYER && !pvp_area(pl, tmp))
				{
					continue;
				}

				break;
			}
		}
	}

	if (tmp != NULL)
	{
		return do_skill_attack(tmp, pl, string);
	}

	if (pl->type == PLAYER)
	{
		new_draw_info(NDI_UNIQUE, pl, "There is nothing to attack!");
	}

	return 0;
}

/**
 * We have got an appropriate opponent from either move_player() or
 * skill_attack(). In this part we get on with attacking, take care of
 * messages from the attack and changes in invisible.
 * Returns true if the attack damaged the opponent.
 * @param tmp Targetted monster.
 * @param op What is attacking.
 * @param string Describes the attack ("karate-chop", "punch", ...).
 * @return 1 if the attack damaged the opponent. */
static int do_skill_attack(object *tmp, object *op, char *string)
{
	int success;
	char *name = query_name(tmp, NULL);

	if (op->type == PLAYER)
	{
		/* No selected weapon, change to our hth skill. */
		if (!CONTR(op)->selected_weapon)
		{
			if (CONTR(op)->skill_weapon)
			{
				if (change_skill_to_skill(op, CONTR(op)->skill_weapon))
				{
					LOG(llevBug, "BUG: do_skill_attack(): couldn't give new hth skill to %s\n", query_name(op, NULL));
					return 0;
				}
			}
			else
			{
				LOG(llevBug, "BUG: do_skill_attack(): no hth skill in player %s\n", query_name(op, NULL));
				return 0;
			}
		}
	}

	/* If we have 'ready weapon' but no 'melee weapons' skill readied
	 * this will flip to that skill. This is only window dressing for
	 * the players -- no need to do this for monsters. */
	if (op->type == PLAYER && QUERY_FLAG(op, FLAG_READY_WEAPON) && (!op->chosen_skill || op->chosen_skill->stats.sp != CONTR(op)->set_skill_weapon))
	{
		change_skill(op, CONTR(op)->set_skill_weapon);
	}

	success = attack_ob(tmp, op);

	/* Print appropriate messages to the player. */
	if (success && string != NULL)
	{
		if (op->type == PLAYER)
		{
			new_draw_info_format(NDI_UNIQUE, op, "You %s %s!", string, name);
		}
		else if (tmp->type == PLAYER)
		{
			new_draw_info_format(NDI_UNIQUE, tmp, "%s %s you!", query_name(op, NULL), string);
		}
	}

	return success;
}

/**
 * Get the level of player's chosen skill.
 * @param op Player.
 * @return The level of the chosen skill, level of the player if no
 * chosen skill. */
int SK_level(object *op)
{
	object *head = op->head ? op->head : op;
	int level;

	if (head->type == PLAYER && head->chosen_skill && head->chosen_skill->level != 0)
	{
		level = head->chosen_skill->level;
	}
	else
	{
		level = head->level;
	}

	/* Safety */
	if (level <= 0)
	{
		LOG(llevBug, "BUG: SK_level(arch %s, name %s): level <= 0\n", op->arch->name, query_name(op, NULL));
		level = 1;
	}

	return level;
}

/**
 * Get pointer to player's chosen skill object.
 * @param op Player.
 * @return Chosen skill object, NULL if no chosen skill. */
object *SK_skill(object *op)
{
	object *head = op->head ? op->head : op;

	if (head->type == PLAYER && head->chosen_skill)
	{
		return head->chosen_skill;
	}

	return NULL;
}

/**
 * Returns the amount of time it takes to use a skill.
 * @param op Player.
 * @param skillnr ID of the skill to check.
 * @return Amount of time the skill takes. */
float get_skill_time(object *op, int skillnr)
{
	float time = skills[skillnr].time;

	if (op->type != PLAYER)
	{
		return 0;
	}

	if (skillnr == SK_USE_MAGIC_ITEM || skillnr == SK_MISSILE_WEAPON || skillnr == SK_THROWING || skillnr == SK_XBOW_WEAP || skillnr == SK_SLING_WEAP)
	{
		CONTR(op)->action_range = global_round_tag + op->chosen_skill->stats.maxsp;
		return 0;
	}

	if (!time)
	{
		return 0.0f;
	}
	else
	{
		int level = SK_level(op) / 10;

		/* Now this should be MUCH harder */
		if (time > 1.0f)
		{
			time -= (level / 3) * 0.1f;

			if (time < 1.0f)
			{
				time = 1.0f;
			}
		}
	}

	return FABS(time);
}

/**
 * We check the action timer for a skill.
 * @param op Player.
 * @param skill Skill object.
 * @return 1 if the skill action is possible, 0 otherwise. */
int check_skill_action_time(object *op, object *skill)
{
	if (!skill)
	{
		return 0;
	}

	if (skill->stats.sp == SK_PRAYING || skill->stats.sp == SK_SPELL_CASTING)
	{
		if (CONTR(op)->action_casting > global_round_tag)
		{
			CONTR(op)->action_timer = (float) (CONTR(op)->action_casting - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			return 0;
		}
	}
	else
	{
		if (CONTR(op)->action_range > global_round_tag)
		{
			CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			return 0;
		}
	}

	return 1;
}

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

/* Created July 95 to separate skill utilities from actual skills -b.t. */

/* Reconfigured skills code to allow linking of skills to experience
 * categories. This is done solely through the init_new_exp_system() fctn.
 * June/July 1995 -b.t. thomas@astro.psu.edu
 */

/* July 1995 - Initial associated skills coding. Experience gains
 * come solely from the use of skills. Overcoming an opponent (in combat,
 * finding/disarming a trap, stealing from somebeing, etc) gains
 * experience. Calc_skill_exp() handles the gained experience using
 * modifications in the skills[] table. - b.t.
 */

/* define the following for skills utility debuging */
/* #define SKILL_UTIL_DEBUG */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <sounds.h>

/* table for stat modification of exp */
float stat_exp_mult[MAX_STAT + 1] =
{
	0.0f,
	0.01f,  0.1f,   0.3f,   0.5f,				/* 1 - 4 */
	0.6f,   0.7f,   0.8f,						/* 5 - 7 */
	0.85f,  0.9f,   0.95f,  0.96f,				/* 8 - 11 */
	0.97f,  0.98f,  0.99f,						/* 12 - 14 */
	1.0f,   1.01f,  1.02f,  1.03f,  1.04f,		/* 15 - 19 */
	1.05f,  1.07f,  1.09f,  1.12f,  1.15f,		/* 20 - 24 */
	1.2f,   1.3f,   1.4f,   1.5f,   1.7f, 2.0f
};


typedef struct _skill_name_table
{
	char *name;
	int id;
}_skill_name_table;

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
	0, /* for secure, item_skill==0 means no item_skill used */
	CS_STAT_SKILLEXP_AGILITY,
	CS_STAT_SKILLEXP_PERSONAL,
	CS_STAT_SKILLEXP_MENTAL,
	CS_STAT_SKILLEXP_PHYSIQUE,
	CS_STAT_SKILLEXP_MAGIC,
	CS_STAT_SKILLEXP_WISDOM
};

/* find and assign the skill exp stuff */
void find_skill_exp_name(object *pl, object *exp, int index)
{
	register int s;

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

/* find and return player skill exp level of given index item_skill
 * using CS_STAT_SKILLEXP_xxx */
int find_skill_exp_level(object *pl, int item_skill)
{
	register int s;

	for (s = 0; s < CONTR(pl)->last_skill_index; s++)
	{
		if (CONTR(pl)->last_skill_id[s] == item_skill_cs_stat[item_skill])
			return CONTR(pl)->last_skill_ob[s]->level;
	}

	/* this should not happen... */
	return 0;
}

/* find and return player skill exp level name of given index item_skill */
char *find_skill_exp_skillname(object *pl, int item_skill)
{
	(void) pl;

	/* funny double use of last entry marker :) */
	if (!item_skill || item_skill > 6)
		return skill_name_table[6].name;

	return skill_name_table[item_skill - 1].name;
}

/* do_skill() - Main skills use function-similar in scope to cast_spell().
 * We handle all requests for skill use outside of some combat here.
 * We require a separate routine outside of fire() so as to allow monsters
 * to utilize skills. Success should be the amount of experience gained
 * from the activity. Also, any skills that monster will use, will need
 * to return a positive value of success if performed correctly. Usually
 * this is handled in calc_skill_exp(). Thus failed skill use re-
 * sults in a 0, or false value of 'success'.
 *  - b.t.  thomas@astro.psu.edu */
int do_skill(object *op, int dir, char *string)
{
	/* needed for monster_skill_use() too */
	int success = 0;
	int skill = op->chosen_skill->stats.sp;

	/*LOG(-1, "DO SKILL: skill %s ->%d\n", op->chosen_skill->name, get_skill_time(op, skill));*/

	switch (skill)
	{
		case SK_LEVITATION:
			if (QUERY_FLAG(op, FLAG_FLYING))
			{
				CLEAR_MULTI_FLAG(op, FLAG_FLYING);
				new_draw_info(NDI_UNIQUE, 0, op, "You come to earth.");
			}
			else
			{
				SET_MULTI_FLAG(op, FLAG_FLYING);
				new_draw_info(NDI_UNIQUE, 0, op, "You rise into the air!");
			}
			break;

		case SK_STEALING:
			success = steal(op, dir);
			break;

		case SK_LOCKPICKING:
			success = pick_lock(op, dir);
			break;

		case SK_HIDING:
			success = hide(op);
			break;

		case SK_JUMPING:
			success = jump(op, dir);
			break;

		case SK_INSCRIPTION:
			success = write_on_item(op,string);
			break;

		case SK_MEDITATION:
			meditate(op);
			break;

			/* note that the following 'attack' skills gain exp through hit_player() */
		case SK_KARATE:
			(void) attack_hth(op, dir, "karate-chopped");
			break;

		case SK_BOXING:
			(void) attack_hth(op, dir, "punched");
			break;

		case SK_FLAME_TOUCH:
			(void) attack_hth(op, dir, "flamed");
			break;

		case SK_CLAWING:
			(void) attack_hth(op, dir, "clawed");
			break;

		case SK_MELEE_WEAPON:
		case SK_SLASH_WEAP:
		case SK_CLEAVE_WEAP:
		case SK_PIERCE_WEAP:
			(void) attack_melee_weapon(op, dir, NULL);
			break;

		case SK_FIND_TRAPS:
			success = find_traps(op, op->chosen_skill->level);
			break;

		case SK_MUSIC:
			success = singing(op, dir);
			break;

		case SK_ORATORY:
			success = use_oratory(op, dir);
			break;

		case SK_SMITH:
		case SK_BOWYER:
		case SK_JEWELER:
		case SK_ALCHEMY:
		case SK_THAUMATURGY:
		case SK_LITERACY:
		case SK_DET_MAGIC:
		case SK_DET_CURSE:
		case SK_WOODSMAN:
			success = skill_ident(op);
			break;

		case SK_REMOVE_TRAP:
			success = remove_trap(op,dir,op->chosen_skill->level);
			break;

		case SK_THROWING:
			new_draw_info(NDI_UNIQUE, 0, op, "This skill is not usable in this way.");
			return success;
			/*success = skill_throw(op, dir, string);*/
			break;

		case SK_SET_TRAP:
			new_draw_info(NDI_UNIQUE, 0, op, "This skill is currently not implemented.");
			return success;
			break;

		case SK_USE_MAGIC_ITEM:
		case SK_MISSILE_WEAPON:
			new_draw_info(NDI_UNIQUE, 0, op, "There is no special attack for this skill.");
			return success;
			break;

		case SK_PRAYING:
			new_draw_info(NDI_UNIQUE, 0, op, "This skill is not usable in this way.");
			return success;
			/*success = pray(op);*/
			break;

		case SK_SPELL_CASTING:
		case SK_CLIMBING:
		case SK_BARGAINING:
			new_draw_info(NDI_UNIQUE, 0, op, "This skill is already in effect.");
			return success;
			break;

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
		/*LOG(-1, "SKILL TIME: skill %s ->%d\n", op->chosen_skill->name, get_skill_time(op, skill));*/
	}

	/* this is a good place to add experience for successfull use of skills.
	 * Note that add_exp() will figure out player/monster experience
	 * gain problems. */

	if (success && skills[skill].category != EXP_NONE)
		add_exp(op, success, op->chosen_skill->stats.sp);

	return success;
}

/* calc_skill_exp() - calculates amount of experience can be gained for
 * successfull use of a skill.  Returns value of experience gain. */
int calc_skill_exp(object *who, object *op)
{
	int who_lvl;
	int op_exp = 0, op_lvl = 0;
	float exp_mul, max_mul, tmp;

	/* no exp for non players... its senseless to do */
	if (!who || who->type != PLAYER)
	{
		LOG(llevDebug, "DEBUG: calc_skill_exp() called with who != PLAYER or NULL (%s (%s)- %s)\n", query_name(who, NULL), !who ? "NULL" : "", query_name(op, NULL));
		return 0;
	}

	/* thats the releated skill level */
	who_lvl= SK_level(who);

	/* hm.... */
	if (!op)
	{
		LOG(llevBug, "BUG: calc_skill_exp() called with op == NULL (%s - %s)\n", query_name(who, NULL), query_name(op, NULL));
		op_lvl = who->map->difficulty < 1 ? 1: who->map->difficulty;
		op_exp = 0;
	}
	/* all traps. If stats.Cha > 1 we use that
	 * for the amount of experience */
	else if (op->type == RUNE)
	{
		op_exp = op->stats.Cha > 1 ? op->stats.Cha : op->stats.exp;
		op_lvl = op->level;
	}
	/* all other items/living creatures */
	else
	{
		/* get base exp */
		op_exp = op->stats.exp;
		op_lvl = op->level;
	}

	/* no exp for no level and no exp ;) */
	if (op_lvl < 1 || op_exp < 1)
		return 0;

	if (who_lvl < 2)
		max_mul = 0.85f;

	if (who_lvl < 3)
		max_mul = 0.7f;
	else if (who_lvl < 4)
		max_mul = 0.6f;
	else if (who_lvl < 5)
		max_mul = 0.45f;
	else if (who_lvl < 7)
		max_mul = 0.35f;
	else if (who_lvl < 8)
		max_mul = 0.3f;
	else
		max_mul = 0.25f;

	/* we get first a global level difference mulitplicator */
	exp_mul = calc_level_difference(who_lvl, op_lvl);
	op_exp = (int)((float) op_exp * lev_exp[op_lvl] * exp_mul);
	tmp = ((float)(new_levels[who_lvl + 1] - new_levels[who_lvl]) * 0.1f) * max_mul;
	if ((float) op_exp > tmp)
	{
		/*LOG(-1,"exp to high(%d)! adjusted to: %d", op_exp, (int)tmp);*/
		op_exp = (int)tmp;
	}

#if 0
	if (who_lvl <= op_lvl)
	{
		LOG(llevDebug, "EXP (lower hitter %d):: target %s (base:%d lvl:%d mul: %f) ", who_lvl, query_name(op), op_exp, op_lvl, lev_exp[op_lvl]);
		op_exp = (int)((float) op_exp * lev_exp[op_lvl]);
		tmp = (float)(new_levels[who_lvl + 1] - new_levels[who_lvl]) * 0.07f;
		if ((float) op_exp > tmp)
		{
			LOG(llevDebug, "exp to high(%d)! adjusted to: %d", op_exp, tmp);
			op_exp = tmp;
		}
	}
	else
	{
		tmp = (who_lvl - op_lvl) * 3;
		LOG(llevDebug, "EXP (higher hitter %d):: target %s (base:%d lvl:%d mul: %f) tmp:%d ",  who_lvl, query_name(op), op_exp, op_lvl, lev_exp[op_lvl], tmp);

		if ((who_lvl - op_lvl) > 2)
			tmp = calc_level_difference(who_lvl, op_lvl);

		if (tmp)
		{
			exp_mul = (float)op_lvl / ((float)op_lvl + (float)tmp * 0.75f);
			LOG(llevDebug, "level diff factor: %d (%4.4f)", tmp, exp_mul);
			op_exp = (int)(((float)op_exp * lev_exp[op_lvl]) * exp_mul);
		}
		else
			op_exp = 0;
	}
#endif

	/*LOG(llevDebug, "\nEXP:: %s (lvl %d(%d)) gets %d exp in %s from %s (lvl %d)(%x - %d) (max:%f)\n", query_name(who), who_lvl, who->level, op_exp, who->chosen_skill ? query_name(who->chosen_skill) : "<BUG: NO SKILL!>", query_name(op), op_lvl, op, op->count, tmp);*/

#if 0
	/* old code. I skipped skill[].lexp and bexp - perhaps later back in */
	if (who->chosen_skill == NULL)
	{
		LOG(llevBug, "BUG: Bad call of calc_skill_exp() by player.\n");
		return 0;
	}

	sk = who->chosen_skill->stats.sp;
	base = (float)(op_exp + (int)skills[sk].bexp);

	if (who_lvl < op_lvl)
		lvl_mult = (float) skills[sk].lexp * (float) ((float) op_lvl - (float) who_lvl);
	else if (who_lvl == op_lvl)
		lvl_mult = (float) skills[sk].lexp;
	else
		lvl_mult = ((float) ((float) op_lvl) / ((float) who_lvl));

	stat_mult = 1.0;

	value =  base * (lvl_mult * stat_mult);
#endif

	return op_exp;
}


/* find relevant stats or a skill then return their weighted sum.
 * I admit the calculation is done in a retarded way.
 * If stat1==NO_STAT_VAL this isnt an associated stat. Returns
 * zero then. -b.t. */
int get_weighted_skill_stat_sum(object *who, int sk)
{
	float sum;
	int number = 1;

	if (skills[sk].stat1 == NO_STAT_VAL)
		return 0;
	else
		sum = get_attr_value(&(who->stats), skills[sk].stat1);

	if (skills[sk].stat2 != NO_STAT_VAL)
	{
		sum += get_attr_value(&(who->stats), skills[sk].stat2);
		number++;
	}

	if (skills[sk].stat3 != NO_STAT_VAL)
	{
		sum += get_attr_value(&(who->stats), skills[sk].stat3);
		number++;
	}

	return (int) sum / number;
}

/* init_new_exp_system() - called in init(). This function will reconfigure
 * the properties of skills array and experience categories, and links
 * each skill to the appropriate experience category
 * -b.t. thomas@astro.psu.edu */
void init_new_exp_system()
{
	static int init_new_exp_done = 0;

	if (init_new_exp_done)
		return;

	init_new_exp_done = 1;

	/* locate the experience objects and create list of them */
	init_exp_obj();
	/* link skills to exp cat, based on shared stats */
	link_skills_to_exp();
}

void dump_skills()
{
	char buf[MAX_BUF];
	int i;

	/*dump_all_objects();*/
	LOG(llevInfo, "exper_catgry \t str \t dex \t con \t wis \t cha \t int \t pow \n");

	for (i = 0; i < nrofexpcat; i++)
		LOG(llevInfo,"%d-%s \t %d \t %d \t %d \t %d \t %d \t %d \t %d \n", i, exp_cat[i]->name, exp_cat[i]->stats.Str, exp_cat[i]->stats.Dex, exp_cat[i]->stats.Con, exp_cat[i]->stats.Wis, exp_cat[i]->stats.Cha, exp_cat[i]->stats.Int, exp_cat[i]->stats.Pow);

	LOG(llevInfo, "\n");
	sprintf(buf, "%20s  %12s  %4s %4s %4s  %5s %5s %5s\n",
			"sk#       Skill name", "ExpCat", "Time", "Base", "xlvl", "Stat1", "Stat2", "Stat3");
	LOG(llevInfo, buf);
	sprintf(buf, "%20s  %12s  %4s %4s %4s  %5s %5s %5s\n",
			"---       ----------", "------", "----", "----", "----", "-----", "-----", "-----");
	LOG(llevInfo,buf);

	for (i = 0; i < NROFSKILLS; i++)
	{
		sprintf(buf, "%2d-%17s  %12s  %4d %4ld %4g  %5s %5s %5s\n", i, skills[i].name, exp_cat[skills[i].category] != NULL ? exp_cat[skills[i].category]->name : "NONE", skills[i].time, skills[i].bexp, skills[i].lexp, skills[i].stat1 != NO_STAT_VAL ? short_stat_name[skills[i].stat1] : "---", skills[i].stat2 != NO_STAT_VAL ? short_stat_name[skills[i].stat2] : "---", skills[i].stat3 != NO_STAT_VAL ? short_stat_name[skills[i].stat3] : "---");
		LOG(llevInfo, buf);
	}
}

/* init_exp_obj() - this routine looks through all the assembled
 * archetypes for experience objects then copies their values into
 * the exp_cat[] array. - bt. */
void init_exp_obj()
{
	archetype *at;

	nrofexpcat = 0;
	for (at = first_archetype; at != NULL; at = at->next)
	{
		if (at->clone.type == EXPERIENCE)
		{
			exp_cat[nrofexpcat] = get_object();
			exp_cat[nrofexpcat]->level = 1;
			exp_cat[nrofexpcat]->stats.exp = 0;
			copy_object(&at->clone, exp_cat[nrofexpcat]);
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

	if (nrofexpcat == 0)
	{
		LOG(llevError, "ERROR: Aborting! No experience objects found in archetypes.\n");
		exit(0);
	}
}

/* link_skills_to_exp() - this links the skills in skills[] to the appropriate
 * experience category.  Linking is based only on the stats (1,2,3) of the
 * skill. If none of the stats assoc. w/ a skill match those in any experience
 * object then the skill becomes 'miscellaneous' -b.t. */
void link_skills_to_exp()
{
	int i = 0, j = 0;

	for (i = 0; i < NROFSKILLS; i++)
	{
		/* link the skill archetype ptr to skill list for fast access.
		 * now we can access the skill archetype by skill number or skill name. */
		if (!(skills[i].at = get_skill_archetype(i)))
			LOG(llevError, "ERROR: Aborting! Skill #%d (%s) not found in archlist!\n", i, skills[i].name);

		for (j=0;j<nrofexpcat;j++)
		{
			if (check_link(skills[i].stat1, exp_cat[j]))
			{
				skills[i].category = j;
				continue;
			}
			else if (skills[i].category == EXP_NONE && check_link(skills[i].stat2, exp_cat[j]))
			{
				skills[i].category = j;
				continue;
			}
			else if (skills[i].category == EXP_NONE && check_link(skills[i].stat3, exp_cat[j]))
			{
				skills[i].category = j;
				continue;
			}
			/* failed to link, set to EXP_NONE */
			else if (j == nrofexpcat || skills[i].stat1 == NO_STAT_VAL)
			{
				skills[i].category = EXP_NONE;
				continue;
			}
		}
	}
}

/* check_link() - this returns true if the specified stat is set in the
 * experience object. Wish this was more 'generalized'. Added new stats
 * will cause this routine to abort CF. - b.t. */
int check_link(int stat, object *exp)
{
	switch (stat)
	{
		case STR:
			if (exp->stats.Str)
				return 1;
			break;

		case CON:
			if (exp->stats.Con)
				return 1;
			break;

		case DEX:
			if (exp->stats.Dex)
				return 1;
			break;

		case INTELLIGENCE:
			if (exp->stats.Int)
				return 1;
			break;

		case WIS:
			if (exp->stats.Wis)
				return 1;
			break;

		case POW:
			if (exp->stats.Pow)
				return 1;
			break;

		case CHA:
			if (exp->stats.Cha)
				return 1;
			break;

		case NO_STAT_VAL:
			return 0;

		default:
			LOG(llevError, "ERROR: Aborting! Tried to link skill with unknown stat!\n");
			exit(0);
	}

	return 0;
}

/* check op (=player) has skill with skillnr or not.
 * 0: no skill, 1: op has skill */
int check_skill_known(object *op, int skillnr)
{
	object *tmp;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && tmp->stats.sp == skillnr)
			return 1;
	}

	return 0;
}

/* lookup_skill_by_name() - based on look_up_spell_by_name - b.t.
 * Given name, we return the index of skill 'string' in the skill
 * array, -1 if no skill is found. */
int lookup_skill_by_name(char *string)
{
	int skillnr = 0, nmlen;
	char name[MAX_BUF];

	if (!string)
		return -1;

	strcpy(name, string);
	nmlen = strlen(name);

	for (skillnr = 0; skillnr < NROFSKILLS; skillnr++)
	{
		/* GROS - This is to prevent strings like "hi" to be matched as "hiding" */
		if (strlen(name) >= strlen(skills[skillnr].name))
			if (!strncmp(name, skills[skillnr].name, MIN((int)strlen(skills[skillnr].name), nmlen)))
				return skillnr;
	}

	return -1;
}

/* check_skill_to_fire() - */
int check_skill_to_fire(object *who)
{
	int skillnr = 0;
	object *tmp;
	rangetype shoottype = range_none;

	if (who->type != PLAYER)
		return 1;

	switch ((shoottype = CONTR(who)->shoottype))
	{
		case range_bow:
			if (!(tmp = CONTR(who)->equipment[PLAYER_EQUIP_BOW]))
				return 0;

			if (tmp->sub_type1 == 2)
				skillnr = SK_SLING_WEAP;
			else if (tmp->sub_type1 == 1)
				skillnr = SK_XBOW_WEAP;
			else
				skillnr = SK_MISSILE_WEAPON;

			break;

		case range_none:
		case range_skill:
			return 1;
			break;

		case range_magic:
			if (spells[CONTR(who)->chosen_spell].flags & SPELL_DESC_WIS)
				skillnr = SK_PRAYING;
			else
				skillnr = SK_SPELL_CASTING;
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
		LOG(llevDebug,"check_skill_to_fire(): got skill:%s for %s\n", skills[skillnr].name, who->name);
#endif
		CONTR(who)->shoottype = shoottype;
		return 1;
	}
	else
		return 0;
}

/* check_skill_to_apply() - When a player tries to use an
 * object which requires a skill this function is called.
 * (examples are weapons like swords and bows)
 * It does 2 things: checks for appropriate skill in player inventory
 * and alters the player status by making the appropriate skill
 * the new chosen_skill.
 * -bt. thomas@astro.psu.edu */

/* This function was strange used.
 * I changed it, so it is used ONLY when a player try to apply something.
 * This function now sets no apply flag or needed one - but it checks and sets
 * the right skill.
 * TODO: monster skills and flags (can_use_xxx) including here.
 * For unapply, only call change_skill(object, NO_SKILL_READY) and fix_player() after it.
 * MT - 4.10.2002 */
int check_skill_to_apply(object *who, object *item)
{
	int skill = 0, tmp;
	/* perhaps we need a additional skill to use */
	int add_skill = NO_SKILL_READY;

	/* this fctn only for players */
	if (who->type != PLAYER)
		return 1;

	/* first figure out the required skills from the item */
	switch (item->type)
	{
		case WEAPON:
			tmp = item->sub_type1;
			/* we have a polearm! */
			if (tmp >= WEAP_POLE_IMPACT)
			{
				/* lets select the right weapon type */
				tmp = item->sub_type1 - WEAP_POLE_IMPACT;
				add_skill = SK_POLEARMS;
			}
			/* no, we have a 2h! */
			else if (tmp >= WEAP_2H_IMPACT)
			{
				/* lets select the right weapon type */
				tmp = item->sub_type1 - WEAP_2H_IMPACT;
				add_skill = SK_TWOHANDS;
			}

			if (tmp == WEAP_1H_IMPACT)
				skill = SK_MELEE_WEAPON;
			else if (tmp == WEAP_1H_SLASH)
				skill = SK_SLASH_WEAP;
			else if (tmp == WEAP_1H_CLEAVE)
				skill = SK_CLEAVE_WEAP;
			else if (tmp == WEAP_1H_PIERCE)
				skill = SK_PIERCE_WEAP;
			break;

		case BOW:
			tmp = item->sub_type1;
			if (tmp == RANGE_WEAP_BOW)
				skill = SK_MISSILE_WEAPON;
			else if (tmp == RANGE_WEAP_XBOWS)
				skill = SK_XBOW_WEAP;
			else
				skill = SK_SLING_WEAP;
			break;

			/* hm, this can be tricky when a player kills himself
			 * applying a bomb potion... must watch it */
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

	/* this should not happen */
	if (skill == NO_SKILL_READY)
		LOG(llevBug, "BUG: check_skill_to_apply() called for %s and item %s with skill NO_SKILL_READY\n", query_name(who, NULL), query_name(item, NULL));

	/* lets check the additional skill if there is one */
	if (add_skill != NO_SKILL_READY)
	{
		if (!change_skill(who, add_skill))
		{
			/*new_draw_info_format(NDI_UNIQUE, 0, who, "You don't have the needed skill '%s'!", skills[add_skill].name);*/
			return 0;
		}
		change_skill(who, NO_SKILL_READY);
	}

	/* if this skill is ready, all is fine. if not, ready it, if it can't readied, we
	 * can't apply/use/do it */
	if (!who->chosen_skill || (who->chosen_skill && who->chosen_skill->stats.sp != skill))
	{
		if (!change_skill(who, skill))
		{
			/*new_draw_info_format(NDI_UNIQUE, 0, who, "You don't have the needed skill '%s'!", skills[skill].name);*/
			return 0;
		}
	}
	return 1;
}

/* init_player_exp() - makes various checks and initialization of player
 * experience objects. If things aren't cool then we change them here.
 *  -b.t. */
/* there is a alot old cf code in here... it will not harm but for clean
 * and performance reason we should clean this whole section by time.
 * MT-2003 */
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

	/* first-pass find all current exp objects */
	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == EXPERIENCE)
		{
			exp_ob[exp_index] = tmp;

			if (tmp->stats.Pow)
				CONTR(pl)->sp_exp_ptr = tmp;

			if (tmp->stats.Wis)
				CONTR(pl)->grace_exp_ptr = tmp;

			find_skill_exp_name(pl, tmp, CONTR(pl)->last_skill_index);
			exp_index++;
		}
		else if (exp_index == MAX_EXP_CAT)
			return 0;
	}

	/* second - if pl has wrong nrof experience objects
	* then give player a complete roster. */

	/*  No exp objects - situation for a new player
	 *  or pre-skills/exp code player file */
	if (!exp_index)
	{
		for (j = 0; j < nrofexpcat; j++)
		{
			tmp = get_object();
			copy_object(exp_cat[j], tmp);
			insert_ob_in_ob(tmp, pl);
			tmp->stats.exp = pl->stats.exp / nrofexpcat;
			exp_ob[j] = tmp;

			if (tmp->stats.Pow)
				CONTR(pl)->sp_exp_ptr = tmp;
			if (tmp->stats.Wis)
				CONTR(pl)->grace_exp_ptr = tmp;

			esrv_send_item(pl, tmp);
			exp_index++;
		}
	}
	/* situation for old save file */
	else if (exp_index != nrofexpcat)
	{
		if (exp_index < nrofexpcat)
			LOG(llevBug, "BUG: init_player_exp(): player has too few experience objects.\n");

		if (exp_index > nrofexpcat)
			LOG(llevBug, "BUG: init_player_exp(): player has too many experience objects.\n");

		for (j = 0; j < nrofexpcat; j++)
		{
			for (i = 0; i <= exp_index; i++)
			{
				if (!strcmp(exp_ob[i]->name, exp_cat[j]->name))
					break;
				else if (i == exp_index)
				{
					tmp = get_object();
					copy_object(exp_cat[j], tmp);
					insert_ob_in_ob(tmp, pl);
					exp_ob[i] = tmp;

					if (tmp->stats.Pow)
						CONTR(pl)->sp_exp_ptr = tmp;

					if (tmp->stats.Wis)
						CONTR(pl)->grace_exp_ptr = tmp;

					esrv_send_item(pl, tmp);
					exp_index++;
				}
			}
		}
	}

	/* now we loop through one more time and set "apply"
	 * flag on valid expericence objects. Fix_player()
	 * requires this, and we get the bonus of being able
	 * to ignore invalid experience objects in the player
	 * inventory (player file from another game set up?).
	 * Also, we reset the score (or "total" experience) of
	 * the player to be the sum of all valid experience
	 * objects. */
	pl->stats.exp = 0;
	for (i = 0; i < exp_index; i++)
	{
		if (!QUERY_FLAG(exp_ob[i], FLAG_APPLIED))
			SET_FLAG(exp_ob[i], FLAG_APPLIED);

		/* GD: Update perm exp when loading player. */
		if (settings.use_permanent_experience)
			calc_perm_exp(exp_ob[i]);

		if (pl->stats.exp < exp_ob[i]->stats.exp)
		{
			pl->stats.exp = exp_ob[i]->stats.exp;
			pl->level = exp_ob[i]->level;
		}

		player_lvl_adj(NULL, exp_ob[i]);
	}

	return 1;
}


/* unlink_skill() - removes skill from a player skill list and
 * unlinks the pointer to the exp object */
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


/* link_player_skills() - linking skills with experience objects
 * and creating a linked list of skills for later fast access.
 *
 * Returns true if successfull. To be usefull this routine has to
 * be called *after* init_player_exp() and give_initial_items()
 *
 * fix_player() should be called somewhere after this function because objects
 * are unapplied here, but fix_player() is not called here.
 *
 * - b.t. thomas@astro.psu.edu */
int link_player_skills(object *pl)
{
	int i, j, cat = 0, sk_index = 0, exp_index = 0;
	object *tmp, *sk_ob[NROFSKILLS], *exp_ob[MAX_EXP_CAT];

	/* We're going to unapply all skills */
	pl->chosen_skill = NULL;
	CLEAR_FLAG(pl, FLAG_READY_SKILL);
	CONTR(pl)->last_skill_index = 0;

	/* first find all exp and skill objects */
	for (tmp = pl->inv; tmp && sk_index < 1000; tmp = tmp->below)
	{
		if (tmp->type == EXPERIENCE)
		{
			exp_ob[exp_index] = tmp;
			find_skill_exp_name(pl, tmp, CONTR(pl)->last_skill_index);
			/* hm, this mutiple instances ... no idea what it is... MT-2004 */
			/* to handle multiple instances */
			tmp->nrof = 1;
			exp_index++;
		}
		else if (tmp->type == SKILL)
		{
			/* for startup, lets unapply all skills */
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			/* to handle multiple instances */
			tmp->nrof = 1;
			sk_ob[sk_index] = tmp;
			sk_index++;
		}
	}

	if (exp_index != nrofexpcat)
	{
		LOG(llevBug, "BUG: link_player_skills() - player %s has bad number of exp obj\n", query_name(pl, NULL));
		if (!init_player_exp(pl))
		{
			LOG(llevBug, "BUG: link_player_skills() - failed to correct problem.\n");
			return 0;
		}
		(void) link_player_skills(pl);
		return 1;
	}

	/* Ok, create linked list and link the associated skills to exp objects */
	for (i = 0; i < sk_index; i++)
	{
		cat = skills[sk_ob[i]->stats.sp].category;

		if (cat == EXP_NONE)
			continue;

		for (j = 0; exp_ob[j] != NULL && j < exp_index; j++)
			if (!strcmp(exp_cat[cat]->name, exp_ob[j]->name))
				break;

		sk_ob[i]->exp_obj = exp_ob[j];
	}

	return 1;
}

/* link_player_skill() - links a  skill to exp object when applied or learned by
 * a player. Returns true if can link. Returns false if got misc
 * skill - bt. */
int link_player_skill(object *pl, object *skillop)
{
	object *tmp;
	int cat;

	cat = skills[skillop->stats.sp].category;

	/* ok the skill has an exp object, now find right one in pl inv */
	if (cat != EXP_NONE)
	{
		for (tmp = pl->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == EXPERIENCE && !strcmp(exp_cat[cat]->name, tmp->name))
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

/* Learn skill. This inserts the requested skill in the player's
 * inventory. The 'slaying' field of the scroll should have the
 * exact name of the requested archetype (there should be a better way!?)
 * added skillnr - direct access to archetype in skills[].
 * -bt. thomas@nomad.astro.psu.edu */
int learn_skill(object *pl, object *scroll, char *name, int skillnr, int scroll_flag)
{
	object *tmp;
	archetype *skill = NULL;
	object *tmp2;
	int has_meditation = 0;

	if (scroll)
		skill = find_archetype(scroll->slaying);
	else if (name)
		skill = find_archetype(name);
	else
		skill = skills[skillnr].at;

	if (!skill)
		return 2;

	tmp = arch_to_object(skill);
	if (!tmp)
		return 2;

	/* check if player already has it */
	for (tmp2 = pl->inv; tmp2; tmp2 = tmp2->below)
	{
		if (tmp2->type == SKILL)
		{
			if  (tmp2->stats.sp == tmp->stats.sp)
			{
				new_draw_info_format (NDI_UNIQUE, 0, pl, "You already know the skill '%s'!", tmp->name);
				return 0;
			}
			else if (tmp2->stats.sp == SK_MEDITATION)
				has_meditation = 1;
		}
	}

	/* Special check - if the player has meditation (monk), they can not
	 * learn melee weapons.  Prevents monk from getting this
	 * skill. */
	if (tmp->stats.sp == SK_MELEE_WEAPON && has_meditation)
	{
		new_draw_info(NDI_UNIQUE, 0, pl, "Your knowledge of inner peace prevents you from learning about melee weapons.");
		return 2;
	}

	/* now a random change to learn, based on player Int */
	/* only check when a scroll - add here god given check later */
	/* god given scrolls should never fail to learn */
	if (scroll_flag)
	{
		/* failure :< */
		if (random_roll(0, 99, pl, PREFER_LOW) > learn_spell[pl->stats.Int])
			return 2;
	}

	/* Everything is cool. Give'em the skill */
	(void) insert_ob_in_ob(tmp,pl);
	(void) link_player_skill(pl,tmp);
	play_sound_player_only (CONTR(pl), SOUND_LEARN_SPELL,SOUND_NORMAL, 0, 0);
	new_draw_info_format (NDI_UNIQUE, 0, pl, "You have learned the skill %s!", tmp->name);

	send_skilllist_cmd(pl, tmp, SPLIST_MODE_ADD);
	esrv_send_item(pl, tmp);

	return 1;
}

/* Gives a percentage clipped to 0% -> 100% of a/b. */
/* Probably belongs in some global utils-type file? */
static int clipped_percent(int a, int b)
{
	int rv;

	if (b <= 0)
		return 0;

	rv = (int)((100.0f * ((float)a) / ((float)b) ) + 0.5f);

	if (rv < 0)
		return 0;
	else if (rv > 100)
		return 100;

	return rv;
}

/* show_skills() - Meant to allow players to examine
 * their current skill list. b.t. (thomas@nomad.astro.psu.edu)
 * I have now added capability to show assoc. experience objects too.
 * Inportant note: the value of chosen_skill->level is not always
 * accurate--it is only updated in change_skill(), therefore long
 * unused skills may have wrong value. This is why we call
 * skill->exp_obj->level instead (the level of the associated experience
 * object -- which is always right).
 * -b.t.
 * this is outdated, we have skill exp/levle now - MT-2003. */
void show_skills(object *op)
{
	object *tmp = NULL;
	char buf[MAX_BUF], *in;
	int i, length, is_first;

	length = 31;
	in = "";
	sprintf(buf, "Player skills by experience category");
	new_draw_info(NDI_UNIQUE, 0, op, buf);

	/* print out skills by category */
	for (i = 0; i <= nrofexpcat; i++)
	{
		char Special[100];
		Special[0] = '\0';
		is_first = 1;
		/* skip to misc exp category */
		if (i == nrofexpcat)
			i = EXP_NONE;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type != SKILL)
				continue;

			if (skills[tmp->stats.sp].category == i && QUERY_FLAG(tmp, FLAG_APPLIED))
			{
				/* header info */
				if (is_first)
				{
					is_first = 0;
					new_draw_info(NDI_UNIQUE, 0, op, " ");
					if (tmp->exp_obj)
					{
						object *tmp_exp = tmp->exp_obj;
						int k = (length - 15 - strlen(tmp_exp->name));
						char tmpbuf[40];
						strcpy(tmpbuf, tmp_exp->name);
						while (k > 0)
						{
							k--;
							strcat(tmpbuf, ".");
						}

						if (settings.use_permanent_experience)
							new_draw_info_format(NDI_UNIQUE, 0, op, "%slvl:%3d (xp:%d/%d/%d%%)", tmpbuf, tmp_exp->level, tmp_exp->stats.exp, level_exp(tmp_exp->level + 1, 1.0), clipped_percent(tmp_exp->last_heal, tmp_exp->stats.exp));
						else
							new_draw_info_format(NDI_UNIQUE,0, op, "%slvl:%3d (xp:%d/%d)", tmpbuf,tmp_exp->level, tmp_exp->stats.exp, level_exp(tmp_exp->level + 1, 1.0));

						if (strcmp(tmp_exp->name, "physique") == 0)
							sprintf(Special, "You can handle %d weapon improvements.", tmp_exp->level / 5 + 5);

						if (strcmp(tmp_exp->name, "wisdom") == 0)
						{
							const char *cp = determine_god(op);
							sprintf(Special, "You worship %s.", cp ? cp : "no god at current time");
						}
					}
					else if (i == EXP_NONE)
						new_draw_info(NDI_UNIQUE, 0, op, "misc.");
				}

				/* print matched skills */
				if ((!op || QUERY_FLAG(op, FLAG_WIZ)))
					(void) sprintf(buf, "%s%s %s (%5d)", in, QUERY_FLAG(tmp, FLAG_APPLIED) ? "*" : "-", skills[tmp->stats.sp].name, tmp->count);
				else
					(void) sprintf(buf, "%s%s %s", in, QUERY_FLAG(tmp, FLAG_APPLIED) ? "*" : "-", skills[tmp->stats.sp].name);

				new_draw_info(NDI_UNIQUE, 0, op, buf);
			}
		}

		if (Special[0] != '\0')
			new_draw_info(NDI_UNIQUE, 0, op, Special);
	}
}

/* use_skill() - similar to invoke command, it executes the skill in the
 * direction that the user is facing. Returns false if we are unable to
 * change to the requested skill, or were unable to use the skill properly.
 * -b.t. */
int use_skill(object *op, char *string)
{
	int sknum = -1;

	/* the skill name appears at the begining of the string,
	 * need to reset the string to next word, if it exists. */
	/* first eat name of skill and then eat any leading spaces */

	if (string && (sknum = lookup_skill_by_name(string)) >= 0)
	{
		int len;

		if (sknum == -1)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "Unable to find skill by name %s", string);
			return 0;
		}

		len = strlen(skills[sknum].name);

		/* All this logic goes and skips over the skill name to find any
		 * options given to the skill. */
		if (len >= (int)strlen(string))
			*string = 0x0;
		else
		{
			while (len--)
			{
				string++;
			}

			while (*string == 0x20)
			{
				string++;
			}
		}

		if (strlen(string) == 0)
			string = NULL;
	}

#ifdef SKILL_UTIL_DEBUG
	LOG(llevDebug, "use_skill() got skill: %s\n", sknum > -1 ? skills[sknum].name : "none");
#endif

	/* Change to the new skill, then execute it. */
	if (change_skill(op, sknum))
	{
		if (op->chosen_skill->sub_type1 != ST1_SKILL_USE)
			new_draw_info(NDI_UNIQUE, 0, op, "You can't use this skill in this way.");
		else
			if (do_skill(op, op->facing, string))
				return 1;
	}
	return 0;
}


/* change_skill() - returns true if we are able to change to the requested
 * skill. Ignore the 'pl' designation, this code is useful for players and
 * monsters.  -bt. thomas@astro.psu.edu
 *
 * sk_index == -1 means that old skill should be unapplied, and no new skill
 * applied. */

/* Sept 95. Got rid of nasty strcmp calls in here -b.t.*/

/* Dec 95 - cleaned up the code a bit, change_skill now passes an indexed
 * value for the skill rather than a character string. Added find_skill.
 * -b.t. */

/* please note that change skill change set_skill_weapon && set_skill_bow are ONLY
 * set in fix_players() */
int change_skill (object *who, int sk_index)
{
	object *tmp;

	if (who->chosen_skill && who->chosen_skill->stats.sp == sk_index)
	{
		/* optimization for changing skill to current skill */
		if (who->type == PLAYER)
			CONTR(who)->shoottype = range_skill;

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
		if (apply_special(who, who->chosen_skill, AP_UNAPPLY))
			LOG(llevBug, "BUG: change_skill(): can't unapply old skill (%s - %d)\n", who->name, sk_index);

	if (sk_index >= 0)
		new_draw_info_format(NDI_UNIQUE, 0, who, "You have no knowledge of %s.", skills[sk_index].name);

	return 0;
}

/* This is like change_skill above, but it is given that
 * skill is already in who's inventory - this saves us
 * time if the caller has already done the work for us.
 * return 0 on success, 1 on failure. */
int change_skill_to_skill(object *who, object *skl)
{
	/* Quick sanity check */
	if (!skl)
		return 1;

	if (who->chosen_skill == skl)
	{
		/* optimization for changing skill to current skill */
		if (who->type == PLAYER)
			CONTR(who)->shoottype = range_skill;

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

/* attack_melee_weapon() - this handles melee weapon attacks -b.t.
 * For now we are just checking to see if we have a ready weapon here.
 * But there is a real neato possible feature of this scheme which
 * bears mentioning:
 * Since we are only calling this from do_skill() in the future
 * we may make this routine handle 'special' melee weapons attacks
 * (like disarming manuever with sai) based on player SK_level and
 * weapon type. */
int attack_melee_weapon(object *op, int dir, char *string)
{
	if (!QUERY_FLAG(op, FLAG_READY_WEAPON))
	{
		if (op->type == PLAYER)
			new_draw_info(NDI_UNIQUE, 0, op, "You have no ready weapon to attack with!");

		return 0;
	}

	return skill_attack(NULL, op, dir, string);
}

/* attack_hth() - this handles all hand-to-hand attacks -b.t. */

/* July 5, 1995 - I broke up attack_hth() into 2 parts. In the first
 * (attack_hth) we check for weapon use, etc in the second (the new
 * function skill_attack() we actually attack. */
int attack_hth(object *pl, int dir, char *string)
{
	object *enemy = NULL, *weapon;

	if (QUERY_FLAG(pl, FLAG_READY_WEAPON))
	{
		for (weapon = pl->inv; weapon; weapon = weapon->below)
		{
			if (weapon->type != WEAPON || !QUERY_FLAG(weapon, FLAG_APPLIED))
				continue;

			CLEAR_FLAG(weapon, FLAG_APPLIED);
			CLEAR_FLAG(pl, FLAG_READY_WEAPON);
			fix_player(pl);
			if (pl->type == PLAYER)
			{
				new_draw_info(NDI_UNIQUE, 0, pl, "You unwield your weapon in order to attack.");
				esrv_update_item(UPD_FLAGS, pl, weapon);
			}
			break;
		}
	}

	return skill_attack(enemy, pl, dir, string);
}

/* skill_attack() - Core routine for use when we attack using a skills
 * system. There are'nt too many changes from before, basically this is
 * a 'wrapper' for the old attack system. In essence, this code handles
 * all skill-based attacks, ie hth, missile and melee weapons should be
 * treated here. If an opponent is already supplied by move_player(),
 * we move right onto do_skill_attack(), otherwise we find if an
 * appropriate opponent exists.
 *
 * This is called by move_player() and attack_hth()
 *
 * Initial implementation by -bt thomas@astro.psu.edu */
int skill_attack(object *tmp, object *pl, int dir, char *string)
{
	int xt, yt;
	mapstruct *m;

	if (!dir)
		dir = pl->facing;

	/* If we don't yet have an opponent, find if one exists, and attack.
	 * Legal opponents are the same as outlined in move_player() */

	if (tmp == NULL)
	{
		xt = pl->x + freearr_x[dir];
		yt = pl->y + freearr_y[dir];

		if (!(m = out_of_map(pl->map, &xt,&yt)))
			return 0;

		/* rewrite this for new "head only" multi arches and battlegrounds. MT. */
		for (tmp = get_map_ob(m, xt, yt); tmp; tmp = tmp->above)
		{
			if ((IS_LIVE(tmp) && (tmp->head == NULL ? tmp->stats.hp > 0 : tmp->head->stats.hp > 0)) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || tmp->type == LOCKED_DOOR)
			{
				/* lets skip pvp outside battleground (pvp area) */
				if (pl->type == PLAYER && tmp->type == PLAYER && !pvp_area(pl, tmp))
					continue;
				break;
			}
		}
	}

	if (tmp != NULL)
		return do_skill_attack(tmp, pl, string);

	if (pl->type == PLAYER)
		new_draw_info(NDI_UNIQUE, 0, pl, "There is nothing to attack!");

	return 0;
}

/* do_skill_attack() - We have got an appropriate opponent from either
 * move_player() or skill_attack(). In this part we get on with
 * attacking, take care of messages from the attack and changes in invisible.
 * Returns true if the attack damaged the opponent.
 * -b.t. thomas@astro.psu.edu */
int do_skill_attack(object *tmp, object *op, char *string)
{
	int success;
	char buf[MAX_BUF], *name = query_name(tmp, NULL);

#if 0
	object *pskill = op->chosen_skill;
	static char *attack_hth_skill[] = {"skill_karate",	"skill_clawing", "skill_flame_touch", "skill_punching", NULL};
#endif

	/* For Players only: if there is no ready weapon, and no "attack" skill
	 * is readied either then check if op has a hand-to-hand "attack skill"
	 * to use instead. We define this an "attack" skill as any skill
	 * that is linked to the strexpobj.
	 * If we are using the wrong skill, change it (if possible) to
	 * a more appropriate skill (in order from the attack_hth_skill[]
	 * list). If the miserable slob still does'nt have any of the hth
	 * skills, give them the last attack_hth_skill on the list. There
	 * is probably a better way to do this...how?? */

	/* simple: if a weapon is applied or unapplied, it is handled in fix_player().
	 * fix_player() is adding the boni from a applied item and it is sorting which
	 * its applied and what objects effect a player. This also means, that for EVERY
	 * effect in the players inv (and skills are in the players inv), a fix_player()
	 * must be called - and it is called.
	 * fix_player() is looping through all the inv items of a player - so we can easily
	 * grap our best "bare hand attack" there on the fly. After we finish the loop,
	 * we look we have a valid applied weapon - if so, we skip our bare hand attack.
	 * if not, we kick in our bare hand skill. And voila: we can skip all this nonsense
	 * here... can't see what before the problem was because the same should work fine
	 * with old crossfire versions too and not only with daimonin. MT.
	 * PS: every player SHOULD have one physical base attack. Without one, this makes
	 * so or so no sense and creating a skill HERE is simply a bad hack. */
	if (op->type == PLAYER)
	{
		/* ok... lets change to our hth skill */
		if (!CONTR(op)->selected_weapon)
		{
			if (CONTR(op)->skill_weapon)
			{
				if (change_skill_to_skill(op, CONTR(op)->skill_weapon))
				{
					LOG(llevBug, "BUG: do_skill_attack() could'nt give new hth skill to %s\n", query_name(op, NULL));
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

#if 0
	if (op->type == PLAYER && !QUERY_FLAG(op, FLAG_READY_WEAPON) && ((!pskill || !pskill->exp_obj) || !pskill->exp_obj->stats.Str))
	{
		int i = 0, got_one = 0;
		object *tmp2 = NULL;

		/*LOG(-1, "TRIGGER?: %s\n", op->name);*/
		if (is_dragon_pl(op))
		{
			for (tmp2 = op->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == SKILL && !strcmp("skill_clawing", tmp2->arch->name))
				{
					got_one = 1;
					break;
				}
			}
		}

		while (attack_hth_skill[i] != NULL && !got_one)
		{
			for (tmp2 = op->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == SKILL && !strcmp(attack_hth_skill[i], tmp2->arch->name))
				{
					got_one = 1;
					break;
				}
			}

			if (got_one)
				break;

			i++;
		}

		if (!got_one)
		{
			archetype *skill;
			skill = find_archetype(attack_hth_skill[i - 1]);
			if (!skill)
			{
				LOG(llevError, "do_skill_attack() couldn't find attack skill for %s\n", op->name);
				return 0;
			}

			tmp2 = arch_to_object(skill);
			insert_ob_in_ob(tmp2, op);
			(void) link_player_skill(op, tmp2);
		}

		if (change_skill_to_skill(op, tmp2))
		{
			LOG(llevError, "do_skill_attack() couldn't give new hth skill to %s\n", op->name);
			return 0;
		}
	}
#endif

	/* if we have 'ready weapon' but no 'melee weapons' skill readied
	* this will flip to that skill. This is only window dressing for
	* the players--no need to do this for monsters. */
	if (op->type == PLAYER && QUERY_FLAG(op, FLAG_READY_WEAPON) && (!op->chosen_skill || op->chosen_skill->stats.sp != CONTR(op)->set_skill_weapon))
	{
		(void) change_skill(op, CONTR(op)->set_skill_weapon);
	}

	success = attack_ob(tmp, op);

	/* print appropriate  messages to the player */
	if (success && string != NULL)
	{
		sprintf(buf, "%s", string);
		if (op->type == PLAYER)
			new_draw_info_format(NDI_UNIQUE, 0, op, "You %s %s!", buf, name);
		else if (tmp->type == PLAYER)
			new_draw_info_format(NDI_UNIQUE, 0, tmp, "%s %s you!", query_name(op, NULL), buf);
	}

	return success;

}


/* This is in the same spirit as the similar routine for spells
 * it should be used anytime a function needs to check the user's
 * level. */
int SK_level(object *op)
{
	object *head = op->head ? op->head : op;
	int level;

	if (head->type == PLAYER && head->chosen_skill && head->chosen_skill->level != 0)
		level = head->chosen_skill->level;
	else
		level = head->level;

	/* safety */
	if (level <= 0)
	{
		LOG(llevBug, "BUG: SK_level(arch %s, name %s): level <= 0\n", op->arch->name, query_name(op, NULL));
		level = 1;
	}

	return level;
}

/* get choosen skill ptr */
object *SK_skill(object *op)
{
	object *head = op->head ? op->head : op;

	if (head->type == PLAYER && head->chosen_skill)
		return head->chosen_skill;

	return NULL;
}

/* returns the amount of time it takes to use a skill.
 * We allow for stats, and level to modify the amount
 * of time. Monsters have no skill use time because of
 * the random nature in which use_monster_skill is called
 * already simulates this. -b.t. */

/* I reworked this function. The original idea was
 * very basic just adding a value to the global
 * (player) object speed value. Thats the old style
 * way from crossfire, but not what we want. I addding
 * here now the "skill action timer" and a modified
 * speed thingy.- MT 2004 */
float get_skill_time(object *op, int skillnr)
{
	float time = skills[skillnr].time;

	if (op->type!=PLAYER)
		return 0;

	/* tis is only temp! Cast delay fixed to 1 seconds - this will
	 * be more senseful values for different spells in the future. */
	if (skillnr == SK_SPELL_CASTING || skillnr == SK_PRAYING)
	{
		CONTR(op)->action_casting = global_round_tag + 8;
		return 0;
	}
	/* these are skills using the "fire/range" menu - throwing, archery
	 * and use of rods/wands... */
	else if (skillnr == SK_USE_MAGIC_ITEM || skillnr == SK_MISSILE_WEAPON || skillnr == SK_THROWING || skillnr == SK_XBOW_WEAP || skillnr == SK_SLING_WEAP)
	{
		CONTR(op)->action_range = global_round_tag+op->chosen_skill->stats.maxsp;
		return 0;
	}

	if (!time)
		return 0.0f;
	else
	{
		/*int sum = get_weighted_skill_stat_sum(op, skillnr);*/
		int level = SK_level(op) / 10;

		/*time *= 1 / (1 + (sum / 15) + level);*/

		/* now this should be MUCH harder */
		if (time > 1.0f)
		{
			time -= (level / 3) * 0.1f;
			if (time < 1.0f)
				time = 1.0f;
		}
	}

	return FABS(time);
}

/* player only: we check the action time for a skill.
 * if skill action is possible, return true.
 * if the action is not possible, drop the right message
 * and/or queue the command. */
int check_skill_action_time(object *op, object *skill)
{
	if (!skill)
		return FALSE;

	if (skill->stats.sp == SK_PRAYING || skill->stats.sp == SK_SPELL_CASTING)
	{
		if (CONTR(op)->action_casting > global_round_tag)
		{
			CONTR(op)->action_timer = (float)(CONTR(op)->action_casting - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			return FALSE;
		}
	}
	else
	{
		if (CONTR(op)->action_range > global_round_tag)
		{
			CONTR(op)->action_timer = (float)(CONTR(op)->action_range - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
			return FALSE;
		}
	}

	return TRUE;
}

/* get_skill_stat1() - returns the value of the primary skill
 * stat. Used in various skills code. -b.t. */
int get_skill_stat1(object *op)
{
	int stat_value = 0, stat = NO_STAT_VAL;

	if ((op->chosen_skill) && ((stat = skills[op->chosen_skill->stats.sp].stat1) != NO_STAT_VAL))
		stat_value = get_attr_value(&(op->stats), stat);

	return stat_value;
}

/* get_skill_stat2() - returns the value of the secondary skill
 * stat. Used in various skills code. -b.t. */
int get_skill_stat2(object *op)
{
	int stat_value = 0,stat = NO_STAT_VAL;

	if ((op->chosen_skill) && ((stat = skills[op->chosen_skill->stats.sp].stat2) != NO_STAT_VAL))
		stat_value = get_attr_value(&(op->stats), stat);

	return stat_value;
}

/* get_skill_stat3() - returns the value of the tertiary skill
 * stat. Used in various skills code. -b.t. */
int get_skill_stat3(object *op)
{
	int stat_value = 0, stat = NO_STAT_VAL;

	if ((op->chosen_skill) && ((stat = skills[op->chosen_skill->stats.sp].stat3) != NO_STAT_VAL))
		stat_value = get_attr_value(&(op->stats), stat);

	return stat_value;
}

/* get_weighted_skill_stats() - */
int get_weighted_skill_stats(object *op)
{
	int value=0;

	value = (get_skill_stat1(op) / 2) + (get_skill_stat2(op) / 4) + (get_skill_stat3(op) / 4);

	return value;
}

/* Looks for the skill of specified name in op's inventory.
 *
 * attributes:
 *      object *op       the object to be searched (most likely a player)
 *      char *skname     name of the desired skill
 *
 * return:
 *      object *         the skill object if found, otherwise NULL */
object *get_skill_from_inventory(object *op, const char *skname)
{
	/* search index */
	object *tmp;

	if (op == NULL)
		return NULL;

	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == SKILL && strcmp(tmp->name, skname) == 0)
			return tmp;
	}
	return NULL;
}

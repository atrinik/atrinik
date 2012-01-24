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
 * Various skill related functions. */

/* define the following for skills utility debugging */
/* #define SKILL_UTIL_DEBUG */

#include <global.h>

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

static int change_skill_to_skill(object *who, object *skl);
static int attack_melee_weapon(object *op, int dir);
static int attack_hth(object *pl, int dir, char *string);
static int do_skill_attack(object *tmp, object *op, char *string);

/**
 * Main skills use function similar in scope to cast_spell().
 *
 * We handle all requests for skill use outside of some combat here.
 * We require a separate routine outside of fire() so as to allow monsters
 * to utilize skills.
 * @param op The object actually using the skill.
 * @param dir The direction in which the skill is used.
 * @param params String option for the skill.
 * @return 0 on failure of using the skill, non-zero otherwise. */
sint64 do_skill(object *op, int dir, const char *params)
{
	sint64 success = 0;
	int skill = op->chosen_skill->stats.sp;

	/* Trigger the map-wide skill event. */
	if (op->map && op->map->events)
	{
		int retval = trigger_map_event(MEVENT_SKILL_USED, op->map, op, NULL, NULL, NULL, dir);

		/* So the plugin's return value can affect the returned value. */
		if (retval)
		{
			return retval - 1;
		}
	}

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
			success = find_traps(op, op->level);
			break;

		case SK_REMOVE_TRAP:
			success = remove_trap(op);
			break;

		case SK_THROWING:
			draw_info(COLOR_WHITE, op, "This skill is not usable in this way.");
			break;

		case SK_USE_MAGIC_ITEM:
		case SK_MISSILE_WEAPON:
			draw_info(COLOR_WHITE, op, "There is no special attack for this skill.");
			return success;
			break;

		case SK_PRAYING:
			draw_info(COLOR_WHITE, op, "This skill is not usable in this way.");
			return success;
			break;

		case SK_SPELL_CASTING:
		case SK_BARGAINING:
			draw_info(COLOR_WHITE, op, "This skill is already in effect.");
			return success;
			break;

		case SK_CONSTRUCTION:
			construction_do(op, dir);
			return success;

		case SK_INSCRIPTION:
			success = skill_inscription(op, params);
			break;

		default:
			logger_print(LOG(DEBUG), "%s attempted to use unknown skill: %d", query_name(op, NULL), op->chosen_skill->stats.sp);
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
	if (success)
	{
		add_exp(op, success, op->chosen_skill->stats.sp, 0);
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
		logger_print(LOG(DEBUG), "called with who != PLAYER or NULL (%s (%s)- %s)", query_name(who, NULL), !who ? "NULL" : "", query_name(op, NULL));
		return 0;
	}

	if (level == -1)
	{
		/* The related skill level */
		who_lvl = SK_level(who);
	}

	if (!op)
	{
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
 * Initialize the experience system. */
void init_new_exp_system(void)
{
	static int init_new_exp_done = 0;
	int i;
	FILE *fp;
	char filename[MAX_BUF];

	if (init_new_exp_done)
	{
		return;
	}

	init_new_exp_done = 1;

	for (i = 0; i < NROFSKILLS; i++)
	{
		/* Link the skill archetype ptr to skill list for fast access.
		 * Now we can access the skill archetype by skill number or skill name. */
		if (!(skills[i].at = get_skill_archetype(i)))
		{
			logger_print(LOG(ERROR), "Aborting! Skill #%d (%s) not found in archlist!", i, skills[i].name);
			exit(1);
		}
	}

	snprintf(filename, sizeof(filename), "%s/%s", settings.datapath, SRV_CLIENT_SKILLS_FILENAME);
	fp = fopen(filename, "w");

	if (!fp)
	{
		logger_print(LOG(ERROR), "Cannot open file '%s' for writing.", filename);
		exit(1);
	}

	for (i = 0; i < NROFSKILLS; i++)
	{
		if (skills[i].description)
		{
			char icon[MAX_BUF], tmpresult[MAX_BUF];

			string_replace(skills[i].name, " ", "_", tmpresult, sizeof(tmpresult));
			snprintf(icon, sizeof(icon), "icon_%s.101", tmpresult);

			if (!find_face(icon, 0))
			{
				logger_print(LOG(ERROR), "Skill '%s' needs face '%s', but it could not be found.", skills[i].name, icon);
				exit(1);
			}

			fprintf(fp, "%s\n%d\n%s\n%s\nend\n", skills[i].name, skills[i].category - 1, icon, skills[i].description);
		}
	}

	fclose(fp);
}

/**
 * Look up a skill by name.
 * @param string Name of the skill to look for.
 * @return ID of the skill if found, -1 otherwise. */
int lookup_skill_by_name(const char *string)
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
 * @param type Fire mode.
 * @param params Optional arguments.
 * @return 1 on success, 0 on failure. */
int check_skill_to_fire(object *who, int type, const char *params)
{
	int skillnr = -1;
	object *tmp;

	if (who->type != PLAYER)
	{
		return 1;
	}

	switch (type)
	{
		case FIRE_MODE_SKILL:
			if (!params)
			{
				return 0;
			}

			skillnr = lookup_skill_by_name(params);

			if (skillnr == -1)
			{
				return 0;
			}

			break;

		case FIRE_MODE_BOW:
			tmp = CONTR(who)->equipment[PLAYER_EQUIP_BOW];

			if (!tmp)
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

		case FIRE_MODE_SPELL:
			if (spells[CONTR(who)->chosen_spell].type == SPELL_TYPE_PRIEST)
			{
				skillnr = SK_PRAYING;
			}
			else
			{
				skillnr = SK_SPELL_CASTING;
			}

			break;

		case FIRE_MODE_WAND:
			skillnr = SK_USE_MAGIC_ITEM;
			break;

		case FIRE_MODE_THROW:
			skillnr = SK_THROWING;
			break;
	}

	if (skillnr == -1)
	{
		return 0;
	}

	return change_skill(who, skillnr);
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
			skill = bow_get_skill(item);
			break;

		case SCROLL:
			skill = SK_LITERACY;
			break;

		case POTION:
		case ROD:
		case WAND:
			skill = SK_USE_MAGIC_ITEM;
			break;

		default:
			logger_print(LOG(DEBUG), "No skill exists for item: %s", query_name(item, NULL));
			return 0;
	}

	/* This should not happen */
	if (skill == NO_SKILL_READY)
	{
		logger_print(LOG(BUG), "check_skill_to_apply() called for %s and item %s with skill NO_SKILL_READY", query_name(who, NULL), query_name(item, NULL));
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
 * Linking skills with experience objects and creating a linked list of
 * skills for later fast access.
 * @param pl Player. */
void link_player_skills(object *pl)
{
	object *tmp;
	int i;

	pl->chosen_skill = NULL;

	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL)
		{
			CONTR(pl)->skill_ptr[tmp->stats.sp] = tmp;
			CLEAR_FLAG(tmp, FLAG_APPLIED);
		}
	}

	pl->stats.exp = 0;

	for (i = 0; i < NROFSKILLS; i++)
	{
		/* Skip unused skill entries. */
		if (!skills[i].description)
		{
			continue;
		}

		if (!CONTR(pl)->skill_ptr[i])
		{
			tmp = object_create_clone(&skills[i].at->clone);
			insert_ob_in_ob(tmp, pl);
			CONTR(pl)->skill_ptr[i] = tmp;
		}

		if (!QUERY_FLAG(CONTR(pl)->skill_ptr[i], FLAG_STAND_STILL))
		{
			pl->stats.exp += CONTR(pl)->skill_ptr[i]->stats.exp;

			if (pl->stats.exp >= (sint64) MAX_EXPERIENCE)
			{
				pl->stats.exp = MAX_EXPERIENCE;
			}
		}
	}

	player_lvl_adj(pl, NULL);
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

	/* The skill name appears at the beginning of the string,
	 * need to reset the string to next word, if it exists. */
	if (string && (sknum = lookup_skill_by_name(string)) >= 0)
	{
		size_t len;

		if (sknum == -1)
		{
			draw_info_format(COLOR_WHITE, op, "Unable to find skill by name %s", string);
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

	/* Change to the new skill, then execute it. */
	if (change_skill(op, sknum))
	{
		if (op->chosen_skill->sub_type != ST1_SKILL_USE)
		{
			draw_info(COLOR_WHITE, op, "You can't use this skill in this way.");
		}
		else
		{
			if (do_skill(op, op->facing, string))
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
		return 1;
	}

	if (sk_index >= 0 && sk_index < NROFSKILLS && (tmp = find_skill(who, sk_index)) != NULL)
	{
		if (object_apply_item(tmp, who, 0) != OBJECT_METHOD_OK)
		{
			logger_print(LOG(BUG), "can't apply new skill (%s - %d)", who->name, sk_index);
			return 0;
		}

		return 1;
	}

	if (who->chosen_skill)
	{
		if (object_apply_item(who->chosen_skill, who, AP_UNAPPLY) != OBJECT_METHOD_OK)
		{
			logger_print(LOG(BUG), "can't unapply old skill (%s - %d)", who->name, sk_index);
		}
	}

	if (sk_index >= 0)
	{
		draw_info_format(COLOR_WHITE, who, "You have no knowledge of %s.", skills[sk_index].name);
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
		return 0;
	}

	if (skl->env != who)
	{
		logger_print(LOG(BUG), "skill is not in players inventory (%s - %s)", query_name(who, NULL), query_name(skl, NULL));
		return 1;
	}

	if (object_apply_item(skl, who, AP_APPLY) != OBJECT_METHOD_OK)
	{
		logger_print(LOG(BUG), "can't apply new skill (%s - %s)", query_name(who, NULL), query_name(skl, NULL));
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
			draw_info(COLOR_WHITE, op, "You have no ready weapon to attack with!");
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
				draw_info(COLOR_WHITE, pl, "You unwield your weapon in order to attack.");
				esrv_update_item(UPD_FLAGS, weapon);
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
 * an opponent is already supplied by move_object(), we move right onto
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
	 * Legal opponents are the same as outlined in move_object() */
	if (tmp == NULL)
	{
		xt = pl->x + freearr_x[dir];
		yt = pl->y + freearr_y[dir];

		if (!(m = get_map_from_coord(pl->map, &xt,&yt)))
		{
			return 0;
		}

		for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above)
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
		draw_info(COLOR_WHITE, pl, "There is nothing to attack!");
	}

	return 0;
}

/**
 * We have got an appropriate opponent from either move_object() or
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
					logger_print(LOG(BUG), "couldn't give new hth skill to %s", query_name(op, NULL));
					return 0;
				}
			}
			else
			{
				logger_print(LOG(BUG), "no hth skill in player %s", query_name(op, NULL));
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
			draw_info_format(COLOR_WHITE, op, "You %s %s!", string, name);
		}
		else if (tmp->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, tmp, "%s %s you!", query_name(op, NULL), string);
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
	float skill_time = skills[skillnr].time;

	if (op->type != PLAYER)
	{
		return 0;
	}

	if (skillnr == SK_USE_MAGIC_ITEM || skillnr == SK_MISSILE_WEAPON || skillnr == SK_THROWING || skillnr == SK_XBOW_WEAP || skillnr == SK_SLING_WEAP)
	{
		skill_time = op->chosen_skill->stats.maxsp;
		CONTR(op)->action_range = global_round_tag + skill_time;
	}
	else if (skillnr == SK_SPELL_CASTING || skillnr == SK_PRAYING)
	{
		skill_time = spells[CONTR(op)->chosen_spell].time;
		CONTR(op)->action_casting = global_round_tag + skill_time;
	}
	else if (skill_time)
	{
		int level = SK_level(op) / 10;

		/* Now this should be MUCH harder */
		if (skill_time > 1.0f)
		{
			skill_time -= (level / 3) * 0.1f;

			if (skill_time < 1.0f)
			{
				skill_time = 1.0f;
			}
		}
	}

	return FABS(skill_time);
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
			return 0;
		}
	}
	else
	{
		if (CONTR(op)->action_range > global_round_tag)
		{
			return 0;
		}
	}

	return 1;
}

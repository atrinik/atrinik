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
 * This file contains all the code implementing @ref DISEASE "diseases",
 * except for odds and ends in @ref attack.c and in @ref living.c.
 *
 * Diseases may be contagious. They are objects which exist in a player's
 * inventory. They themselves do nothing, except modify
 * @ref SYMPTOM "symptoms", or spread to other live objects.
 * @ref SYMPTOM "symptoms" are what actually damage the player. */

#include <global.h>

static int is_susceptible_to_disease(object *victim, object *disease);
static void remove_symptoms(object *disease);
static object *find_symptom(object *disease);
static void check_infection(object *disease);
static void do_symptoms(object *disease);
static void grant_immunity(object *disease);

/**
 * Check if victim is susceptible to disease.
 * @param victim Victim.
 * @param disease Disease to check.
 * @return 1 if the victim can be infected, 0 otherwise. */
static int is_susceptible_to_disease(object *victim, object *disease)
{
	if (!IS_LIVE(victim))
	{
		return 0;
	}

	if (strstr(disease->race, "*") && !QUERY_FLAG(victim, FLAG_UNDEAD))
	{
		return 1;
	}

	if (strstr(disease->race, "undead") && QUERY_FLAG(victim, FLAG_UNDEAD))
	{
		return 1;
	}

	if ((victim->race && strstr(disease->race, victim->race)) || strstr(disease->race, victim->name))
	{
		return 1;
	}

	return 0;
}

/**
 * Ticks the clock for disease: infect, do symptoms, etc.
 * @param disease The disease.
 * @return 1 if the disease was removed, 0 otherwise. */
int move_disease(object *disease)
{
	/* first task is to determine if the disease is inside or outside of someone.
	 * If outside, we decrement 'value' until we're gone. */

	/* we're outside of someone */
	if (disease->env == NULL)
	{
		disease->value--;

		if (disease->value == 0)
		{
			/* drop inv since disease may carry secondary infections */
			destruct_ob(disease);
			return 1;
		}
	}
	else
	{
		/* if we're inside a person, have the disease run its course.
		 * negative foods denote "perpetual" diseases. */
		if (disease->stats.food > 0 && is_susceptible_to_disease(disease->env, disease))
		{
			disease->stats.food--;

			if (disease->stats.food == 0)
			{
				/* remove the symptoms of this disease */
				remove_symptoms(disease);
				grant_immunity(disease);
				/* drop inv since disease may carry secondary infections */
				destruct_ob(disease);
				return 1;
			}
		}
	}

	/* check to see if we infect others */
	check_infection(disease);

	/* impose or modify the symptoms of the disease */
	if (disease->env)
	{
		do_symptoms(disease);
	}

	return 0;
}

/**
 * Remove any symptoms of disease.
 * @param disease The disease. */
static void remove_symptoms(object *disease)
{
	object *symptom = find_symptom(disease);

	if (symptom != NULL)
	{
		object *victim = symptom->env;
		destruct_ob(symptom);

		if (victim)
		{
			fix_player(victim);
		}
	}
}

/**
 * Find a symptom for a disease in disease's env.
 * @param disease The disease.
 * @return Matching symptom object, NULL if not found. */
static object *find_symptom(object *disease)
{
	object *walk;

	/* check the inventory for symptoms */
	for (walk = disease->env->inv; walk; walk = walk->below)
	{
		if (!strcmp(walk->name, disease->name) && walk->type == SYMPTOM)
		{
			return walk;
		}
	}

	return NULL;
}

/**
 * Searches around for more victims to infect.
 * @param disease Disease infecting. */
static void check_infection(object *disease)
{
	int x, y, i, j, range, xt, yt;
	mapstruct *map, *m;
	object *tmp;
	rv_vector rv;

	range = abs(disease->magic);

	if (disease->env)
	{
		x = disease->env->x;
		y = disease->env->y;
		map = disease->env->map;
	}
	else
	{
		x = disease->x;
		y = disease->y;
		map = disease->map;
	}

	if (map == NULL)
	{
		return;
	}

	for (i = -range; i < range + 1; i++)
	{
		for (j = -range; j < range + 1; j++)
		{
			xt = x + i;
			yt = y + j;

			if (!(m = get_map_from_coord(map, &xt, &yt)))
			{
				continue;
			}

			if (!(GET_MAP_FLAGS(m, xt, yt) & (P_IS_MONSTER | P_IS_PLAYER)))
			{
				continue;
			}

			for (tmp = GET_MAP_OB(m, xt, yt); tmp; tmp = tmp->above)
			{
				if (!QUERY_FLAG(tmp, FLAG_MONSTER) && tmp->type != PLAYER)
				{
					continue;
				}

				if (!get_rangevector(disease->env ? disease->env : disease, tmp, &rv, 0) || !obj_in_line_of_sight(tmp, &rv))
				{
					continue;
				}

				infect_object(tmp, disease, 0);
			}
		}
	}
}

/**
 * Try to infect something with a disease. Rules are:
 * - Objects with immunity aren't infectable.
 * - Objects already infected aren't infectable.
 * - Dead objects aren't infectable.
 * - Undead objects are infectible only if specifically named.
 * @param victim Victim to try infect.
 * @param disease The disease.
 * @param force Don't do a random check for infection. Other checks
 * (susceptible to disease, not immune, and so on) are still done.
 * @return 1 if the victim was infected, 0 otherwise. */
int infect_object(object *victim, object *disease, int force)
{
	object *tmp, *new_disease, *owner;

	/* only use the head */
	if (victim->head)
	{
		victim = victim->head;
	}

	/* don't infect inanimate objects */
	if (!QUERY_FLAG(victim, FLAG_MONSTER) && victim->type != PLAYER)
	{
		return 0;
	}

	if ((owner = get_owner(disease)))
	{
		if (is_friend_of(owner, victim))
		{
			return 0;
		}
	}

	/* check and see if victim can catch disease:  diseases are specific */
	if (!is_susceptible_to_disease(victim, disease))
	{
		return 0;
	}

	/* roll the dice on infection before doing the inventory check! */
	if (!force && (rndm(0, 126) >= disease->stats.wc))
	{
		return 0;
	}

	for (tmp = victim->inv; tmp; tmp = tmp->below)
	{
		/* possibly an immunity, or diseased */
		if (tmp->type == SIGN || tmp->type == DISEASE)
		{
			if (!strcmp(tmp->name, disease->name) && tmp->level >= disease->level)
			{
				return 0;
			}
		}
	}

	/* If we've gotten this far, go ahead and infect the victim. */
	new_disease = get_object();
	copy_object(disease, new_disease, 0);
	new_disease->stats.food = disease->stats.maxgrace;
	new_disease->value = disease->stats.maxhp;
	/* self-limiting factor */
	new_disease->stats.wc -= disease->last_grace;

	/* Unfortunately, set_owner does the wrong thing to the skills pointers
	 * resulting in exp going into the owners *current* chosen skill. */
	if (get_owner(disease))
	{
		set_owner(new_disease, disease->owner);
		new_disease->chosen_skill = disease->chosen_skill;
	}
	/* for diseases which are passed by hitting, set owner and praying skill */
	else
	{
		if (disease->env && disease->env->type == PLAYER)
		{
			object *pl = disease->env;

			/* hm, we should for hit use the weapon? or the skill attached to this
			 * specific disease? hmmm */
			new_disease->chosen_skill = find_skill(pl, SK_PRAYING);

			if (new_disease->chosen_skill)
			{
				set_owner(new_disease, pl);
			}
		}
	}

	insert_ob_in_ob(new_disease, victim);
	CLEAR_FLAG(new_disease, FLAG_NO_PASS);

	if (new_disease->owner && new_disease->owner->type == PLAYER)
	{
		char buf[128];

		/* if the disease has a title, it has a special infection message */
		if (new_disease->title)
		{
			snprintf(buf, sizeof(buf), "%s %s!!", disease->title, victim->name);
		}
		else
		{
			snprintf(buf, sizeof(buf), "You infect %s with your disease, %s!", victim->name, new_disease->name);
		}

		if (victim->type == PLAYER)
		{
			draw_info(COLOR_RED, new_disease->owner, buf);
		}
		else
		{
			draw_info(COLOR_WHITE, new_disease->owner, buf);
		}
	}

	if (victim->type == PLAYER)
	{
		draw_info(COLOR_RED, victim, "You suddenly feel ill.");
	}

	return 1;
}

/**
 * This function monitors the symptoms caused by the disease (if any),
 * causes symptoms, and modifies existing symptoms in the case of
 * existing diseases.
 * @param disease The disease. */
static void do_symptoms(object *disease)
{
	object *symptom, *victim = disease->env, *tmp;

	/* This is a quick hack - for whatever reason, disease->env will point
	* back to disease, causing endless loops. Why this happens really needs
	* to be found, but this should at least prevent the infinite loops. */

	/* no-one to inflict symptoms on. */
	if (victim == NULL || victim == disease)
	{
		return;
	}

	symptom = find_symptom(disease);

	/* no symptom?  need to generate one! */
	if (symptom == NULL)
	{
		object *new_symptom;
		int i;

		/* first check and see if the carrier of the disease
		 * is immune. If so, no symptoms!  */
		if (!is_susceptible_to_disease(victim, disease))
		{
			return;
		}

		/* check for an actual immunity */
		/* do an immunity check */
		/* hm, disease should be NEVER in a tail of a multi part object */
		if (victim->head)
		{
			tmp = victim->head->inv;
		}
		else
		{
			tmp = victim->inv;
		}

		/* tmp initialized in if, above */
		for (; tmp; tmp = tmp->below)
		{
			/* possibly an immunity, or diseased */
			if (tmp->type == SIGN)
			{
				if (!strcmp(tmp->name, disease->name) && tmp->level >= disease->level)
				{
					return;
				}
			}
		}

		new_symptom = get_archetype("symptom");

		/* Something special done with dam. We want diseases to be more
		 * random in what they'll kill, so we'll make the damage they
		 * do random, note, this has a weird effect with progressive diseases. */
		if (disease->stats.dam != 0)
		{
			int dam = disease->stats.dam;

			/* reduce the damage, on average, 50%, and making things random. */
			dam = rndm(1, FABS(dam));

			if (disease->stats.dam < 0)
			{
				dam = -dam;
			}

			new_symptom->stats.dam = dam;
		}

		new_symptom->stats.maxsp = disease->stats.maxsp;
		new_symptom->stats.food = new_symptom->stats.maxgrace;

		FREE_AND_COPY_HASH(new_symptom->name, disease->name);
		new_symptom->level = disease->level;
		new_symptom->speed = disease->speed;
		new_symptom->value = 0;
		new_symptom->stats.Str = disease->stats.Str;
		new_symptom->stats.Dex = disease->stats.Dex;
		new_symptom->stats.Con = disease->stats.Con;
		new_symptom->stats.Wis = disease->stats.Wis;
		new_symptom->stats.Int = disease->stats.Int;
		new_symptom->stats.Pow = disease->stats.Pow;
		new_symptom->stats.Cha = disease->stats.Cha;
		new_symptom->stats.sp  = disease->stats.sp;
		new_symptom->stats.food = disease->last_eat;
		new_symptom->stats.maxsp = disease->stats.maxsp;
		new_symptom->last_sp = disease->last_sp;
		new_symptom->stats.exp = 0;
		new_symptom->stats.hp = disease->stats.hp;
		FREE_AND_COPY_HASH(new_symptom->msg, disease->msg);
		new_symptom->other_arch = disease->other_arch;

		for (i = 0; i < NROFATTACKS; i++)
		{
			if (disease->attack[i])
			{
				new_symptom->attack[i] = disease->attack[i];
			}
		}

		set_owner(new_symptom, disease->owner);

		/* Unfortunately, set_owner does the wrong thing to the skills pointers
		 * resulting in exp going into the owners *current* chosen skill. */
		new_symptom->chosen_skill = disease->chosen_skill;

		CLEAR_FLAG(new_symptom, FLAG_NO_PASS);
		insert_ob_in_ob(new_symptom, victim);
		return;
	}

	/* now deal with progressing diseases: we increase the debility
	 * caused by the symptoms.  */
	if (disease->stats.ac != 0)
	{
		float scale;

		symptom->value += disease->stats.ac;
		scale = (float) 1.0 + (float) symptom->value / (float) 100.0;

		/* now rescale all the debilities */
		symptom->stats.Str = (int) (scale * disease->stats.Str);
		symptom->stats.Dex = (int) (scale * disease->stats.Dex);
		symptom->stats.Con = (int) (scale * disease->stats.Con);
		symptom->stats.Wis = (int) (scale * disease->stats.Wis);
		symptom->stats.Int = (int) (scale * disease->stats.Int);
		symptom->stats.Pow = (int) (scale * disease->stats.Pow);
		symptom->stats.Cha = (int) (scale * disease->stats.Cha);
		symptom->stats.dam = (int) (scale * disease->stats.dam);
		symptom->stats.sp = (int) (scale * disease->stats.sp);
		symptom->stats.food = (int) (scale * disease->last_eat);
		symptom->stats.maxsp = (int) (scale * disease->stats.maxsp);
		symptom->last_sp = (int) (scale * disease->last_sp);
		symptom->stats.exp = 0;
		symptom->stats.hp = (int) (scale * disease->stats.hp);
		FREE_AND_COPY_HASH(symptom->msg, disease->msg);
		symptom->other_arch = disease->other_arch;
	}

	SET_FLAG(symptom, FLAG_APPLIED);
	fix_player(victim);
}

/**
 * Grants immunity to a disease.
 * @param disease disease to grant immunity to. */
static void grant_immunity(object *disease)
{
	object *immunity, *walk;

	/* Don't give immunity to this disease if last_heal is set. */
	if (disease->last_heal)
	{
		return;
	}

	/* first, search for an immunity of the same name */
	for (walk = disease->env->inv; walk; walk = walk->below)
	{
		if (walk->type == SIGN && !strcmp(disease->name, walk->name))
		{
			walk->level = disease->level;
			/* just update the existing immunity. */
			return;
		}
	}

	immunity = get_archetype("immunity");
	FREE_AND_COPY_HASH(immunity->name, disease->name);
	immunity->level = disease->level;
	CLEAR_FLAG(immunity, FLAG_NO_PASS);
	insert_ob_in_ob(immunity, disease->env);
}

/**
 * Make the symptom do the nasty things it does.
 * @param symptom Symptom to move. */
void move_symptom(object *symptom)
{
	object *victim = symptom->env;
	object *new_ob;
	int sp_reduce;

	/* outside a monster/player, die immediately */
	if (victim == NULL || victim->map == NULL)
	{
		object_remove(symptom, 0);
		object_destroy(symptom);
		return;
	}

	if (symptom->stats.dam > 0)
	{
		hit_player(victim, symptom->stats.dam, symptom, AT_INTERNAL);
	}
	else
	{
		hit_player(victim, (int) MAX((float) 1, (float) - victim->stats.maxhp * (float) symptom->stats.dam / (float) 100.0), symptom, AT_INTERNAL);
	}

	if (symptom->stats.maxsp > 0)
	{
		sp_reduce = symptom->stats.maxsp;
	}
	else
	{
		sp_reduce = (int) MAX((float) 1, (float) victim->stats.maxsp * (float) symptom->stats.maxsp / (float) 100.0);
	}

	victim->stats.sp = MAX(0, victim->stats.sp - sp_reduce);

	/* create the symptom "other arch" object and drop it here
	 * under every part of the monster */

	/* The victim may well have died. */
	if (victim->map == NULL)
	{
		return;
	}

	if (symptom->other_arch)
	{
		object *tmp = victim;

		if (tmp->head != NULL)
		{
			tmp = tmp->head;
		}

		/* tmp initialized above */
		for (; tmp != NULL; tmp = tmp->more)
		{
			new_ob = arch_to_object(symptom->other_arch);
			new_ob->x = tmp->x;
			new_ob->y = tmp->y;
			new_ob->map = victim->map;
			insert_ob_in_map(new_ob, victim->map, victim, INS_NO_MERGE | INS_NO_WALK_ON);
		}
	}

	if (victim->type == PLAYER)
	{
		draw_info(COLOR_RED, victim, symptom->msg);
	}
}

/**
 * Possibly infect due to direct physical contact.
 *
 * Called from doing physical attack in hit_player_attacktype().
 * @param victim The victim.
 * @param hitter The hitter. */
void check_physically_infect(object *victim, object *hitter)
{
	object *walk;

	/* search for diseases, give every disease a chance to infect */
	for (walk = hitter->inv; walk != NULL; walk = walk->below)
	{
		if (walk->type == DISEASE)
		{
			infect_object(victim, walk, 0);
		}
	}
}

/**
 * Do the cure disease stuff, from the spell "cure disease".
 * @param sufferer Who is getting cured.
 * @param caster Spell object used for curing. If NULL all diseases are
 * removed, otherwise only those of lower level than caster or randomly
 * chosen.
 * @return 1 if at least one disease was cured, 0 otherwise. */
int cure_disease(object *sufferer, object *caster)
{
	object *disease, *next;
	int casting_level, is_disease = 0;

	if (caster)
	{
		casting_level = caster->level;
	}
	else
	{
		caster = sufferer;
		/* if null caster, CURE all. */
		casting_level = 1000;
	}

	if (caster != sufferer && sufferer->type == PLAYER)
	{
		draw_info_format(COLOR_WHITE, sufferer, "%s casts cure disease on you!", caster->name ? caster->name : "someone");
	}

	for (disease = sufferer->inv; disease; disease = next)
	{
		next = disease->below;

		/* attempt to cure this disease */
		/* If caster level is higher than disease level, cure chance
		 * is automatic. If lower, then the chance is basically
		 * 1 in level_diff - if there is a 5 level difference, chance
		 * is 1 in 5. */
		if (disease->type == DISEASE)
		{
			is_disease = 1;

			if ((casting_level >= disease->level) || (!(rndm(0, (disease->level - casting_level - 1)))))
			{
				if (sufferer->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, sufferer, "You are healed from disease %s.", disease->name);
				}

				if (sufferer != caster && caster->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, caster, "You heal %s from disease %s.", sufferer->name, disease->name);
				}

				remove_symptoms(disease);
				object_remove(disease, 0);

				/* we assume the caster has the right casting skill applied */
				if (caster && !caster->type == PLAYER)
				{
					add_exp(caster, disease->stats.exp, caster->chosen_skill->stats.sp, 0);
				}

				object_destroy(disease);
			}
			else
			{
				if (sufferer->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, sufferer, "The disease %s resists the cure prayer!", disease->name);
				}

				if (sufferer != caster && caster->type == PLAYER)
				{
					draw_info_format(COLOR_WHITE, caster, "The disease %s resists the cure prayer!", disease->name);
				}
			}
		}
	}

	if (!is_disease)
	{
		if (sufferer->type == PLAYER)
		{
			draw_info(COLOR_WHITE, sufferer, "You are not diseased!");
		}

		if (sufferer != caster && caster->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, caster, "%s is not diseased!", sufferer->name ? sufferer->name : "someone");
		}
	}

	return 1;
}

/**
 * Reduces disease progression.
 * @param sufferer The sufferer.
 * @param reduction How much to reduce the disease progression.
 * @return 1 if we actually reduce a disease, 0 otherwise. */
int reduce_symptoms(object *sufferer, int reduction)
{
	object *walk;
	int success = 0;

	for (walk = sufferer->inv; walk; walk = walk->below)
	{
		if (walk->type == SYMPTOM)
		{
			if (walk->value > 0)
			{
				success = 1;
				walk->value = MAX(0, walk->value - 2 * reduction);
				/* give the disease time to modify this symptom,
				 * and reduce its severity. */
				walk->speed_left = 0;
			}
		}
	}

	if (success)
	{
		draw_info(COLOR_WHITE, sufferer, "Your illness seems less severe.");
	}

	return success;
}

/**
 * Initialize the disease type object methods. */
void object_type_init_disease(void)
{
}

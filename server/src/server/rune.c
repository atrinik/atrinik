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
 * All rune related functions. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

#ifndef sqr
#define sqr(x) ((x) * (x))
#endif

static void rune_attack(object *op, object *victim);

/**
 * Player is attempting to write a magical rune.
 * @param op Rune writer.
 * @param dir Orientation of rune, direction rune's contained spell will
 * be cast in, if applicable.
 * @param inspell ID of the spell.
 * @param level Level.
 * @param runename Name of the rune or message displayed by the rune for
 * a rune of marking.
 * @retval 0 No rune was written.
 * @retval 1 Rune written. */
int write_rune(object *op, int dir, int inspell, int level, char *runename)
{
	object *tmp;
	archetype *at = NULL;
	mapstruct *mt;
	char buf[MAX_BUF];
	int nx, ny;

	if (!dir)
	{
		dir = 1;
	}

	nx = op->x + freearr_x[dir];
	ny = op->y + freearr_y[dir];

	if (!(mt = get_map_from_coord(op->map, &nx, &ny)))
	{
		return 0;
	}

	if (blocked(op, mt, nx, ny, op->terrain_flag))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Can't make a rune there!");
		return 0;
	}

	for (tmp = get_map_ob(mt, nx, ny); tmp != NULL; tmp = tmp->above)
	{
		if (tmp->type == RUNE)
		{
			break;
		}
	}

	if (tmp)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You can't write a rune there.");
		return 0;
	}

	/* Can't have runes of small fireball!  */
	if (inspell)
	{
		if (inspell == -1)
		{
			new_draw_info(NDI_UNIQUE, 0, op, "You can't make a rune containing a spell you don't know. (idiot!)");
			return 0;
		}

		at = find_archetype(runename);

		/* What it compares to should probably be expanded.  But basically,
		 * creating a rune of sword should not be allowed. */
		if (at && at->clone.type != RUNE)
		{
			new_draw_info_format(NDI_UNIQUE, 0, op, "You can't make a rune of %s", runename);
			return 0;
		}
		/* next it attempts to look up a rune_archetype for this spell
		 * by doing some string manipulations */
		if (!at)
		{
			char insp[MAX_BUF];
			int i;

			strcpy(insp, spells[inspell].name);

			for (i = 0; i < (int)strlen(insp); i++)
			{
				if (insp[i] == ' ')
				{
					insp[i] = '_';
				}
			}

			snprintf(buf, sizeof(buf), "rune_%s", insp);
			at = find_archetype(buf);
		}

		if (!at)
		{
			tmp = get_archetype("generic_rune");
		}
		else
		{
			tmp = arch_to_object(at);
		}

		/* The spell it contains */
		tmp->stats.sp = inspell;

		snprintf(buf, sizeof(buf), "You set off a rune of %s", spells[inspell].name);
		FREE_AND_COPY_HASH(tmp->msg, buf);
		at = NULL;
	}
	else if (level == -2 || (at = find_archetype(runename)) == NULL)
	{
		char rune[HUGE_BUF];

		level = 0;
		/* This is a rune of marking */
		tmp = get_archetype("rune_mark");
		at = NULL;

		if (runename)
		{
			if (strstr(runename, "endmsg"))
			{
				new_draw_info_format(NDI_UNIQUE, 0, op, "Trying to cheat are we?", runename);
				LOG(llevInfo, "CHEAT: write_rune(): Player %s tried to write bogus rune.\n", op->name);
				return 0;
			}

			strncpy(rune, runename, HUGE_BUF - 2);
			rune[HUGE_BUF - 2] = 0;
			strcat(rune, "\n");
		}
		else
		{
			/* Not totally efficient, but keeps code simpler */
			strcpy(rune, "There is no message\n");
		}

		FREE_AND_COPY_HASH(tmp->msg, rune);
	}

	if (at)
	{
		tmp = get_archetype(runename);
	}

	/* The invisibility parameter */
	tmp->stats.Cha = op->level / 2;
	tmp->x = nx;
	tmp->y = ny;
	tmp->map = op->map;
	/* Where any spell will go upon detonation */
	tmp->direction = dir;
	/* What level to cast the spell at */
	tmp->level = SK_level(op);

	/* Runes without need no owner */
	if (inspell || tmp->stats.dam)
	{
		set_owner(tmp, op);
	}

	insert_ob_in_map(tmp, op->map, op, 0);

	return 1;
}

/**
 * This function handles those runes which detonate but do not cast
 * spells. Typically, poisoned or diseased runes.
 * @param op Rune.
 * @param victim Victim of the rune. */
static void rune_attack(object *op, object *victim)
{
	int dam = op->stats.dam;

	/* lets first calc the damage - we use base dmg * level
	 * For rune, the damage will *not* get additional
	 * level range boni
	 * we do here a more simple system like the normal monster damage.
	 * with the hard set float we can control the damage a bit better. */
	op->stats.dam = (sint16) ((float) dam * (LEVEL_DAMAGE(op->level) * 0.925f));

	if (victim)
	{
		tag_t tag = victim->count;
		hit_player(victim, op->stats.dam, op, AT_INTERNAL);

		if (was_destroyed(victim, tag))
		{
			op->stats.dam = dam;
			return;
		}

		/* If there's a disease in the needle, put it in the player */
		if (op->randomitems != NULL)
		{
			create_treasure(op->randomitems, op, 0, op->level ? op->level : victim->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
		}

		if (op->inv && op->inv->type == DISEASE)
		{
			object *disease = op->inv;
			infect_object(victim, disease, 1);
			remove_ob(disease);
			check_walk_off(disease, NULL, MOVE_APPLY_VANISHED);
		}
	}
	else
	{
		hit_map(op, 0);
	}

	op->stats.dam = dam;
}

/**
 * This function generalizes attacks by runes/traps. This ought to make
 * it possible for runes to attack from the inventory, it'll spring the
 * trap on the victim.
 * @param trap Trap that activates.
 * @param victim Victim of the trap. */
void spring_trap(object *trap, object *victim)
{
	int spell_in_rune;
	object *env;
	tag_t trap_tag = trap->count;

	/* Prevent recursion */
	if (trap->stats.hp <= 0)
	{
		return;
	}

	/* get the spell number from the name in the slaying field, and set
	 * that as the spell to be cast. */
	if (trap->slaying && (spell_in_rune = look_up_spell_by_name(NULL, trap->slaying)) != -1)
	{
		trap->stats.sp = spell_in_rune;
	}

	/* Only living objects can trigger runes that don't cast spells, as
	 * doing direct damage to a non-living object doesn't work anyway.
	 * Typical example is an arrow attacking a door. */
	if (!IS_LIVE(victim) && ! trap->stats.sp)
	{
		return;
	}

	/* decrement detcount */
	trap->stats.hp--;

	if (victim && victim->type == PLAYER)
	{
		new_draw_info(NDI_UNIQUE, 0, victim, trap->msg);
	}

	/* Flash an image of the trap on the map so the poor sod
	 * knows what hit him. */
	for (env = trap; env->env != NULL; env = env->env)
	{
	}

	trap_show(trap, env);
	/* Make the trap impotent */
	trap->type = MISC_OBJECT;
	CLEAR_FLAG(trap, FLAG_FLY_ON);
	CLEAR_FLAG(trap, FLAG_WALK_ON);
	FREE_AND_CLEAR_HASH2(trap->msg);
	/* Make it stick around until its spells are gone */
	trap->stats.food = 20;
	/* Ok, let the trap wear off */
	SET_FLAG(trap, FLAG_IS_USED_UP);
	trap->speed = trap->speed_left = 1.0f;
	update_ob_speed(trap);

	if (!trap->stats.sp)
	{
		rune_attack(trap, victim);
		set_traped_flag(env);

		if (was_destroyed(trap, trap_tag))
		{
			return;
		}
	}
	else
	{
		/* This is necessary if the trap is inside something else */
		remove_ob(trap);
		check_walk_off(trap, NULL, MOVE_APPLY_VANISHED);
		set_traped_flag(env);
		trap->x = victim->x;
		trap->y = victim->y;

		if (!insert_ob_in_map(trap, victim->map, trap, 0))
		{
			return;
		}

		cast_spell(trap, trap, trap->direction, trap->stats.sp - 1, 1, spellNormal, NULL);
	}

	if (trap->stats.hp <= 0)
	{
		/* Make the trap impotent */
		trap->type = MISC_OBJECT;
		CLEAR_FLAG(trap, FLAG_FLY_ON);
		CLEAR_FLAG(trap, FLAG_WALK_ON);
		FREE_AND_CLEAR_HASH2(trap->msg);
		/* Make it stick around until its spells are gone */
		trap->stats.food = 20;
		SET_FLAG(trap, FLAG_IS_USED_UP);
		trap->speed = trap->speed_left = 1.0f;
		update_ob_speed(trap);
	}
}

/**
 * Should op see trap?
 * @param op Living that could spot the trap.
 * @param trap Trap that is invisible.
 * @param level Level.
 * @retval 0 Trap wasn't spotted.
 * @retval 1 Trap was spotted. */
int trap_see(object *op, object *trap, int level)
{
	int chance = random_roll(0, 99, op, PREFER_HIGH);

	/* Decide if we see the rune or not */
	if ((trap->level <= level && RANDOM() % 10) || trap->stats.Cha == 1 || (chance > MIN(95, MAX(5, ((int) ((float) (op->map->difficulty + trap->level + trap->stats.Cha - op->level) / 10.0 * 50.0))))))
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You spot a %s (lvl %d)!", trap->name, trap->level);
		return 1;
	}

	return 0;
}

/**
 * Handles showing of a trap.
 * @param trap The trap.
 * @param where Where.
 * @return 1 if the trap was shown, 0 otherwise. */
int trap_show(object *trap, object *where)
{
	object *env;

	if (where == NULL)
	{
		return 0;
	}

	env = trap->env;
	/* We must remove and reinsert it.. */
	remove_ob(trap);
	CLEAR_FLAG(trap, FLAG_SYS_OBJECT);
	CLEAR_MULTI_FLAG(trap, FLAG_IS_INVISIBLE);
	trap->layer = 7;

	if (env && env->type != PLAYER && env->type != MONSTER && env->type != LOCKED_DOOR && !QUERY_FLAG(env, FLAG_NO_PASS))
	{
		SET_FLAG(env, FLAG_IS_TRAPED);

		/* Env object is on map */
		if (!env->env)
		{
			update_object(env, UP_OBJ_FACE);
		}
		/* Somewhere else - if visible, update */
		else
		{
			if (env->env->type == PLAYER || env->env->type == CONTAINER)
			{
				esrv_update_item(UPD_FLAGS, env->env, env);
			}
		}

		insert_ob_in_ob(trap, env);

		if (env->type == PLAYER || env->type == CONTAINER)
		{
			esrv_update_item(UPD_LOCATION, env, trap);
		}
	}
	else
	{
		insert_ob_in_map(trap, where->map, NULL, 0);
	}

	return 1;
}

/**
 * Try to disarm a trap.
 * @param disarmer Object disarming the trap.
 * @param trap Trap to disarm.
 * @param risk If 0, trap won't spring if disarm failure. Otherwise it
 * will spring.
 * @return Experience to award, 0 on failure. */
int trap_disarm(object *disarmer, object *trap, int risk)
{
	object *env = trap->env;
	int trapworth;
	int disarmer_level = SK_level(disarmer);

	/* this formula awards a more reasonable amount of exp */
	trapworth =  MAX(1, trap->level) * disarmer->map->difficulty * sqr(MAX(trap->stats.dam, spells[trap->stats.sp].sp)) / disarmer_level;

	if ((trap->level <= disarmer_level && (RANDOM() % 10)) || !(random_roll(0, (MAX(2, MIN(20, trap->level-disarmer_level + 5 - disarmer->stats.Dex / 2)) - 1), disarmer, PREFER_LOW)))
	{
		new_draw_info_format(NDI_UNIQUE, 0, disarmer, "You successfuly remove the %s (lvl %d)!", trap->name, trap->level);
		remove_ob(trap);
		check_walk_off(trap, NULL,MOVE_APPLY_VANISHED);
		set_traped_flag(env);

		/* If it is your own trap, (or any players trap), you don't
		 * get exp for it. */
		if (trap->owner && trap->owner->type != PLAYER && risk)
		{
			return trapworth;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		new_draw_info_format(NDI_UNIQUE, 0, disarmer, "You fail to remove the %s (lvl %d).", trap->name, trap->level);

		if ((trap->level > disarmer_level * 1.4f || (RANDOM() % 3)))
		{
			if (!(random_roll(0, (MAX(2, disarmer_level-trap->level + disarmer->stats.Dex / 2 - 6)) - 1, disarmer, PREFER_LOW)) && risk)
			{
				new_draw_info(NDI_UNIQUE, 0, disarmer, "In fact, you set it off!");
				spring_trap(trap, disarmer);
			}
		}

		return 0;
	}
}

/**
 * Adjust trap difficulty to the map. The default traps are too strong
 * for wimpy level 1 players, and unthreatening to anyone of high level.
 * @param trap Trap to adjust.
 * @param difficulty Map difficulty. */
void trap_adjust(object *trap, int difficulty)
{
	int off;

	if (difficulty < 1)
	{
		difficulty = 1;
	}

	off = (int) ((float) difficulty * 0.2f);

	trap->level = rndm(difficulty - off, difficulty + off);

	if (trap->level < 1)
	{
		trap->level = 1;
	}

	/* Set the hiddenness of the trap */
	trap->stats.Cha = rndm(0, 19) + rndm(difficulty - off, difficulty + off);

	if (trap->stats.Cha < 1)
	{
		trap->stats.Cha = 1;
	}
}

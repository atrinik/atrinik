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
 * All rune related functions. */

#include <global.h>

/**
 * This function handles those runes which detonate but do not cast
 * spells. Typically, poisoned or diseased runes.
 * @param op Rune.
 * @param victim Victim of the rune. */
static void rune_attack(object *op, object *victim)
{
	int dam = op->stats.dam;

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
		}
	}
	else
	{
		hit_map(op, 0, 0);
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
	object *env;

	/* Prevent recursion. */
	if (trap->stats.hp == 0)
	{
		return;
	}

	/* Only living objects can trigger runes that don't cast spells, as
	 * doing direct damage to a non-living object doesn't work anyway.
	 * Typical example is an arrow attacking a door. */
	if (!IS_LIVE(victim) && trap->stats.sp == -1)
	{
		return;
	}

	if (victim && victim->type == PLAYER && trap->msg)
	{
		draw_info(COLOR_WHITE, victim, trap->msg);
	}

	env = get_env_recursive(trap);
	trap_show(trap, env);

	if (victim->type == PLAYER)
	{
		CONTR(victim)->stat_traps_sprung++;
	}

	/* No spell, simple attack. */
	if (trap->stats.sp == -1)
	{
		tag_t trap_tag = trap->count;

		rune_attack(trap, victim);

		if (was_destroyed(trap, trap_tag))
		{
			return;
		}
	}
	else
	{
		object *old_env = trap->env;

		/* This is necessary if the trap is inside something else, otherwise
		 * the rune couldn't cast its spell. */
		if (!trap->map)
		{
			remove_ob(trap);
			trap->x = env->x;
			trap->y = env->y;

			if (!insert_ob_in_map(trap, victim->map, trap, 0))
			{
				return;
			}
		}

		cast_spell(trap, trap, trap->stats.maxsp, trap->stats.sp, 1, CAST_NORMAL, NULL);

		/* Add the trap back to the object it was in, unless it was on
		 * map to begin with. */
		if (old_env)
		{
			remove_ob(trap);
			insert_ob_in_ob(trap, old_env);
		}
	}

	/* Decrement detonation count and see if it's the last one, but only
	 * if the count is not -1 already (infinite). */
	if (trap->stats.hp != -1 && --trap->stats.hp == 0)
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
		/* Clear trapped flag. */
		set_trapped_flag(env);
		return;
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
	int chance = rndm(0, 99);

	/* Decide if we can see the rune or not */
	if ((trap->level <= level && rndm_chance(10)) || trap->stats.Cha == 1 || (chance > MIN(95, MAX(5, ((int) ((float) (op->map->difficulty + trap->level + trap->stats.Cha - op->level) / 10.0 * 50.0))))))
	{
		draw_info_format(COLOR_WHITE, op, "You spot a %s (lvl %d)!", trap->name, trap->level);

		if (trap->stats.Cha != 1)
		{
			CONTR(op)->stat_traps_found++;
		}

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
	/* We must remove and reinsert it so the layer is updated correctly. */
	remove_ob(trap);
	CLEAR_FLAG(trap, FLAG_SYS_OBJECT);
	CLEAR_MULTI_FLAG(trap, FLAG_IS_INVISIBLE);
	trap->layer = LAYER_EFFECT;

	/* The trap is not hidden anymore. */
	if (trap->stats.Cha > 1)
	{
		trap->stats.Cha = 1;
	}

	if (env && env->type != PLAYER && env->type != MONSTER && env->type != DOOR && !QUERY_FLAG(env, FLAG_NO_PASS))
	{
		insert_ob_in_ob(trap, env);
		set_trapped_flag(env);
	}
	else
	{
		insert_ob_in_map(trap, where->map, NULL, 0);
	}

	return 1;
}

/**
 * Try to disarm a trap.
 * @param disarmer Player disarming the trap.
 * @param trap Trap to disarm.
 * @return 1 if trap was disarmed, 0 otherwise. */
int trap_disarm(object *disarmer, object *trap)
{
	object *env = trap->env;
	int disarmer_level = CONTR(disarmer)->exp_ptr[EXP_AGILITY]->level;

	if ((trap->level <= disarmer_level && rndm_chance(10)) || !(rndm(0, (MAX(2, MIN(20, trap->level - disarmer_level + 5 - disarmer->stats.Dex / 2)) - 1))))
	{
		draw_info_format(COLOR_WHITE, disarmer, "You successfully remove the %s (lvl %d)!", trap->name, trap->level);
		remove_ob(trap);
		set_trapped_flag(env);
		CONTR(disarmer)->stat_traps_disarmed++;
		return 1;
	}
	else
	{
		draw_info_format(COLOR_WHITE, disarmer, "You fail to remove the %s (lvl %d).", trap->name, trap->level);

		if (trap->level > disarmer_level * 1.4f || rndm(0, 2))
		{
			if (!(rndm(0, (MAX(2, disarmer_level - trap->level + disarmer->stats.Dex / 2 - 6)) - 1)))
			{
				draw_info(COLOR_WHITE, disarmer, "In fact, you set it off!");
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
	int off, level, hide;

	if (difficulty < 1)
	{
		difficulty = 1;
	}

	off = (int) ((float) difficulty * 0.2f);
	level = rndm(difficulty - off, difficulty + off);
	level = MAX(1, MIN(level, MAXLEVEL));
	hide = rndm(0, 19) + rndm(difficulty - off, difficulty + off);
	hide = MAX(1, MIN(hide, SINT8_MAX));

	trap->level = level;
	trap->stats.Cha = hide;
}

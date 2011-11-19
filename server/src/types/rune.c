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
 * Handles code for @ref RUNE "runes".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Springs a rune.
 * @param op The rune.
 * @param victim Victim of the rune. */
void rune_spring(object *op, object *victim)
{
	object *env;

	if (!op || !victim)
	{
		return;
	}

	if (op->stats.hp == 0)
	{
		return;
	}

	/* Only living objects can trigger runes that do direct damage. */
	if (!IS_LIVE(victim) && op->stats.sp == -1)
	{
		return;
	}

	if (op->msg)
	{
		draw_info(COLOR_WHITE, victim, op->msg);
	}

	env = get_env_recursive(op);
	trap_show(op, env);

	if (victim->type == PLAYER)
	{
		CONTR(victim)->stat_traps_sprung++;
	}

	/* Direct damage. */
	if (op->stats.sp == -1)
	{
		OBJ_DESTROYED_BEGIN(op);
		OBJ_DESTROYED_BEGIN(victim);

		hit_player(victim, (sint16) ((float) op->stats.dam * (LEVEL_DAMAGE(op->level) * 0.925f)), op, AT_INTERNAL);

		if (!OBJ_DESTROYED(victim))
		{
			object *tmp, *next;

			if (op->randomitems)
			{
				create_treasure(op->randomitems, op, 0, op->level ? op->level : env->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
			}

			for (tmp = op->inv; tmp; tmp = next)
			{
				next = tmp->below;

				if (tmp->type == DISEASE)
				{
					infect_object(victim, tmp, 1);
					object_remove(tmp, 0);
					object_destroy(tmp);
				}
			}
		}

		if (OBJ_DESTROYED(op))
		{
			return;
		}

		OBJ_DESTROYED_END(victim);
		OBJ_DESTROYED_END(op);
	}
	/* Spell. */
	else
	{
		cast_spell(env, op, op->stats.maxsp, op->stats.sp, 1, CAST_NORMAL, NULL);
	}

	/* Decrement detonation count and see if it's the last one, but only
	 * if the count is not -1 already (infinite). */
	if (op->stats.hp != -1 && --op->stats.hp == 0)
	{
		/* Make the trap impotent */
		op->type = MISC_OBJECT;
		CLEAR_FLAG(op, FLAG_FLY_ON);
		CLEAR_FLAG(op, FLAG_WALK_ON);
		FREE_AND_CLEAR_HASH2(op->msg);
		/* Make it stick around until its spells are gone */
		op->stats.food = 20;
		SET_FLAG(op, FLAG_IS_USED_UP);
		op->speed = op->speed_left = 1.0f;
		update_ob_speed(op);
		/* Clear trapped flag. */
		set_trapped_flag(env);
	}
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
	(void) originator;
	(void) state;

	rune_spring(op, victim);
	return OBJECT_METHOD_OK;
}

/**
 * Initialize the rune type object methods. */
void object_type_init_rune(void)
{
	object_type_methods[RUNE].move_on_func = move_on_func;
}

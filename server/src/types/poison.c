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
 * Handles code for @ref POISON "poison" objects. */

#include <global.h>

/**
 * Apply poisoned object.
 * @param op The object applying this.
 * @param tmp The poison object. */
void apply_poison(object *op, object *tmp)
{
	if (op->type == PLAYER)
	{
		play_sound_player_only(CONTR(op), SOUND_DRINK_POISON, NULL, 0, 0, 0, 0);
		new_draw_info(NDI_UNIQUE, op, "Yech! That tasted poisonous!");
		strcpy(CONTR(op)->killer, "poisonous food");
	}

	if (tmp->stats.dam)
	{
		/* internal damage part will take care about our poison */
		hit_player(op, tmp->stats.dam, tmp, AT_POISON);
	}

	op->stats.food -= op->stats.food / 4;
	decrease_ob(tmp);
}

/**
 * A @ref POISONING "poisoning" object does its tick.
 * @param op The poisoning object. */
void poison_more(object *op)
{
	if (op->env == NULL || !IS_LIVE(op->env) || op->env->stats.hp < 0)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (!op->stats.food)
	{
		/* need to remove the object before fix_player is called, else fix_player
		 * will not do anything. */
		if (op->env->type == PLAYER)
		{
			CLEAR_FLAG(op, FLAG_APPLIED);
			fix_player(op->env);
			new_draw_info(NDI_UNIQUE, op->env, "You feel much better now.");
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (op->env->type == PLAYER)
	{
		op->env->stats.food--;
		new_draw_info(NDI_UNIQUE, op->env, "You feel very sick...");
	}

	/* If we successfully do damage to the player, the poison effects
	 * worsen... */
	if (hit_player(op->env, op->stats.dam, op, AT_INTERNAL))
	{
		int i;

		/* Pick some stats to 'deplete'. */
		for (i = 0; i < NUM_STATS; i++)
		{
			if (!(RANDOM() % 2) && get_attr_value(&op->stats, i) > -(MAX_STAT / 2))
			{
				/* Now deplete the stat. Relatively small chance that the depletion
				 * will be worse than usual. */
				change_attr_value(&op->stats, i, !(RANDOM() % 6) ? -2 : -1);
				new_draw_info(NDI_UNIQUE | NDI_GREY, op->env, lose_msg[i]);
			}
		}

		fix_player(op->env);
	}
}

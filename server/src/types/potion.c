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
 * Handles potion related code.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * @defgroup POTION_IMPROVE_xxx POTION_IMPROVE_xxx
 * Defines used in improvement potions handling code.
 *@{*/
/** Number of stats available for improvement. */
#define POTION_IMPROVE_STATS 3
/** Health. */
#define POTION_IMPROVE_HP 0
/** Mana. */
#define POTION_IMPROVE_SP 1
/** Grace. */
#define POTION_IMPROVE_GRACE 2
/*@}*/

/**
 * Offsets of the levxxx arrays in the player structure. These are used
 * in potion_improve_apply() for dynamic access. */
static const size_t potion_improve_stat_offsets[POTION_IMPROVE_STATS] =
{
	offsetof(player, levhp), offsetof(player, levsp), offsetof(player, levgrace)
};

/**
 * String representations of the stats improvement potions can improve.
 * These are used in potion_improve_apply() to inform a player which stat
 * has increased (or decreased, in case of cursed potions). */
static const char *const potion_improve_stat_names[POTION_IMPROVE_STATS] =
{
	"health", "mana", "grace"
};

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	(void) aflags;

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	/* Use magic devices skill for using potions. */
	if (!change_skill(applier, SK_USE_MAGIC_ITEM))
	{
		return OBJECT_METHOD_ERROR;
	}

	/* We are using it, so we now know what it does. */
	if (!QUERY_FLAG(op, FLAG_IDENTIFIED))
	{
		identify(op);
	}

	CONTR(applier)->stat_potions_used++;

	/* Potions with temporary effects. */
	if (op->last_eat == -1)
	{
		object *force;
		int i, val;

		/* Create a force and copy the effects in. */
		force = get_archetype("force");
		force->type = POTION_EFFECT;
		/* Copy the amount of time the effect should last. */
		force->stats.food = op->stats.food;
		SET_FLAG(force, FLAG_IS_USED_UP);

		/* Make sure the effect lasts for at least a little while. */
		if (force->stats.food <= 0)
		{
			force->stats.food = 1;
		}

		if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
		{
			int protection;

			/* The potion is cursed/damned, so the negative effects stay
			 * longer. */
			force->stats.food *= 3;

			/* Copy over protection values to the force from the potion, but
			 * reverse them first. Attacks are ignored, as it's not actually
			 * possible to store negative attack values in objects. */
			for (i = 0; i < NROFATTACKS; i++)
			{
				protection = op->protection[i] > 0 ? -op->protection[i] : op->protection[i];

				/* If the potion is damned, the effects worsen... */
				if (QUERY_FLAG(op, FLAG_DAMNED))
				{
					protection *= 2;
				}

				/* Actually set the protection value, but make sure it's in a
				 * valid range. */
				force->protection[i] = MIN(100, MAX(-100, protection));
			}

			insert_spell_effect("meffect_purple", applier->map, applier->x, applier->y);
			play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
		}
		else
		{
			memcpy(force->protection, op->protection, sizeof(op->protection));
			memcpy(force->attack, op->attack, sizeof(op->attack));

			insert_spell_effect("meffect_green", applier->map, applier->x, applier->y);
			play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
		}

		/* Copy over stat values. */
		for (i = 0; i < NUM_STATS; i++)
		{
			/* Get the stat value from the potion. */
			val = get_attr_value(&op->stats, i);

			/* No value, nothing to do. */
			if (!val)
			{
				continue;
			}

			/* If the potion is cursed/damned and the stat increase is
			 * positive, reverse it. */
			if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
			{
				val = -ABS(val);

				/* The potion is damned, so the negative effect is worse. */
				if (QUERY_FLAG(op, FLAG_DAMNED))
				{
					val *= 2;
				}
			}

			/* Now set the stat value of the force to the one calculcated
			 * above, but make sure it doesn't overflow sint8. */
			change_attr_value(&force->stats, i, MIN(SINT8_MAX, MAX(SINT8_MIN, val)));
		}

		/* Insert the force into the player and apply it. */
		force->speed_left = -1;
		force = insert_ob_in_ob(force, applier);
		SET_FLAG(force, FLAG_APPLIED);

		if (!change_abil(applier, force))
		{
			draw_info(COLOR_WHITE, applier, "Nothing happened.");
		}
	}
	/* Potion of minor restoration (removes depletion). */
	else if (op->last_eat == 1)
	{
		int i;

		/* Cursed potion of minor restoration; reverse effects (stats are
		 * depleted). */
		if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))
		{
			/* Drain 2 stats if the potion is cursed, 4 if it's damned. */
			for (i = QUERY_FLAG(op, FLAG_DAMNED) ? 0 : 2; i < 4; i++)
			{
				drain_stat(applier);
			}

			insert_spell_effect("meffect_purple", applier->map, applier->x, applier->y);
			play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
		}
		else
		{
			archetype *at;
			object *depletion;

			at = find_archetype("depletion");

			depletion = present_arch_in_ob(at, applier);

			if (depletion)
			{
				for (i = 0; i < NUM_STATS; i++)
				{
					if (get_attr_value(&depletion->stats, i))
					{
						draw_info(COLOR_WHITE, applier, restore_msg[i]);
					}
				}

				object_remove(depletion, 0);
				object_destroy(depletion);
				fix_player(applier);
			}
			else
			{
				draw_info(COLOR_WHITE, applier, "You are not depleted.");
			}

			insert_spell_effect("meffect_green", applier->map, applier->x, applier->y);
			play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
		}
	}
	/* Improvement potion. */
	else if (op->last_eat == 2)
	{
		int indices[POTION_IMPROVE_STATS] = {POTION_IMPROVE_HP, POTION_IMPROVE_SP, POTION_IMPROVE_GRACE};
		int stat_id, i, level, max, val, done;
		char *levarr, oldlev;

		/* Shuffle the array so we check all the stats for possible
		 * improvement in random order. */
		permute(indices, 0, POTION_IMPROVE_STATS);

		/* Check all the stats. As 'indices' has been shuffled, that is used
		 * to determine which stat we're working on, instead of directly
		 * using 'stat'. */
		for (stat_id = 0, done = 0; stat_id < POTION_IMPROVE_STATS && !done; stat_id++)
		{
			/* Determine level related to this stat, and the maximum possible
			 * value it can be improved to. */
			if (indices[stat_id] == POTION_IMPROVE_HP)
			{
				level = applier->level;
				max = applier->arch->clone.stats.maxhp;
			}
			else if (indices[stat_id] == POTION_IMPROVE_SP)
			{
				level = CONTR(applier)->exp_ptr[EXP_MAGICAL]->level;
				max = applier->arch->clone.stats.maxsp;
			}
			else
			{
				level = CONTR(applier)->exp_ptr[EXP_WISDOM]->level;
				max = applier->arch->clone.stats.maxgrace;
			}

			/* Check to see if we can increase (or decrease, if the potion is
			 * cursed) any stats. If the potion is cursed, we start at level
			 * 2, because level 1 has the improvement fixed to the maximum
			 * value (since it's the first level, obviously). */
			for (i = OBJECT_CURSED(op) ? 2 : 1; i <= level; i++)
			{
				/* Get pointer to the array that stores the level
				 * improvements for this stat. */
				levarr = (void *) ((char *) CONTR(applier) + potion_improve_stat_offsets[indices[stat_id]]);

				val = OBJECT_CURSED(op) ? 1 : max;

				/* The value is the same for this level, go on. */
				if (levarr[i] == val)
				{
					continue;
				}

				oldlev = levarr[i];
				levarr[i] = val;
				fix_player(applier);

				if (OBJECT_CURSED(op))
				{
					insert_spell_effect("meffect_purple", applier->map, applier->x, applier->y);
					play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
					draw_info_format(COLOR_WHITE, applier, "The foul potion burns like fire inside you, and your %s decreases by %d!", potion_improve_stat_names[indices[stat_id]], oldlev - levarr[i]);
				}
				else
				{
					insert_spell_effect("meffect_yellow", applier->map, applier->x, applier->y);
					play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
					draw_info_format(COLOR_WHITE, applier, "You feel a little more perfect, and your %s increases by %d!", potion_improve_stat_names[indices[stat_id]], levarr[i] - oldlev);
				}

				done = 1;
				break;
			}
		}

		/* No effect (because there is nothing to increase [or decrease, in
		 * case of cursed potions]); inform the player. */
		if (!done)
		{
			if (OBJECT_CURSED(op))
			{
				play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
				draw_info(COLOR_WHITE, applier, "The potion was foul but had no effect on your tortured body.");
			}
			else
			{
				play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
				draw_info(COLOR_WHITE, applier, "The potion had no effect - you are already perfect.");
			}
		}
	}
	/* Spell potion. */
	else if (op->stats.sp != SP_NO_SPELL)
	{
		/* Fire in the player's facing direction, unless the spell is
		 * something like healing or cure disease. */
		cast_spell(applier, op, spells[op->stats.sp].flags & SPELL_DESC_SELF ? 0 : applier->facing, op->stats.sp, 1, CAST_POTION, NULL);
	}
	else
	{
		draw_info(COLOR_WHITE, applier, "Nothing happens as you apply it.");
	}

	decrease_ob(op);

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the potion type object methods. */
void object_type_init_potion(void)
{
	object_type_methods[POTION].apply_func = apply_func;
}

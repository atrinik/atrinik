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
 * Handles potion related code. */

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

/**
 * Handle applying an improvement potion.
 * @param op Player applying the potion.
 * @param tmp The potion.
 * @return 1. */
static int potion_improve_apply(object *op, object *tmp)
{
	int indices[POTION_IMPROVE_STATS] = {POTION_IMPROVE_HP, POTION_IMPROVE_SP, POTION_IMPROVE_GRACE};
	int stat_id, i, level, max, cursed;
	char *levarr, oldlev;

	/* Shuffle the array so we check all the stats for possible
	 * improvement in random order. */
	permute(indices, 0, POTION_IMPROVE_STATS);
	/* Determine whether the potion is cursed or not. */
	cursed = QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED);

	/* A potion will be used up even if we don't actually increase any
	 * stats. */
	decrease_ob(tmp);

	/* Check all the stats. As 'indices' has been shuffled, that is used
	 * to determine which stat we're working on, instead of directly
	 * using 'stat'. */
	for (stat_id = 0; stat_id < POTION_IMPROVE_STATS; stat_id++)
	{
		/* Determine level related to this stat, and the maximum possible
		 * value it can be improved to. */
		if (indices[stat_id] == POTION_IMPROVE_HP)
		{
			level = op->level;
			max = op->arch->clone.stats.maxhp;
		}
		else if (indices[stat_id] == POTION_IMPROVE_SP)
		{
			level = CONTR(op)->exp_ptr[EXP_MAGICAL]->level;
			max = op->arch->clone.stats.maxsp;
		}
		else
		{
			level = CONTR(op)->exp_ptr[EXP_WISDOM]->level;
			max = op->arch->clone.stats.maxgrace;
		}

		/* Check to see if we can increase (or decrease, if the potion is
		 * cursed) any stats. If the potion is cursed, we start at level
		 * 2, because level 1 has the improvement fixed to the maximum
		 * value (since it's the first level, obviously). */
		for (i = cursed ? 2 : 1; i <= level; i++)
		{
			/* Get pointer to the array that stores the level
			 * improvements for this stat. */
			levarr = (void *) ((char *) CONTR(op) + potion_improve_stat_offsets[indices[stat_id]]);

			if (cursed)
			{
				/* The potion is cursed and there is a stat improvement
				 * for this level, so go ahead and remove the
				 * improvement. */
				if (levarr[i] != 1)
				{
					oldlev = levarr[i];
					levarr[i] = 1;
					fix_player(op);
					insert_spell_effect("meffect_purple", op->map, op->x, op->y);
					play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
					draw_info_format(COLOR_WHITE, op, "The foul potion burns like fire inside you, and your %s decreases by %d!", potion_improve_stat_names[indices[stat_id]], oldlev - levarr[i]);
					return 1;
				}
			}
			else
			{
				/* The stat improvement for this level has not been maxed
				 * yet, so go ahead and increase it. */
				if (levarr[i] != max)
				{
					oldlev = levarr[i];
					levarr[i] = max;
					fix_player(op);
					insert_spell_effect("meffect_yellow", op->map, op->x, op->y);
					play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
					draw_info_format(COLOR_WHITE, op, "You feel a little more perfect, and your %s increases by %d!", potion_improve_stat_names[indices[stat_id]], levarr[i] - oldlev);
					return 1;
				}
			}
		}
	}

	/* No effect (because there is nothing to increase [or decrease, in
	 * case of cursed potions]); inform the player. */
	if (cursed)
	{
		play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
		draw_info(COLOR_WHITE, op, "The potion was foul but had no effect on your tortured body.");
	}
	else
	{
		play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
		draw_info(COLOR_WHITE, op, "The potion had no effect - you are already perfect.");
	}

	return 1;
}

/**
 * Apply a potion that gives player temporary effects (resists, stat
 * increases, etc).
 * @param op Player applying the potion.
 * @param tmp The potion.
 * @return 1 on success, 0 on failure. */
static int potion_special_apply(object *op, object *tmp)
{
	object *force;
	int i, val;

	/* Create a force and copy the effects in. */
	force = get_archetype("force");

	if (!force)
	{
		LOG(llevBug, "potion_special_apply(): Can't create force object.\n");
		return 0;
	}

	force->type = POTION_EFFECT;
	/* Copy the amount of time the effect should last. */
	force->stats.food = tmp->stats.food;
	SET_FLAG(force, FLAG_IS_USED_UP);

	/* Make sure the effect lasts for at least a little while. */
	if (force->stats.food <= 0)
	{
		force->stats.food = 1;
	}

	if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
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
			protection = tmp->protection[i] > 0 ? -tmp->protection[i] : tmp->protection[i];

			/* If the potion is damned, the effects worsen... */
			if (QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				protection *= 2;
			}

			/* Actually set the protection value, but make sure it's in a
			 * valid range. */
			force->protection[i] = MIN(100, MAX(-100, protection));
		}

		insert_spell_effect("meffect_purple", op->map, op->x, op->y);
		play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
	}
	else
	{
		memcpy(force->protection, tmp->protection, sizeof(tmp->protection));
		memcpy(force->attack, tmp->attack, sizeof(tmp->attack));

		insert_spell_effect("meffect_green", op->map, op->x, op->y);
		play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
	}

	/* Copy over stat values. */
	for (i = 0; i < NUM_STATS; i++)
	{
		/* Get the stat value from the potion. */
		val = get_attr_value(&tmp->stats, i);

		/* No value, nothing to do. */
		if (!val)
		{
			continue;
		}

		/* If the potion is cursed/damned and the stat increase is
		 * positive, reverse it. */
		if ((QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)) && val > 0)
		{
			val = -val;
		}

		/* The potion is damned, so the negative effect is worse. */
		if (QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			val *= 2;
		}

		/* Now set the stat value of the force to the one calculcated
		 * above, but make sure it doesn't overflow sint8. */
		change_attr_value(&force->stats, i, MIN(SINT8_MAX, MAX(SINT8_MIN, val)));
	}

	/* Insert the force into the player and apply it. */
	force->speed_left = -1;
	force = insert_ob_in_ob(force, op);
	SET_FLAG(force, FLAG_APPLIED);

	if (!change_abil(op, force))
	{
		draw_info(COLOR_WHITE, op, "Nothing happened.");
	}

	decrease_ob(tmp);
	return 1;
}

/**
 * Apply a potion of minor restoration; cures depleted stats. If the
 * potion is cursed or damned, stats are depleted instead.
 * @param op Player applying the potion.
 * @param tmp The potion.
 * @return 1 on success, 0 on failure. */
static int potion_restoration_apply(object *op, object *tmp)
{
	object *depletion;
	archetype *at;
	int i;

	/* Cursed potion of minor restoration; reverse effects (stats are
	 * depleted). */
	if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
	{
		/* Drain 2 stats if the potion is cursed, 4 if it's damned. */
		for (i = QUERY_FLAG(tmp, FLAG_DAMNED) ? 0 : 2; i < 4; i++)
		{
			drain_stat(op);
		}

		insert_spell_effect("meffect_purple", op->map, op->x, op->y);
		play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
		decrease_ob(tmp);
		return 1;
	}

	at = find_archetype("depletion");

	if (!at)
	{
		LOG(llevBug, "potion_restoration_apply(): Could not find archetype 'depletion'.\n");
		return 0;
	}

	depletion = present_arch_in_ob(at, op);

	if (depletion)
	{
		for (i = 0; i < NUM_STATS; i++)
		{
			if (get_attr_value(&depletion->stats, i))
			{
				draw_info(COLOR_WHITE, op, restore_msg[i]);
			}
		}

		remove_ob(depletion);
		object_destroy(depletion);
		fix_player(op);
	}
	else
	{
		draw_info(COLOR_WHITE, op, "You are not depleted.");
	}

	insert_spell_effect("meffect_green", op->map, op->x, op->y);
	play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
	decrease_ob(tmp);
	return 1;
}

/**
 * Apply a potion.
 * @param op Object applying the potion.
 * @param tmp The potion.
 * @return 1 if the potion was successfully applied, 0 otherwise. */
int apply_potion(object *op, object *tmp)
{
	if (op->type != PLAYER)
	{
		return 0;
	}

	if (!CONTR(op) || !CONTR(op)->exp_ptr[EXP_MAGICAL] || !CONTR(op)->exp_ptr[EXP_WISDOM])
	{
		LOG(llevBug, "apply_potion(): Called with invalid player object (no controller or no magic/wisdom exp object).\n");
		return 0;
	}

	/* Use magic devices skill for using potions. */
	if (!change_skill(op, SK_USE_MAGIC_ITEM))
	{
		return 0;
	}

	/* We are using it, so we now know what it does. */
	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
	{
		identify(tmp);
	}

	/* Potions with temporary effects. */
	if (tmp->last_eat == -1)
	{
		CONTR(op)->stat_potions_used++;
		return potion_special_apply(op, tmp);
	}
	/* Potion of minor restoration (removes depletion). */
	else if (tmp->last_eat == 1)
	{
		CONTR(op)->stat_potions_used++;
		return potion_restoration_apply(op, tmp);
	}
	/* Improvement potion. */
	else if (tmp->last_eat == 2)
	{
		CONTR(op)->stat_potions_used++;
		return potion_improve_apply(op, tmp);
	}

	if (tmp->stats.sp == SP_NO_SPELL)
	{
		draw_info(COLOR_WHITE, op, "Nothing happens as you apply it.");
		decrease_ob(tmp);
		return 0;
	}

	CONTR(op)->stat_potions_used++;

	/* Fire in the player's facing direction, unless the spell is
	 * something like healing or cure disease. */
	cast_spell(op, tmp, spells[tmp->stats.sp].flags & SPELL_DESC_SELF ? 0 : op->facing, tmp->stats.sp, 1, CAST_POTION, NULL);
	decrease_ob(tmp);

	if (!QUERY_FLAG(op, FLAG_REMOVED))
	{
		fix_player(op);
	}

	return 1;
}

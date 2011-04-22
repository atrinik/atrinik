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
	int stat, i, level, max, cursed;
	char *levarr, oldlev;

	/* Shuffle the array so we check all the stats for possible
	 * improvement in random order. */
	permute(indices, 0, POTION_IMPROVE_STATS);
	/* Determine whether the potion is cursed or not. */
	cursed = QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED);

	/* A potion will be used up even if we don't actually increase any
	 * stats. */
	CLEAR_FLAG(tmp, FLAG_APPLIED);
	decrease_ob(tmp);

	/* Check all the stats. As 'indices' has been shuffled, that is used
	 * to determine which stat we're working on, instead of directly
	 * using 'stat'. */
	for (stat = 0; stat < POTION_IMPROVE_STATS; stat++)
	{
		/* Determine level related to this stat, and the maximum possible
		 * value it can be improved to. */
		if (indices[stat] == POTION_IMPROVE_HP)
		{
			level = op->level;
			max = op->arch->clone.stats.maxhp;
		}
		else if (indices[stat] == POTION_IMPROVE_SP)
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
			levarr = (void *) ((char *) CONTR(op) + potion_improve_stat_offsets[indices[stat]]);

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
					new_draw_info_format(NDI_UNIQUE, op, "The foul potion burns like fire inside you, and your %s decreases by %d!", potion_improve_stat_names[indices[stat]], oldlev - levarr[i]);
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
					new_draw_info_format(NDI_UNIQUE, op, "You feel a little more perfect, and your %s increases by %d!", potion_improve_stat_names[indices[stat]], levarr[i] - oldlev);
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
		new_draw_info(NDI_UNIQUE, op, "The potion was foul but had no effect on your tortured body.");
	}
	else
	{
		play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
		new_draw_info(NDI_UNIQUE, op, "The potion had no effect - you are already perfect.");
	}

	return 1;
}

/**
 * Apply a potion.
 * @param op Object applying the potion.
 * @param tmp The potion.
 * @return 1 if the potion was successfully applied, 0 otherwise. */
int apply_potion(object *op, object *tmp)
{
	int i;

	/* some sanity checks */
	if (!op || !tmp || (op->type == PLAYER && (!CONTR(op) || !CONTR(op)->exp_ptr[EXP_MAGICAL] || !CONTR(op)->exp_ptr[EXP_WISDOM])))
	{
		LOG(llevBug,"apply_potion() called with invalid objects! obj: %s (%s - %s) tmp:%s\n", query_name(op, NULL), CONTR(op) ? query_name(CONTR(op)->exp_ptr[EXP_MAGICAL], NULL) : "<no contr>", CONTR(op) ? query_name(CONTR(op)->exp_ptr[EXP_WISDOM], NULL) : "<no contr>", query_name(tmp, NULL));
		return 0;
	}

	if (op->type == PLAYER)
	{
		/* set chosen_skill to "magic device" - that's used when we "use" a potion */
		if (!change_skill(op, SK_USE_MAGIC_ITEM))
		{
			/* no skill, no potion use (dust & balm too!) */
			return 0;
		}

		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			identify(tmp);
		}

		/* special potions. Only players get this */
		if (tmp->last_eat == -1)
		{
			/* create a force and copy the effects in */
			object *force = get_archetype("force");

			if (!force)
			{
				LOG(llevBug, "apply_potion: Can't create force object?\n");
				return 0;
			}

			force->type = POTION_EFFECT;
			/* or it will auto destroy with first tick */
			SET_FLAG(force, FLAG_IS_USED_UP);
			/* how long this force will stay */
			force->stats.food += tmp->stats.food;

			if (force->stats.food <= 0)
			{
				force->stats.food = 1;
			}

			/* negative effects because cursed or damned */
			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				/* now we have a bit work because we change (multiply,...) the
				 * base values of the potion - that can invoke out of bounce
				 * values we must catch here. */

				/* effects stays 3 times longer */
				force->stats.food *= 3;

				for (i = 0; i < NROFATTACKS; i++)
				{
					int tmp_a;
					int tmp_p;

					tmp_a = tmp->attack[i] > 0 ? -tmp->attack[i] : tmp->attack[i];
					tmp_p = tmp->protection[i] > 0 ? -tmp->protection[i] : tmp->protection[i];

					/* double bad effect when damned */
					if (QUERY_FLAG(tmp, FLAG_DAMNED))
					{
						tmp_a *= 2;
						tmp_p *= 2;
					}

					/* we don't want out of bound values ... */
					if ((int) force->protection[i] + tmp_p > 100)
					{
						force->protection[i] = 100;
					}
					else if ((int) force->protection[i] + tmp_p < -100)
					{
						force->protection[i] = -100;
					}
					else
					{
						force->protection[i] += (sint8) tmp_p;
					}

					if ((int) force->attack[i] + tmp_a > 100)
					{
						force->attack[i] = 100;
					}
					else if ((int) force->attack[i] + tmp_a < 0)
					{
						force->attack[i] = 0;
					}
					else
					{
						force->attack[i] += tmp_a;
					}
				}

				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
			}
			/* all positive (when not on default negative) */
			else
			{
				/* we don't must do the hard way like cursed/damned (no multiplication or
				 * sign change). */
				memcpy(force->protection, tmp->protection, sizeof(tmp->protection));
				memcpy(force->attack, tmp->attack, sizeof(tmp->attack));
				insert_spell_effect("meffect_green", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
			}

			/* now copy stats values */
			force->stats.Str = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Str > 0 ? -tmp->stats.Str : tmp->stats.Str) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Str > 0 ? (-tmp->stats.Str) * 2 : tmp->stats.Str * 2) : tmp->stats.Str);
			force->stats.Con = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Con > 0 ? -tmp->stats.Con : tmp->stats.Con) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Con > 0 ? (-tmp->stats.Con) * 2 : tmp->stats.Con * 2) : tmp->stats.Con);
			force->stats.Dex = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Dex > 0 ? -tmp->stats.Dex : tmp->stats.Dex) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Dex > 0 ? (-tmp->stats.Dex) * 2 : tmp->stats.Dex * 2) : tmp->stats.Dex);
			force->stats.Int = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Int > 0 ? -tmp->stats.Int : tmp->stats.Int) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Int > 0 ? (-tmp->stats.Int) * 2 : tmp->stats.Int * 2) : tmp->stats.Int);
			force->stats.Wis = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Wis > 0 ? -tmp->stats.Wis : tmp->stats.Wis) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Wis > 0 ? (-tmp->stats.Wis) * 2 : tmp->stats.Wis * 2) : tmp->stats.Wis);
			force->stats.Pow = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Pow > 0 ? -tmp->stats.Pow : tmp->stats.Pow) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Pow > 0 ? (-tmp->stats.Pow) * 2 : tmp->stats.Pow * 2) : tmp->stats.Pow);
			force->stats.Cha = QUERY_FLAG(tmp, FLAG_CURSED) ? (tmp->stats.Cha > 0 ? -tmp->stats.Cha : tmp->stats.Cha) : (QUERY_FLAG(tmp, FLAG_DAMNED) ? (tmp->stats.Cha > 0 ? (-tmp->stats.Cha) * 2 : tmp->stats.Cha * 2) : tmp->stats.Cha);

			/* kick the force in, and apply it to player */
			force->speed_left = -1;
			force = insert_ob_in_ob(force, op);
			CLEAR_FLAG(tmp, FLAG_APPLIED);
			SET_FLAG(force, FLAG_APPLIED);

			/* implicit fix_player() here */
			if (!change_abil(op, force))
			{
				new_draw_info(NDI_UNIQUE, op, "Nothing happened.");
			}

			decrease_ob(tmp);
			return 1;
		}

		/* Potion of minor restoration */
		if (tmp->last_eat == 1)
		{
			object *depl;
			archetype *at;

			if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
			{
				if (QUERY_FLAG(tmp, FLAG_DAMNED))
				{
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
					drain_stat(op);
				}
				else
				{
					drain_stat(op);
					drain_stat(op);
				}

				fix_player(op);
				decrease_ob(tmp);
				insert_spell_effect("meffect_purple", op->map, op->x, op->y);
				play_sound_map(op->map, CMD_SOUND_EFFECT, "poison.ogg", op->x, op->y, 0, 0);
				return 1;
			}

			if ((at = find_archetype("depletion")) == NULL)
			{
				LOG(llevBug, "BUG: Could not find archetype depletion.");
				return 0;
			}

			depl = present_arch_in_ob(at, op);

			if (depl != NULL)
			{
				for (i = 0; i < NUM_STATS; i++)
				{
					if (get_attr_value(&depl->stats, i))
					{
						new_draw_info(NDI_UNIQUE, op, restore_msg[i]);
					}
				}

				/* in inventory of ... */
				remove_ob(depl);
				fix_player(op);
			}
			else
			{
				new_draw_info(NDI_UNIQUE, op, "You feel a great loss...");
			}

			decrease_ob(tmp);
			insert_spell_effect("meffect_green", op->map, op->x, op->y);
			play_sound_map(op->map, CMD_SOUND_EFFECT, "magic_default.ogg", op->x, op->y, 0, 0);
			return 1;
		}
		/* Improvement potion. */
		else if (tmp->last_eat == 2)
		{
			return potion_improve_apply(op, tmp);
		}
	}

	if (tmp->stats.sp == SP_NO_SPELL)
	{
		new_draw_info(NDI_UNIQUE, op, "Nothing happens as you apply it.");
		decrease_ob(tmp);
		return 0;
	}

	/* A potion that casts a spell.  Healing, restore spellpoint (power
	 * potion) and heroism all fit into this category. */
	if (tmp->stats.sp != SP_NO_SPELL)
	{
		/* apply potion fires in player's facing direction, unless the spell is SELF one, ie, healing or cure illness. */
		cast_spell(op, tmp, spells[tmp->stats.sp].flags & SPELL_DESC_SELF ? 0 : op->facing, tmp->stats.sp, 1, CAST_POTION, NULL);
		decrease_ob(tmp);

		/* if you're dead, no point in doing this... */
		if (!QUERY_FLAG(op, FLAG_REMOVED))
		{
			fix_player(op);
		}

		return 1;
	}

	/* CLEAR_FLAG is so that if the character has other potions
	 * that were grouped with the one consumed, his
	 * stat will not be raised by them.  fix_player just clears
	 * up all the stats. */
	CLEAR_FLAG(tmp, FLAG_APPLIED);
	fix_player(op);
	decrease_ob(tmp);
	return 1;
}

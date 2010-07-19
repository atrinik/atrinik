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
 * Range related commands (casting, shooting, throwing, etc.). */

#include <global.h>
#include <newclient.h>

/**
 * Finds spell by name.
 * @param op The caster.
 * @param params Spell name.
 * @param options Specifies if the search is done with the length of the
 * input spell name, or the length of the stored spell name. This allows
 * you to find out if the spell name entered had extra optional
 * parameters at the end (ie: marking rune \<text\>)
 * @return The index value of the spell in the spells array for a match,
 * -1 if there is no match, -2 if there are multiple matches. */
static int find_spell_byname(object *op, char *params, int options)
{
	/* number of spells known by op */
	int numknown;
	/* number of spell that is being cast */
	int spnum, match = -1, i;
	size_t paramlen = 0;

	/* DMs know all spells */
	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		numknown = NROFREALSPELLS;
	}
	else
	{
		numknown = CONTR(op)->nrofknownspells;
	}

	for (i = 0; i < numknown; i++)
	{
		if (QUERY_FLAG(op, FLAG_WIZ))
		{
			spnum = i;
		}
		else
		{
			spnum = CONTR(op)->known_spells[i];
		}

		if (!options)
		{
			paramlen = strlen(params);
		}

		if (!strncmp(params, spells[spnum].name, options ? strlen(spells[spnum].name) : paramlen))
		{
			/* We already found a match previously - thus params is not
			 * not unique, so return -2 stating this. */
			if (match >= 0)
			{
				return -2;
			}
			else
			{
				match = spnum;
			}
		}
	}

	return match;
}

/**
 * Casts the specified spell.
 * @param op Caster.
 * @param params Spell name.
 * @return 1 on success, 0 otherwise. */
int command_cast_spell(object *op, char *params)
{
	char *cp = NULL;
	rangetype orig_rangetype = CONTR(op)->shoottype;
	int orig_spn = CONTR(op)->chosen_spell;
	/* number of spell that is being cast */
	int spnum = -1, spnum2 = -1, value;

	if (!CONTR(op)->nrofknownspells && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, op, "You don't know any spells.");
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Cast which spell?");
		return 0;
	}

	/* This assumes simply that if the name of
	 * the spell being cast as input by the player is shorter than or
	 * equal to the length of the spell name, then there is no options
	 * but if it is longer, then everything after the spell name is
	 * an option.  It determines if the spell name is shorter or
	 * longer by first iterating through the actual spell names, checking
	 * to the length of the typed in name.  If that fails, then it checks
	 * to the length of each spell name.  If that passes, it assumes that
	 * anything after the length of the actual spell name is extra options
	 * typed in by the player (ie: marking rune Hello there)  */
	if (((spnum2 = spnum = find_spell_byname(op, params, 0)) < 0) && ((spnum = find_spell_byname(op, params, 1)) >= 0))
	{
		params[strlen(spells[spnum].name)] = '\0';
		cp = &params[strlen(spells[spnum].name) + 1];

		if (strncmp(cp, "of ", 3) == 0)
		{
			cp += 3;
		}
	}

	/* We don't know this spell name */
	if (spnum == -1)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You don't know the spell %s.", params);
		return 0;
	}

	CONTR(op)->shoottype = range_magic;
	CONTR(op)->chosen_spell = spnum;

	if (!check_skill_to_fire(op))
	{
		if (!QUERY_FLAG(op, FLAG_WIZ))
		{
			CONTR(op)->chosen_spell = orig_spn;
			return 0;
		}
	}

	CONTR(op)->chosen_spell = orig_spn;
	CONTR(op)->shoottype = orig_rangetype;

	/* We still need to wait */
	if (!check_skill_action_time(op, op->chosen_skill))
	{
		return 0;
	}

	value = cast_spell(op, op, op->facing, spnum, 0, spellNormal, cp);

	if (value)
	{
		CONTR(op)->action_casting = pticks + spells[spnum].time;

		if (spells[spnum].type == SPELL_TYPE_PRIEST)
		{
			op->stats.grace -= value;
		}
		else
		{
			op->stats.sp -= value;
		}

		CONTR(op)->action_timer = (float) (CONTR(op)->action_casting - pticks) / (1000000 / MAX_TIME) * 1000.0f;

		if (CONTR(op)->last_action_timer > 0)
		{
			CONTR(op)->action_timer *= -1;
		}
	}

	return 1;
}

/**
 * Similar to command_cast_spell(), but used from command_fire().
 * @param op Caster.
 * @param params Spell name.
 * @return 1 on success, 0 otherwise.
 * @todo Avoid code repetition by merging with command_cast_spell(). */
int fire_cast_spell(object *op, char *params)
{
	char *cp = NULL;
	rangetype orig_rangetype = CONTR(op)->shoottype;
	int orig_spn = CONTR(op)->chosen_spell;
	/* number of spell that is being cast */
	int spnum = -1, spnum2 = -1;

	if (!CONTR(op)->nrofknownspells && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, op, "You don't know any spells.");
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Cast which spell?");
		return 0;
	}

	/* This assumes simply that if the name of
	 * the spell being cast as input by the player is shorter than or
	 * equal to the length of the spell name, then there is no options
	 * but if it is longer, then everything after the spell name is
	 * an option.  It determines if the spell name is shorter or
	 * longer by first iterating through the actual spell names, checking
	 * to the length of the typed in name.  If that fails, then it checks
	 * to the length of each spell name.  If that passes, it assumes that
	 * anything after the length of the actual spell name is extra options
	 * typed in by the player (ie: marking rune Hello there) */
	if (((spnum2 = spnum = find_spell_byname(op, params, 0)) < 0) && ((spnum = find_spell_byname(op, params, 1)) >= 0))
	{
		params[strlen(spells[spnum].name)] = '\0';
		cp = &params[strlen(spells[spnum].name) + 1];

		if (strncmp(cp, "of ", 3) == 0)
		{
			cp += 3;
		}
	}

	/* We don't know this spell name */
	if (spnum == -1)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You don't know the spell %s.", params);
		return 0;
	}

	CONTR(op)->shoottype = range_magic;
	CONTR(op)->chosen_spell = spnum;

	if (!QUERY_FLAG(op, FLAG_WIZ))
	{
		if (!check_skill_to_fire(op))
		{
			CONTR(op)->chosen_spell = orig_spn;
			CONTR(op)->shoottype = orig_rangetype;
			return 0;
		}
	}

	return 1;
}

/**
 * Check for the validity of a player range.
 * @param op Player to check.
 * @param r Range to check.
 * @retval 1 Range specified is legal - that is, the character has an
 * item that is equipped for that range type.
 * @retval 0 No item of that range type that is usable. */
int legal_range(object *op, int r)
{
	int i;
	object *tmp;

	switch (r)
	{
		/* "Nothing" is always legal */
		case range_none:
			return 1;

		/* Bows */
		case range_bow:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
			{
				if (tmp->type == BOW && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					return 1;
				}
			}

			return 0;

		/* Cast spells */
		case range_magic:
			if (CONTR(op)->nrofknownspells == 0)
			{
				return 0;
			}

			for (i = 0; i < CONTR(op)->nrofknownspells; i++)
			{
				if (CONTR(op)->known_spells[i] == CONTR(op)->chosen_spell)
				{
					return 1;
				}
			}

			CONTR(op)->chosen_spell = CONTR(op)->known_spells[0];
			return 1;

		/* Use wands */
		case range_wand:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
			{
				if (tmp->type == WAND && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
					{
						CONTR(op)->known_spell = 1;
					}
					else
					{
						CONTR(op)->known_spell = 0;
					}

					return 1;
				}
			}

			return 0;

		/* Rod */
		case range_rod:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
			{
				if (tmp->type == ROD && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
					{
						CONTR(op)->known_spell = 1;
					}
					else
					{
						CONTR(op)->known_spell = 0;
					}

					return 1;
				}
			}

			return 0;

		/* Horn */
		case range_horn:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
			{
				if (tmp->type == HORN && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
					{
						CONTR(op)->known_spell = 1;
					}
					else
					{
						CONTR(op)->known_spell = 0;
					}

					return 1;
				}
			}

			return 0;

		/* Use scrolls */
		case range_scroll:
			return 0;

		case range_skill:
			if (op->chosen_skill)
			{
				return 1;
			}

			return 0;
	}

	return 0;
}

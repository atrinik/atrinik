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
 * Range related commands (casting, shooting, throwing, etc.). */

#include <global.h>

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
	int spnum = -1, spnum2 = -1;

	if (!CONTR(op)->nrofknownspells && !QUERY_FLAG(op, FLAG_WIZ))
	{
		draw_info(COLOR_WHITE, op, "You don't know any spells.");
		return 0;
	}

	if (!params)
	{
		draw_info(COLOR_WHITE, op, "Cast which spell?");
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

	/* We don't know this spell. */
	if (spnum == -1)
	{
		draw_info_format(COLOR_WHITE, op, "You don't know the spell %s.", params);
		return 0;
	}

	CONTR(op)->chosen_spell = spnum;
	fire(op, op->facing, FIRE_MODE_SPELL, NULL);

	return 1;
}

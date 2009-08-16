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

/* This file deals with range related commands (casting, shooting,
 * throwing, etc.
 */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif
#include <newclient.h>



/* object *op is the caster, params is the spell name.  We return the index
 * value of the spell in the spells array for a match, -1 if there is no
 * match, -2 if there are multiple matches.  Note that 0 is a valid entry, so
 * we can't use that as failure.
 *
 * Modified 03/24/98 - extra parameter 'options' specifies if the search is
 * done with the length of the input spell name, or the length of the stored
 * spell name.  This allows you to find out if the spell name entered had
 * extra optional parameters at the end (ie: marking rune <text>)
 *
 */
static int find_spell_byname(object *op, char *params, int options)
{
	/* number of spells known by op */
	int numknown;
	/* number of spell that is being cast */
	int spnum;
	int match = -1, i;
	int paramlen = 0;

	/* DMs know all spells */
	if (QUERY_FLAG(op, FLAG_WIZ))
		numknown = NROFREALSPELLS;
	else
		numknown = CONTR(op)->nrofknownspells;

	for (i = 0; i < numknown; i++)
	{
		if (QUERY_FLAG(op, FLAG_WIZ))
			spnum = i;
		else
			spnum = CONTR(op)->known_spells[i];

		if (!options)
			paramlen = strlen(params);

		if (!strncmp(params, spells[spnum].name, options ? (int) strlen(spells[spnum].name) : paramlen))
		{
			/* We already found a match previously - thus params is not
			 * not unique, so return -2 stating this. */
			if (match >= 0)
				return -2;
			else
				match = spnum;
		}
	}

	return match;
}


/* Shows all spells that op knows.  If params is supplied, the must match
 * that.  If cleric is 1, show cleric spells, if not set, show mage
 * spells.
 */
/* disabled - we have now spell list in client
static void show_matching_spells(object *op, char *params, int cleric)
{
    int i,spnum,first_match=0;
    char lev[80], cost[80];

    for (i=0; i<(QUERY_FLAG(op, FLAG_WIZ)?NROFREALSPELLS:CONTR(op)->nrofknownspells); i++) {
	if (QUERY_FLAG(op,FLAG_WIZ)) spnum=i;
	else spnum = CONTR(op)->known_spells[i];

	if (spells[spnum].type != (unsigned int) cleric) continue;
	if (params && strncmp(spells[spnum].name,params, strlen(params)))
		continue;
	if (!first_match) {
	    first_match=1;
	    if (!cleric)
		new_draw_info(NDI_UNIQUE, 0, op, "Mage spells");
	    else
		new_draw_info(NDI_UNIQUE, 0, op, "Priest spells");
	    new_draw_info(NDI_UNIQUE, 0,op,"[ sp] [lev] spell name");
	}
	if (spells[spnum].path & op->path_denied) {
	    strcpy(lev,"den");
            strcpy(cost,"den");
	} else {
	    sprintf(lev,"%3d",spells[spnum].level);
            sprintf(cost,"%3d",SP_level_spellpoint_cost(op,op,spnum));
        }

	new_draw_info_format(NDI_UNIQUE,0,op,"[%s] [%s] %s",
		cost, lev, spells[spnum].name);
    }
}

*/

/* sets up to cast a spell.  op is the caster, params is the spell name,
 * This function use the name of a spell following the /cast command
 * to invoke a spell (cast_spell() does the rest).
 * the next function fire_cast_spell prepares for fire command the same
 * but without invoking the spell. */

int command_cast_spell(object *op, char *params)
{
	char *cp = NULL;
	rangetype orig_rangetype = CONTR(op)->shoottype;
	int orig_spn = CONTR(op)->chosen_spell;
	/* number of spell that is being cast */
	int spnum = -1, spnum2 = -1;
	int value;

	if (!CONTR(op)->nrofknownspells && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You don't know any spells.");
		return 0;
	}

	if (params==NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cast which spell?");
		return 0;
	}

	/* When we control a golem we can't cast again - if we do, it breaks control */
	if (CONTR(op)->golem != NULL)
	{
		send_golem_control(CONTR(op)->golem, GOLEM_CTR_RELEASE);
		remove_friendly_object(CONTR(op)->golem);
		destruct_ob(CONTR(op)->golem);
		CONTR(op)->golem = NULL;
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
			cp += 3;
	}

	/* we don't know this spell name */
	if (spnum == -1)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You don't know the spell %s.", params);
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

	/* we still recover from a casted spell before */
	if (!check_skill_action_time(op, op->chosen_skill))
		return 0;

	value = cast_spell(op, op, op->facing, spnum, 0, spellNormal, cp);

	if (value)
	{
		get_skill_time(op, op->chosen_skill->stats.sp);

		if (spells[spnum].flags & SPELL_DESC_WIS)
			op->stats.grace -= value;
		else
			op->stats.sp -= value;
	}

	CONTR(op)->action_timer = (float)(CONTR(op)->action_casting - global_round_tag) / (1000000 / MAX_TIME) * 1000.0f;
	if (CONTR(op)->last_action_timer > 0)
		CONTR(op)->action_timer *= -1;
	return 1;
}

/* thats from fire command - i put it in here because its very similiar to
 * above. mirror changes from above here too. */
int fire_cast_spell(object *op, char *params)
{
	char *cp = NULL;
	rangetype orig_rangetype = CONTR(op)->shoottype;
	int orig_spn = CONTR(op)->chosen_spell;
	/* number of spell that is being cast */
	int spnum = -1, spnum2 = -1;

	if (!CONTR(op)->nrofknownspells && !QUERY_FLAG(op, FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, 0, op, "You don't know any spells.");
		return 0;
	}

	if (params == NULL)
	{
		new_draw_info(NDI_UNIQUE, 0, op, "Cast which spell?");
		return 0;
	}

	/* When we control a golem we can't cast again - if we do, it breaks control */
	if (CONTR(op)->golem != NULL)
	{
		send_golem_control(CONTR(op)->golem, GOLEM_CTR_RELEASE);
		remove_friendly_object(CONTR(op)->golem);
		destruct_ob(CONTR(op)->golem);
		CONTR(op)->golem = NULL;
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
			cp += 3;
	}

	/* we don't know this spell name */
	if (spnum == -1)
	{
		new_draw_info_format(NDI_UNIQUE, 0, op, "You don't know the spell %s.", params);
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
/**************************************************************************/

/* Returns TRUE if the range specified (int r) is legal - that is,
 * the character has an item that is equipped for that range type.
 * return 0 if there is no item of that range type that is usable. */

int legal_range(object *op, int r)
{
	int i;
	object *tmp;

	switch (r)
	{
			/* "Nothing" is always legal */
		case range_none:
			return 1;

			/* bows */
		case range_bow:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
				if (tmp->type == BOW && QUERY_FLAG(tmp, FLAG_APPLIED))
					return 1;

			return 0;

			/* cast spells */
		case range_magic:
			if (CONTR(op)->nrofknownspells == 0)
				return 0;

			for (i = 0; i < CONTR(op)->nrofknownspells; i++)
				if (CONTR(op)->known_spells[i] == CONTR(op)->chosen_spell)
					return 1;

			CONTR(op)->chosen_spell = CONTR(op)->known_spells[0];
			return 1;

			/* use wands */
		case range_wand:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
				if (tmp->type == WAND && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
						CONTR(op)->known_spell = 1;
					else
						CONTR(op)->known_spell = 0;

					CONTR(op)->chosen_item_spell = tmp->stats.sp;
					return 1;
				}

			return 0;


		case range_rod:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
				if (tmp->type == ROD && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
						CONTR(op)->known_spell = 1;
					else
						CONTR(op)->known_spell = 0;

					CONTR(op)->chosen_item_spell = tmp->stats.sp;
					return 1;
				}

			return 0;


		case range_horn:
			for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
				if (tmp->type == HORN && QUERY_FLAG(tmp, FLAG_APPLIED))
				{
					if (QUERY_FLAG(tmp, FLAG_BEEN_APPLIED) || QUERY_FLAG(tmp, FLAG_IDENTIFIED))
						CONTR(op)->known_spell = 1;
					else
						CONTR(op)->known_spell = 0;

					CONTR(op)->chosen_item_spell = tmp->stats.sp;
					return 1;
				}

			return 0;

			/* Use scrolls */
		case range_scroll:
			return 0;

		case range_skill:
			if (op->chosen_skill)
				return 1;
			else
				return 0;
	}

	return 0;
}

/* this should not trigger in daimonin, but i added send_golem_control() for secure */
void change_spell(object *op, char k)
{
	char buf[MAX_BUF];

	if (CONTR(op)->golem != NULL)
	{
		send_golem_control(CONTR(op)->golem, GOLEM_CTR_RELEASE);
		remove_friendly_object(CONTR(op)->golem);
		destruct_ob(CONTR(op)->golem);
		CONTR(op)->golem = NULL;
	}

	do
	{
		CONTR(op)->shoottype += ((k == '+') ? 1 : -1);

		if (CONTR(op)->shoottype >= range_size)
			CONTR(op)->shoottype = range_none;
		else if (CONTR(op)->shoottype <= range_bottom)
			CONTR(op)->shoottype = (rangetype)(range_size - 1);
	}
	while (!legal_range(op, CONTR(op)->shoottype));

	switch (CONTR(op)->shoottype)
	{
		case range_none:
			strcpy(buf, "No ranged attack chosen.");
			break;

		case range_bow:
		{
			object *tmp;
			for (tmp = op->inv; tmp; tmp = tmp->below)
				if (tmp->type == BOW && QUERY_FLAG(tmp, FLAG_APPLIED))
					break;

			sprintf(buf, "Switched to %s and %s.", query_name(tmp, NULL), tmp && tmp->race ? tmp->race : "nothing");
		}
		break;

		case range_magic:
			sprintf(buf,"Switched to spells (%s).", spells[CONTR(op)->chosen_spell].name);
			break;

		case range_wand:
			sprintf(buf, "Switched to wand (%s).", CONTR(op)->known_spell ? spells[CONTR(op)->chosen_item_spell].name : "unknown");
			break;

		case range_rod:
			sprintf(buf, "Switched to rod (%s).", CONTR(op)->known_spell ? spells[CONTR(op)->chosen_item_spell].name : "unknown");
			break;

		case range_horn:
			sprintf(buf, "Switched to horn (%s).", CONTR(op)->known_spell ? spells[CONTR(op)->chosen_item_spell].name : "unknown");
			break;

		case range_skill:
			sprintf (buf, "Switched to skill: %s", op->chosen_skill ? op->chosen_skill->name : "none");
			break;

		default:
			break;
	}

	new_draw_info(NDI_UNIQUE, 0, op, buf);
}


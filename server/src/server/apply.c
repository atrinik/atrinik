/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Handles objects being applied, and their effect. */

#include <global.h>

/**
 * Make player learn a new spell.
 * @param op The player object learning the new spell.
 * @param spell Spell ID. */
void do_learn_spell(object *op, int spell)
{
	if (op->type != PLAYER)
	{
		return;
	}

	if (check_spell_known(op, spell))
	{
		draw_info_format(COLOR_WHITE, op, "You already know the spell '%s'!", spells[spell].name);
		return;
	}

	play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "learnspell.ogg", 0, 0, 0, 0);
	CONTR(op)->known_spells[CONTR(op)->nrofknownspells++] = spell;

	if (CONTR(op)->nrofknownspells == 1)
	{
		CONTR(op)->chosen_spell = spell;
	}

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_ADD);
	draw_info_format(COLOR_WHITE, op, "You have learned the spell %s!", spells[spell].name);
}

/**
 * Make player forget a spell.
 * @param op Player object to make forget the spell.
 * @param spell ID of the spell. */
void do_forget_spell(object *op, int spell)
{
	int i;

	if (op->type != PLAYER)
	{
		return;
	}

	if (!check_spell_known(op, spell))
	{
		return;
	}

	play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, "lose_some.ogg", 0, 0, 0, 0);
	draw_info_format(COLOR_WHITE, op, "You lose knowledge of %s.", spells[spell].name);

	send_spelllist_cmd(op, spells[spell].name, SPLIST_MODE_REMOVE);

	for (i = 0; i < CONTR(op)->nrofknownspells; i++)
	{
		if (CONTR(op)->known_spells[i] == spell)
		{
			CONTR(op)->known_spells[i] = CONTR(op)->known_spells[--CONTR(op)->nrofknownspells];
			return;
		}
	}
}

/**
 * Main apply handler.
 *
 * Checks for unpaid items before applying.
 * @param op ::object causing tmp to be applied.
 * @param tmp ::object being applied.
 * @param aflag Special (always apply/unapply) flags. Nothing is done
 * with them in this function - they are passed to apply_special().
 * @retval 0 Player or monster can't apply objects of that type.
 * @retval 1 Has been applied, or there was an error applying the object.
 * @retval 2 Objects of that type can't be applied if not in
 * inventory. */
int manual_apply(object *op, object *tmp, int aflag)
{
	tmp = HEAD(tmp);

	if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		if (op->type == PLAYER)
		{
			draw_info(COLOR_WHITE, op, "You should pay for it first.");
			return OBJECT_METHOD_OK;
		}
		/* Monsters just skip unpaid items */
		else
		{
			return OBJECT_METHOD_UNHANDLED;
		}
	}

	/* Monsters must not apply random chests. */
	if (op->type != PLAYER && tmp->type == TREASURE)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	/* Trigger the APPLY event */
	if (!(aflag & AP_NO_EVENT) && trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, aflag, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return OBJECT_METHOD_OK;
	}

	/* Trigger the map-wide apply event. */
	if (!(aflag & AP_NO_EVENT) && op->map && op->map->events)
	{
		int retval = trigger_map_event(MEVENT_APPLY, op->map, op, tmp, NULL, NULL, aflag);

		if (retval != -1)
		{
			return retval;
		}
	}

	aflag &= ~AP_NO_EVENT;

	if (tmp->item_level)
	{
		int tmp_lev;

		if (tmp->item_skill && op->type == PLAYER)
		{
			tmp_lev = CONTR(op)->skill_ptr[tmp->item_skill]->level;
		}
		else
		{
			tmp_lev = op->level;
		}

		if (tmp->item_level > tmp_lev)
		{
			draw_info(COLOR_WHITE, op, "The item's level is too high to apply.");
			return OBJECT_METHOD_OK;
		}
	}

	return object_apply(tmp, op, aflag);
}

/**
 * Living thing is applying an object.
 * @param pl ::object causing op to be applied.
 * @param op ::object being applied.
 * @param aflag Special (always apply/unapply) flags. Nothing is done
 * with them in this function - they are passed to apply_special().
 * @param quiet If 1, suppresses the "don't know how to apply" and "you
 * must get it first" messages as needed by player_apply_below(). There
 * can still be "but you are floating high above the ground" messages.
 * @retval 0 Player or monster can't apply objects of that type.
 * @retval 1 Has been applied, or there was an error applying the object.
 * @retval 2 Objects of that type can't be applied if not in
 * inventory. */
int player_apply(object *pl, object *op, int aflag, int quiet)
{
	int tmp;

	if (op->env == NULL && QUERY_FLAG(pl, FLAG_FLYING))
	{
		/* Player is flying and applying object not in inventory */
		if (!QUERY_FLAG(op, FLAG_FLYING) && !QUERY_FLAG(op, FLAG_FLY_ON))
		{
			draw_info(COLOR_WHITE, pl, "But you are floating high above the ground!");
			return 0;
		}
	}

	tmp = manual_apply(pl, op, aflag);

	if (!quiet)
	{
		if (tmp == OBJECT_METHOD_UNHANDLED)
		{
			draw_info_format(COLOR_WHITE, pl, "I don't know how to apply the %s.", query_name(op, NULL));
		}
		else if (tmp == OBJECT_METHOD_ERROR)
		{
			if (op->env != pl)
			{
				draw_info_format(COLOR_WHITE, pl, "You must get it first!\n");
			}
		}
	}

	return tmp;
}

/**
 * Attempt to apply the object 'below' the player.
 *
 * If the player has an open container, we use that for below, otherwise
 * we use the ground.
 * @param pl Player. */
void player_apply_below(object *pl)
{
	object *tmp, *next;
	int floors;

	if (pl->type != PLAYER)
	{
		return;
	}

	tmp = pl->below;

	/* This is perhaps more complicated.  However, I want to make sure that
	 * we don't use a corrupt pointer for the next object, so we get the
	 * next object in the stack before applying.  This is can only be a
	 * problem if player_apply() has a bug in that it uses the object but does
	 * not return a proper value. */
	for (floors = 0; tmp != NULL; tmp = next)
	{
		next = tmp->below;

		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
		{
			floors++;
		}
		/* Process only floor objects after first floor object */
		else if (floors > 0)
		{
			return;
		}

		if (!IS_INVISIBLE(tmp, pl) || QUERY_FLAG(tmp, FLAG_WALK_ON) || QUERY_FLAG(tmp, FLAG_FLY_ON))
		{
			if (player_apply(pl, tmp, 0, 1) == 1)
			{
				return;
			}
		}

		/* Process at most two floor objects */
		if (floors >= 2)
		{
			return;
		}
	}
}

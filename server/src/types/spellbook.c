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
 * Handles code for @ref SPELLBOOK "spellbooks". */

#include <global.h>

static void spellbook_failure(object *op, int failure, int power);

/**
 * Apply a spellbook.
 * @param op Object applying the spellbook.
 * @param tmp The spellbook. */
void apply_spellbook(object *op, object *tmp)
{
	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op,FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to read while blind.");
		return;
	}

	/* artifact_spellbooks have 'slaying' field point to a spell name,
	 * instead of having their spell stored in stats.sp.  We should update
	 * stats->sp to point to that spell */
	if (tmp->slaying != NULL)
	{
		if ((tmp->stats.sp = look_up_spell_name(tmp->slaying)) < 0)
		{
			tmp->stats.sp = -1;
			new_draw_info_format(NDI_UNIQUE, op, "The book's formula for %s is incomplete.", tmp->slaying);
			return;
		}
		/* now clear tmp->slaying since we no longer need it */
		FREE_AND_CLEAR_HASH2(tmp->slaying);
	}

	/* need a literacy skill to learn spells. Also, having a literacy level
	 * lower than the spell will make learning the spell more difficult */
	if (!change_skill(op, SK_LITERACY))
	{
		new_draw_info(NDI_UNIQUE, op, "You can't read! Your attempt fails.");
		return;
	}

	if (tmp->stats.sp < 0 || tmp->stats.sp >= NROFREALSPELLS || spells[tmp->stats.sp].level > (SK_level(op) + 10))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to decipher the strange symbols.");
		return;
	}

	new_draw_info_format(NDI_UNIQUE, op, "The spellbook contains the %s level spell %s.", get_levelnumber(spells[tmp->stats.sp].level), spells[tmp->stats.sp].name);

	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
	{
		identify(tmp);
		if (tmp->env)
			esrv_update_item(UPD_FLAGS|UPD_NAME,op,tmp);
		else
			CONTR(op)->socket.update_tile = 0;
	}

	if (check_spell_known(op, tmp->stats.sp) && (tmp->stats.Wis || find_special_prayer_mark(op, tmp->stats.sp) == NULL))
	{
		new_draw_info(NDI_UNIQUE, op, "You already know that spell.\n");
		return;
	}

	/* I changed spell learning in 3 ways:
	 *
	 *  1- MU spells use Int to learn, Cleric spells use Wisdom
	 *
	 *  2- The learner's level (in skills sytem level==literacy level; if no
	 *     skills level == overall level) impacts the chances of spell learning.
	 *
	 *  3 -Automatically fail to learn if you read while confused
	 *
	 * Overall, chances are the same but a player will find having a high
	 * literacy rate very useful!  -b.t. */
	if (QUERY_FLAG(op, FLAG_CONFUSED))
	{
		new_draw_info(NDI_UNIQUE, op, "In your confused state you flub the wording of the text!");
		spellbook_failure(op, 0 - rndm(0, spells[tmp->stats.sp].level), spells[tmp->stats.sp].sp);
	}
	else if (QUERY_FLAG(tmp, FLAG_STARTEQUIP) || rndm(0, 149) - (2 * SK_level(op)) < learn_spell[spells[tmp->stats.sp].type == SPELL_TYPE_PRIEST ? op->stats.Wis : op->stats.Int])
	{
		new_draw_info(NDI_UNIQUE, op, "You succeed in learning the spell!");
		do_learn_spell(op, tmp->stats.sp, 0);

		/* xp gain to literacy for spell learning */
		if (!QUERY_FLAG(tmp, FLAG_STARTEQUIP))
		{
			add_exp(op, calc_skill_exp(op, tmp, -1), op->chosen_skill->stats.sp);
		}
	}
	else
	{
		play_sound_player_only(CONTR(op), SOUND_FUMBLE_SPELL, NULL, 0, 0, 0, 0);
		new_draw_info(NDI_UNIQUE, op, "You fail to learn the spell.\n");
	}

	decrease_ob(tmp);
}

/**
 * Called when we try to learn a spell from spellbook by applying
 * a spellbook in apply_spellbook().
 * @param op The object that applied the spellbook.
 * @param failure Failure power.
 * @param power Spell points. */
static void spellbook_failure(object *op, int failure, int power)
{
	/* set minimum effect */
	if (abs(failure / 4) > power)
	{
		power = abs(failure / 4);
	}

	/* wonder */
	if (failure <= -1 && failure > -15)
	{
		new_draw_info(NDI_UNIQUE, op, "Your spell warps!");
#if 0
		cast_cone(op, op, 0, 10, SP_WOW, spellarch[SP_WOW]);
#endif
	}
	/* drain mana */
	else if (failure <= -15 && failure > -35)
	{
		new_draw_info(NDI_UNIQUE, op, "Your mana is drained!");
		op->stats.sp -= rndm(0, power - 1);

		if (op->stats.sp < 0)
		{
			op->stats.sp = 0;
		}
	}
}

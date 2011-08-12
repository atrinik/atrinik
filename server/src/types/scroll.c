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
 * Handles code for @ref SCROLL "scrolls". */

#include <global.h>

/**
 * Apply a scroll.
 * @param op Object applying the scroll.
 * @param tmp The scroll. */
void apply_scroll(object *op, object *tmp)
{
	int scroll_spell = tmp->stats.sp, old_spell = 0;
	rangetype old_shoot = range_none;

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ))
	{
		draw_info(COLOR_WHITE, op, "You are unable to read while blind.");
		return;
	}

	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
	{
		identify(tmp);
	}

	if (scroll_spell < 0 || scroll_spell >= NROFREALSPELLS)
	{
		draw_info(COLOR_WHITE, op, "The scroll just doesn't make sense!");
		return;
	}

	if (op->type == PLAYER)
	{
		/* players need a literacy skill to read stuff! */
		if (!change_skill(op, SK_LITERACY))
		{
			draw_info(COLOR_WHITE, op, "You are unable to decipher the strange symbols.");
			return;
		}

		/* that's new: literacy for reading but a player need also the
		 * right spellcasting spell. Reason: the exp goes then in that
		 * skill. This makes scroll different from wands or potions. */
		if (!change_skill(op, (spells[scroll_spell].type == SPELL_TYPE_PRIEST ? SK_PRAYING : SK_SPELL_CASTING)))
		{
			draw_info(COLOR_WHITE, op, "You can read the scroll but you don't understand it.");
			return;
		}

		/* Now, call here so the right skill is readied -- literacy
		 * isn't necessarily connected to the exp obj to which the xp
		 * will go (for kills made by the magic of the scroll)  */
		old_shoot = CONTR(op)->shoottype;
		old_spell = CONTR(op)->chosen_spell;
		CONTR(op)->shoottype = range_scroll;
		CONTR(op)->chosen_spell = scroll_spell;
		CONTR(op)->stat_scrolls_used++;
	}

	draw_info_format(COLOR_WHITE, op, "The scroll of %s turns to dust.", spells[tmp->stats.sp].name);

	cast_spell(op, tmp, op->facing ? op->facing : 4, scroll_spell, 0, CAST_SCROLL, NULL);
	decrease_ob(tmp);

	if (op->type == PLAYER)
	{
		CONTR(op)->shoottype = old_shoot;
		CONTR(op)->chosen_spell = old_spell;
	}
}

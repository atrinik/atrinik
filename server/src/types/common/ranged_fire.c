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
 * Common range firing code.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::ranged_fire_func */
int common_object_ranged_fire(object *op, object *shooter, int dir)
{
	object *skill;

	if (!dir)
	{
		draw_info(COLOR_WHITE, shooter, "You can't throw that at yourself.");
		return OBJECT_METHOD_UNHANDLED;
	}

	if (QUERY_FLAG(op, FLAG_STARTEQUIP))
	{
		draw_info(COLOR_WHITE, shooter, "The gods won't let you throw that.");
		return OBJECT_METHOD_UNHANDLED;
	}

	if (op->weight <= 0)
	{
		draw_info_format(COLOR_WHITE, shooter, "You can't throw %s.", query_base_name(op, shooter));
		return OBJECT_METHOD_UNHANDLED;
	}

	if (QUERY_FLAG(op, FLAG_APPLIED))
	{
		if (OBJECT_CURSED(op))
		{
			draw_info_format(COLOR_WHITE, shooter, "The %s sticks to your hand!", query_base_name(op, shooter));
			return OBJECT_METHOD_UNHANDLED;
		}

		if (object_apply_item(op, shooter, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK)
		{
			return OBJECT_METHOD_UNHANDLED;
		}
	}

	if (shooter->chosen_skill)
	{
		shooter->chosen_skill->stats.maxsp = op->last_grace;
	}

	if (QUERY_FLAG(op, FLAG_DUST))
	{
		if ((shooter->type == PLAYER && !change_skill(shooter, SK_USE_MAGIC_ITEM)) || op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS || !spells[op->stats.sp].archname || !(spells[op->stats.sp].flags & SPELL_DESC_DIRECTION))
		{
			draw_info_format(COLOR_WHITE, shooter, "You scatter the %s, but nothing happens.", query_short_name(op, shooter));
		}
		else
		{
			draw_info_format(COLOR_WHITE, shooter, "You scatter the %s.", query_short_name(op, shooter));
			cast_cone(shooter, op, dir, 10, op->stats.sp, spellarch[op->stats.sp]);
		}

		decrease_ob(op);

		return OBJECT_METHOD_OK;
	}

	op = object_stack_get_removed(op, 1);

	/* Save original WC, damage and range. */
	op->last_heal = op->stats.wc;
	op->stats.hp = op->stats.dam;
	op->last_grace = op->last_sp;

	/* Calculate moving speed. */
	op->speed = MIN(1.0f, (speed_bonus[shooter->stats.Str] + 1.0f) / 1.5f);

	skill = SK_skill(shooter);

	/* Add skill bonuses. */
	if (skill)
	{
		op->stats.wc += skill->last_heal;
	}

	op->stats.dam += op->magic;
	op->stats.wc += op->magic;

	if (shooter->type == PLAYER)
	{
		op->stats.dam += dam_bonus[shooter->stats.Str] / 2;
		op->stats.wc += thaco_bonus[shooter->stats.Dex];
	}
	else
	{
		op->stats.wc += 5;
	}

	op->stats.dam = (sint16) ((double) op->stats.dam * LEVEL_DAMAGE(SK_level(shooter)));
	op->stats.wc += SK_level(shooter);
	op->stats.dam = MAX(0, (sint16) (((double) op->stats.dam / 100.0f) * (double) op->item_condition));

	op = object_projectile_fire(op, shooter, dir);

	if (!op)
	{
		return OBJECT_METHOD_OK;
	}

	if (shooter->type == PLAYER)
	{
		CONTR(shooter)->stat_missiles_thrown++;
	}

	play_sound_map(shooter->map, CMD_SOUND_EFFECT, "throw.ogg", shooter->x, shooter->y, 0, 0);

	return OBJECT_METHOD_OK;
}
